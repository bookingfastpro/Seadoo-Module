// Copier ce fichier en "config.h" (meme dossier) et remplir les valeurs.
// config.h est ignore par git : vos cles ne partent pas sur GitHub.
#pragma once

// ---------- Identite du boitier (un config.h par jet-ski) ----------
#define DEVICE_ID   "gtx-2023"                 // doit exister dans la table "devices"
#define DEVICE_KEY  "CHANGER-MOI-cle-longue-aleatoire"

// ---------- Reseau ----------
#define APN         "iot.1nce.net"             // APN de votre carte SIM M2M
#define APN_USER    ""
#define APN_PASS    ""
// URL de la fonction Supabase "ingest" (voir docs/03-backend.md)
#define INGEST_URL  "https://VOTRE-PROJET.supabase.co/functions/v1/ingest"

// ---------- Mode ----------
// 1 = mode sniffer : enregistre toutes les trames CAN sur la carte SD + port serie,
//     sans modem. A utiliser en premier pour decoder le bus (docs/02-bus-can.md).
// 0 = mode tracker normal.
#define SNIFFER_MODE 1

// ---------- Bus CAN ----------
#define CAN_BITRATE_KBPS 500                   // essayer 250 si aucune trame en 500
#define CAN_TX_PIN  22                         // vers CTX du transceiver (jamais utilise : ecoute seule)
#define CAN_RX_PIN  21                         // vers CRX du transceiver

// Reveil au demarrage du moteur : un fil de plus, de CRX (meme broche RX du
// transceiver) vers ce GPIO. Quand le moteur demarre, le bus s'anime et reveille
// le boitier en ~1 s, au lieu d'attendre le prochain reveil programme.
// Doit etre une broche RTC en entree seule : 34, 35, 36 ou 39. -1 = desactive.
#define WAKE_PIN    34

// ---------- Rythme ----------
#define ACTIVE_INTERVAL_S   30                 // jet-ski allume : un point toutes les 30 s
#define PARKED_INTERVAL_S   1800               // a l'arret : reveil toutes les 30 min
#define CAN_SILENCE_S       60                 // bus muet depuis 60 s = jet-ski eteint
#define GPS_FIX_TIMEOUT_S   90

// ---------- Anti-collision entre nos modules (radio directe ESP-NOW) ----------
// Les modules se parlent DIRECTEMENT (sans internet) et calculent le risque
// localement : reaction en millisecondes, meme sans reseau. Voir docs/05.
#define COLLISION_ENABLE     1
#define COLLISION_DISTANCE_M 120     // seuil d'alerte par defaut (reglable depuis le tableau de bord)
#define COLLISION_TX_HZ      4       // diffusions de sa position par seconde
#define PEER_STALE_MS        3000    // au-dela, un module silencieux est ignore
#define BUZZER_PIN           19      // GPIO d'un buzzer actif (-1 = pas de buzzer). A confirmer sur la carte.

// ---------- Anti-demarrage (phase 3, optionnel) ----------
// -1 = desactive. Sinon GPIO qui pilote le relais (contact NF en serie
// avec le circuit de demarrage : bobine alimentee = demarrage bloque).
#define RELAY_PIN   -1

// ---------- Tension batterie 12 V (optionnel) ----------
// -1 = desactive. Sinon GPIO ADC derriere un pont diviseur 100k / 22k.
#define VBAT_ADC_PIN -1
#define VBAT_DIVIDER 5.545f                    // (100k + 22k) / 22k

// ---------- Brochage carte LilyGO T-SIM7600G-H (ne pas changer) ----------
#define MODEM_TX_PIN     27
#define MODEM_RX_PIN     26
#define MODEM_PWRKEY_PIN 4
#define MODEM_FLIGHT_PIN 25
#define SD_MISO_PIN 2
#define SD_MOSI_PIN 15
#define SD_SCLK_PIN 14
#define SD_CS_PIN   13
#define LED_PIN     12
