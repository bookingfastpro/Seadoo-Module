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
// URL de la fonction Supabase "ingest" (utilisee seulement en mode HTTPS ci-dessous)
#define INGEST_URL  "https://VOTRE-PROJET.supabase.co/functions/v1/ingest"

// ---------- Transport vers le serveur ----------
// 0 = HTTPS : un envoi a la fois, simple, ~30 s entre points. Bon pour debuter.
// 1 = MQTT  : liaison gardee ouverte, un point par seconde, quasi temps reel.
//     Demande un broker MQTT (voir docs/06-mqtt.md). Recommande pour l'usage final.
#define TRANSPORT_MQTT  1
#define MQTT_HOST       "xxxxxxxx.s1.eu.hivemq.cloud"  // adresse de votre broker
#define MQTT_PORT       8883                            // 8883 = TLS (conseille), 1883 = sans TLS
#define MQTT_USE_TLS    1
#define MQTT_USER       "seadoo"
#define MQTT_PASS       "CHANGER-MOI"
#define MQTT_PUBLISH_MS 1000                            // navigation : un point par seconde

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
// Rayon de securite de CE jet-ski (reglable depuis le tableau de bord). L'alerte
// se declenche quand les rayons de deux jet-skis se touchent, c.-a-d. distance <
// rayon_A + rayon_B. Deux rayons de 100 m => alerte a 200 m.
#define COLLISION_RADIUS_M   100
#define COLLISION_TX_HZ      4       // diffusions de sa position par seconde
#define PEER_STALE_MS        3000    // au-dela, un module silencieux est ignore
#define BUZZER_PIN           19      // GPIO d'un buzzer actif (-1 = pas de buzzer). A confirmer sur la carte.
// Mode journal : l'algo evalue le risque et ECRIT ses decisions sur la carte SD
// (/collision_log.csv), SANS JAMAIS agir sur le jet-ski. Sert a mesurer la
// fiabilite (faux positifs) sur de vraies sorties avant d'envisager quoi que ce soit.
#define COLLISION_LOG          1
#define COLLISION_ACTION_LEVEL 2     // niveau a partir duquel il NOTE "aurait agi" (journal seulement)

// ---------- Limitation des gaz — UNIQUEMENT engins SANS PILOTE, telecommandes ----------
// DANGER si un humain est a bord : reduire les gaz retire la direction (poussee).
// A n'activer que sur jet-ski SANS pilote, telecommande, plan d'eau prive ferme.
// Montage en SERIE : la commande gaz de la telecommande entre ici, ressort plafonnee.
// Le module ne peut que REDUIRE, jamais augmenter. Desactive par defaut.
#define THROTTLE_LIMIT_ENABLE 0        // mettre 1 en connaissance de cause
#define THROTTLE_IN_PIN       35       // entree PWM (servo) depuis le recepteur RC (gaz operateur)
#define THROTTLE_OUT_PIN      18       // sortie PWM (servo) vers l'actionneur de gaz
#define THROTTLE_CAP_L1       60       // % de gaz max au niveau 1 (vigilance, plafond)
#define THROTTLE_HOLD_L2      10       // % de gaz MAINTENU au niveau 2 (imminent) : ne coupe pas,
                                       // garde un peu de poussee pour conserver la direction
#define THROTTLE_MIN_US       1000     // largeur d'impulsion servo a 0 % (gachette relachee) — a CALIBRER
#define THROTTLE_MAX_US       2000     // largeur d'impulsion servo a 100 % (plein gaz) — a CALIBRER
// Relais de contournement (securite du montage en serie). Un relais dont le
// contact relie DIRECTEMENT recepteur -> servo au repos. Le module l'excite
// seulement quand il tourne et prend la main. Module eteint, en veille ou plante
// (reset watchdog) => relais retombe => la telecommande reprend le servo en direct.
// Fortement recommande. -1 = pas de relais.
#define BYPASS_RELAY_PIN      23

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
