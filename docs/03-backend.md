# 3. Serveur et tableau de bord

Le serveur tient dans un projet Supabase (l'offre gratuite suffit pour deux
jet-skis). Trois éléments : la base, la fonction `ingest`, le tableau de bord.

## 3.1 Créer le projet et la base

1. Créer un projet sur supabase.com (région Europe).
2. SQL Editor → coller et exécuter `backend/supabase/migrations/001_init.sql`.
3. Authentication → Users → **Add user** : votre e-mail + mot de passe. C'est
   le compte du tableau de bord. Désactiver ensuite les inscriptions publiques
   (Authentication → Sign In / Providers → décocher « Allow new users to sign up »).

## 3.2 Enregistrer les boîtiers

Choisir une clé longue et aléatoire par boîtier (`openssl rand -hex 24`), puis
dans le SQL Editor :

```sql
insert into public.devices (id, name, key_hash, vehicle_make, vehicle_model, vehicle_year, firmware_profile) values
  ('gtx-2023', 'GTX 2023', encode(digest('CLE-DU-BOITIER-1', 'sha256'), 'hex'), 'Sea-Doo', 'GTX', '2023', 'profiles/gtx-2023.h'),
  ('gtx-2026', 'GTX 2026', encode(digest('CLE-DU-BOITIER-2', 'sha256'), 'hex'), 'Sea-Doo', 'GTX', '2026', 'profiles/gtx-2026.h');
```

Depuis le tableau de bord, « Ajouter un module » fait la même chose en demandant
le véhicule (type, marque, modèle, année) : il en déduit le **profil firmware**
à flasher sur ce module.

Seule l'empreinte de la clé est stockée. La clé en clair va dans le `config.h`
du boîtier correspondant (`DEVICE_ID` et `DEVICE_KEY`).

## 3.3 Déployer la fonction `ingest`

```bash
npm install -g supabase
supabase login
cd backend
supabase link --project-ref VOTRE-REF
supabase functions deploy ingest --no-verify-jwt
```

`--no-verify-jwt` est voulu : le boîtier s'identifie par sa clé, vérifiée dans
la fonction. L'adresse obtenue
(`https://VOTRE-REF.supabase.co/functions/v1/ingest`) va dans `INGEST_URL`.

Test sans boîtier :

```bash
curl -X POST https://VOTRE-REF.supabase.co/functions/v1/ingest \
  -H 'content-type: application/json' \
  -d '{"device_id":"gtx-2023","key":"CLE-DU-BOITIER-1","fix":true,"lat":43.55,"lon":7.02,"can_alive":false}'
# réponse attendue : {"start_inhibit":false,"active_interval_s":30,"parked_interval_s":1800}
```

## 3.4 Tableau de bord

```bash
cd dashboard
cp config.example.js config.js      # y mettre l'URL du projet et la clé "anon"
python3 -m http.server 8080         # puis ouvrir http://localhost:8080
```

Pour l'avoir sur le téléphone : héberger le dossier `dashboard/` sur n'importe
quel hébergement statique (Netlify, Cloudflare Pages, GitHub Pages…).

## 3.5 Passer les boîtiers en mode suivi

Dans `firmware/include/config.h` : `SNIFFER_MODE 0`, remplir `DEVICE_ID`,
`DEVICE_KEY`, `APN`, `INGEST_URL`, puis `pio run -t upload`. Le moniteur série
doit afficher `Envoi : OK (HTTP 200)`.

## Fonctionnement des consignes

Le boîtier ne reçoit rien « en direct » : à chaque envoi, la réponse du serveur
contient les consignes (anti-démarrage, rythmes). Une consigne est donc
appliquée au prochain contact : 30 s si le jet-ski est allumé, jusqu'à 30 min
s'il dort (régler `parked_interval_s` dans la table `devices` pour raccourcir,
au prix de la consommation).

## Améliorations possibles ensuite

- Notification des alertes sur le téléphone (e-mail, Telegram ou push) depuis
  la table `alerts`.
- Purge automatique de la télémétrie de plus de 12 mois.
- Liaison MQTT permanente pour des consignes instantanées et moins de données.
