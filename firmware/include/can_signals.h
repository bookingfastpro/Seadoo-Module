// Table de decodage du bus CAN du Sea-Doo.
//
// BRP ne publie pas ses messages CAN : ces lignes sont VIDES au depart (id = 0
// = signal ignore). On les remplit apres la seance de sniffing decrite dans
// docs/02-bus-can.md. Les identifiants peuvent differer entre le GTX 2023 et
// le GTX 2026 : faire le releve sur chacun.
//
// valeur = (octets bruts) * scale + offset
#pragma once
#include <Arduino.h>

struct CanSignal {
  const char *name;     // nom du champ envoye au serveur
  uint32_t id;          // identifiant CAN (0 = desactive)
  uint8_t startByte;    // premier octet (0..7)
  uint8_t lenBytes;     // 1 ou 2
  bool bigEndian;       // true = octet de poids fort en premier
  float scale;
  float offset;
};

static const CanSignal CAN_SIGNALS[] = {
  // name           id     start len  BE     scale  offset
  {"rpm",           0x000, 0,    2,   true,  1.0f,  0.0f},
  {"speed_kmh",     0x000, 0,    2,   true,  1.0f,  0.0f},
  {"fuel_pct",      0x000, 0,    1,   true,  1.0f,  0.0f},
  {"engine_hours",  0x000, 0,    2,   true,  1.0f,  0.0f},
  {"coolant_c",     0x000, 0,    1,   true,  1.0f,  0.0f},
};
static const size_t CAN_SIGNAL_COUNT = sizeof(CAN_SIGNALS) / sizeof(CAN_SIGNALS[0]);
