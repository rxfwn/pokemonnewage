# Multijoueur

Choix fait avec L'amie le 2026-11-02 (comparaison complète : [Multijoueur : choix de l'architecture](https://claude.ai/artifact/KAKJxSruASw2pW8KgK1fjS)).

**Un joueur crée un monde, jusqu'à 3 amis le rejoignent (4 au total). Chacun garde sa save sur son PC.** Quand personne n'héberge, chacun joue seul sur la même carte de Kanto avec sa propre save. Les amis voient la carte dresseur des autres même hors ligne (lecture seule : ni échange ni combat avec un joueur absent).

## Comment ça marche

| Quoi | Où | Détail |
| --- | --- | --- |
| Progression du joueur | `APokemonPlayerState` | Équipe, PC, sac, argent, badges, drapeaux, Pokédex. Modifiée seulement par le serveur, répliquée (SCRUM-68). |
| Save | `Saved/SaveGames/Partie.sav` sur le PC du joueur | Au lancement, le client l'envoie au serveur (`ServerLoadSave`). Il ne peut pas la réécrire tant qu'elle n'est pas revenue (`bSaveApplied`), sinon il effacerait sa partie avec un PlayerState vide. |
| Sauvegarde en quittant | `APokemonNewAgePlayerController::EndPlay` | Fermer le jeu, créer ou rejoindre un monde, perdre l'hôte : on sauvegarde (si la sauvegarde auto est activée). |
| Fenêtres de test | `Partie_PIE1.sav`, `Partie_PIE2.sav`... | Dans l'éditeur, chaque fenêtre en plus a son fichier : 4 joueurs sur un même PC ne s'écrasent pas. |
| Carte dresseur | `<dossier des amis>/<Nom>_<Numéro>.json` | Écrite à chaque sauvegarde : équipe (espèce, surnom, niveau), badges, Pokédex vus/capturés, temps de jeu, date. Environ 1 Ko. |
| Monde | La même map `L_Kanto` chez tous | Rien du monde n'est sauvegardé ni partagé : l'hôte ne garde que sa propre save. |
| Nombre de joueurs | `Config/DefaultGame.ini`, `[/Script/Engine.GameSession] MaxPlayers=4` | Avec `MaxWorldPlayers` dans le PlayerController (affichage). |

## Règle pour tous les systèmes à venir

Ce qui change dans le monde à cause de l'histoire **dépend du joueur**, jamais de l'hôte : objet ramassé, dresseur battu, PNJ qui bouge, ville débloquée. On le décide avec les drapeaux du joueur (`StoryFlags`, `Badges`), et on ne détruit jamais un acteur côté serveur pour tout le monde. Les seules choses communes sont l'heure et la météo (à donner par le serveur quand le cycle jour/nuit sera branché, et pour SCRUM-74).

## Barrière d'histoire (`AStoryGate`)

Une ville débloquée pour un joueur et pas pour un autre : placer un **Story Gate** (**Placer des acteurs**, chercher « Story Gate ») en travers du passage.

1. Étirer la boîte rouge sur toute la largeur du passage. La **flèche bleue pointe vers le côté fermé** (la ville à débloquer).
2. Dans **Détails** (*Details*) > **Story Gate** : **Required Flags** (drapeaux à avoir, par exemple `GotPokedex`) et/ou **Required Badges** (`BoulderBadge`).
3. **Conversation Id** : une scène de `DT_Dialogues` (Trigger = Script) jouée au joueur repoussé ; sinon le **Message** s'affiche en haut de l'écran.

Le serveur vérifie chaque joueur avec son PlayerState. Celui qui n'a pas les drapeaux est remis 1,5 m devant la boîte, du côté ouvert. Les autres passent librement.

## Jouer à plusieurs

- **Créer un monde** : Pokédex > **En ligne** > **Créer un monde**, ou `CreerMonde` dans la console (touche **²**). La map se relance en hébergeant, la page **En ligne** affiche **Ton adresse** à donner aux amis.
- **Rejoindre** : `Rejoindre 192.168.1.20` dans la console la première fois, puis **Rejoindre** dans la page **En ligne** (il retient la dernière adresse).
- **Quitter / fermer le monde** : page **En ligne**, ou `QuitterMonde`. Si l'hôte ferme, les autres repartent en solo avec leur save à jour.
- **À distance** (pas sur la même box) : installer [Tailscale](https://tailscale.com) sur chaque PC, avec le même compte ou un partage de machine. L'hôte donne son adresse Tailscale (`100.x.x.x`, visible aussi dans **Ton adresse**). Autoriser le jeu dans le pare-feu Windows quand il le demande (port UDP 7777).

## Serveur de saves

Un Raspberry Pi à la maison garde aussi la save de chaque joueur, et les cartes des amis : voir `docs/SERVEUR_SAVES.md`. Sans lui, tout marche quand même avec la save du PC et le dossier partagé ci-dessous.

## Cartes des amis (sans serveur de saves)

1. Créer un dossier partagé entre vous (OneDrive, Google Drive, Dropbox...), synchronisé sur chaque PC.
2. Dans le jeu, console : `DossierAmis "D:\OneDrive\PokemonAmis"` (`DossierAmis` sans chemin affiche le dossier choisi). Ta carte est écrite tout de suite.
3. Pokédex > **En ligne** > **Cartes des amis** : nom, badges, capturés, temps de jeu, date, et équipe de chaque ami.

## Tester dans Unreal

1. Fermer Unreal, compiler (« PokemonNewAgeEditor Win64 Development Build »), rouvrir.
2. À côté de **Jouer** (*Play*), menu **⋮** > **Nombre de joueurs** (*Number of Players*) : 2 à 4 ; **Mode réseau** (*Net Mode*) : **Jouer en tant que serveur d'écoute** (*Play As Listen Server*).
3. Lancer : chaque fenêtre est un joueur avec sa save (`Partie`, `Partie_PIE1`...). Donner un Pokémon à l'un (`DonnerPokemon Pikachu 5`) et vérifier que l'autre ne l'a pas.
4. Barrière : poser un Story Gate avec `Required Flags = TestGate`, faire `Drapeau TestGate` dans une seule fenêtre : celle-ci passe, l'autre est repoussée.

## Pas encore fait

- Échanges et combats entre joueurs **connectés** (SCRUM-69).
- Heure et météo données par le serveur (chaque PC a encore son propre cycle jour/nuit).
- Rejoindre sans adresse IP (Steam ou Epic Online Services), si un jour Tailscale ne suffit plus.

## Même version pour tout le monde

L'hôte refuse un joueur qui n'a pas la même version du jeu. Ouvrir le jeu par le launcher (`docs/MISES_A_JOUR.md`) le met à jour automatiquement depuis le Raspberry Pi.
