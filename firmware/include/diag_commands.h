// Commandes de diagnostic envoyees au calculateur (ECM) pour (de)activer le
// bridage de vitesse INTEGRE au Sea-Doo — le meme mecanisme que BUDS et l'OtoTrak.
//
// ⚠️ VIDE au depart. A REMPLIR uniquement avec les trames CAPTUREES avec BUDS2
// (voir docs/08-bridage-diag.md). NE JAMAIS inventer ces trames : une mauvaise
// commande de diagnostic peut ecrire un autre reglage dans l'ECM.
//
// Tant que ces sequences sont vides, le firmware n'emet rien, meme si
// DIAG_LIMIT_ENABLE vaut 1.
#pragma once
#include <Arduino.h>

struct DiagFrame {
  uint32_t id;        // identifiant CAN de la trame
  uint8_t  dlc;       // nombre d'octets (0..8)
  uint8_t  data[8];   // octets
  uint16_t gap_ms;    // pause apres cette trame (pour respecter le rythme capture)
};

// Sequence pour ACTIVER le bridage (capturee en comparant BUDS2 avant/apres).
static const DiagFrame DIAG_LIMIT_ON[] = {
  // Exemple de FORME (a remplacer par la capture reelle) :
  // {0x7E0, 8, {0x02, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, 60},
  // {0x7E0, 8, {0x04, 0x2E, 0xXX, 0xXX, 0xVV, 0x00, 0x00, 0x00}, 60},
};
static const size_t DIAG_LIMIT_ON_COUNT = sizeof(DIAG_LIMIT_ON) / sizeof(DIAG_LIMIT_ON[0]);

// Sequence pour RETIRER le bridage (retour normal).
static const DiagFrame DIAG_LIMIT_OFF[] = {
};
static const size_t DIAG_LIMIT_OFF_COUNT = sizeof(DIAG_LIMIT_OFF) / sizeof(DIAG_LIMIT_OFF[0]);
