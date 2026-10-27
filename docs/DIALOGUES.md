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

La boîte de dialogue elle-même (widget, touche d'action sur un PNJ via `Interactable`) est le ticket SCRUM-26.

## Multijoueur

Les tables sont identiques sur toutes les machines. Les drapeaux d'histoire appartiennent à **chaque joueur** (une `TArray<FName>` dans son `PlayerState`, sauvegardée avec lui) : deux joueurs qui parlent au même PNJ voient chacun la conversation de leur propre avancement. Les fonctions de `UDialogueLibrary` ne gardent aucun état, on leur passe les drapeaux du joueur qui parle. Les événements (objets, combats, soins) sont appliqués par le serveur au seul joueur concerné, et l'affichage reste local à son écran. Les scènes avec Régis ou Chen sont jouées pour ce joueur uniquement.
