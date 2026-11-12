// Verification de la logique de limitation des gaz (firmware/include/throttle.h).
// g++ -std=c++11 -I../firmware/include throttle_test.cpp -o /tmp/tt && /tmp/tt
#include "throttle.h"
#include <cstdio>

static int fails = 0;
static void eq(const char *name, int got, int want) {
  bool ok = got == want;
  if (!ok) fails++;
  printf("%s  %-42s = %d (attendu %d)\n", ok ? "OK " : "NON", name, got, want);
}

int main() {
  ThrottleCfg c = {60, 25};  // 60 % au niveau 1, 25 % au niveau 2

  // Plafond selon le niveau
  eq("niveau 0 -> aucune limite", allowedThrottlePct(0, false, c), 100);
  eq("niveau 1 -> 60 %",          allowedThrottlePct(1, false, c), 60);
  eq("niveau 2 -> 25 %",          allowedThrottlePct(2, false, c), 25);

  // Fail-safe : plus de donnees -> operateur reprend tout, meme si un niveau trainait
  eq("link perdu au niveau 2 -> 100", allowedThrottlePct(2, true, c), 100);
  eq("link perdu au niveau 0 -> 100", allowedThrottlePct(0, true, c), 100);

  // Le plafond ne peut que REDUIRE la commande operateur
  eq("operateur 80, plafond 100 -> 80", cappedThrottle(80, 100), 80);  // pas de risque : passe tel quel
  eq("operateur 80, plafond 60  -> 60", cappedThrottle(80, 60), 60);   // plafonne
  eq("operateur 40, plafond 60  -> 40", cappedThrottle(40, 60), 40);   // sous le plafond : inchange
  eq("operateur 90, plafond 25  -> 25", cappedThrottle(90, 25), 25);   // imminent : fort plafond
  eq("jamais augmente (10,100)  -> 10", cappedThrottle(10, 100), 10);

  printf("\n%s (%d echec%s)\n", fails ? "ECHECS" : "TOUT PASSE", fails, fails > 1 ? "s" : "");
  return fails ? 1 : 0;
}
