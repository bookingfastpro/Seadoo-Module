// Fonction Supabase "ingest" : recoit les points envoyes par les boitiers.
// A deployer SANS verification JWT (le boitier s'authentifie par sa cle).
// Reponse = consignes pour le boitier.
import { createClient } from "npm:@supabase/supabase-js@2";

const supabase = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
);

async function sha256Hex(text: string): Promise<string> {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function distanceM(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const rad = Math.PI / 180;
  const dLat = (lat2 - lat1) * rad;
  const dLon = (lon2 - lon1) * rad;
  const a = Math.sin(dLat / 2) ** 2 +
    Math.cos(lat1 * rad) * Math.cos(lat2 * rad) * Math.sin(dLon / 2) ** 2;
  return 2 * 6371000 * Math.asin(Math.sqrt(a));
}

const num = (v: unknown): number | null =>
  typeof v === "number" && Number.isFinite(v) ? v : null;

Deno.serve(async (req) => {
  if (req.method !== "POST") return new Response("method not allowed", { status: 405 });

  let p: Record<string, unknown>;
  try {
    p = await req.json();
  } catch {
    return new Response("bad json", { status: 400 });
  }
  if (typeof p.device_id !== "string" || typeof p.key !== "string") {
    return new Response("missing device_id/key", { status: 400 });
  }

  const { data: dev } = await supabase.from("devices").select("*").eq("id", p.device_id)
    .maybeSingle();
  if (!dev || dev.key_hash !== await sha256Hex(p.key)) {
    return new Response("unauthorized", { status: 401 });
  }

  const lat = num(p.lat), lon = num(p.lon);
  const fix = p.fix === true && lat !== null && lon !== null;

  const { error } = await supabase.from("telemetry").insert({
    device_id: dev.id,
    fix,
    lat: fix ? lat : null,
    lon: fix ? lon : null,
    gps_speed_kmh: num(p.gps_speed_kmh),
    hdop: num(p.hdop),
    can_alive: p.can_alive === true,
    rpm: num(p.rpm),
    speed_kmh: num(p.speed_kmh),
    fuel_pct: num(p.fuel_pct),
    engine_hours: num(p.engine_hours),
    coolant_c: num(p.coolant_c),
    batt_v: num(p.batt_v),
    rssi: num(p.rssi),
  });
  if (error) return new Response(error.message, { status: 500 });

  // Cloture virtuelle : alerte au franchissement uniquement (pas a chaque point).
  let outside = dev.outside_geofence as boolean;
  if (fix && dev.geofence_lat !== null && dev.geofence_lon !== null && dev.geofence_radius_m) {
    const d = distanceM(lat!, lon!, dev.geofence_lat, dev.geofence_lon);
    const nowOutside = d > dev.geofence_radius_m;
    if (nowOutside !== outside) {
      await supabase.from("alerts").insert({
        device_id: dev.id,
        kind: nowOutside ? "geofence_exit" : "geofence_enter",
        message: nowOutside
          ? `${dev.name} est sorti de la zone (${Math.round(d)} m du centre)`
          : `${dev.name} est revenu dans la zone`,
      });
      outside = nowOutside;
    }
  }

  const update: Record<string, unknown> = {
    last_seen: new Date().toISOString(),
    last_can_alive: p.can_alive === true,
    inhibit_active: p.inhibit_active === true,
    outside_geofence: outside,
  };
  if (fix) {
    update.last_lat = lat;
    update.last_lon = lon;
  }
  await supabase.from("devices").update(update).eq("id", dev.id);

  return Response.json({
    start_inhibit: dev.start_inhibit,
    active_interval_s: dev.active_interval_s,
    parked_interval_s: dev.parked_interval_s,
  });
});
