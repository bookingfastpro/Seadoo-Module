# 1. Matériel

Prix indicatifs (à vérifier au moment de la commande). Quantités pour **un** boîtier ;
doubler pour équiper les deux GTX.

## Liste de courses

| Pièce | Référence conseillée | Prix env. |
|---|---|---|
| Carte principale (ESP32 + LTE + GPS + lecteur SD) | **LilyGO T-SIM7600G-H** (version « G-H » = bandes mondiales) | 60–75 € |
| Antennes LTE + GPS | fournies avec la carte en général ; sinon antennes adhésives u.FL | 0–10 € |
| Transceiver CAN 3,3 V | module **SN65HVD230** (ou TJA1051T/3) | 3–5 € |
| Convertisseur 12 V → 5 V | **Pololu D24V22F5** (17,8 × 17,8 mm, entrée 5,3–36 V, 2,2 A, protégé inversion) | 15–19 € |
| Accu Li-ion 18650 | cellule de marque 3000–3500 mAh (Samsung, LG, Molicel…), pas de « 9900 mAh » sans marque | 6–10 € |
| Porte-fusible en ligne + fusible 1 A | type automobile mini, sur le +12 V à l'entrée du boîtier | 4 € |
| Carte microSD | 8–32 Go, pour les relevés CAN | 6 € |
| Carte SIM data M2M | 1NCE, Things Mobile, Emnify… | env. 10–15 € |
| Boîtier étanche | imprimé 3D (`hardware/boitier/`) ou IP67 du commerce | 10–15 € |
| Connecteur | embase **Deutsch DT04-6P-L012** sur le boîtier (se branche sur une fiche DT06-6S ; rallonge si le genre ou la longueur ne collent pas) | 15–25 € |
| Vernis de tropicalisation | bombe pour circuits imprimés | 10 € (pour tous) |
| Buzzer actif 5 V (alerte anti-collision) | « active buzzer 5V » (celui qui sonne seul sous tension) | 2 € |
| Optionnel : GPS rapide 5-10 Hz | module u-blox **NEO-M9N** (meilleure anticipation de collision) | 15-25 € |
| Optionnel : relais anti-démarrage | module relais 5 V 1 voie avec optocoupleur | 3 € |
| Optionnel : mesure tension batterie | résistances 100 kΩ + 22 kΩ | < 1 € |

Outillage : multimètre, fer à souder, câble USB-C, pince à sertir Deutsch (ou
cosses pré-serties).

## Câblage

```
Embase 4 (+12 V) ──[fusible 1 A]── buck VIN     buck VOUT (5 V) ── carte : connecteur SOLAIRE (+)
Embase 3 (masse) ───────────────── buck GND     buck GND ───────── carte : connecteur SOLAIRE (−)

Accu 18650 dans le support au dos de la carte, interrupteur de la carte sur ON.

Embase DT04-6P du boîtier (brochage choisi par nous)
   1  CAN-H   → transceiver CANH
   2  CAN-L   → transceiver CANL
   3  Masse   → masse commune (buck GND, transceiver GND, carte GND)
   4  +12 V   → fusible 1 A → buck VIN
   5-6        libres
Brochage de la prise du jet-ski à relever au multimètre (doc 02).

Transceiver CAN          Carte LilyGO
   3V3  ───────────────── 3V3
   GND  ───────────────── GND
   CTX (D) ────────────── GPIO 22
   CRX (R) ────────────── GPIO 21
   CRX (R) ────────────── GPIO 34   (2e fil depuis CRX : reveil au demarrage moteur)
   CANH ───────────────── CAN-H du jet-ski (prise diagnostic)
   CANL ───────────────── CAN-L du jet-ski

Buzzer actif             Carte LilyGO
   +    ───────────────── GPIO 19
   −    ───────────────── GND
```

Points importants :

- **Retirer la résistance de terminaison 120 Ω** du module transceiver (souvent
  marquée `R2` ou `121` entre CANH et CANL). Le bus du jet-ski est déjà terminé ;
  en ajouter une troisième peut le perturber.
- Le firmware met le contrôleur CAN en **écoute seule** : le boîtier ne peut pas
  émettre de trame ni même d'accusé de réception. C'est la garantie de ne rien
  perturber sur le jet-ski.
- Si les broches 21/22 ne sont pas accessibles sur votre révision de carte,
  prendre deux autres GPIO libres et modifier `CAN_TX_PIN` / `CAN_RX_PIN` dans
  `config.h`.
- Masse commune obligatoire entre le boîtier et le jet-ski.
- **Alimentation par la prise diagnostic.** Sur les BRP, son +12 V n'est présent
  que jet-ski réveillé (BRP vend d'ailleurs une interface pour garder l'outil
  alimenté contact coupé). Le boîtier vit donc sur son **accu 18650** :
  - jet-ski en marche : le buck charge l'accu par l'entrée solaire de la carte
    (4,4–6 V, c'est ce que conseille LilyGO pour une alimentation 5 V) ;
  - jet-ski coupé : l'accu prend le relais, le suivi continue.
- **Fusible 1 A sur le +12 V, juste derrière l'embase** : ce +12 V alimente aussi
  l'ECU. Un court-circuit dans le boîtier doit faire sauter notre fusible, pas
  celui du jet-ski.
- Repérer au multimètre quelle broche porte le +12 V et la masse (doc 02) :
  le brochage des GTX 2023/2026 n'est pas confirmé.
- Sur cette carte, le pont diviseur de mesure de l'accu est câblé sur GPIO 35,
  déjà utilisé par `THROTTLE_IN_PIN` (option servo, désactivée par défaut) :
  changer cette broche avant d'activer l'option.

## Consommation et batterie

Jet-ski éteint, le boîtier tourne sur l'accu 18650 : il dort et se réveille
toutes les 30 minutes (réglable) pour donner sa position. Il ne tire plus rien
sur la batterie du jet-ski. L'autonomie dépend du courant en sommeil et du
nombre de réveils : **la mesurer** (relever la tension de l'accu sur quelques
jours). Espacer les réveils pour tenir plus longtemps.

- Accu Li-ion : éviter de laisser le jet-ski en plein soleil bâché sur de longues
  périodes (au-delà de 60 °C dans la coque, l'accu vieillit vite).
- Hivernage : retirer l'accu ou mettre l'interrupteur de la carte sur OFF.

## Volume de données

Chaque envoi ouvre une session HTTPS (quelques kilo-octets). Ordre de grandeur :
un point toutes les 30 s pendant 4 h ≈ 2 à 3 Mo. Un forfait M2M de 500 Mo
couvre donc plusieurs saisons d'usage normal ; descendre à 10 s consomme trois
fois plus. Ce sont des estimations : surveiller la consommation réelle sur le
portail de l'opérateur le premier mois.

## Étanchéité

- Boîtier IP67, sorties de câbles par presse-étoupe orienté vers le bas.
- Vernis de tropicalisation sur les cartes (protéger connecteurs, SIM et SD).
- Monter le boîtier en hauteur dans la coque, **antenne GPS face au ciel**, loin
  du moteur et des masses métalliques. La fibre de la coque laisse passer GPS et LTE.
- Sachet de silice dans le boîtier.
