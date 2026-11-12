// Decision de limitation des gaz — UNIQUEMENT pour jet-ski SANS PILOTE, telecommande.
//
// Fonction PURE (testable sur ordinateur). Elle calcule le pourcentage de gaz a
// envoyer a l'actionneur, a partir de la commande operateur et du niveau de risque.
//
// Regles :
//  - niveau 0            : la commande operateur passe telle quelle.
//  - niveau 1 (vigilance): plafond (reduit seulement, jamais au-dessus de capLevel1).
//  - niveau 2 (imminent) : MAINTIEN a holdLevel2 % (ex. 10 %). On ne coupe PAS :
//                          garder un peu de poussee conserve la direction pour
//                          s'ecarter. Ce maintien peut remonter la commande si
//                          l'operateur demandait moins.
//  - link perdu          : fail-safe, l'operateur reprend 100 %.
#pragma once

struct ThrottleCfg {
  int capLevel1;    // % de gaz max au niveau 1 (plafond)
  int holdLevel2;   // % de gaz maintenu au niveau 2 (ni plus, ni moins)
};

static inline int clampPct(int p) {
  if (p < 0) return 0;
  if (p > 100) return 100;
  return p;
}

// Renvoie le % de gaz a envoyer a l'actionneur.
static inline int throttleOutputPct(int operatorPct, int level, bool linkStale,
                                     const ThrottleCfg &c) {
  operatorPct = clampPct(operatorPct);
  if (linkStale) return operatorPct;                    // fail-safe : operateur maitre
  if (level >= 2) return clampPct(c.holdLevel2);        // maintien (garde la direction)
  if (level == 1)                                       // plafond : reduit seulement
    return operatorPct < c.capLevel1 ? operatorPct : c.capLevel1;
  return operatorPct;                                   // pas de risque
}
