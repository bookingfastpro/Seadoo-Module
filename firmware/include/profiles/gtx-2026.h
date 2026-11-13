// Profil véhicule : Sea-Doo GTX 2026
//
// À remplir après la séance de décodage (docs/02) et la capture de bridage (docs/08)
// faites SUR CE modèle — ses identifiants peuvent différer du GTX 2023.
#pragma once
#include "profile_types.h"

#define PROFILE_NAME "Sea-Doo GTX 2026"

static const CanSignal CAN_SIGNALS[] = {
  // name           id     start len  BE     scale  offset
  {"rpm",           0x000, 0,    2,   true,  1.0f,  0.0f},
  {"speed_kmh",     0x000, 0,    2,   true,  1.0f,  0.0f},
  {"fuel_pct",      0x000, 0,    1,   true,  1.0f,  0.0f},
  {"engine_hours",  0x000, 0,    2,   true,  1.0f,  0.0f},
  {"coolant_c",     0x000, 0,    1,   true,  1.0f,  0.0f},
};
static const size_t CAN_SIGNAL_COUNT = sizeof(CAN_SIGNALS) / sizeof(CAN_SIGNALS[0]);

static const DiagFrame DIAG_LIMIT_ON[] = {
};
static const size_t DIAG_LIMIT_ON_COUNT = sizeof(DIAG_LIMIT_ON) / sizeof(DIAG_LIMIT_ON[0]);

static const DiagFrame DIAG_LIMIT_OFF[] = {
};
static const size_t DIAG_LIMIT_OFF_COUNT = sizeof(DIAG_LIMIT_OFF) / sizeof(DIAG_LIMIT_OFF[0]);
