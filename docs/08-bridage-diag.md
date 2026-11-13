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
On la **capture** donc en espionnant un appareil qui sait déjà brider, sur ton
propre jet-ski — sans l'ouvrir, en regardant ce qu'il envoie sur le bus.

## Méthode recommandée : capturer l'OtoTrak

C'est la plus fiable, car l'OtoTrak fait déjà exactement ce qu'on veut, par le
diag. On enregistre le bus pendant qu'il bride. (Le V800/BUDS n'expose en général
pas de bridage « en direct » — il bride via la clé d'apprentissage — donc on
préfère capturer l'OtoTrak.)

Matériel : boîtier en **mode sniffer** (`SNIFFER_MODE 1`), tapé sur le CAN (CAN-H,
CAN-L, masse) sur le jet-ski **équipé de l'OtoTrak**, carte SD insérée.

1. **Enregistrement A (repos)** : OtoTrak actif, **sans** limite, ~30 s.
2. Couper/rallumer le boîtier (nouveau fichier).
3. **Enregistrement B (activation)** : depuis l'**appli OtoTrak**, **active la
   limite de vitesse**. Continue d'enregistrer ~30 s — pour voir si la commande est
   envoyée **une fois** ou **répétée en continu** (session maintenue).
4. Nouveau fichier. **Enregistrement C (retrait)** : depuis l'appli, **retire la
   limite**.

Note l'heure exacte de chaque action. On ne copie pas le logiciel de l'OtoTrak :
on observe seulement les trames qu'il envoie à l'ECM de ton jet-ski.

## Autre source : la clé d'apprentissage (BUDS)

Si tu veux comparer, tu peux aussi enregistrer pendant que tu **programmes une clé
d'apprentissage** dans BUDS (c'est le bridage d'origine BRP). Même principe :
un fichier avant, un pendant la programmation.

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
