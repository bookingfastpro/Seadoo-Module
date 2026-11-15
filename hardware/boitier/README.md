# Boîtier imprimé 3D

Fichiers prêts à imprimer :

- `boitier_fond.stl` (178 × 148 × 56 mm, pattes comprises)
- `boitier_couvercle.stl` (178 × 110 × 7 mm, s'imprime face extérieure sur le plateau)

Le modèle est paramétrique : il suffit de modifier les cotes en haut de `boitier.py`, puis de relancer
`python3 boitier.py` (dépendances : `pip install manifold3d trimesh numpy`).

## Ce qu'il contient

| Élément | Détail |
|---|---|
| Volume intérieur | 150 × 82 × 53 mm |
| Carte LilyGO T-SIM7600G-H | berceau à rails pour 111 × 33 mm, maintenue par 2 colliers |
| Convertisseur 12→5 V + module CAN | bande libre de 30 mm avec 4 passants de colliers |
| Prise diag | embase Deutsch **DT04-6P-L012** en façade (petite paroi) : découpe 25,40 × 25,40 mm R6,35, 4 trous Ø4,32 à 45,01 × 24,00 mm — cotes du plan TE 0425-017-0000 |
| Câble de réserve | 1 trou PG7 Ø12,7 sur une grande paroi (relais / servo) |
| Buzzer actif Ø12 | logement sur la paroi opposée, paroi amincie à 1 mm devant (le son passe, l'eau non) |
| Étanchéité | gorge pour cordon silicone Ø2 mm, vis placées à l'extérieur du joint |
| Fixation couvercle | 6 inserts laiton M3 (trou Ø4) dans la collerette + vis M3 × 10 |
| Fixation coque | 4 pattes avec fente pour vis M5 |

## À acheter en plus

- 1 embase Deutsch DT04-6P-L012 + 6 contacts mâles taille 16 + cale W6P
- 4 vis M4 × 12 + écrous nylstop + rondelles (trous Ø4,32 prévus par TE)
- 1 joint plat ou un cordon de mastic silicone sous la collerette : la L012 n'a pas de joint de collerette
- 1 presse-étoupe PG7 (étanche, avec écrou)
- 6 inserts thermo M3 (longueur 5–6 mm) + 6 vis M3 × 10 tête cylindrique
- 0,6 m de cordon silicone Ø2 mm
- Rallonge Deutsch : fiche DT06-6S côté boîtier, et à l'autre bout le genre inverse de la prise du jet-ski (à vérifier sur la machine)

## Impression

- **ASA** (résiste aux UV et à la chaleur) ou **PETG** au minimum. Le PLA est à éviter.
- 4 périmètres minimum, 5 couches dessus/dessous, remplissage 40 %. Une paroi FDM fine laisse passer l'eau.
- Le fond s'imprime sans support. Le dessous de la collerette est chanfreiné à 45°.

## À vérifier avant d'imprimer

- **Cotes de la carte** : j'ai pris 111 × 33 × 20 mm, la taille commerciale avec le support 18650.
  Mesure ta carte. Si elle diffère, ajuste `BOARD_L` / `BOARD_W`.
- **USB-C** : il y a 25 mm de dégagement côté presse-étoupe. Mets le port USB de ce côté et
  utilise un câble coudé pour flasher, couvercle ouvert.
- **Antennes** : colle-les sous le couvercle (flèche gravée = côté ciel).
- **Genre de la prise diag du jet-ski** : l'embase du boîtier porte des broches (DT04). La rallonge
  doit avoir une DT06-6S côté boîtier et le genre opposé à la prise du Sea-Doo de l'autre côté.

## Cotes vérifiées

| Pièce | Cote utilisée | Source |
|---|---|---|
| Embase DT04-6P-L012 | collerette 54,99 × 35,51 × 3,71 mm, profondeur derrière collerette 20,65 mm | plan TE 0425-017-0000, feuille 1 |
| | découpe 25,40 × 25,40 R6,35, 4 × Ø4,32 à 45,01 × 24,00 | plan TE, feuille 2 (« recommended cutout ») |
| | épaisseur de paroi max 16,5 mm (la nôtre : 3 mm) | spécification d'application TE 114-151009 |
| Presse-étoupe PG7 | filetage Ø12,5, perçage 12,5–12,7 mm | fiches fabricants (VIOX, Phoenix Contact) |
| Carte LilyGO | 111 × 33 mm | fiche revendeur (TinyTronics) — à mesurer |
