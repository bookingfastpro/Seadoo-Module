// Types partagés par tous les profils de véhicule.
//
// Un "profil" décrit comment lire et commander UN modèle précis (Sea-Doo GTX 2023,
// GTX 2026, un bateau…). Chaque modèle a son fichier dans profiles/, et config.h
// choisit lequel charger via VEHICLE_PROFILE. Comme chaque boîtier est flashé pour
// un véhicule donné, il embarque le bon profil.
#pragma once
#include <Arduino.h>

// --- Décodage d'un signal du bus CAN ---
// valeur = (octets bruts) * scale + offset
struct CanSignal {
  const char *name;     // nom du champ envoyé au serveur (rpm, speed_kmh, fuel_pct…)
  uint32_t id;          // identifiant CAN (0 = désactivé)
  uint8_t startByte;    // premier octet (0..7)
  uint8_t lenBytes;     // 1 ou 2
  bool bigEndian;       // true = octet de poids fort en premier
  float scale;
  float offset;
};

// --- Trame de diagnostic à émettre (bridage, façon OtoTrak) ---
struct DiagFrame {
  uint32_t id;          // identifiant CAN de la trame
  uint8_t dlc;          // nombre d'octets (0..8)
  uint8_t data[8];      // octets
  uint16_t gap_ms;      // pause après cette trame
};
