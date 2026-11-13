# 2. Écouter et décoder le bus CAN

BRP ne publie pas le contenu de son bus CAN. On le relève donc soi-même, en
écoute passive, sur chaque jet-ski (le GTX 2023 et le GTX 2026 peuvent différer,
notamment si le tableau de bord a changé).

## 2.1 Trouver CAN-H, CAN-L, masse et 12 V

La prise diagnostic est un connecteur Deutsch 6 voies, en général près du
boîtier à fusibles / de l'ECM (voir le manuel d'atelier du modèle).

Les brochages qui circulent sur les forums **se contredisent**. Ne pas s'y
fier : mesurer.

| Mesure au multimètre | Résultat attendu |
|---|---|
| Continuité entre une broche et le (−) batterie | c'est la **masse** |
| Tension broche/masse ≈ 12 V | c'est le **+12 V** (noter s'il est permanent ou seulement jet-ski réveillé) |
| Jet-ski réveillé, tension broche/masse ≈ 2,5 à 3,5 V | **CAN-H** |
| Jet-ski réveillé, tension broche/masse ≈ 1,5 à 2,5 V | **CAN-L** |
| Batterie débranchée, résistance entre CAN-H et CAN-L | ≈ 60 Ω (confirme la paire) |

Noter le résultat ici pour chaque jet-ski :

| Broche | GTX 2023 | GTX 2026 |
|---|---|---|
| 1 / A | | |
| 2 / B | | |
| 3 / C | | |
| 4 / D | | |
| 5 / E | | |
| 6 / F | | |

## 2.2 Flasher le boîtier en mode sniffer

```bash
cd firmware
cp include/config.example.h include/config.h     # laisser SNIFFER_MODE à 1
pip install platformio
pio run -t upload
pio device monitor
```

Insérer une carte microSD : chaque mise sous tension crée un fichier
`can_000.csv`, `can_001.csv`…

Le moniteur affiche toutes les 5 s une ligne `# trames=… erreurs_bus=…`.

| Ce qu'on voit | Signification |
|---|---|
| `trames` augmente | tout va bien |
| 0 trame, `erreurs_bus` augmente | mauvais débit : mettre `CAN_BITRATE_KBPS` à 250 ; ou CAN-H/CAN-L inversés |
| 0 trame, 0 erreur | bus endormi (réveiller le jet-ski) ou câblage coupé |

## 2.3 Faire les relevés

Un fichier par situation, en notant ce qu'on fait et à quel moment. Couper et
remettre l'alimentation du boîtier entre deux relevés pour changer de fichier.

| Relevé | Manipulation | Ce qu'on cherche |
|---|---|---|
| A | jet-ski réveillé, moteur arrêté, 1 min | la base |
| B | moteur au ralenti, 1 min (**sur l'eau ou avec refroidissement branché**) | régime |
| C | paliers de régime : ~2000, ~3000, ~4000 tr/min, 20 s chacun | régime |
| D | navigation avec un téléphone GPS : paliers 10, 30, 50 km/h | vitesse |
| E | avant et après un plein | niveau carburant |
| F | noter les heures moteur affichées au compteur | heures moteur |

## 2.4 Analyser

```bash
python3 tools/can_analyze.py resume can_000.csv            # vue d'ensemble
python3 tools/can_analyze.py compare ralenti.csv 4000.csv  # octets qui changent de plage
python3 tools/can_analyze.py suivre can_002.csv 0x102      # un identifiant dans le temps
```

Méthode :

1. `compare` entre A (moteur arrêté) et C (moteur à régime) donne les octets
   candidats pour le régime.
2. `suivre` sur l'identifiant candidat : chercher une colonne 16 bits qui vaut
   directement le régime (ou régime × 4, × 8… un facteur simple). Comparer avec
   le compte-tours.
3. Même démarche pour la vitesse (relevé D, comparer au GPS du téléphone), le
   carburant (E), les heures (F : chercher la valeur affichée, parfois en
   dixièmes d'heure ou en minutes).

## 2.5 Reporter dans le firmware

Remplir le profil du modèle (`firmware/include/profiles/<modèle>.h`, ex. `profiles/gtx-2023.h`). Exemple fictif : régime sur l'ID
`0x102`, octets 2-3, gros-boutiste, valeur brute = tr/min × 4 :

```cpp
{"rpm", 0x102, 2, 2, true, 0.25f, 0.0f},
```

Tableau de résultats à tenir :

| Signal | Jet-ski | ID | Octets | Ordre | Échelle | Vérifié avec |
|---|---|---|---|---|---|---|
| rpm | GTX 2023 | | | | | compte-tours |
| speed_kmh | GTX 2023 | | | | | GPS téléphone |
| fuel_pct | GTX 2023 | | | | | jauge |
| engine_hours | GTX 2023 | | | | | compteur |
| rpm | GTX 2026 | | | | | |
| … | | | | | | |

Si les deux jet-skis ont des identifiants différents, chaque boîtier est flashé
avec son propre profil (`profiles/<modèle>.h`).

Les fichiers de relevé peuvent être déposés dans ce dépôt (dossier `releves/`)
pour être analysés ensemble.
