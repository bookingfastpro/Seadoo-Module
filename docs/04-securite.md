# 4. Sécurité, anti-démarrage et limites

## Règles de conception

- **Lecture seule sur le bus CAN.** Le boîtier n'émet jamais. Il ne peut ni
  commander le moteur ni perturber l'ECM, l'iBR ou le tableau de bord.
- **Jamais de coupure moteur en marche.** Couper un moteur sur l'eau retire la
  direction (un jet-ski ne se dirige que sous poussée) : c'est dangereux pour
  le pilote et les autres. Le firmware n'active le blocage que moteur arrêté ;
  le déblocage est toujours immédiat.
- **Panne du boîtier = jet-ski normal.** Le relais est câblé de sorte qu'un
  boîtier éteint, débranché ou en panne laisse le démarrage autorisé.

## Anti-démarrage (optionnel)

Principe : un relais dont le contact **normalement fermé (NF)** est mis en série
avec le circuit de **commande** du démarrage (signal du bouton Start/Stop ou
commande du solénoïde de démarreur — jamais le câble de puissance du démarreur).

- Bobine au repos → contact fermé → démarrage normal.
- Bobine alimentée par le boîtier → contact ouvert → le bouton Start n'a plus d'effet.

Le fil exact à interrompre dépend du modèle et de l'année : l'identifier sur le
schéma électrique du manuel d'atelier BRP du GTX concerné. Mettre `RELAY_PIN`
dans `config.h` sur le GPIO qui pilote le module relais.

À savoir :

- Relais bloqué = bobine alimentée en permanence (30 à 70 mA) : sur une longue
  période, cela vide la batterie. Pour un blocage longue durée, remplacer par
  un relais **bistable** (à faire évoluer dans le firmware).
- Intervenir sur le faisceau peut remettre en cause la **garantie BRP**, surtout
  sur le GTX 2026. La lecture du CAN par la prise diagnostic, elle, se branche
  et se débranche sans rien modifier.
- La clé D.E.S.S. d'origine reste la première protection ; ce relais s'y ajoute.

## Pourquoi pas de bridage de vitesse à distance

Brider la vitesse impose de s'intercaler sur le signal d'accélérateur
électronique ou d'émettre sur le bus CAN. Une erreur à cet endroit peut
provoquer une accélération involontaire ou une perte de poussée. Ce n'est pas
un risque raisonnable pour un montage personnel.

Alternative d'origine : les GTX disposent des **clés d'apprentissage** et des
modes de limitation de vitesse intégrés (réglables au tableau de bord ou chez
le concessionnaire). C'est la bonne manière de brider pour un débutant.

## Sécurité informatique

- Les clés des boîtiers ne sont stockées qu'en empreinte SHA-256 côté serveur.
- `config.h` et `dashboard/config.js` sont exclus de git.
- Le tableau de bord exige une connexion ; les inscriptions publiques doivent
  être désactivées (docs/03, étape 3.1).
- Boîtier volé : supprimer sa ligne dans `devices`, sa clé devient inutilisable.
