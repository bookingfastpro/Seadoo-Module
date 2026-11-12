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

## Déclenchement

L'alarme ne sonne que si **deux conditions** sont réunies :

1. distance < seuil (par défaut **120 m**, réglable depuis le tableau de bord), **et**
2. les deux jet-skis se **rapprochent** vraiment (trajectoires qui convergent).

Donc rouler côte à côte à distance constante **ne sonne pas**. Le calcul (point
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

## Réglage de la distance

Tableau de bord → bouton « Anti-collision : 120 m » → nouvelle valeur. Elle
descend aux modules par la 4G, puis chaque module l'utilise dans son calcul
local. À régler sur la **même valeur** pour tous les jet-skis.

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

## Rappel de sécurité

C'est une aide, pas une garantie. À tester prudemment, loin d'autres usagers,
avant de s'y fier. Ne jamais réduire sa vigilance parce que le système est
présent.
