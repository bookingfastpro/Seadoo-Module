# 1. Matériel

Prix indicatifs (à vérifier au moment de la commande). Quantités pour **un** boîtier ;
doubler pour équiper les deux GTX.

## Liste de courses

| Pièce | Référence conseillée | Prix env. |
|---|---|---|
| Carte principale (ESP32 + LTE + GPS + lecteur SD) | **LilyGO T-SIM7600G-H** (version « G-H » = bandes mondiales) | 60–75 € |
| Antennes LTE + GPS | fournies avec la carte en général ; sinon antennes adhésives u.FL | 0–10 € |
| Transceiver CAN 3,3 V | module **SN65HVD230** (ou TJA1051T/3) | 3–5 € |
| Convertisseur 12 V → 5 V | module buck 3 A étanche/encapsulé (entrée 8–32 V) | 6–10 € |
| Porte-fusible en ligne + fusible 2 A | type automobile mini | 4 € |
| Carte microSD | 8–32 Go, pour les relevés CAN | 6 € |
| Carte SIM data M2M | 1NCE, Things Mobile, Emnify… | env. 10–15 € |
| Boîtier étanche | IP67, environ 120 × 80 × 55 mm, + presse-étoupe PG7 | 10–15 € |
| Connecteur prise diagnostic | Deutsch DT 6 voies (vérifier mâle/femelle sur le jet-ski) + contacts | 8–12 € |
| Vernis de tropicalisation | bombe pour circuits imprimés | 10 € (pour tous) |
| Optionnel : relais anti-démarrage | module relais 5 V 1 voie avec optocoupleur | 3 € |
| Optionnel : mesure tension batterie | résistances 100 kΩ + 22 kΩ | < 1 € |

Outillage : multimètre, fer à souder, câble USB-C, pince à sertir Deutsch (ou
cosses pré-serties).

## Câblage

```
Batterie 12 V (+) ──[fusible 2 A]── buck IN+        buck OUT+ (5 V) ── carte : broche 5V / VIN
Batterie 12 V (−) ───────────────── buck IN−        buck OUT− ──────── carte : GND

Transceiver CAN          Carte LilyGO
   3V3  ───────────────── 3V3
   GND  ───────────────── GND
   CTX (D) ────────────── GPIO 22
   CRX (R) ────────────── GPIO 21
   CRX (R) ────────────── GPIO 34   (2e fil depuis CRX : reveil au demarrage moteur)
   CANH ───────────────── CAN-H du jet-ski (prise diagnostic)
   CANL ───────────────── CAN-L du jet-ski
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
- Alimenter depuis la batterie (via fusible) et non depuis un 12 V commuté,
  sinon plus de suivi quand le jet-ski est éteint.

## Consommation et batterie

Jet-ski éteint, le boîtier dort et se réveille toutes les 30 minutes (réglable)
pour donner sa position. Le courant en sommeil dépend de la carte et du
convertisseur : **le mesurer au multimètre** une fois monté. Viser moins de
5 mA ; au-delà, choisir un buck à faible courant de repos. Pour un hivernage
long, débrancher le boîtier ou garder la batterie sur mainteneur de charge.

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
