// Verification de la logique de limitation des gaz (firmware/include/throttle.h).
// g++ -std=c++11 -I../firmware/include throttle_test.cpp -o /tmp/tt && /tmp/tt
#include "throttle.h"
#include <cstdio>

static int fails = 0;
static void eq(const char *name, int got, int want) {
  bool ok = got == want;
  if (!ok) fails++;
  printf("%s  %-46s = %d (attendu %d)\n", ok ? "OK " : "NON", name, got, want);
}

int main() {
  ThrottleCfg c = {60, 10};  // plafond 60 % au niveau 1, maintien 10 % au niveau 2

  // Niveau 0 : rien de limite
  eq("niv0, op 80 -> 80", throttleOutputPct(80, 0, false, c), 80);
  eq("niv0, op 0  -> 0",  throttleOutputPct(0, 0, false, c), 0);

  // Niveau 1 : plafond, reduit seulement
  eq("niv1, op 80 -> 60 (plafond)", throttleOutputPct(80, 1, false, c), 60);
  eq("niv1, op 40 -> 40 (sous plafond)", throttleOutputPct(40, 1, false, c), 40);

  // Niveau 2 : MAINTIEN a 10 %, ne coupe pas
  eq("niv2, op 90 -> 10 (maintien)", throttleOutputPct(90, 2, false, c), 10);
  eq("niv2, op 0  -> 10 (ne coupe pas)", throttleOutputPct(0, 2, false, c), 10);
  eq("niv2, op 5  -> 10 (garde la poussee)", throttleOutputPct(5, 2, false, c), 10);

  // Fail-safe : link perdu -> operateur reprend tout
  eq("link perdu, niv2, op 80 -> 80", throttleOutputPct(80, 2, true, c), 80);
  eq("link perdu, niv2, op 0  -> 0",  throttleOutputPct(0, 2, true, c), 0);

  // Bornes
  eq("op 150 ramene a 100 (niv0)", throttleOutputPct(150, 0, false, c), 100);

  printf("\n%s (%d echec%s)\n", fails ? "ECHECS" : "TOUT PASSE", fails, fails > 1 ? "s" : "");
  return fails ? 1 : 0;
}
