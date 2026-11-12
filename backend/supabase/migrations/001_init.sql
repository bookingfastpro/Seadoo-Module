-- Schema du serveur de suivi Sea-Doo (Supabase / Postgres)

create extension if not exists pgcrypto;

-- Un boitier par jet-ski
create table public.devices (
  id                text primary key,              -- ex. 'gtx-2023' (= DEVICE_ID du firmware)
  name              text not null,
  key_hash          text not null,                 -- sha256 hex de DEVICE_KEY
  start_inhibit     boolean not null default false,-- consigne anti-demarrage
  active_interval_s integer not null default 30,
  parked_interval_s integer not null default 1800,
  collision_distance_m integer not null default 120,  -- seuil d'alerte anti-collision
  -- cloture virtuelle (optionnelle)
  geofence_lat      double precision,
  geofence_lon      double precision,
  geofence_radius_m integer,
  -- dernier etat connu (mis a jour par la fonction ingest)
  last_seen         timestamptz,
  last_lat          double precision,
  last_lon          double precision,
  last_can_alive    boolean,
  inhibit_active    boolean,
  outside_geofence  boolean not null default false
);

create table public.telemetry (
  id            bigint generated always as identity primary key,
  device_id     text not null references public.devices(id) on delete cascade,
  ts            timestamptz not null default now(),
  fix           boolean not null default false,
  lat           double precision,
  lon           double precision,
  gps_speed_kmh real,
  hdop          real,
  can_alive     boolean,
  rpm           real,
  speed_kmh     real,
  fuel_pct      real,
  engine_hours  real,
  coolant_c     real,
  batt_v        real,
  rssi          smallint
);
create index telemetry_device_ts on public.telemetry (device_id, ts desc);

create table public.alerts (
  id         bigint generated always as identity primary key,
  device_id  text not null references public.devices(id) on delete cascade,
  ts         timestamptz not null default now(),
  kind       text not null,                        -- 'geofence_exit', 'geofence_enter', 'low_battery'
  message    text not null
);
create index alerts_device_ts on public.alerts (device_id, ts desc);

-- Securite : seuls les utilisateurs connectes (vous) lisent les donnees.
-- Les boitiers n'accedent jamais aux tables : ils passent par la fonction
-- "ingest", qui verifie leur cle et ecrit avec le role de service.
alter table public.devices   enable row level security;
alter table public.telemetry enable row level security;
alter table public.alerts    enable row level security;

create policy "lecture devices"   on public.devices   for select to authenticated using (true);
create policy "reglage devices"   on public.devices   for update to authenticated using (true) with check (true);
create policy "lecture telemetry" on public.telemetry for select to authenticated using (true);
create policy "lecture alerts"    on public.alerts    for select to authenticated using (true);

-- La cle ne doit pas etre lisible depuis le tableau de bord
-- (un retrait par colonne est sans effet tant que le droit existe sur la table :
--  on retire le droit global puis on redonne colonne par colonne)
revoke all on public.devices from authenticated, anon;
grant select (id, name, start_inhibit, active_interval_s, parked_interval_s,
              collision_distance_m, geofence_lat, geofence_lon, geofence_radius_m,
              last_seen, last_lat, last_lon, last_can_alive, inhibit_active, outside_geofence)
  on public.devices to authenticated;
grant update (name, start_inhibit, active_interval_s, parked_interval_s,
              collision_distance_m, geofence_lat, geofence_lon, geofence_radius_m)
  on public.devices to authenticated;

-- Enregistrer un boitier (a lancer une fois par jet-ski, dans l'editeur SQL) :
--   insert into public.devices (id, name, key_hash)
--   values ('gtx-2023', 'GTX 2023',
--           encode(digest('LA-MEME-CLE-QUE-DEVICE_KEY', 'sha256'), 'hex'));
