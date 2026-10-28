# Capture des Pokémon sauvages (épopée SCRUM-9)

La formule de capture (SCRUM-41) est dans le combat ([COMBAT.md](COMBAT.md)). Ce document couvre la suite :
les rencontres dans les hautes herbes (SCRUM-39), le lancer de Poké Ball en 3D (SCRUM-40), l'envoi au PC
et le surnom (SCRUM-42), et l'entrée au Pokédex.

## Ce qui est géré

- **Hautes herbes** : un acteur invisible `TallGrassZone` posé sur l'herbe. À chaque pas (1,5 m) d'un joueur dedans,
  une chance de rencontre comme en 3G : taux × 16 / 2880, soit environ 12 % par pas sur les routes (taux 21) et
  8 % dans la Forêt de Jade (taux 14). L'espèce et le niveau sont tirés dans la table `DT_Encounters` de la zone
  (`Route1_Walk`…). Pas de rencontre en sautant, ni pendant les 4 pas qui suivent un combat (pour sortir des herbes),
  ni si l'équipe est K.O. En entrant dans l'herbe, le lieu du joueur devient celui de la table (lieu de capture noté
  sur le Pokémon).
- **Lancer de Poké Ball** : la Ball part de la main du joueur en cloche, s'ouvre dans un éclair rouge, tombe,
  secoue 0 à 3 fois, puis clic et éclat doré (capturé) ou s'ouvre dans un éclair blanc (le Pokémon se libère).
  Couleur selon la Ball : Poké Ball rouge, Super Ball bleue, Hyper Ball jaune, Master Ball violette. Le résultat
  vient du serveur ; le lancer n'est qu'un spectacle joué chez le joueur.
- **Après la capture** : le Pokémon va dans l'équipe, ou au PC de Léo si elle est pleine (soigné, comme dans les jeux).
  Première capture de l'espèce : message « Les données de … ont été ajoutées au Pokédex ». Puis la question du surnom
  (12 caractères au plus).
- **PC de Léo** : 12 boîtes de 30 (360 places). Déposer, retirer, échanger, relâcher. On ne peut pas déposer son
  dernier Pokémon en forme, ni toucher au PC pendant un combat. Équipe et PC pleins : les Poké Balls sont refusées
  en combat (« Plus de place dans l'équipe ni dans le PC ! »), la Ball n'est pas perdue.

## Fichiers

| Fichier | Rôle |
|---------|------|
| `Source/PokemonNewAge/Capture/PokemonStorage.h` | Règles de l'équipe et du PC, surnoms, messages après capture (sans acteur, testées hors Unreal). |
| `Source/PokemonNewAge/Capture/CaptureComponent.h` | Sur le PlayerController : range le Pokémon capturé (serveur), prévient le joueur, demandes du PC, lancer de Ball. |
| `Source/PokemonNewAge/Capture/PokeBallThrow.h` | L'acteur du lancer (trajectoire, secousses, éclairs, sons). |
| `Source/PokemonNewAge/World/TallGrassZone.h` | La zone de hautes herbes et ses rencontres. |
| `Tools/Capture/place_grass_zones.py` | Pose une zone sur chacun des 110 carrés d'herbe de Kanto, avec la table de la route la plus proche. |
| `Tools/Capture/make_pokeball.py` | Modèle de la Ball (Blender ou `pip install bpy`) : `Generated/SM_PokeBall.fbx`. |
| `Tools/Capture/make_ball_sounds.py` | Sons provisoires du lancer (synthétisés). |
| `Tools/Capture/import_pokeball_unreal.py` | Importe la Ball, son matériau `M_PokeBall` et les sons dans `Content/Capture`. |

## Installer et tester dans Unreal

1. **Fermer Unreal**, compiler (« PokemonNewAgeEditor Win64 Development Build »), rouvrir.
2. **La Ball** : dézipper `capture/PokeBall_Generated.zip` (fichiers du projet) dans `Tools/Capture/Generated/`,
   puis **Outils > Exécuter un script Python** (*Tools > Execute Python Script*) > `Tools/Capture/import_pokeball_unreal.py`.
   Sans ce fichier, le jeu lance une simple sphère rouge : tout le reste marche quand même.
3. **Les herbes** : ouvrir `L_Kanto`, puis **Outils > Exécuter un script Python** > `Tools/Capture/place_grass_zones.py`.
   Il pose 110 zones dans le dossier `Kanto_Gameplay/Rencontres` de l'Organiseur (*Outliner*) et n'efface rien d'autre :
   le décor fait à la main ne bouge pas. Relançable.
   Pour ajouter des herbes ailleurs (Forêt de Jade, herbes peintes à la main) : **Placer des acteurs** (*Place Actors*),
   chercher « Tall Grass Zone », la glisser sur l'herbe, régler sa taille (**Étendue de la boîte**, *Box Extent*) et
   `Encounter Table` (`ViridianForest_Walk`, `Route2_SouthTowardsViridianCity_Walk`…) dans le panneau **Détails**.
   Les zones se voient en vert dans l'éditeur, pas en jeu.
4. Lancer le jeu (PIE) dans `L_Kanto`, ouvrir la console (touche **²**) :
   - `DonnerPokemon Bulbasaur 10`, `DonnerObjet PokeBall 20`, `DonnerObjet GreatBall 5`
   - marcher dans les hautes herbes de la Route 1 : un combat doit démarrer au bout de quelques pas ;
   - `Objet PokeBall` (ou `Objet GreatBall`) : la Ball part devant le joueur, secoue, puis clic ou ouverture ;
   - après une capture : le message du Pokédex (première fois), puis `Surnom Bulbi` pour lui donner un nom
     (sans cette commande, il garde le nom de l'espèce) ;
   - `Equipe`, `VoirPC`, `Deposer 2`, `Retirer 1`, `Echanger 2 1`, `Relacher 1` ;
   - `Rencontres 0` pour traverser les herbes tranquille, `Rencontres 1` pour les remettre.
   - Pour tester l'envoi au PC : capturer avec 6 Pokémon dans l'équipe (`DonnerPokemon` plusieurs fois).
5. Tests automatiques : **Outils > Session Frontend**, onglet **Automation**, cocher `PokemonNewAge.Capture`
   (et `PokemonNewAge.Battle`), **Lancer les tests**.

Sans Unreal (dans le cloud) : `./Tools/BattleSim/run.sh` lance aussi les tests de capture.

## Brancher le reste (UI)

- **Écran de combat** : après `On Battle Ended` avec `Outcome = Caught`, `CaptureComponent > On Pokemon Caught`
  donne le Pokémon, où il est rangé et s'il est nouveau au Pokédex. Afficher `Get Caught Messages` (fiche du Pokédex
  d'abord si `bNewInPokedex`), proposer le clavier du surnom, puis `Set Caught Nickname` (vide = garder le nom).
  Mettre `bPrintToScreen` à faux quand l'UI existe.
- **Écran du PC** (Centre Pokémon) : `Deposit Pokemon`, `Withdraw Pokemon`, `Swap With Storage`, `Release Pokemon`
  (demander confirmation avant de relâcher). Le PC est `StoragePC` du PlayerState ; boîte = index / 30.
  Refus : `On Storage Refused` + `Get Error Text`.
- **Pokémon sauvage en 3D** : quand un acteur montre le Pokémon sauvage, appeler `Set Ball Target` avec sa position
  pour que la Ball tombe à ses pieds (sinon elle tombe à 4,5 m devant le joueur), et le cacher pendant que la Ball
  est fermée.
- **Changer la Ball ou ses sons** : créer un Blueprint enfant de `PokeBallThrow`, changer `Ball Mesh`, les sons, les
  couleurs ou les durées, puis le mettre dans `Poke Ball Throw Class` du `CaptureComponent` (Blueprint du
  PlayerController). Si le bouton ne regarde pas le joueur, régler `Button Yaw Offset`.
- **Pas encore fait** : Repousse (objet), herbes qui bougent au passage, cris des Pokémon, séquence « Pokédex » animée
  après la capture (le menu Pokédex 3D affiche déjà les espèces capturées).
