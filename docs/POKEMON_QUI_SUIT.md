# Pokémon qui suit le joueur

Ticket : SCRUM-78 (épopée Exploration). Comme dans HeartGold/SoulSilver ou Let's Go : le **premier Pokémon
de l'équipe** sort de sa Ball et accompagne le joueur hors combat.

## Ce qu'il fait

| Façon de suivre | Qui | Comportement |
|---|---|---|
| **Marche** (`Walk`) | la plupart (Bulbizarre, Pikachu, Carapuce, Onix...) | marche autour du joueur, un peu derrière ou sur le côté (voir « Son comportement ») |
| **Vole** (`Fly`) | Roucool, Piafabec, Papilusion, Dardargnan, Nosferapti, Dracaufeu, Dracolosse, les oiseaux légendaires, Mew... | vole sur un côté du joueur, à hauteur d'épaule, en ondulant |
| **Flotte** (`Float`) | Fantominus, Spectrum, Smogo, Smogogo, Magnéti, Magnéton, Porygon, Draco | flotte à 40 cm du sol, autour du joueur |
| **Ne suit pas** (`None`) | les Pokémon qui vivent dans l'eau : Magicarpe, Léviator, Poissirène, Tentacool, Kokiyas, Hypotrempe, Stari, Lokhlass, Otaria, Amonita, Minidraco... | reste dans sa Ball |

- Il **rentre dans sa Ball** pendant les combats, s'il est K.O., ou si l'équipe est vide.
- Il **change tout seul** quand on change l'ordre de l'équipe (Tab > Équipe > Déplacer), qu'on dépose le premier au PC
  ou qu'il évolue.
- S'il est à **plus de 25 m** du joueur ou **coincé** loin de lui (mur, falaise, eau), il réapparaît derrière le joueur.
- Il ne bloque **ni le joueur, ni la caméra, ni les autres joueurs**.
- **Touche d'action (E)** quand on est tourné vers lui : il se tourne vers nous et montre sa joie
  (animation `Happy` si son modèle en a une, sinon un petit saut). Une porte ou un PNJ à portée passe avant lui.
- Il prend le modèle 3D importé (`/Game/Pokemon/Models/<Espèce>/SK_<Espèce>`, voir [MODELES_POKEMON.md](MODELES_POKEMON.md))
  et ses animations `Idle`, `Walk`, `Run`, `Happy`. Sans modèle : une sphère de 50 cm à sa place.
- Taille réelle du Pokédex (Onix fait 8,8 m !). Pour le réduire, voir « Régler » plus bas.

## Son comportement (depuis le 2026-11-05)

Il ne marche pas dans les pas du joueur : il a sa petite vie.

- **Quand tu bouges** : il se choisit une place autour de toi (derrière, un peu à gauche ou à droite, entre
  1,2 m et 2,6 m) et en change toutes les 4 à 9 secondes. Il ne repart pas au moindre pas : il attend que tu
  sortes de sa zone de confort, puis te rejoint et marche à ton rythme. Au-delà de 5 m il court.
  Il tourne avant d'avancer (des courbes, pas de glissade de côté) et accélère ou freine en douceur.
  S'il y a un mur entre lui et sa place, il passe par où tu es passé.
- **Sa démarche suit sa taille** : un petit Pokémon fait de petits pas. Pikachu (40 cm) marche à 1 m/s et court
  à 3,2 m/s : quand tu marches, il trottine. L'animation (marche ou course) est choisie d'après sa vitesse
  réelle et jouée au rythme du sol, pour que les pattes ne glissent pas.
- **Quand tu t'arrêtes** (après 1,5 s) : il se balade jusqu'à 4 m de toi, s'arrête quelques secondes,
  regarde autour de lui, se retourne vers toi, sautille de joie (animation `Happy`), parfois pique un petit sprint.
  Après 45 s sans bouger, il **s'endort** (animation `Sleep` : Bulbizarre, Salamèche, Carapuce, Pikachu).
  Il se réveille dès que tu repars ou que tu le caresses.

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

Tous les réglages sont sur l'acteur `APokemonFollower` (catégories **Follower** dans Détails). Pour les changer :
créer un Blueprint enfant de `PokemonFollower` (clic droit > Classe Blueprint > chercher PokemonFollower),
puis dans le Blueprint du PlayerController : composant **FollowerComponent > Follower Class** = ce Blueprint.

| Réglage | Par défaut | Rôle |
|---|---|---|
| Min Distance | 120 cm | il ne s'approche pas plus près du joueur |
| Comfort Distance | 260 cm | tant qu'il est plus près, il ne bouge pas |
| Max Distance | 500 cm | au-delà, il court |
| Side Angle | 75° | jusqu'où il se place sur le côté (0 = toujours juste derrière) |
| Replan Interval | 4 à 9 s | il change de place aussi souvent |
| Walk / Run Speed Per Meter | 250 / 800 cm/s | vitesse de marche / course par mètre de taille (la démarche) |
| Speed Overrides | vide | vitesses pour une espèce précise, ex. Pikachu = (110, 350) si ses pattes glissent encore |
| Turn Rate | 300°/s | vitesse à laquelle il tourne |
| Idle Delay | 1,5 s | délai avant qu'il commence à se balader quand le joueur est arrêté |
| Wander Radius | 400 cm | distance max de ses balades |
| Pause Duration | 2 à 6 s | pause entre deux envies |
| Sleep After | 45 s | il s'endort après ce temps (0 = jamais) |
| Fly Height / Float Height | 150 / 40 cm | hauteur des volants / de ceux qui flottent |
| Teleport Distance | 25 m | au-delà, il réapparaît derrière le joueur |
| Max Display Height | 0 (taille réelle) | ex. 250 cm : Onix est réduit à 2,5 m quand il suit |

**Si les pattes glissent encore** (il avance plus vite que ses pas) : baisser `Walk Speed Per Meter` et
`Run Speed Per Meter`, ou mettre une ligne dans `Speed Overrides` pour cette espèce. S'il fait du sur-place,
les monter.

**Changer la façon de suivre d'une espèce** : colonne `FollowMode` de `DT_Species`
(Walk, Fly, Float, None). Pour que ça reste après une régénération des données, la modifier aussi dans
`Tools/Data/build_pokemon_data.py` (`FOLLOW_OVERRIDES`).

## Code

- `Source/PokemonNewAge/Follower/PokemonFollower.h/.cpp` : l'acteur (Character sans contrôleur, déplacé par le serveur).
  Petite machine à états côté serveur : suivre (place autour du joueur, marche/course, contournement par le
  chemin du joueur), pause, balade, regarder autour, dormir. Les animations sont choisies chez chaque joueur
  d'après la vitesse réelle ; la pose Sommeil est répliquée.
- `Source/PokemonNewAge/Follower/PokemonFollowerComponent.h/.cpp` : sur le PlayerController ; 4 fois par seconde,
  le serveur regarde le premier de l'équipe et sort, change ou range le Pokémon.
- `EFollowMode` (`Pokemon/PokemonTypes.h`) et `FSpeciesRow::FollowMode` (`Pokemon/PokemonDataRows.h`).
- Commande `Suiveur` dans `PokemonNewAgePlayerController`.

## Plus tard (idées)

- Sortie et retour avec la Poké Ball du Pokémon (lancer, éclair rouge).
- Petites réactions selon le lieu ou l'humeur (bulles comme dans HeartGold), objets trouvés.
- Les nageurs pourraient suivre **dans l'eau** quand le joueur surfera.
- Garder le choix `Suiveur` dans la sauvegarde et le mettre dans les Paramètres.
