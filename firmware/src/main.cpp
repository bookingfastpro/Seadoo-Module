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
#include "driver/twai.h"
#include "esp_sleep.h"

#include "config.h"
#include "can_signals.h"

#define SerialAT Serial1

TinyGsm modem(SerialAT);
Preferences prefs;

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

// Envoie un point au serveur. waitFix : attendre un fix GNSS (reveil a froid).
static bool sendReport(bool waitFix) {
  if (!modemConnect()) return false;

  float lat = 0, lon = 0, speed = 0, alt = 0, acc = 0;
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

  String body, response;
  serializeJson(doc, body);
  int status = 0;
  bool ok = httpsPost(body, response, status) && status == 200;
  Serial.printf("Envoi : %s (HTTP %d)\n", ok ? "OK" : "ECHEC", status);
  if (ok) handleServerResponse(response);
  return ok;
}

static void goToSleep() {
  Serial.printf("Sommeil pour %lu s\n", (unsigned long)parkedIntervalS);
  if (modemReady) modem.poweroff();
  twai_stop();
  twai_driver_uninstall();
#if RELAY_PIN >= 0
  gpio_hold_en((gpio_num_t)RELAY_PIN);            // le relais garde son etat pendant le sommeil
  gpio_deep_sleep_hold_en();
#endif
  digitalWrite(LED_PIN, HIGH);
  esp_sleep_enable_timer_wakeup((uint64_t)parkedIntervalS * 1000000ULL);
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

  if (!reportedOnce || (alive && now - lastReportMs >= activeIntervalS * 1000UL)) {
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
