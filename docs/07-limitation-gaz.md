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
- Niveau 2 (imminent) → gaz **maintenus** à `THROTTLE_HOLD_L2` (10 % par défaut) :
  **on ne coupe pas**. Garder un peu de poussée conserve la direction pour
  s'écarter plutôt que de filer tout droit.

**À savoir :** au niveau 1, la sortie ne fait que réduire la commande. Au niveau 2,
elle **maintient** 10 % — donc si la télécommande demandait moins (0 %), le module
**remonte** à 10 % pour garder la manœuvrabilité. C'est volontaire et réservé aux
engins sans pilote. La logique est vérifiée par `tools/throttle_test.cpp`.

## Fail-safe

Si le module n'a plus de données fiables des autres jet-skis depuis
`PEER_STALE_MS` (3 s), il **ne limite plus** : la télécommande reprend 100 % du
contrôle. Perte de radio = retour au comportement normal, comme demandé.

## Branchement (à adapter à ta conversion RC)

| Signal | Broche carte | Vers |
|---|---|---|
| Entrée gaz opérateur (PWM servo) | `THROTTLE_IN_PIN` (35) | sortie « gaz » du récepteur RC |
| Sortie gaz plafonnée (PWM servo) | `THROTTLE_OUT_PIN` (18) | actionneur de gaz (servo / ESC / entrée gaz) |
| Masse commune | GND | masse du système RC |

Réglages PWM : `THROTTLE_MIN_US` / `THROTTLE_MAX_US` (1000–2000 µs par défaut,
standard servo). À ajuster si ton actionneur utilise d'autres largeurs.

> Ceci suppose une commande de gaz en **PWM servo standard**. Si ta conversion
> pilote les gaz autrement (tension analogique, bus, ESC particulier), dis-le :
> on adapte l'étage de sortie. C'est le seul point qui dépend de ton matériel.

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
