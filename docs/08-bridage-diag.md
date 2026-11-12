# 8. Bridage par le calculateur (façon OtoTrak)

But : reproduire le fonctionnement de l'OtoTrak — brider la vitesse **à distance,
par la seule prise diag**, en activant la **limitation intégrée au calculateur
(ECM)**. Ce n'est pas « commander les gaz » : c'est demander à l'ECM d'appliquer
sa propre limite (le même mécanisme que BUDS et la clé d'apprentissage). L'ECM
l'applique lui-même, le jet-ski reste contrôlable sous la limite.

> ⚠️ Réservé aux engins **sans pilote**. Et ici le boîtier **émet** sur le bus :
> à n'activer qu'avec la commande **capturée et testée au banc**, jamais devinée.

## Pourquoi il faut capturer (et ne pas deviner)

La commande exacte qui active la limite est propre à BRP et peut varier selon le
modèle/année. Une mauvaise trame de diagnostic pourrait écrire un autre réglage.
On la **capture** donc en espionnant BUDS2, qui sait déjà le faire.

## Capturer la commande avec BUDS2

Matériel : boîtier en **mode sniffer** (`SNIFFER_MODE 1`), branché en **Y** sur la
prise diag en même temps que l'interface BUDS2 (carte SD insérée).

1. **Enregistrement A (repos)** : BUDS2 connecté, lecture seule, ne rien changer,
   ~30 s. → fichier `can_000.csv` par ex.
2. Couper/rallumer le boîtier pour ouvrir un nouveau fichier.
3. **Enregistrement B (activation)** : dans BUDS2, **active la limite de vitesse**
   (ou le mode vitesse réduite). → `can_001.csv`.
4. Nouveau fichier, puis **Enregistrement C (retrait)** : dans BUDS2, **enlève la
   limite**. → `can_002.csv`.

Note l'heure exacte de chaque action.

## Isoler la commande

```bash
python3 tools/can_analyze.py resume can_001.csv              # vue d'ensemble
python3 tools/can_analyze.py compare can_000.csv can_001.csv # ce qui apparait a l'activation
python3 tools/can_analyze.py compare can_001.csv can_002.csv # ce qui change au retrait
```

On cherche les trames **émises vers l'ECM** au moment de ton action (souvent des
identifiants de diagnostic type `0x7E0` / `0x7E8`, requête puis réponse). Déposer
les fichiers `.csv` dans le dépôt : je t'aide à repérer la bonne séquence.

Points d'attention repérés à la capture :
- **Protocole multi-trames (ISO-TP)** : si la commande tient sur plusieurs trames
  liées, on adapte l'envoi (pour l'instant le firmware envoie une liste simple).
- **Déverrouillage (seed-key)** : si BUDS fait un échange d'authentification avant,
  il faudra le reproduire. La capture le montrera.

## Remplir et activer

1. Recopier la séquence capturée dans `firmware/include/diag_commands.h` :
   `DIAG_LIMIT_ON[]` (activation) et `DIAG_LIMIT_OFF[]` (retrait).
2. Passer `DIAG_LIMIT_ENABLE` à `1` dans `config.h` (le CAN passe alors en mode
   normal pour pouvoir émettre).
3. **Tester au banc / à l'arrêt d'abord** : déclencher « Brider la vitesse » depuis
   le tableau de bord, vérifier avec BUDS2 que la limite s'active, puis se retire.
4. Seulement ensuite, essai sur l'eau, sans personne à bord.

## Pilotage

Depuis le tableau de bord, bouton **« Brider la vitesse »** par jet-ski. La
consigne descend par la 4G/MQTT (comme les autres), et le boîtier envoie la
commande de diagnostic à l'ECM. Tant que `diag_commands.h` est vide, rien n'est
émis : le cadre est prêt, il attend la capture.
