// Verification du calcul de risque de collision (firmware/include/collision.h).
// Compilation : g++ -std=c++11 -I../firmware/include collision_test.cpp -o /tmp/ct && /tmp/ct
#include "collision.h"
#include <cstdio>
#include <cmath>

// Decale une position de base de (est, nord) metres.
static CraftState at(double lat0, double lon0, double east_m, double north_m,
                     float spd, float hdg) {
  const double R = 6371000.0;
  double lat = lat0 + (north_m / R) * 180.0 / M_PI;
  double lon = lon0 + (east_m / (R * cos(lat0 * M_PI / 180.0))) * 180.0 / M_PI;
  return CraftState{lat, lon, spd, hdg, true};
}

static int fails = 0;
static void check(const char *name, int got, int want, const Threat &t) {
  bool ok = got == want;
  if (!ok) fails++;
  printf("%s  %-34s niveau=%d (attendu %d)  dist=%.0fm  rappr=%.1fm/s  tcpa=%.1fs  dcpa=%.0fm\n",
         ok ? "OK " : "NON", name, got, want, t.distance_m, t.range_rate_mps, t.tcpa_s, t.dcpa_m);
}

int main() {
  const double LAT = 43.55, LON = 7.02;  // un coin de Mediterranee
  const float TH = 120.0f;               // seuil par defaut demande

  // 1. Cote a cote, meme cap, meme vitesse, 100 m : ils ne se rapprochent pas.
  {
    CraftState me = at(LAT, LON, 0, 0, 15, 0);
    CraftState ot = at(LAT, LON, 100, 0, 15, 0);
    Threat t = assessThreat(me, ot, TH);
    check("cote-a-cote 100m", t.level, 0, t);
  }
  // 2. Face a face, 100 m, 15 m/s chacun : danger imminent.
  {
    CraftState me = at(LAT, LON, 0, 0, 15, 0);     // va vers le nord
    CraftState ot = at(LAT, LON, 0, 100, 15, 180);  // 100m au nord, vient vers le sud
    Threat t = assessThreat(me, ot, TH);
    check("face-a-face 100m", t.level, 2, t);
  }
  // 3. Face a face encore loin, 250 m : hors zone mais on anticipe -> vigilance.
  {
    CraftState me = at(LAT, LON, 0, 0, 15, 0);
    CraftState ot = at(LAT, LON, 0, 250, 15, 180);
    Threat t = assessThreat(me, ot, TH);
    check("face-a-face 250m (anticipe)", t.level, 1, t);
  }
  // 4. L'autre est a l'arret, j'arrive droit dessus a 80 m : imminent.
  {
    CraftState me = at(LAT, LON, 0, 0, 12, 0);
    CraftState ot = at(LAT, LON, 0, 80, 0, 0);
    Threat t = assessThreat(me, ot, TH);
    check("arret devant moi 80m", t.level, 2, t);
  }
  // 5. Je m'eloigne de lui : rien.
  {
    CraftState me = at(LAT, LON, 0, 0, 15, 0);      // je file au nord
    CraftState ot = at(LAT, LON, 0, 60, 15, 0);     // il est au sud... non : 60m nord, meme cap
    // pour un vrai eloignement : il est derriere moi (sud) et je pars au nord
    ot = at(LAT, LON, 0, -60, 5, 0);
    Threat t = assessThreat(me, ot, TH);
    check("je m'eloigne", t.level, 0, t);
  }
  // 6. Trajectoires croisees qui se manquent de loin : pas d'alarme.
  {
    CraftState me = at(LAT, LON, 0, 0, 15, 0);       // nord
    CraftState ot = at(LAT, LON, 300, 0, 15, 270);   // 300m a l'est, file vers l'ouest, loin devant
    Threat t = assessThreat(me, ot, TH);
    check("croisement qui se manque", t.level, 0, t);
  }
  // 7. Convergence a angle droit qui va se croiser de pres.
  {
    CraftState me = at(LAT, LON, 0, 0, 14, 0);       // nord
    CraftState ot = at(LAT, LON, 90, 90, 14, 270);   // vient de la droite vers le point devant moi
    Threat t = assessThreat(me, ot, TH);
    printf("INFO croisement serre : niveau=%d dist=%.0f tcpa=%.1f dcpa=%.0f\n",
           t.level, t.distance_m, t.tcpa_s, t.dcpa_m);
  }

  printf("\n%s (%d echec%s)\n", fails ? "ECHECS" : "TOUT PASSE", fails, fails > 1 ? "s" : "");
  return fails ? 1 : 0;
}
