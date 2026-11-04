# Pokémon qui suit le joueur

Ticket : SCRUM-78 (épopée Exploration). Comme dans HeartGold/SoulSilver ou Let's Go : le **premier Pokémon
de l'équipe** sort de sa Ball et accompagne le joueur hors combat.

## Ce qu'il fait

| Façon de suivre | Qui | Comportement |
|---|---|---|
| **Marche** (`Walk`) | la plupart (Bulbizarre, Pikachu, Carapuce, Onix...) | marche **dans les pas** du joueur, environ 1 m derrière lui (il suit le chemin du joueur, il ne coupe pas à travers les murs) ; court pour rattraper son retard |
| **Vole** (`Fly`) | Roucool, Piafabec, Papilusion, Dardargnan, Nosferapti, Dracaufeu, Dracolosse, les oiseaux légendaires, Mew... | vole **à côté** du joueur (à sa droite, un peu en arrière), à hauteur d'épaule, en ondulant |
| **Flotte** (`Float`) | Fantominus, Spectrum, Smogo, Smogogo, Magnéti, Magnéton, Porygon, Draco | flotte à 40 cm du sol, dans les pas du joueur |
| **Ne suit pas** (`None`) | les Pokémon qui vivent dans l'eau : Magicarpe, Léviator, Poissirène, Tentacool, Kokiyas, Hypotrempe, Stari, Lokhlass, Otaria, Amonita, Minidraco... | reste dans sa Ball |

- Il **rentre dans sa Ball** pendant les combats, s'il est K.O., ou si l'équipe est vide.
- Il **change tout seul** quand on change l'ordre de l'équipe (Tab > Équipe > Déplacer), qu'on dépose le premier au PC
  ou qu'il évolue.
- Il est **aussi loin que 25 m** du joueur, ou **coincé** 2 secondes (mur, falaise, eau) : il réapparaît derrière le joueur.
- Il ne bloque **ni le joueur, ni la caméra, ni les autres joueurs**.
- **Touche d'action (E)** quand on est tourné vers lui : il se tourne vers nous et montre sa joie
  (animation `Happy` si son modèle en a une, sinon un petit saut). Une porte ou un PNJ à portée passe avant lui.
- Il prend le modèle 3D importé (`/Game/Pokemon/Models/<Espèce>/SK_<Espèce>`, voir [MODELES_POKEMON.md](MODELES_POKEMON.md))
  et ses animations `Idle`, `Walk`, `Run`, `Happy`. Sans modèle : une sphère de 50 cm à sa place.
- Taille réelle du Pokédex (Onix fait 8,8 m !). Pour le réduire, voir « Régler » plus bas.

**Multijoueur** : chaque joueur du monde a son Pokémon qui le suit, et tout le monde voit ceux des autres.
C'est le serveur (celui qui a créé le monde) qui les déplace ; on peut caresser le Pokémon d'un ami.

## Installer (une fois)

1. **Fermer Unreal**, puis compiler dans VS Code : tâche **PokemonNewAgeEditor Win64 Development Build**.
   (Nouvelle colonne dans `DT_Species` et nouveaux fichiers C++ : Live Coding ne suffit pas.)
2. Ouvrir Unreal, puis **Outils > Exécuter un script Python...** (Tools > Execute Python Script) →
   `Tools/Data/import_data_tables.py`. Il remet à jour `DT_Species` avec la nouvelle colonne **FollowMode**.
   Sans cette étape, tous les Pokémon marchent (même Magicarpe).
3. Rien à placer dans la map : le composant est déjà sur le PlayerController (`FollowerComponent`).

## Tester

Lancer le jeu dans `L_Kanto`, console **²** :

| Commande | Ce qu'on doit voir |
|---|---|
| `DonnerPokemon Pikachu 10` | Pikachu sort et marche derrière le joueur |
| `DonnerPokemon Pidgey 5` puis Tab > Équipe > Déplacer Roucool en premier | Roucool vole à côté |
| `DonnerPokemon Gastly 20` en premier | Fantominus flotte au ras du sol |
| `DonnerPokemon Magikarp 5` en premier | rien ne sort |
| `CombatSauvage Rattata 3` | il rentre pendant le combat et ressort après |
| `Suiveur` | le range / le ressort (`Suiveur 0` : rangé, `Suiveur 1` : sorti) |

À vérifier en particulier : courir (Maj) puis s'arrêter, monter une pente, passer une porte de maison,
sauter d'une falaise (il doit réapparaître derrière), et l'animation de vol de Roucool
(les modèles Let's Go n'ont pas toujours une animation de vol pour l'attente : à me dire si ça fait bizarre).

## Régler

Tous les réglages sont sur l'acteur `APokemonFollower` (catégorie **Follower** dans Détails). Pour les changer :
créer un Blueprint enfant de `PokemonFollower` (clic droit > Classe Blueprint > chercher PokemonFollower),
puis dans le Blueprint du PlayerController : composant **FollowerComponent > Follower Class** = ce Blueprint.

| Réglage | Par défaut | Rôle |
|---|---|---|
| Follow Distance | 110 cm | écart derrière le joueur (en plus de la taille du Pokémon) |
| Walk Speed / Run Speed | 270 / 620 cm/s | vitesse normale / pour rattraper le joueur |
| Fly Height | 150 cm | hauteur du milieu du corps des Pokémon volants |
| Fly Side Offset | 120 cm | écart sur le côté droit des volants |
| Float Height | 40 cm | hauteur des Pokémon qui flottent |
| Teleport Distance | 25 m | au-delà, il réapparaît derrière le joueur |
| Max Display Height | 0 (taille réelle) | ex. 250 cm : Onix est réduit à 2,5 m quand il suit |

**Changer la façon de suivre d'une espèce** : colonne `FollowMode` de `DT_Species`
(Walk, Fly, Float, None). Pour que ça reste après une régénération des données, la modifier aussi dans
`Tools/Data/build_pokemon_data.py` (`FOLLOW_OVERRIDES`).

## Code

- `Source/PokemonNewAge/Follower/PokemonFollower.h/.cpp` : l'acteur (Character sans contrôleur, déplacé par le serveur).
  Il garde les 150 dernières positions du joueur (une tous les 20 cm) et vise le point situé à la bonne distance
  sur ce chemin. Les volants visent un point à côté du joueur et restent au moins 30 cm au-dessus du sol.
- `Source/PokemonNewAge/Follower/PokemonFollowerComponent.h/.cpp` : sur le PlayerController ; 4 fois par seconde,
  le serveur regarde le premier de l'équipe et sort, change ou range le Pokémon.
- `EFollowMode` (`Pokemon/PokemonTypes.h`) et `FSpeciesRow::FollowMode` (`Pokemon/PokemonDataRows.h`).
- Commande `Suiveur` dans `PokemonNewAgePlayerController`.

## Plus tard (idées)

- Sortie et retour avec la Poké Ball du Pokémon (lancer, éclair rouge).
- Petites réactions selon le lieu ou l'humeur (bulles comme dans HeartGold), objets trouvés.
- Les nageurs pourraient suivre **dans l'eau** quand le joueur surfera.
- Garder le choix `Suiveur` dans la sauvegarde et le mettre dans les Paramètres.
