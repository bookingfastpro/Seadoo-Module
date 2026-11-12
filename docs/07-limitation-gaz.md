# 7. Limitation des gaz — engins SANS PILOTE uniquement

> ⚠️ **À n'activer que sur un jet-ski sans pilote, télécommandé, sur plan d'eau
> privé et fermé, encadré par des professionnels.** Réduire les gaz retire la
> direction (un jet-ski se dirige par la poussée). **Avec un humain à bord, c'est
> dangereux — ne jamais activer.** Désactivé par défaut (`THROTTLE_LIMIT_ENABLE 0`).

Cette fonction permet à l'anti-collision de **plafonner** les gaz d'un engin
télécommandé quand un rapprochement devient critique, puis de rendre la main.

## Topologie : limiteur en série

Le module s'intercale entre la **commande de gaz de la télécommande** (sortie du
récepteur RC) et **l'actionneur de gaz** de la conversion :

```
Récepteur RC (gaz opérateur) ──PWM──► [module : plafond] ──PWM──► actionneur de gaz
```

- Pas de risque → la commande ressort **telle quelle**.
- Niveau 1 (vigilance) → gaz **plafonnés** à `THROTTLE_CAP_L1` (60 % par défaut) :
  réduit seulement.
- Niveau 2 (imminent) → le robot **maintient l'appui** à `THROTTLE_HOLD_L2`
  (10 % de course par défaut) : **on n'arrête pas d'appuyer**. Garder un peu de
  poussée conserve la direction pour s'écarter plutôt que de filer tout droit.

> Les % sont une **course de gâchette** (jusqu'où le robot appuie), pas une valeur
> lue sur le jet-ski. La course n'est pas linéaire avec la vitesse : cale les 10 %
> sur l'appui qui donne, chez toi, une allure lente et manœuvrable.

**À savoir :** au niveau 1, la sortie ne fait que réduire la commande. Au niveau 2,
elle **maintient** 10 % — donc si la télécommande demandait moins (0 %), le module
**remonte** à 10 % pour garder la manœuvrabilité. C'est volontaire et réservé aux
engins sans pilote. La logique est vérifiée par `tools/throttle_test.cpp`.

## Fail-safe

Si le module n'a plus de données fiables des autres jet-skis depuis
`PEER_STALE_MS` (3 s), il **ne limite plus** : la télécommande reprend 100 % du
contrôle. Perte de radio = retour au comportement normal, comme demandé.

## Branchement

Le servo tire physiquement la gâchette. Notre module s'intercale sur le **fil de
signal** entre le récepteur RC et ce servo.

```
Récepteur RC (voie gaz) ──signal──► module (IN 35)
                                     module (OUT 18) ──signal──► servo gâchette
                              masse commune partout
```

| Signal | Broche carte | Vers |
|---|---|---|
| Entrée : gâchette opérateur (PWM) | `THROTTLE_IN_PIN` (35) | sortie voie gaz du récepteur RC |
| Sortie : gâchette plafonnée (PWM) | `THROTTLE_OUT_PIN` (18) | fil signal du servo de gâchette |
| Masse commune | GND | masse du récepteur **et** du servo |
| Commande du relais de contournement | `BYPASS_RELAY_PIN` (23) | bobine du relais (via driver) |

> **N'alimente PAS le servo depuis la carte** : seul le fil de **signal** passe par
> le module. Le servo garde son alimentation 5–6 V du système RC (BEC/batterie).

### Relais de contournement (sécurité du montage en série)

Un relais dont le contact relie **directement récepteur → servo** au repos :

- Module **éteint, en veille ou planté** (reset) → relais retombé → la télécommande
  pilote le servo **en direct**. Un module mort ne bloque jamais la gâchette.
- Module **en marche** → il excite le relais et prend la main (sortie plafonnée).

C'est le pendant matériel du fail-safe logiciel. Fortement recommandé ; sinon un
plantage pourrait figer la gâchette sur la dernière valeur.

### Calibration (à faire une fois)

Règle `THROTTLE_MIN_US` / `THROTTLE_MAX_US` sur les largeurs réelles qui
correspondent, **sur ton montage**, à gâchette **relâchée** (0 %) et **plein gaz**
(100 %). Mesure-les (servo tester / moniteur) : beaucoup de servos ne sont pas
exactement 1000/2000 µs. Vérifie aussi le sens (que `MIN_US` = ralenti, pas
l'inverse).

## Procédure de test (banc / remorque d'abord)

1. `THROTTLE_LIMIT_ENABLE 1`, reflasher **un** module, moteur **à l'arrêt**.
2. Vérifier d'abord **sans risque** : bouger les gaz à la télécommande, la sortie
   doit suivre exactement (pas de limitation).
3. Simuler un niveau 2 (approcher deux modules alimentés sous le rayon, en
   rapprochement) : la sortie doit se **maintenir à 10 %** (même manette relâchée),
   puis revenir à 100 % après 3 s de perte de contact.
4. Mesurer la sortie (servo tester / oscilloscope) **avant** de la relier à
   l'actionneur réel.
5. Garder le **mode journal** actif en parallèle pour continuer à mesurer les
   faux positifs sur l'eau.

## Rappel

Un faux positif ici **coupe les gaz pour de vrai**. D'où l'ordre imposé : d'abord
le journal (docs/05) pour prouver que l'algorithme se trompe rarement, puis ces
tests, et toujours sans personne à bord.
