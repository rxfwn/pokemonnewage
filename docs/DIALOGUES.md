# Dialogues de la V1 (SCRUM-71)

Tous les textes du jeu, de Bourg Palette au badge de Pierre, sont dans deux Data Tables. Rien n'est écrit en dur dans les Blueprints : un PNJ ne connaît que son identifiant.

**Pour relire les textes : [`Data/Dialogues/SCRIPT_V1.md`](../Data/Dialogues/SCRIPT_V1.md)**, classés par lieu.

## En bref

| Data Table | Contenu | Lignes |
|---|---|---|
| `DT_Npcs` | PNJ, panneaux et objets : nom affiché, genre (personnage, panneau, objet, narration), lieu, dresseur éventuel de `DT_Trainers` | 83 |
| `DT_Dialogues` | Conversations : à qui elles appartiennent, quand elles se jouent, les répliques, les choix et les événements | 155 (321 répliques) |

Contenu : maman, Prof. Chen (hautes herbes, choix du starter, Colis, Pokédex, évaluation), Régis (labo, Route 22, victoire ou défaite), Nina et la Carte, PNJ et panneaux de Bourg Palette, Route 1 (Potion offerte), Jadielle (vieil homme et démonstration de capture, Boutique et Colis de Chen, arène fermée, école), Route 22 (garde de la Ligue), Route 2 et postes de la forêt, Forêt de Jade (5 Scouts), Argenta (musée payant, garçon qui mène à l'arène, Campeur Liam, Pierre, Badge Roche et CT39, Rondoudou), Centres Pokémon et Boutiques.

Textes originaux écrits dans l'esprit de Rouge Feu / Vert Feuille, pas recopiés. Noms français officiels (Régis, Nina, Pierre, Joëlle, Bourg Palette...). Le joueur ne parle pas, et rien ne suppose qu'il est une fille ou un garçon (« Te voilà ! » plutôt que « Tu es arrivé ! »).

## Comment ça marche

- **Parler à un PNJ** : parmi les conversations de ce PNJ en mode `Talk` dont les conditions sont remplies, on joue celle de plus haute `Priority`. Exemple : maman a 4 conversations (0 par défaut, 10 après le starter, 20 après le Pokédex, 30 après le badge).
- **Conditions** : des **drapeaux d'histoire** (`GotStarter`, `GotPokedex`...). `RequiredFlags` = il faut les avoir tous, `BlockedByFlags` = il ne faut en avoir aucun. `SetFlags` = donnés à la fin de la conversation. La liste commentée est en tête de `Tools/Data/dialogues_v1.py`.
- **Scènes** (`Trigger = Script`) : jouées seulement par leur nom, depuis un volume au sol (Chen qui arrête le joueur, Régis devant le labo, le vieil homme de Jadielle, le garçon d'Argenta), un choix Oui/Non, ou la fin d'un combat.
- **Événements** : une réplique peut donner un objet, un Pokémon ou un badge, prendre de l'argent (avec une conversation de repli si le joueur n'en a pas assez), soigner l'équipe, lancer un combat, ouvrir la boutique ou le PC, jouer une cinématique. Liste dans `EDialogueEvent` (`Source/PokemonNewAge/Dialogue/DialogueRows.h`).
- **Variables** : `{PlayerName}`, `{StarterName}`, `{RivalStarterName}`, `{SeenCount}`, `{CaughtCount}`, `{MoneyLost}`, remplacées avec le nœud **Format Dialogue Text**.

### Combats de dresseurs

- La conversation d'intro se termine par `StartTrainerBattle` avec le dresseur (`BugCatcherRick`, `Brock`). Pour le rival, on donne le groupe (`RivalLab`, `RivalRoute22`) : le combat prend la ligne `RivalLab_<starter du joueur>` de `DT_Trainers`.
- En fin de combat, **Get Trainer Outcome Conversation** donne `Trainer_<Dresseur>_PlayerWins` ou `_PlayerLoses`, affichée dans le combat. Sans réplique de défaite propre, c'est `Battle_PlayerBlackout` (retour au Centre Pokémon). Le `NextConversation` se joue ensuite, de retour sur la carte (Pierre remet le badge, Régis s'en va).
- Le système de combat pose le drapeau `Beaten_<Dresseur>` quand le joueur gagne. Au labo, le combat contre Régis continue l'histoire même si on perd, comme dans Rouge Feu.

## Identifiants

- PNJ : `Lieu_Role` (`PalletTown_Girl`, `PewterGym_Brock`), ou un nom seul pour les personnages qui bougent (`ProfOak`, `Rival`, `Mom`, `Daisy`) et les PNJ génériques (`PokemonCenterNurse`).
- Conversations : `Lieu_Pnj_Situation` (`PlayerHouse_Mom_AfterStarter`), `Trainer_<Dresseur>_PlayerWins` pour les fins de combat.
- Objets, badges, boutiques, cinématiques cités : `Potion`, `OaksParcel`, `Pokedex`, `PokeBall`, `TownMap`, `TM39`, `BoulderBadge`, `ViridianPokeMart`, `PewterPokeMart`, `OakLeadsToLab`, `CatchTutorial`, `LeadToPewterGym`, `JigglypuffSong`. Ils deviendront des lignes de leurs propres tables (objets, boutiques) ou des Level Sequences.
- On ne renomme jamais un identifiant ni un drapeau : les sauvegardes s'y réfèrent.

## Modifier un dialogue

1. Modifier le texte dans `Tools/Data/dialogues_v1.py` (règles d'écriture en tête du fichier).
2. Lancer `python Tools/Data/build_dialogues.py`. Il vérifie tout (PNJ, drapeaux, conversations suivantes, dresseurs, espèces, variables, longueur des répliques) et s'arrête en cas d'erreur. Il met les espaces insécables avant `! ? : ;`.
3. Il réécrit `Data/Dialogues/DT_*.json` et `SCRIPT_V1.md`. Puis réimporter dans Unreal.

## Mettre les dialogues dans Unreal

Même chemin que les données Pokémon (`docs/DONNEES_POKEMON.md`) : fermer Unreal, compiler (tâche « PokemonNewAgeEditor Win64 Development Build »), rouvrir, puis **Outils > Exécuter un script Python** (*Tools > Execute Python Script*) avec `Tools/Data/import_data_tables.py`. Le **Journal de sortie** (*Output Log*) doit afficher `DT_Npcs : 83 lignes importées` et `DT_Dialogues : 155 lignes importées`. Les tables arrivent dans `Content/World/Dialogues/`.

La boîte de dialogue elle-même est décrite plus bas (SCRUM-26).

## Multijoueur

Les tables sont identiques sur toutes les machines. Les drapeaux d'histoire appartiennent à **chaque joueur** (une `TArray<FName>` dans son `PlayerState`, sauvegardée avec lui) : deux joueurs qui parlent au même PNJ voient chacun la conversation de leur propre avancement. Les fonctions de `UDialogueLibrary` ne gardent aucun état, on leur passe les drapeaux du joueur qui parle. Les événements (objets, combats, soins) sont appliqués par le serveur au seul joueur concerné, et l'affichage reste local à son écran. Les scènes avec Régis ou Chen sont jouées pour ce joueur uniquement.

## Boîte de dialogue en jeu (SCRUM-26)

### Ce qui existe

| Pièce | Fichier | Rôle |
|---|---|---|
| `UDialogueComponent` | `Source/PokemonNewAge/Dialogue/DialogueComponent.*` | Posé d'office sur le PlayerController. Choisit la conversation, applique les événements et les drapeaux (serveur), affiche les répliques (client). |
| `UDialogueWidget` | `Source/PokemonNewAge/UI/DialogueWidget.*` | La boîte en bas de l'écran : cartouche du nom, texte lettre par lettre, flèche ▼, réponses Oui / Non. Construite en C++, pas besoin de Widget Blueprint. |
| `ADialogueNpc` | `Source/PokemonNewAge/Dialogue/DialogueNpc.*` | PNJ, panneau ou objet à poser dans la map. On choisit son **Npc Id** dans une liste (lue dans `DT_Npcs`). Cylindre de blockout tant qu'il n'a pas de modèle. |
| `ADialogueTriggerVolume` | `Source/PokemonNewAge/Dialogue/DialogueTriggerVolume.*` | Boîte au sol qui lance une scène (`Trigger = Script`) quand le joueur entre, si ses drapeaux le permettent. **Conversation Id** se choisit aussi dans une liste. |

### Commandes

- **E** (ou **X** manette) devant un PNJ : parler. Le PNJ se tourne vers le joueur (décocher **Face Player** pour un panneau ou un objet).
- **E**, **Espace**, **Entrée**, clic gauche, **A** ou **X** manette : afficher tout le texte d'un coup, puis passer à la suite.
- **Z / S**, flèches, croix de la manette : changer de réponse. **Échap** ou **B** : choisit la dernière réponse (« Non »).
- Pendant une conversation le joueur ne bouge plus, mais le monde continue (pas de pause, pour le multijoueur).

### Ce que fait chaque événement

| Événement | Quand | Effet |
|---|---|---|
| `GiveItem`, `TakeItem`, `TakeMoney` | à l'affichage de la réplique | Sac ou argent du joueur. Pas assez d'argent : on passe à `FailConversation` (musée d'Argenta). |
| `GivePokemon` | à l'affichage | Crée le Pokémon (niveau `EventAmount`, Poké Ball, lieu de rencontre), l'ajoute à l'équipe ou au PC, et au Pokédex. |
| `GiveBadge`, `HealParty` | à l'affichage | Badge ajouté, équipe soignée. |
| `StartTrainerBattle` | quand le joueur a lu la réplique | La boîte se ferme, le combat commence. À la fin, la réplique `Trainer_<Dresseur>_PlayerWins` / `_PlayerLoses` s'affiche (1,5 s après, le temps que l'écran de combat se ferme), puis son `NextConversation`. Équipe K.O. contre un sauvage : `Battle_PlayerBlackout`. |
| `OpenShop`, `OpenPC`, `PlayCutscene`, `OfferNickname`, `ReturnToLastHeal` | | Pas encore codés : la conversation continue, et l'événement **On Unhandled Event** du DialogueComponent est envoyé pour qu'un Blueprint puisse s'y brancher. `ReturnToLastHeal` soigne déjà l'équipe (le retour au Centre viendra avec les Centres Pokémon). |

Variables remplacées : `{PlayerName}` (nom du joueur dans la partie, en solo c'est le nom du PC tant que le choix du nom n'existe pas), `{StarterName}` (surnom ou espèce du starter), `{RivalStarterName}` (d'après `RivalLab_*` de `DT_Trainers`, déjà juste dans la scène du choix), `{SeenCount}`, `{CaughtCount}`, `{MoneyLost}` (argent perdu au dernier combat).

### Voix (pour SCRUM-72)

Tout est prêt pour brancher des voix plus tard, sans toucher au code :

- Une voix par réplique, importée dans `Content/Audio/Voices/` et nommée **`VO_<Conversation>_<NN>`**, NN = numéro de la réplique à partir de 01. Exemple : la 2e réplique de `PalletTown_Girl_Talk` → `VO_PalletTown_Girl_Talk_02`. Les identifiants sont dans `SCRIPT_V1.md`.
- Si le fichier existe il est joué quand la réplique s'affiche, et coupé quand on passe à la suivante. Sinon la réplique reste muette : on peut doubler petit à petit.
- Sans voix, un petit son de frappe peut accompagner les lettres : régler **Typing Sound** sur le DialogueComponent (et **Advance Sound** pour le son quand on continue).
- Le Blueprint peut aussi écouter **On Line Shown** (conversation, numéro, réplique, texte final) pour des sous-titres, une animation de bouche, etc.
- Le dossier se règle dans **Voice Folder**.

### Réglages (Blueprint du PlayerController > composant DialogueComponent > Détails)

- **Letters Per Second** : vitesse d'écriture (45 ; 0 = tout d'un coup).
- **Dialogue Widget Class** : pour habiller la boîte avec un `WBP_Dialogue` enfant de `DialogueWidget` (l'événement **On Line Started** sert à l'animer).
- **Battle Outcome Delay** : attente entre la fin d'un combat et la réplique du dresseur.
- **Dialogues Table** / **Npcs Table** : `/Game/World/Dialogues/DT_Dialogues` et `DT_Npcs` par défaut.

### Tester dans Unreal

Le code n'a pas pu être compilé dans le cloud : c'est le premier test.

1. Fermer Unreal. Dans VS Code, **Terminal > Exécuter la tâche** (*Run Task*) > « PokemonNewAgeEditor Win64 Development Build ». Rouvrir le projet.
2. Si ce n'est pas déjà fait : **Outils > Exécuter un script Python** (*Tools > Execute Python Script*) > `Tools/Data/import_data_tables.py` (données Pokémon et dialogues).
3. Ouvrir `L_Kanto`, lancer **Jouer** (*Play*), ouvrir la console avec **²** et taper :
   - `Parler PalletTown_Girl` : la boîte s'ouvre avec son nom, deux répliques. **E** pour avancer.
   - `Parler PalletTown_Sign` : panneau, pas de cartouche de nom.
   - `Drapeau LabIntroDone` puis `Parler OaksLab_PokeBall_Bulbasaur` : question Oui / Non. Choisir **Oui** : Bulbizarre reçu, « Régis reçoit Salamèche », puis `Equipe` et `Drapeaux` pour vérifier (Bulbizarre N.5, `GotStarter`, `Starter_Bulbasaur`).
   - `Parler Mom` : la conversation change avec les drapeaux (après le starter, maman soigne l'équipe).
   - `Dialogue OaksLab_Rival_Challenge` : combat contre Régis à la dernière réplique, puis sa réplique de fin.
   - `Parler PewterMuseum_Receptionist` puis **Oui** : 50 ₽ retirés (le joueur commence avec 3000 ₽). Le repli « pas assez d'argent » ne se teste qu'avec moins de 50 ₽.
4. Dans la map : **Placer des acteurs** (*Place Actors*) > chercher **Dialogue Npc**, le poser, choisir **Npc Id** dans la liste du panneau **Détails**, jouer et appuyer sur **E** devant lui. Même chose avec **Dialogue Trigger Volume** et une scène (`PalletTown_ProfOak_StopGrass`, par exemple).

À regarder pendant le test : le texte qui déborde de la boîte, la boîte qui garde le clavier (sinon cliquer dedans), et, une fois l'écran de combat du Pokédex (PR #19) dans `main`, que la réplique de fin de combat arrive bien après sa fermeture (sinon augmenter **Battle Outcome Delay**).
