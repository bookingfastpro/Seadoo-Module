# Boîtier imprimé 3D

Fichiers prêts à imprimer :

- `boitier_fond.stl` (168 × 148 × 39 mm, pattes comprises)
- `boitier_couvercle.stl` (168 × 110 × 7 mm, s'imprime face extérieure sur le plateau)

Le modèle est paramétrique : il suffit de modifier les cotes en haut de `boitier.py`, puis de relancer
`python3 boitier.py` (dépendances : `pip install manifold3d trimesh numpy`).

## Ce qu'il contient

| Élément | Détail |
|---|---|
| Volume intérieur | 140 × 82 × 36 mm |
| Carte LilyGO T-SIM7600G-H | berceau à rails pour 111 × 33 mm, maintenue par 2 colliers |
| Convertisseur 12→5 V + module CAN | bande libre de 30 mm avec 4 passants de colliers |
| Câbles | 2 trous PG7 (Ø12,7) sur la petite paroi : 1 vers la prise diag, 1 en réserve (relais/servo) |
| Buzzer actif Ø12 | logement sur la paroi opposée, paroi amincie à 1 mm devant (le son passe, l'eau non) |
| Étanchéité | gorge pour cordon silicone Ø2 mm, vis placées à l'extérieur du joint |
| Fixation couvercle | 6 inserts laiton M3 (trou Ø4) dans la collerette + vis M3 × 10 |
| Fixation coque | 4 pattes avec fente pour vis M5 |

## À acheter en plus

- 2 presse-étoupe PG7 (étanches, avec écrou)
- 6 inserts thermo M3 (longueur 5–6 mm) + 6 vis M3 × 10 tête cylindrique
- 0,6 m de cordon silicone Ø2 mm
- Câble 4 conducteurs étanche avec fiche Deutsch DT06-6S au bout, pour la prise diagnostic

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
- Le connecteur Deutsch n'est pas monté en façade. Il reste en bout de câble, qui sort par un
  presse-étoupe comme prévu dans `docs/01-materiel.md`. C'est plus simple et plus fiable à étancher.
