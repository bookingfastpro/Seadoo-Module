# 6. Temps réel par MQTT

Par défaut, chaque point rouvre une connexion HTTPS : simple, mais lourd si on
veut un point par seconde. MQTT garde **une liaison ouverte** : les points
arrivent en continu, pour très peu de données, et les consignes (blocage,
seuils) descendent instantanément.

## Vue d'ensemble

```
Boîtier ──(MQTT, 1 s)──► Broker ──(webhook)──► fonction "ingest" (historique + alertes)
   ▲                        │
   └──(consignes retenues)──┘◄── Tableau de bord (carte en direct + réglages)
```

- Le **broker** est un petit relais de messages (service gratuit, voir plus bas).
- Le boîtier **publie** ses points sur `seadoo/<id>/telemetry`.
- Le boîtier **écoute** ses consignes sur `seadoo/<id>/cmd` (message *retenu* : au
  réveil, il reçoit tout de suite la dernière consigne).
- Le **tableau de bord** s'abonne pour afficher la carte en direct, et publie les
  consignes quand tu changes un réglage.
- Le **webhook du broker** recopie chaque point vers la fonction `ingest` : on
  réutilise tel quel l'historique, la cloture virtuelle et les alertes de l'étape 3.

## 1. Créer un broker gratuit

Au choix (offres gratuites suffisantes pour quelques jet-skis) :

- **HiveMQ Cloud** : crée un cluster, note l'adresse (`xxxx.s1.eu.hivemq.cloud`).
- **EMQX Cloud (serverless)** : idem.

Dans le broker, crée **deux identifiants** (menu « Access / Credentials ») :

| Usage | Nom | Droits |
|---|---|---|
| Boîtiers | `seadoo` | publier/souscrire sur `seadoo/#` |
| Tableau de bord | `dashboard` | publier/souscrire sur `seadoo/#` |

Note les ports :
- **8883** = MQTT sur TLS (pour les boîtiers).
- **8884 / wss** = MQTT sur WebSocket sécurisé (pour le tableau de bord dans le navigateur).

## 2. Régler les boîtiers

Dans `firmware/include/config.h` :

```c
#define TRANSPORT_MQTT  1
#define MQTT_HOST       "xxxx.s1.eu.hivemq.cloud"
#define MQTT_PORT       8883
#define MQTT_USE_TLS    1
#define MQTT_USER       "seadoo"
#define MQTT_PASS       "le-mot-de-passe-du-broker"
```

Puis reflasher. Le moniteur série doit afficher `MQTT : connecte` puis
`Envoi MQTT : OK`.

## 3. Relier le broker à Supabase (historique + alertes)

Dans le broker, crée une **règle / intégration webhook** :

- Déclencheur : messages sur le sujet `seadoo/+/telemetry`.
- Action : requête **POST** vers l'URL de la fonction `ingest`
  (`https://VOTRE-REF.supabase.co/functions/v1/ingest`), corps = le message tel quel.

Comme chaque point contient déjà `device_id` et `key`, la fonction `ingest`
authentifie, enregistre et calcule les alertes sans aucune modification.

## 4. Régler le tableau de bord

Dans `dashboard/config.js` :

```js
mqttUrl: "wss://xxxx.s1.eu.hivemq.cloud:8884/mqtt",
mqttUser: "dashboard",
mqttPass: "le-mot-de-passe-dashboard",
```

Laisser `mqttUrl` vide revient au mode Supabase (rafraîchissement toutes les
15 s). Avec, les jet-skis bougent en direct sur la carte, et tes réglages sont
appliqués immédiatement.

## Repères de données

Un point MQTT fait ~100 à 200 octets. À 1 s, une navigation de 4 h ≈ 2-3 Mo.
Un forfait M2M de 500 Mo tient donc largement plusieurs saisons. Surveille la
consommation réelle le premier mois sur le portail de l'opérateur.

## Si quelque chose cloche

| Symptôme (moniteur série) | Piste |
|---|---|
| `MQTT : echec connexion (etat -2)` | hôte/port injoignable : vérifier `MQTT_HOST`, le port 8883, le réseau |
| `etat 4` / `etat 5` | identifiants refusés : vérifier `MQTT_USER` / `MQTT_PASS` et les droits |
| Connecté mais rien sur la carte | le tableau de bord pointe le mauvais port WSS, ou le webhook n'est pas configuré |
| Carte en direct OK mais pas d'historique | webhook broker → `ingest` absent ou mal réglé |
