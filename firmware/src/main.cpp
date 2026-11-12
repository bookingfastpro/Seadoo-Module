// Boitier de suivi Sea-Doo perso
// Carte : LilyGO T-SIM7600G-H (ESP32 + modem LTE SIM7600 + GNSS) + transceiver CAN 3,3 V
//
// Principe :
//  - ecoute le bus CAN du jet-ski en LECTURE SEULE (le boitier n'emet jamais sur le bus)
//  - releve la position GNSS
//  - envoie le tout en HTTPS a votre serveur (fonction Supabase "ingest")
//  - la reponse du serveur contient les consignes (anti-demarrage, rythme d'envoi)
//  - jet-ski eteint : sommeil profond, reveil periodique pour donner la position

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGsmClient.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/twai.h"
#include "esp_sleep.h"

#include "config.h"
#include "can_signals.h"
#include "collision.h"
#include "throttle.h"

#if COLLISION_ENABLE
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#endif

#define SerialAT Serial1

TinyGsm modem(SerialAT);
Preferences prefs;

#if TRANSPORT_MQTT
#include <PubSubClient.h>
#if MQTT_USE_TLS
TinyGsmClientSecure netClient(modem);   // TLS fait par le modem
#else
TinyGsmClient netClient(modem);
#endif
PubSubClient mqtt(netClient);
static char telTopic[48], cmdTopic[48];
#endif

// ---------- Etat ----------
static float signalValue[CAN_SIGNAL_COUNT];
static uint32_t signalSeenMs[CAN_SIGNAL_COUNT];   // 0 = jamais recu
static uint32_t lastCanMs = 0;                    // 0 = aucune trame depuis le reveil
static uint32_t canFrameCount = 0;
static uint32_t lastReportMs = 0;
static bool reportedOnce = false;
static bool modemReady = false;
static bool inhibitWanted = false;                // consigne du serveur
static bool inhibitActive = false;                // etat reel du relais
static uint32_t activeIntervalS = ACTIVE_INTERVAL_S;
static uint32_t parkedIntervalS = PARKED_INTERVAL_S;

// ---------- CAN ----------
static bool canStart() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN,
                                                        TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 128;
#if CAN_BITRATE_KBPS == 250
  twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
#elif CAN_BITRATE_KBPS == 1000
  twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();
#else
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
#endif
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  return twai_start() == ESP_OK;
}

static void decodeFrame(const twai_message_t &m) {
  for (size_t i = 0; i < CAN_SIGNAL_COUNT; i++) {
    const CanSignal &s = CAN_SIGNALS[i];
    if (s.id == 0 || s.id != m.identifier) continue;
    if (s.startByte + s.lenBytes > m.data_length_code) continue;
    uint32_t raw = m.data[s.startByte];
    if (s.lenBytes == 2) {
      uint8_t a = m.data[s.startByte], b = m.data[s.startByte + 1];
      raw = s.bigEndian ? ((uint32_t)a << 8 | b) : ((uint32_t)b << 8 | a);
    }
    signalValue[i] = raw * s.scale + s.offset;
    signalSeenMs[i] = millis() | 1;
  }
}

// Vide la file de reception. Renvoie le nombre de trames lues.
static int pollCan(File *log = nullptr) {
  twai_message_t m;
  int n = 0;
  while (twai_receive(&m, 0) == ESP_OK) {
    n++;
    canFrameCount++;
    lastCanMs = millis() | 1;
    if (m.rtr) continue;
    decodeFrame(m);
    if (log) {
      char line[80];
      int len = snprintf(line, sizeof(line), "%lu,%lX,%d,%d", (unsigned long)millis(),
                         (unsigned long)m.identifier, m.extd ? 1 : 0, m.data_length_code);
      for (int i = 0; i < m.data_length_code && i < 8; i++)
        len += snprintf(line + len, sizeof(line) - len, ",%02X", m.data[i]);
      Serial.println(line);
      if (*log) log->println(line);
    }
  }
  return n;
}

static bool canAlive() {
  return lastCanMs != 0 && (millis() - lastCanMs) < (uint32_t)CAN_SILENCE_S * 1000UL;
}

// Valeur d'un signal si elle a ete recue il y a moins de 5 s.
static bool signalFresh(size_t i, float &out) {
  if (signalSeenMs[i] == 0 || millis() - signalSeenMs[i] > 5000) return false;
  out = signalValue[i];
  return true;
}

static bool engineRunning() {
  for (size_t i = 0; i < CAN_SIGNAL_COUNT; i++) {
    if (strcmp(CAN_SIGNALS[i].name, "rpm") == 0 && CAN_SIGNALS[i].id != 0) {
      float rpm;
      return signalFresh(i, rpm) && rpm > 300;
    }
  }
  // Regime pas encore decode : par prudence, bus actif = moteur considere en marche.
  return canAlive();
}

// ---------- Mode sniffer ----------
static void snifferLoop() {
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  File log;
  if (SD.begin(SD_CS_PIN)) {
    char name[24];
    for (int i = 0; i < 1000; i++) {
      snprintf(name, sizeof(name), "/can_%03d.csv", i);
      if (!SD.exists(name)) break;
    }
    log = SD.open(name, FILE_WRITE);
    Serial.printf("# Enregistrement sur la carte SD : %s\n", name);
  } else {
    Serial.println("# Pas de carte SD : sortie sur le port serie uniquement");
  }
  Serial.println("ms,id,ext,dlc,b0,b1,b2,b3,b4,b5,b6,b7");
  if (log) log.println("ms,id,ext,dlc,b0,b1,b2,b3,b4,b5,b6,b7");

  uint32_t lastFlush = 0, lastStatus = 0;
  for (;;) {
    pollCan(&log);
    uint32_t now = millis();
    if (log && now - lastFlush > 1000) {
      log.flush();
      lastFlush = now;
    }
    if (now - lastStatus > 5000) {
      lastStatus = now;
      twai_status_info_t st;
      twai_get_status_info(&st);
      // Beaucoup d'erreurs et 0 trame = mauvais debit (essayer 250) ou CAN-H/CAN-L inverses.
      Serial.printf("# trames=%lu erreurs_bus=%lu perdues=%lu\n", (unsigned long)canFrameCount,
                    (unsigned long)st.bus_error_count, (unsigned long)st.rx_missed_count);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    delay(1);
  }
}

// ---------- Relais anti-demarrage ----------
static void relayWrite(bool inhibit) {
#if RELAY_PIN >= 0
  gpio_hold_dis((gpio_num_t)RELAY_PIN);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, inhibit ? HIGH : LOW);
#endif
  inhibitActive = inhibit;
}

// Regle de securite : on ne BLOQUE jamais pendant que le moteur tourne.
// Le deblocage, lui, est toujours immediat.
static void applyInhibit() {
  if (inhibitWanted == inhibitActive) return;
  if (inhibitWanted && engineRunning()) return;   // on attendra l'arret du moteur
  relayWrite(inhibitWanted);
  prefs.putBool("inhibit", inhibitActive);
}

// ---------- Anti-collision (radio directe entre modules) ----------
// Couche RADIO uniquement. Le calcul du risque est dans collision.h (teste a part).
#if COLLISION_ENABLE
struct __attribute__((packed)) Beacon {
  uint32_t magic;
  char id[12];
  double lat, lon;
  float speed_mps, heading_deg;
  float radius_m;          // rayon de securite de CE jet-ski
  uint32_t seq;
};
static const uint32_t BEACON_MAGIC = 0x5EAD0C02;  // +1 : le format a change (rayon)
static uint8_t BCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct Peer { char id[12]; CraftState st; float radius; uint32_t lastMs; };
static Peer peers[8];
static CraftState ownState = {0, 0, 0, 0, false};
static float collisionRadius = COLLISION_RADIUS_M;   // notre propre rayon (reglable)
static uint32_t ownSeq = 0, lastBeaconMs = 0, lastBuzzToggleMs = 0;
static bool buzzOn = false;

// arduino-esp32 2.0.x : rappel de reception (mac, data, len).
static void onBeacon(const uint8_t *, const uint8_t *data, int len) {
  if (len != (int)sizeof(Beacon)) return;
  Beacon b;
  memcpy(&b, data, sizeof(b));
  if (b.magic != BEACON_MAGIC) return;
  if (strncmp(b.id, DEVICE_ID, sizeof(b.id)) == 0) return;  // ignorer son propre echo
  int slot = -1, freeSlot = -1;
  for (int i = 0; i < 8; i++) {
    if (peers[i].lastMs && strncmp(peers[i].id, b.id, sizeof(b.id)) == 0) { slot = i; break; }
    if (!peers[i].lastMs && freeSlot < 0) freeSlot = i;
  }
  if (slot < 0) slot = (freeSlot >= 0) ? freeSlot : 0;
  strncpy(peers[slot].id, b.id, sizeof(peers[slot].id));
  peers[slot].st = {b.lat, b.lon, b.speed_mps, b.heading_deg, true};
  peers[slot].radius = b.radius_m;
  peers[slot].lastMs = millis() | 1;
}

static void collisionInit() {
#if BUZZER_PIN >= 0
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW : echec init"); return; }
  esp_now_register_recv_cb(onBeacon);
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, BCAST_ADDR, 6);
  p.channel = 1;
  p.encrypt = false;
  esp_now_add_peer(&p);
}

static void collisionBroadcast() {
  if (!ownState.valid) return;
  Beacon b = {BEACON_MAGIC, {0}, ownState.lat, ownState.lon,
              ownState.speed_mps, ownState.heading_deg, collisionRadius, ++ownSeq};
  strncpy(b.id, DEVICE_ID, sizeof(b.id));
  esp_now_send(BCAST_ADDR, (uint8_t *)&b, sizeof(b));
}

// Renvoie le niveau de risque le plus eleve parmi les modules entendus recemment.
static int collisionWorst() {
  int worst = 0;
  Threat wt = {0, 0, 0, 0, 0};
  for (int i = 0; i < 8; i++) {
    if (!peers[i].lastMs || millis() - peers[i].lastMs > PEER_STALE_MS) continue;
    // alerte quand les deux rayons se touchent : distance < somme des rayons
    Threat t = assessThreat(ownState, peers[i].st, collisionRadius + peers[i].radius);
    if (t.level > worst) { worst = t.level; wt = t; }
  }
  if (worst && millis() - lastBeaconMs < 50) {  // trace de temps en temps
    Serial.printf("COLLISION niveau %d  dist=%.0fm  rappr=%.1fm/s\n", worst, wt.distance_m,
                  wt.range_rate_mps);
  }
  return worst;
}

// Buzzer : eteint (0), bip lent (vigilance), bip rapide (imminent).
static void buzzerUpdate(int level) {
#if BUZZER_PIN >= 0
  uint32_t period = (level >= 2) ? 150 : (level == 1 ? 600 : 0);
  if (period == 0) {
    if (buzzOn) { digitalWrite(BUZZER_PIN, LOW); buzzOn = false; }
    return;
  }
  if (millis() - lastBuzzToggleMs >= period) {
    buzzOn = !buzzOn;
    digitalWrite(BUZZER_PIN, buzzOn ? HIGH : LOW);
    lastBuzzToggleMs = millis();
  }
#endif
}

// A appeler souvent quand le moteur tourne : diffuse, evalue, pilote le buzzer.
static void collisionTick() {
  uint32_t now = millis();
  if (now - lastBeaconMs >= (uint32_t)(1000 / COLLISION_TX_HZ)) {
    collisionBroadcast();
    lastBeaconMs = now;
  }
  int level = collisionWorst();
  buzzerUpdate(level);
#if THROTTLE_LIMIT_ENABLE
  bool fresh = false;
  for (int i = 0; i < 8; i++)
    if (peers[i].lastMs && millis() - peers[i].lastMs <= PEER_STALE_MS) { fresh = true; break; }
  throttleApply(level, !fresh);     // !fresh = fail-safe : plus de donnees -> operateur maitre
#endif
#if COLLISION_LOG
  collisionLogTick();
#endif
}

// Met a jour son propre etat depuis un point GPS ; le cap est deduit du deplacement.
static void collisionSetOwn(double lat, double lon, float speed_mps) {
  if (ownState.valid) {
    double dx = (lon - ownState.lon) * cos(lat * M_PI / 180.0);
    double dy = (lat - ownState.lat);
    if (fabs(dx) + fabs(dy) > 1e-7)
      ownState.heading_deg = fmod(atan2(dx, dy) * 180.0 / M_PI + 360.0, 360.0);
  }
  ownState.lat = lat;
  ownState.lon = lon;
  ownState.speed_mps = speed_mps;
  ownState.valid = true;
}

#if COLLISION_LOG
static bool collLogReady = false;
static uint32_t lastLogMs = 0;

static void collisionLogInit() {
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Journal collision : pas de carte SD (sortie serie seule)");
    return;
  }
  collLogReady = true;
  if (!SD.exists("/collision_log.csv")) {
    File f = SD.open("/collision_log.csv", FILE_WRITE);
    if (f) { f.println("ms,peer,dist_m,range_rate_mps,tcpa_s,dcpa_m,level,would_act"); f.close(); }
  }
}

// Journalise (~2 Hz) toute menace >= niveau 1, SANS jamais agir sur le jet-ski.
// would_act = 1 marque les instants ou un systeme actif "aurait agi" : c'est ce
// chiffre qu'on analyse apres coup pour juger des faux positifs.
static void collisionLogTick() {
  if (millis() - lastLogMs < 500) return;
  lastLogMs = millis();
  File f;
  if (collLogReady) f = SD.open("/collision_log.csv", FILE_APPEND);
  for (int i = 0; i < 8; i++) {
    if (!peers[i].lastMs || millis() - peers[i].lastMs > PEER_STALE_MS) continue;
    Threat t = assessThreat(ownState, peers[i].st, collisionRadius + peers[i].radius);
    if (t.level < 1) continue;
    int wouldAct = (t.level >= COLLISION_ACTION_LEVEL) ? 1 : 0;
    char line[128];
    snprintf(line, sizeof(line), "%lu,%s,%.0f,%.1f,%.1f,%.0f,%d,%d", (unsigned long)millis(),
             peers[i].id, t.distance_m, t.range_rate_mps, t.tcpa_s, t.dcpa_m, t.level, wouldAct);
    Serial.print("JOURNAL ");
    Serial.println(line);
    if (f) f.println(line);
  }
  if (f) f.close();
}
#endif  // COLLISION_LOG

#if THROTTLE_LIMIT_ENABLE
// Limiteur de gaz EN SERIE (engins sans pilote). Lit la commande PWM de la
// telecommande, la ressort plafonnee. Ne peut que REDUIRE, jamais augmenter.
static volatile uint32_t thrInRiseUs = 0;
static volatile uint32_t thrInPulseUs = THROTTLE_MIN_US;

static void IRAM_ATTR onThrottleIn() {
  if (digitalRead(THROTTLE_IN_PIN)) {
    thrInRiseUs = micros();
  } else {
    uint32_t w = micros() - thrInRiseUs;
    if (w >= 700 && w <= 2300) thrInPulseUs = w;   // impulsion servo plausible
  }
}

static ThrottleCfg throttleCfg = {THROTTLE_CAP_L1, THROTTLE_HOLD_L2};

static void throttleInit() {
#if BYPASS_RELAY_PIN >= 0
  pinMode(BYPASS_RELAY_PIN, OUTPUT);
  digitalWrite(BYPASS_RELAY_PIN, LOW);  // contournement : recepteur -> servo en direct
#endif
  pinMode(THROTTLE_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(THROTTLE_IN_PIN), onThrottleIn, CHANGE);
  ledcSetup(6, 50, 16);                 // canal 6, 50 Hz (servo), 16 bits
  ledcAttachPin(THROTTLE_OUT_PIN, 6);
  // On laisse passer un instant la commande operateur avant de prendre la main,
  // le temps d'avoir une largeur d'impulsion valide.
  delay(60);
#if BYPASS_RELAY_PIN >= 0
  digitalWrite(BYPASS_RELAY_PIN, HIGH); // module en controle (sortie plafonnee vers le servo)
#endif
}

// Rend le servo a la telecommande (contournement direct). Appele avant la veille.
static void throttleRelease() {
#if BYPASS_RELAY_PIN >= 0
  digitalWrite(BYPASS_RELAY_PIN, LOW);
#endif
}

static int pulseToPct(uint32_t us) {
  if (us <= THROTTLE_MIN_US) return 0;
  if (us >= THROTTLE_MAX_US) return 100;
  return (int)((us - THROTTLE_MIN_US) * 100 / (THROTTLE_MAX_US - THROTTLE_MIN_US));
}

static void writeThrottlePct(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  uint32_t us = THROTTLE_MIN_US + (uint32_t)(THROTTLE_MAX_US - THROTTLE_MIN_US) * pct / 100;
  ledcWrite(6, (uint32_t)((uint64_t)us * 65535UL / 20000UL));   // periode servo 20 ms
}

// Calcule et applique le gaz de sortie pour le niveau courant.
static void throttleApply(int level, bool linkStale) {
  int opPct = pulseToPct(thrInPulseUs);
  writeThrottlePct(throttleOutputPct(opPct, level, linkStale, throttleCfg));
}
#endif  // THROTTLE_LIMIT_ENABLE
#endif  // COLLISION_ENABLE

// ---------- Modem ----------
static void modemPowerOn() {
  pinMode(MODEM_FLIGHT_PIN, OUTPUT);
  digitalWrite(MODEM_FLIGHT_PIN, HIGH);           // sortie du mode avion
  pinMode(MODEM_PWRKEY_PIN, OUTPUT);
  digitalWrite(MODEM_PWRKEY_PIN, HIGH);
  delay(300);
  digitalWrite(MODEM_PWRKEY_PIN, LOW);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
}

static bool modemConnect() {
  if (!modemReady) {
    modemPowerOn();
    bool up = false;
    for (int i = 0; i < 30 && !up; i++) {         // le SIM7600 met ~15 s a demarrer
      up = modem.testAT(1000);
      pollCan();
    }
    if (!up || !modem.init()) {
      Serial.println("Modem injoignable");
      return false;
    }
    modem.enableGPS();
    modemReady = true;
  }
  if (!modem.isNetworkConnected() && !modem.waitForNetwork(60000L)) {
    Serial.println("Pas de reseau cellulaire");
    return false;
  }
  if (!modem.isGprsConnected() && !modem.gprsConnect(APN, APN_USER, APN_PASS)) {
    Serial.println("Echec connexion data (APN ?)");
    return false;
  }
  return true;
}

// POST HTTPS via la pile HTTP(S) integree au SIM7600 (le TLS est fait par le modem).
static bool httpsPost(const String &body, String &response, int &status) {
  status = 0;
  response = "";
  modem.sendAT(GF("+HTTPTERM"));
  modem.waitResponse(2000);
  modem.sendAT(GF("+HTTPINIT"));
  if (modem.waitResponse(10000) != 1) return false;

  bool ok = false;
  do {
    modem.sendAT(GF("+HTTPPARA=\"URL\",\""), INGEST_URL, GF("\""));
    if (modem.waitResponse(5000) != 1) break;
    modem.sendAT(GF("+HTTPPARA=\"CONTENT\",\"application/json\""));
    if (modem.waitResponse(5000) != 1) break;
    modem.sendAT(GF("+HTTPDATA="), body.length(), GF(",10000"));
    if (modem.waitResponse(10000, GF("DOWNLOAD")) != 1) break;
    SerialAT.print(body);
    if (modem.waitResponse(10000) != 1) break;
    modem.sendAT(GF("+HTTPACTION=1"));
    if (modem.waitResponse(10000) != 1) break;
    if (modem.waitResponse(60000, GF("+HTTPACTION: 1,")) != 1) break;
    status = SerialAT.readStringUntil(',').toInt();
    int len = SerialAT.readStringUntil('\n').toInt();
    if (len > 0) {
      if (len > 1024) len = 1024;
      modem.sendAT(GF("+HTTPREAD=0,"), len);
      // Selon la version du modem, l'entete de reponse varie : on lit tout jusqu'au
      // marqueur de fin puis on isole l'objet JSON.
      String raw;
      uint32_t t0 = millis();
      while (millis() - t0 < 10000 && raw.indexOf("+HTTPREAD: 0") < 0) {
        while (SerialAT.available()) raw += (char)SerialAT.read();
        delay(5);
      }
      int a = raw.indexOf('{'), b = raw.lastIndexOf('}');
      if (a >= 0 && b > a) response = raw.substring(a, b + 1);
    }
    ok = true;
  } while (false);

  modem.sendAT(GF("+HTTPTERM"));
  modem.waitResponse(2000);
  return ok;
}

static float readBatteryVolts() {
#if VBAT_ADC_PIN >= 0
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(VBAT_ADC_PIN);
  return (mv / 8) / 1000.0f * VBAT_DIVIDER;
#else
  return -1;
#endif
}

static void handleServerResponse(const String &response) {
  JsonDocument doc;
  if (response.isEmpty() || deserializeJson(doc, response)) return;
  if (doc["start_inhibit"].is<bool>()) inhibitWanted = doc["start_inhibit"].as<bool>();
#if COLLISION_ENABLE
  float cr = doc["collision_radius_m"] | 0.0f;
  if (cr >= 10 && cr <= 1000) collisionRadius = cr;
#endif
  uint32_t a = doc["active_interval_s"] | 0;
  uint32_t p = doc["parked_interval_s"] | 0;
  if (a >= 5 && a <= 3600 && a != activeIntervalS) {
    activeIntervalS = a;
    prefs.putUInt("active_s", a);
  }
  if (p >= 60 && p <= 86400 && p != parkedIntervalS) {
    parkedIntervalS = p;
    prefs.putUInt("parked_s", p);
  }
  prefs.putBool("inhibit_w", inhibitWanted);
  applyInhibit();
}

// Acquiert un fix GNSS (waitFix : attendre, au reveil a froid) et met a jour
// l'etat anti-collision. Renvoie true si position valide.
static bool acquireFix(bool waitFix, float &lat, float &lon, float &speed, float &acc) {
  lat = lon = speed = acc = 0;
  float alt = 0;
  int vsat = 0, usat = 0;
  bool fix = false;
  uint32_t t0 = millis();
  do {
    fix = modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &acc);
    if (fix || !waitFix) break;
    for (int i = 0; i < 20; i++) {                // 2 s entre deux essais, sans perdre de trames
      pollCan();
      delay(100);
    }
  } while (millis() - t0 < (uint32_t)GPS_FIX_TIMEOUT_S * 1000UL);
#if COLLISION_ENABLE
  if (fix) collisionSetOwn(lat, lon, speed / 3.6f);  // speed en km/h -> m/s
#endif
  return fix;
}

// Construit le JSON d'un point (identique pour HTTPS et MQTT).
static void buildReportBody(String &body, bool fix, float lat, float lon, float speed, float acc) {
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["key"] = DEVICE_KEY;
  doc["fix"] = fix;
  if (fix) {
    doc["lat"] = serialized(String(lat, 6));
    doc["lon"] = serialized(String(lon, 6));
    doc["gps_speed_kmh"] = speed;
    doc["hdop"] = acc;
  }
  doc["can_alive"] = canAlive();
  doc["inhibit_active"] = inhibitActive;
  doc["rssi"] = modem.getSignalQuality();
  float vbat = readBatteryVolts();
  if (vbat >= 0) doc["batt_v"] = vbat;
  for (size_t i = 0; i < CAN_SIGNAL_COUNT; i++) {
    float v;
    if (CAN_SIGNALS[i].id != 0 && signalFresh(i, v)) doc[CAN_SIGNALS[i].name] = v;
  }
  serializeJson(doc, body);
}

#if TRANSPORT_MQTT
// Consigne recue du serveur/tableau de bord (meme format que la reponse HTTP).
static void mqttCallback(char *, byte *payload, unsigned int len) {
  String s;
  s.reserve(len);
  for (unsigned i = 0; i < len; i++) s += (char)payload[i];
  handleServerResponse(s);
}

static bool mqttEnsure() {
  if (!modemConnect()) return false;
  if (mqtt.connected()) return true;
  snprintf(telTopic, sizeof(telTopic), "seadoo/%s/telemetry", DEVICE_ID);
  snprintf(cmdTopic, sizeof(cmdTopic), "seadoo/%s/cmd", DEVICE_ID);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);
  mqtt.setKeepAlive(30);
  mqtt.setCallback(mqttCallback);
  String cid = String("seadoo-") + DEVICE_ID;
  if (!mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.printf("MQTT : echec connexion (etat %d)\n", mqtt.state());
    return false;
  }
  mqtt.subscribe(cmdTopic, 1);   // recoit la derniere consigne (message retenu par le broker)
  Serial.println("MQTT : connecte");
  return true;
}
#endif

// Envoie un point au serveur. waitFix : attendre un fix GNSS (reveil a froid).
static bool sendReport(bool waitFix) {
  if (!modemConnect()) return false;
  float lat, lon, speed, acc;
  bool fix = acquireFix(waitFix, lat, lon, speed, acc);
  String body;
  buildReportBody(body, fix, lat, lon, speed, acc);

#if TRANSPORT_MQTT
  if (!mqttEnsure()) return false;
  bool ok = mqtt.publish(telTopic, body.c_str());
  Serial.printf("Envoi MQTT : %s\n", ok ? "OK" : "ECHEC");
  return ok;
#else
  String response;
  int status = 0;
  bool ok = httpsPost(body, response, status) && status == 200;
  Serial.printf("Envoi : %s (HTTP %d)\n", ok ? "OK" : "ECHEC", status);
  if (ok) handleServerResponse(response);
  return ok;
#endif
}

// Intervalle entre points moteur tournant : ~1 s en MQTT, sinon le reglage serveur.
static uint32_t activeIntervalMs() {
#if TRANSPORT_MQTT
  return MQTT_PUBLISH_MS;
#else
  return activeIntervalS * 1000UL;
#endif
}

static void goToSleep() {
  Serial.printf("Sommeil pour %lu s\n", (unsigned long)parkedIntervalS);
#if COLLISION_ENABLE && THROTTLE_LIMIT_ENABLE
  throttleRelease();                    // en veille : la telecommande garde le servo
#endif
#if TRANSPORT_MQTT
  if (mqtt.connected()) mqtt.disconnect();
#endif
  if (modemReady) modem.poweroff();
#if COLLISION_ENABLE
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
#if BUZZER_PIN >= 0
  digitalWrite(BUZZER_PIN, LOW);
#endif
#endif
  twai_stop();
  twai_driver_uninstall();
#if RELAY_PIN >= 0
  gpio_hold_en((gpio_num_t)RELAY_PIN);            // le relais garde son etat pendant le sommeil
  gpio_deep_sleep_hold_en();
#endif
  digitalWrite(LED_PIN, HIGH);
  esp_sleep_enable_timer_wakeup((uint64_t)parkedIntervalS * 1000000ULL);
#if WAKE_PIN >= 0
  // Au repos, CRX reste au niveau haut ; des la premiere trame (moteur demarre)
  // il passe au bas et reveille le boitier. Les broches 34-39 sont en entree
  // seule et pilotees par le transceiver : pas de pull interne a regler.
  esp_sleep_enable_ext1_wakeup(1ULL << WAKE_PIN, ESP_EXT1_WAKEUP_ALL_LOW);
#endif
  esp_deep_sleep_start();
}

// ---------- Programme principal ----------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  delay(200);
  Serial.printf("\nBoitier Sea-Doo [%s]\n", DEVICE_ID);

  prefs.begin("seadoo", false);
  inhibitWanted = prefs.getBool("inhibit_w", false);
  activeIntervalS = prefs.getUInt("active_s", ACTIVE_INTERVAL_S);
  parkedIntervalS = prefs.getUInt("parked_s", PARKED_INTERVAL_S);
  relayWrite(prefs.getBool("inhibit", false));

  if (!canStart()) Serial.println("Echec demarrage CAN");

#if SNIFFER_MODE
  snifferLoop();
#endif

#if COLLISION_ENABLE
  collisionInit();
#if COLLISION_LOG
  collisionLogInit();
#endif
#if THROTTLE_LIMIT_ENABLE
  throttleInit();
#endif
#endif

  // Laisse 3 s au bus pour se manifester avant de decider si le jet-ski est allume.
  uint32_t t0 = millis();
  while (millis() - t0 < 3000) {
    pollCan();
    delay(5);
  }
}

void loop() {
  pollCan();
  applyInhibit();

  bool alive = canAlive();
  uint32_t now = millis();

#if TRANSPORT_MQTT
  if (modemReady && mqtt.connected()) mqtt.loop();   // traite les consignes entrantes
#endif

#if COLLISION_ENABLE
  if (alive) {
    static uint32_t lastGps = 0;
    if (modemReady && millis() - lastGps > 1000) {  // rafraichit sa position ~1 Hz
      float la, lo, sp;
      if (modem.getGPS(&la, &lo, &sp)) collisionSetOwn(la, lo, sp / 3.6f);
      lastGps = millis();
    }
    collisionTick();
  }
#endif

  if (!reportedOnce || (alive && now - lastReportMs >= activeIntervalMs())) {
    sendReport(!reportedOnce);                    // premier envoi : on attend le fix GNSS
    reportedOnce = true;
    lastReportMs = millis();
    return;
  }

  if (!alive) {
    // Jet-ski eteint et position envoyee : un dernier point puis sommeil profond.
    if (lastCanMs != 0) sendReport(false);        // il vient de s'eteindre
    goToSleep();
  }
  delay(2);
}
