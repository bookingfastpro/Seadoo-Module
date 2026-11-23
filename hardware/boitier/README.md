# Boîtier imprimé 3D (version compacte)

![Aperçu](apercu.png)

Fichiers prêts à imprimer :

- `boitier_fond.stl` : 178 × 54 × 38 mm hors tout
- `boitier_couvercle.stl` : 178 × 54 × 7 mm, s'imprime face extérieure sur le plateau

Fermé : environ 178 × 54 × 42 mm (contre 178 × 110 × 64 mm pour la première version).

Le modèle est paramétrique : il suffit de modifier les cotes en haut de `boitier.py`, puis de relancer
`python3 boitier.py` (dépendances : `pip install manifold3d trimesh numpy matplotlib`).

## Marquage du couvercle

![Marquage](couvercle_marquage.png)

« **NAUTIX** » (capitales de 12 mm) avec « *by WinO* » en dessous (4,8 mm), gravés sur 0,6 mm de profondeur au centre
du plateau, entre la flèche GPS et les bandes. Police Poppins Bold (DejaVu Sans si elle n'est pas installée).
Les textes et tailles se changent avec `BRAND`, `SIGN`, `BRAND_H`, `SIGN_H` dans `boitier.py`.
Astuce : imprimer le couvercle en deux couleurs (changement de filament à 0,6 mm) pour faire ressortir le logo.

## Implantation

| Zone (dans la longueur) | Contenu |
|---|---|
| 0–20 mm | buck Pololu D24V22F5 + module CAN SN65HVD230 à plat (adhésif VHB), buzzer Ø12 au-dessus |
| 20–48 mm | arrière de l'embase Deutsch + fusible |
| 48–160 mm | carte LilyGO T-SIM7600G-H (111 × 33 × 20 mm, accu 18650 dessous), calée par butées et nervures |

Volume intérieur 160 × 36 × 35,5 mm. Antennes GPS et LTE collées sous le couvercle (flèche gravée = côté ciel).

## Prise 6 broches

Embase **Deutsch DT04-6P-L012** sur la grande paroi, collerette à plat sur un plot en relief. Il n'existe pas
d'embase DT 6 voies plus petite : toutes les versions à collerette de TE (L012, LE14, CL06, CL09) ont une
collerette d'environ 31–35,5 × 55 mm. La monter sur la grande paroi plutôt qu'en bout évite d'élargir le boîtier.

- Découpe 25,40 × 25,40 mm R6,35 ; 4 trous à 45,01 × 24,00 mm (plan TE 0425-017-0000)
- Fixation : 4 vis M4 dans des **inserts M4 noyés dans le plot** (trous borgnes) : rien ne dépasse à l'intérieur
  et aucun trou ne traverse la paroi
- Joint plat ou cordon de silicone sous la collerette (la L012 n'a pas de joint de collerette)
- Se branche sur une fiche DT06-6S

| Broche embase | Signal |
|---|---|
| 1 | CAN-H |
| 2 | CAN-L |
| 3 | masse |
| 4 | libre (K-Line ou « alimentation ECU » selon les modèles BRP) |
| 5 | +12 V (probable, à confirmer au multimètre sur le jet) |
| 6 | libre |

Le câble Y recopie les 6 broches du jet à l'identique : l'embase suit donc le
brochage de la prise diagnostic BRP (voir `docs/02-bus-can.md`).

## À acheter en plus

- Embase Deutsch DT04-6P-L012 + 4 contacts mâles taille 16 + cale W6P
- 4 inserts thermo M4 (Ø5,6, longueur 6–7 mm) + 4 vis M4 × 10
- Porte-fusible mini lame + fusible 1 A, en ligne sur le +12 V (dans la zone 20–48 mm)
- 4 inserts thermo M3 (longueur 5–6 mm) + 4 vis M3 × 8 pour le couvercle
- 0,5 m de cordon silicone Ø1,5 mm
- Adhésif double face VHB pour le buck et le module CAN

## Impression

- **ASA** (résiste aux UV et à la chaleur) ou **PETG** au minimum. Le PLA est à éviter.
- 4 périmètres minimum, 5 couches dessus/dessous, remplissage 40 %. Une paroi FDM fine laisse passer l'eau.
- Sans support. Le logement du buzzer est en porte-à-faux sur la paroi : vérifier l'aperçu du trancheur.

## Cotes vérifiées

| Pièce | Cote utilisée | Source |
|---|---|---|
| Embase DT04-6P-L012 | collerette 54,99 × 35,51 × 3,71, découpe 25,40 R6,35, trous 45,01 × 24,00, corps 20,65 mm | plan TE 0425-017-0000 |
| Pololu D24V22F5 | 0,7″ × 0,7″ × 0,31″, entrée 5,3–36 V, 2,2 A continu | pololu.com |
| Carte LilyGO | 111 × 33 × 20 mm | manuel / fiche revendeur — à mesurer |

## À vérifier avant d'imprimer

- **Cotes de la carte** : mesure ta carte. Si elle diffère, ajuste `BOARD_L` / `BOARD_W` / `BOARD_H`.
- **USB-C** : on flashe couvercle ouvert ; un câble USB-C coudé est plus pratique dans un boîtier de 36 mm de large.
