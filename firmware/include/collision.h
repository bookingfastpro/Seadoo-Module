// Calcul du risque de collision entre deux jet-skis.
//
// Logique volontairement simple et sans materiel (testable sur ordinateur) :
// a partir de la position, la vitesse et le cap de chaque jet-ski, on calcule
//   - la distance actuelle,
//   - s'ils se rapprochent (vitesse de rapprochement),
//   - le point de plus courte approche (CPA) : dans combien de temps ils seront
//     au plus pres, et a quelle distance.
//
// L'alarme ne se declenche QUE s'ils se rapprochent vraiment : rouler cote a
// cote a distance constante ne sonne pas.
#pragma once
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct CraftState {
  double lat;         // degres
  double lon;         // degres
  float speed_mps;    // vitesse sol en m/s
  float heading_deg;  // cap : 0 = Nord, sens horaire, en degres
  bool valid;         // position utilisable ?
};

struct Threat {
  int level;             // 0 = rien, 1 = vigilance, 2 = imminent
  float distance_m;      // distance actuelle
  float range_rate_mps;  // < 0 = ils se rapprochent
  float tcpa_s;          // temps jusqu'au plus pres (0 si non pertinent)
  float dcpa_m;          // distance au plus pres
};

static inline double _velEast(const CraftState &c) {
  return (double)c.speed_mps * sin(c.heading_deg * M_PI / 180.0);
}
static inline double _velNorth(const CraftState &c) {
  return (double)c.speed_mps * cos(c.heading_deg * M_PI / 180.0);
}

// me / other : etats des deux jet-skis. threshold_m : distance d'alerte (reglable).
// horizon_s : a combien de secondes a l'avance on anticipe une entree en zone.
static inline Threat assessThreat(const CraftState &me, const CraftState &other,
                                  float threshold_m, float horizon_s = 10.0f) {
  Threat t = {0, 0.0f, 0.0f, 0.0f, 0.0f};
  if (!me.valid || !other.valid) return t;

  // Repere local en metres autour de "me" (approximation plan equirectangulaire).
  const double R = 6371000.0;
  double latr = me.lat * M_PI / 180.0;
  double rx = (other.lon - me.lon) * M_PI / 180.0 * R * cos(latr);  // est
  double ry = (other.lat - me.lat) * M_PI / 180.0 * R;             // nord
  double dist = sqrt(rx * rx + ry * ry);
  t.distance_m = (float)dist;

  // Vitesse relative (autre par rapport a moi), composantes est/nord.
  double vrx = _velEast(other) - _velEast(me);
  double vry = _velNorth(other) - _velNorth(me);
  double v2 = vrx * vrx + vry * vry;
  double rv = rx * vrx + ry * vry;

  t.range_rate_mps = dist > 0.1 ? (float)(rv / dist) : 0.0f;

  double tcpa = v2 > 1e-6 ? -rv / v2 : 0.0;  // instant du plus pres
  double dcpa = dist;
  if (tcpa > 0) {
    double cx = rx + vrx * tcpa, cy = ry + vry * tcpa;
    dcpa = sqrt(cx * cx + cy * cy);
  } else {
    tcpa = 0.0;  // le plus pres est deja passe : ils s'eloignent
  }
  t.tcpa_s = (float)tcpa;
  t.dcpa_m = (float)dcpa;

  bool closing = t.range_rate_mps < -0.3f;  // rapprochement de plus de 0,3 m/s

  if (dist <= threshold_m && closing) {
    t.level = 1;
    bool imminent = (tcpa > 0 && tcpa <= horizon_s && dcpa <= threshold_m * 0.6f) ||
                    dist <= threshold_m * 0.4f;
    if (imminent) t.level = 2;
  } else if (dist > threshold_m && closing && tcpa > 0 && tcpa <= horizon_s &&
             dcpa <= threshold_m) {
    t.level = 1;  // hors zone mais va y entrer bientot
  }
  return t;
}
