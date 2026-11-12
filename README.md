# Boîtier de suivi Sea-Doo — système perso

Système complet et indépendant pour suivre mes Sea-Doo GTX (2023 et 2026) :
un boîtier par jet-ski, mon propre serveur, mon propre tableau de bord.
Aucun code ni service d'un fabricant tiers n'est repris.

```
 Sea-Doo                Boîtier (un par jet-ski)                 Serveur (Supabase)        Moi
┌────────┐  CAN   ┌──────────────────────────────────┐  LTE   ┌──────────────────┐   ┌────────────┐
│  ECM   ├───────►│ transceiver CAN (écoute seule)   │ HTTPS  │ fonction ingest  │   │ tableau de │
│ compteur│       │ ESP32 + modem SIM7600 + GNSS     ├───────►│ base Postgres    │◄──┤ bord web   │
└────────┘        │ relais anti-démarrage (option)   │◄───────┤ consignes        │   │ (carte)    │
   12 V ─────────►│ alimentation 12 V → 5 V          │        └──────────────────┘   └────────────┘
                  └──────────────────────────────────┘
```

## Ce que fait le système

| Fonction | État |
|---|---|
| Position GPS en direct (~1 s en MQTT) + trace des dernières 24 h | prêt |
| Zone autorisée (geofence) avec alerte à la sortie | prêt |
| Jet-ski allumé / éteint, réseau, tension batterie | prêt |
| Régime, vitesse, carburant, heures moteur | **à décoder** sur le bus CAN (étape 2) |
| Anti-démarrage à distance | prêt côté logiciel, câblage à faire (étape 5) |
| **Anti-collision** entre nos modules (buzzer, seuil réglable) | logique vérifiée, radio à tester sur l'eau (docs/05) |
| Mode journal anti-collision (mesure de fiabilité, aucune action) | prêt (docs/05) |
| Limitation des gaz (engins SANS pilote, télécommandés) | prêt, désactivée par défaut (docs/07) |
| Bridage de vitesse pour un pilote à bord | volontairement absent — dangereux (docs/04, docs/07) |

## Contenu du dépôt

| Dossier | Rôle |
|---|---|
| `firmware/` | Programme du boîtier (ESP32, PlatformIO) |
| `backend/supabase/` | Base de données + fonction qui reçoit les données |
| `dashboard/` | Tableau de bord web (carte, état, commandes) |
| `tools/can_analyze.py` | Outil pour décoder le bus CAN |
| `tools/collision_test.cpp` | Vérification du calcul anti-collision |
| `tools/collision_log_analyze.py` | Analyse du journal anti-collision (fiabilité) |
| `tools/throttle_test.cpp` | Vérification de la logique de limitation des gaz |
| `docs/` | Guides pas à pas |

## Feuille de route

1. **Commander le matériel** — [docs/01-materiel.md](docs/01-materiel.md) (environ 110 à 140 € par boîtier)
2. **Écouter et décoder le bus CAN** — [docs/02-bus-can.md](docs/02-bus-can.md)
3. **Mettre le serveur en route** — [docs/03-backend.md](docs/03-backend.md)
4. **Passer le boîtier en mode suivi**, le monter dans un boîtier étanche
5. **Anti-démarrage** (optionnel) — [docs/04-securite.md](docs/04-securite.md)
6. **Anti-collision** entre modules — [docs/05-anti-collision.md](docs/05-anti-collision.md)
7. **Temps réel (1 s) par MQTT** — [docs/06-mqtt.md](docs/06-mqtt.md)

## État de vérification

- `tools/can_analyze.py` : testé sur des relevés synthétiques.
- `firmware/` : **pas encore compilé ni essayé sur carte** (pas d'accès au
  matériel ni au registre PlatformIO depuis l'environnement de rédaction).
  Première chose à faire à réception de la carte : `pio run`, corriger au besoin.
- `backend/supabase/migrations/001_init.sql` : exécuté sur un Postgres 16 local,
  règles d'accès vérifiées (clé illisible, écriture limitée, anonyme bloqué).
- Fonction `ingest` et tableau de bord : écrits mais pas encore déployés ni essayés.
- Brochage de la prise diagnostic et identifiants CAN : **non confirmés**, à
  mesurer et relever sur chaque jet-ski (c'est l'objet de l'étape 2).
