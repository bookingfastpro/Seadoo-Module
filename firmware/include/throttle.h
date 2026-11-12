// Decision de limitation des gaz — UNIQUEMENT pour jet-ski SANS PILOTE, telecommande.
//
// Fonction PURE (testable sur ordinateur). Elle ne decide QUE d'un plafond de gaz
// en pourcentage. L'application reelle (lire la commande operateur et la plafonner)
// est dans main.cpp. Par construction, un plafond ne peut que REDUIRE la commande,
// jamais l'augmenter.
#pragma once

struct ThrottleCfg {
  int capLevel1;   // % de gaz max au niveau 1 (vigilance)
  int capLevel2;   // % de gaz max au niveau 2 (imminent)
};

// level     : niveau de risque collision (0, 1, 2).
// linkStale : true si on n'a plus de donnees fiables des autres modules
//             -> fail-safe : aucune limitation, l'operateur reprend tout.
// Renvoie le plafond de gaz autorise en % (100 = aucune limitation).
static inline int allowedThrottlePct(int level, bool linkStale, const ThrottleCfg &c) {
  if (linkStale) return 100;            // fail-safe : plus de donnees -> operateur maitre
  if (level >= 2) return c.capLevel2;
  if (level == 1) return c.capLevel1;
  return 100;                           // pas de risque -> rien de limite
}

// Applique un plafond a une commande operateur (toutes deux en %).
// Ne peut que reduire : min(commande, plafond).
static inline int cappedThrottle(int operatorPct, int capPct) {
  if (operatorPct < 0) operatorPct = 0;
  if (operatorPct > 100) operatorPct = 100;
  return operatorPct < capPct ? operatorPct : capPct;
}
