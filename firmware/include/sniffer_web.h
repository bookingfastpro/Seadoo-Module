// Page web locale du mode sniffer.
// Le boitier cree son propre reseau WiFi « NAUTIX-xxxx » : on s'y connecte avec
// un telephone ou un PC, puis on ouvre http://192.168.4.1 pour voir en direct :
// etat du boitier, carte SD, compteurs CAN et tableau des identifiants recus
// (les octets qui viennent de changer sont surlignes : pratique pour decoder
// le bus pendant qu'on fait varier le regime avec la valise BRP).
#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include "driver/twai.h"

#ifndef SNIFFER_AP_PASS
#define SNIFFER_AP_PASS "nautix123"   // 8 caracteres minimum
#endif

namespace snifweb {

struct IdEntry {
  uint32_t id;
  uint8_t ext, dlc;
  uint8_t data[8];
  uint32_t count;
  uint32_t lastMs;
  uint32_t changedMs[8];   // instant du dernier changement de chaque octet
};

static const int MAX_IDS = 96;
static IdEntry ids[MAX_IDS];
static int idCount = 0;
static uint32_t *frameCount = nullptr;
static char sdFile[24] = "";
static bool sdOk = false;
static uint32_t busErrors = 0, rxMissed = 0;
static char apName[24];
// GPS : 0 = modem en demarrage, 1 = recherche des satellites, 2 = position trouvee, 3 = modem muet
static int gpsState = 0;
static float gpsLat = 0, gpsLon = 0, gpsSpeed = 0, gpsAlt = 0, gpsAcc = 0;
static int gpsVsat = 0, gpsUsat = 0;
static uint32_t gpsFixMs = 0;

static void setGps(int state, bool fix, float lat, float lon, float speed, float alt, int vsat,
                   int usat, float acc) {
  gpsState = state;
  gpsVsat = vsat;
  gpsUsat = usat;
  if (fix) {
    gpsLat = lat; gpsLon = lon; gpsSpeed = speed; gpsAlt = alt; gpsAcc = acc;
    gpsFixMs = millis() | 1;
  }
}
static WebServer server(80);

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NAUTIX - sniffer CAN</title>
<style>
body{margin:0;font:14px system-ui,sans-serif;background:#0b1720;color:#e6eef3}
header{padding:14px 16px;background:#102634;display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}
h1{margin:0;font-size:20px;letter-spacing:2px}h1 small{font-size:12px;color:#7fb3c9;letter-spacing:0}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px;padding:12px 16px}
.card{background:#132e3e;border-radius:10px;padding:10px 12px}
.card b{display:block;font-size:22px;margin-top:4px}.card span{color:#8fb0c0;font-size:12px}
.ok{color:#5be39a}.ko{color:#ff7a6b}.warn{color:#ffc75e}
table{width:calc(100% - 32px);margin:0 16px 20px;border-collapse:collapse;font:13px ui-monospace,monospace}
th,td{padding:5px 6px;text-align:left;border-bottom:1px solid #1d3d50}
th{color:#8fb0c0;font-weight:600;position:sticky;top:0;background:#0b1720}
td.b{width:2.2em;text-align:center;border-radius:4px}td.hot{background:#ffb02e;color:#000}
.card span#gpsd{display:block;margin-top:4px}
.note{padding:0 16px 16px;color:#8fb0c0;font-size:12px}
</style></head><body>
<header><h1>NAUTIX <small>by WinO</small></h1><span id="dev" style="color:#8fb0c0"></span></header>
<div class="cards">
<div class="card"><span>Boitier</span><b id="st" class="ok">connecte</b></div>
<div class="card"><span>Allume depuis</span><b id="up">-</b></div>
<div class="card"><span>GPS</span><b id="gps">-</b><span id="gpsd"></span></div>
<div class="card"><span>Carte SD</span><b id="sd">-</b></div>
<div class="card"><span>Trames CAN</span><b id="fr">-</b></div>
<div class="card"><span>Trames / s</span><b id="fps">-</b></div>
<div class="card"><span>Erreurs bus</span><b id="be">-</b></div>
<div class="card"><span>Trames perdues</span><b id="mi">-</b></div>
<div class="card"><span>Identifiants vus</span><b id="ni">-</b></div>
</div>
<table><thead><tr><th>ID</th><th>DLC</th><th>Nb</th><th>Hz</th>
<th>b0</th><th>b1</th><th>b2</th><th>b3</th><th>b4</th><th>b5</th><th>b6</th><th>b7</th></tr></thead>
<tbody id="tb"></tbody></table>
<p class="note">Orange = octet modifie il y a moins d'une seconde. Le bus est enregistre en continu sur la carte SD.</p>
<script>
let last=null,lastT=0,prev={};
function fmt(s){s=Math.floor(s);const h=Math.floor(s/3600),m=Math.floor(s/60)%60;return (h?h+'h ':'')+m+'min '+(s%60)+'s'}
async function tick(){
 try{
  const r=await fetch('/data',{cache:'no-store'});const d=await r.json();
  document.getElementById('st').textContent='connecte';document.getElementById('st').className='ok';
  document.getElementById('dev').textContent=d.dev+' - '+d.ap;
  document.getElementById('up').textContent=fmt(d.ms/1000);
  const sd=document.getElementById('sd');sd.textContent=d.sd?d.file:'absente';sd.className=d.sd?'ok':'warn';
  const g=d.gps,ge=document.getElementById('gps'),gd=document.getElementById('gpsd');
  if(g.st==0){ge.textContent='demarrage';ge.className='warn';gd.textContent='modem en cours de demarrage (~20 s)'}
  else if(g.st==3){ge.textContent='modem muet';ge.className='ko';gd.textContent='le modem ne repond pas'}
  else if(g.st==2){ge.textContent='position OK';ge.className='ok';
   gd.textContent=g.lat.toFixed(6)+', '+g.lon.toFixed(6)+' - '+g.su+' satellites - '+g.spd.toFixed(1)+' km/h'}
  else{ge.textContent='recherche';ge.className='warn';
   gd.textContent=(g.sv?g.sv+' satellites visibles':'recherche en cours')+(g.age>=0?' (derniere position il y a '+Math.round(g.age)+' s)':'')+' - antenne GPS dehors ou pres d\'une fenetre'}
  document.getElementById('fr').textContent=d.frames;
  const now=performance.now();
  if(last!==null){document.getElementById('fps').textContent=Math.round((d.frames-last)*1000/(now-lastT))}
  last=d.frames;lastT=now;
  const be=document.getElementById('be');be.textContent=d.be;be.className=d.be>0?'ko':'ok';
  const mi=document.getElementById('mi');mi.textContent=d.miss;mi.className=d.miss>0?'warn':'ok';
  document.getElementById('ni').textContent=d.ids.length;
  let h='';
  d.ids.sort((a,b)=>a.id-b.id).forEach(e=>{
   const p=prev[e.id];const hz=p?((e.n-p.n)*1000/(d.ms-p.ms)).toFixed(1):'-';
   prev[e.id]={n:e.n,ms:d.ms};
   h+='<tr><td>0x'+e.id.toString(16).toUpperCase()+(e.x?' (ext)':'')+'</td><td>'+e.l+'</td><td>'+e.n+'</td><td>'+hz+'</td>';
   for(let i=0;i<8;i++){const v=i<e.l?e.d[i].toString(16).toUpperCase().padStart(2,'0'):'';
    h+='<td class="b'+((e.c>>i)&1?' hot':'')+'">'+v+'</td>'}
   h+='</tr>'});
  document.getElementById('tb').innerHTML=h;
 }catch(e){const s=document.getElementById('st');s.textContent='deconnecte';s.className='ko'}
}
setInterval(tick,700);tick();
</script></body></html>)HTML";

static void handleRoot() { server.send_P(200, "text/html; charset=utf-8", PAGE); }

static void handleData() {
  uint32_t now = millis();
  String s;
  s.reserve(160 + idCount * 90);
  s += "{\"dev\":\"" DEVICE_ID "\",\"ap\":\"";
  s += apName;
  s += "\",\"ms\":";
  s += now;
  s += ",\"sd\":";
  s += sdOk ? "true" : "false";
  s += ",\"file\":\"";
  s += sdFile;
  s += "\",\"frames\":";
  s += frameCount ? *frameCount : 0;
  s += ",\"be\":";
  s += busErrors;
  s += ",\"miss\":";
  s += rxMissed;
  s += ",\"gps\":{\"st\":";
  s += gpsState;
  s += ",\"lat\":";
  s += String(gpsLat, 6);
  s += ",\"lon\":";
  s += String(gpsLon, 6);
  s += ",\"spd\":";
  s += String(gpsSpeed, 1);
  s += ",\"alt\":";
  s += String(gpsAlt, 0);
  s += ",\"sv\":";
  s += gpsVsat;
  s += ",\"su\":";
  s += gpsUsat;
  s += ",\"age\":";
  s += gpsFixMs ? String((now - gpsFixMs) / 1000.0f, 0) : String(-1);
  s += "}";
  s += ",\"ids\":[";
  for (int i = 0; i < idCount; i++) {
    const IdEntry &e = ids[i];
    uint8_t changed = 0;
    for (int b = 0; b < 8; b++)
      if (e.changedMs[b] && now - e.changedMs[b] < 1000) changed |= 1 << b;
    if (i) s += ',';
    s += "{\"id\":";
    s += e.id;
    s += ",\"x\":";
    s += e.ext;
    s += ",\"l\":";
    s += e.dlc;
    s += ",\"n\":";
    s += e.count;
    s += ",\"c\":";
    s += changed;
    s += ",\"d\":[";
    for (int b = 0; b < 8; b++) {
      if (b) s += ',';
      s += e.data[b];
    }
    s += "]}";
  }
  s += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

// A appeler une fois au debut du mode sniffer.
static void begin(uint32_t *frames, bool sd, const char *file) {
  frameCount = frames;
  sdOk = sd;
  strncpy(sdFile, file ? file : "", sizeof(sdFile) - 1);
  uint8_t mac[6];
  WiFi.mode(WIFI_AP);
  WiFi.softAPmacAddress(mac);
  snprintf(apName, sizeof(apName), "NAUTIX-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(apName, SNIFFER_AP_PASS);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.printf("# WiFi : reseau \"%s\" (mot de passe \"%s\") -> http://%s\n", apName,
                SNIFFER_AP_PASS, WiFi.softAPIP().toString().c_str());
}

// A appeler pour chaque trame recue.
static void onFrame(const twai_message_t &m) {
  if (m.rtr) return;
  uint32_t now = millis();
  IdEntry *e = nullptr;
  for (int i = 0; i < idCount; i++)
    if (ids[i].id == m.identifier && ids[i].ext == (m.extd ? 1 : 0)) { e = &ids[i]; break; }
  if (!e) {
    if (idCount >= MAX_IDS) return;
    e = &ids[idCount++];
    memset(e, 0, sizeof(*e));
    e->id = m.identifier;
    e->ext = m.extd ? 1 : 0;
    memcpy(e->data, m.data, 8);
  }
  for (int b = 0; b < 8 && b < m.data_length_code; b++)
    if (e->count && e->data[b] != m.data[b]) e->changedMs[b] = now;
  memcpy(e->data, m.data, 8);
  e->dlc = m.data_length_code;
  e->count++;
  e->lastMs = now;
}

// A appeler souvent dans la boucle du sniffer.
static void loop(uint32_t errors, uint32_t missed) {
  busErrors = errors;
  rxMissed = missed;
  server.handleClient();
}

}  // namespace snifweb
