# 5. Anti-collision entre les modules

Fonction d'**aide** qui prévient le pilote par un buzzer quand deux de nos
jet-skis risquent de se percuter. Ce n'est pas un pilote automatique : le pilote
reste responsable et garde toujours une surveillance visuelle.

## Principe : deux liaisons, deux métiers

- **Liaison mobile (4G) :** sert à *te montrer* les positions sur le tableau de
  bord et à régler le seuil. Trop lente et trop incertaine pour éviter une
  collision.
- **Radio directe entre modules (ESP-NOW) :** sert à *l'anti-collision*. Les
  modules se parlent directement, sans internet, en quelques millisecondes.

Chaque module diffuse sa position plusieurs fois par seconde. Chaque module
entend les autres, calcule le risque **localement** et sonne si besoin. Ça marche
même sans réseau mobile.

## Modèle « rayon »

Chaque jet-ski porte un **rayon de sécurité** (par défaut **100 m**, réglable par
jet-ski depuis le tableau de bord, où il est dessiné sur la carte). Chaque module
diffuse son rayon aux autres.

L'alarme ne sonne que si **deux conditions** sont réunies :

1. les **deux rayons se touchent** : distance < rayon_A + rayon_B (deux rayons de
   100 m → les cercles se touchent à 200 m), **et**
2. les deux jet-skis se **rapprochent** vraiment (trajectoires qui convergent).

Donc rouler côte à côte à distance constante **ne sonne pas**, même si les cercles
se chevauchent. Le calcul (point
de plus courte approche) est dans `firmware/include/collision.h` et il est
vérifié par `tools/collision_test.cpp` :

```bash
cd tools
g++ -std=c++11 -I../firmware/include collision_test.cpp -o /tmp/ct && /tmp/ct
```

Niveaux :

| Niveau | Quand | Buzzer |
|---|---|---|
| 0 | pas de risque | silencieux |
| 1 | dans la zone en se rapprochant, ou va y entrer bientôt | bip lent |
| 2 | collision imminente | bip rapide |

## Réglage du rayon

Tableau de bord → bouton « Rayon anti-collision : 100 m » → nouvelle valeur. Elle
descend aux modules par la 4G (message retenu), puis chaque module diffuse son
rayon et l'utilise dans son calcul local. Chaque jet-ski peut avoir son propre
rayon ; l'alerte se déclenche sur la **somme** des deux. Le cercle est dessiné
autour de chaque jet-ski sur la carte.

## Portée radio : à vérifier sur l'eau

La portée doit être **nettement plus grande que le seuil** (viser 300-500 m pour
un seuil de 120 m), pour que les modules se voient avant d'entrer en zone.

- **ESP-NOW** (intégré à la carte, gratuit) : quelques centaines de mètres sur
  l'eau dégagée. **À confirmer sur le terrain** avec deux modules.
- Si la portée réelle est juste : passer au **LoRa** (plusieurs km), module à
  ajouter. Le calcul de risque ne change pas, seule la radio change.

Test de portée simple : deux modules alimentés, s'éloigner et noter la distance
(au GPS du tableau de bord) à partir de laquelle un module cesse d'apparaître
dans les traces `COLLISION …` du moniteur série.

## Qualité du GPS

Le GPS intégré (~1 Hz) suffit pour commencer, mais anticipe mal les
rapprochements rapides. Pour une détection plus fine, ajouter un **GPS 5-10 Hz**
(u-blox NEO-M9N) dédié à cette fonction. Le cap est pour l'instant déduit du
déplacement ; un GPS rapide le donne directement et plus proprement.

## Buzzer

Buzzer **actif** 5 V (celui qui sonne seul sous tension), sur `GPIO 19` et la
masse. Changer `BUZZER_PIN` dans `config.h` si besoin, ou `-1` pour le couper.

## Mode journal (test de fiabilité, aucune action)

Le système **n'agit jamais** sur le jet-ski : il reste un avertisseur. Le mode
journal sert à **mesurer** la fiabilité de l'algorithme avant de lui faire
confiance pour quoi que ce soit de plus.

Activé par défaut (`COLLISION_LOG 1`). À chaque sortie, l'algo évalue le risque
en continu et **écrit ses décisions** sur la carte SD dans
`/collision_log.csv` (et sur le port série, lignes `JOURNAL …`), **sans rien
changer au comportement du jet-ski**. La colonne `would_act` note les instants où
un système actif « aurait agi » (niveau ≥ `COLLISION_ACTION_LEVEL`).

Après tes sorties, analyse le fichier :

```bash
python3 tools/collision_log_analyze.py collision_log.csv
```

Il regroupe les notes en **épisodes** et montre, pour chacun : distance minimale,
rapprochement maximal, niveau atteint, et si le système aurait agi. Tu confrontes
ça à ce que tu as vécu sur l'eau :

- `aurait agi = OUI` alors qu'il n'y avait **pas** de danger réel → **faux
  positif** à corriger (ajuster le seuil, le calcul de cap, la cadence GPS).
- rapprochement proche de 0 → vous ne vous rapprochiez pas vraiment.

**Tant qu'il reste des faux positifs, le système n'a rien à faire d'autre que
prévenir.** C'est l'étape obligatoire avant même d'envisager une quelconque
action — et cette décision, de toute façon, ne se prend pas à la légère sur un
engin qui se dirige par la poussée.

## Rappel de sécurité

C'est une aide, pas une garantie. À tester prudemment, loin d'autres usagers,
avant de s'y fier. Ne jamais réduire sa vigilance parce que le système est
présent.
