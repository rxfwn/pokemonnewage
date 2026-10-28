# Menu Pokédex 3D (SCRUM-64) et écran de combat (SCRUM-36)

Quand on appuie sur **Tab**, le Pokédex rouge modélisé par L'amie sort du bas de l'écran (sans mains), s'ouvre sur sa charnière et le décor se floute derrière. Les **10 boutons à icônes** du volet ouvrent les menus. Tout ce qui s'affiche vient des vraies données (équipe, sac, Pokédex et badges du PlayerState, Data Tables des espèces, attaques, objets et rencontres).

Pendant un combat, Tab ne fait rien : c'est l'écran de combat qui s'affiche.

## 1. Compiler (Unreal fermé)

1. Fermer Unreal.
2. Dans VS Code : **Terminal > Exécuter la tâche…** puis **PokemonNewAgeEditor Win64 Development Build**.
3. Rouvrir le projet.

## 2. Mettre le modèle 3D dans le projet

Le modèle de L'amie (`Pokedex.blend`) a été découpé en 33 pièces par `Tools/Pokedex/prepare_pokedex_model.py` (chaque bouton s'enfonce seul, le volet tourne, les écrans reçoivent l'interface). Ses textures sont gardées.

1. Prendre `Pokedex_Generated.zip` dans les fichiers du projet (dossier **pokedex/**) et le dézipper dans `Tools/Pokedex/`. On doit avoir `Tools/Pokedex/Generated/Pokedex.fbx` et `Tools/Pokedex/Generated/textures/` (16 images).
   Autre possibilité, si Blender est installé dans `C:\Program Files\Blender Foundation\` : mettre `Pokedex.blend` (dossier **pokedex/source/**) dans `Tools/Pokedex/Source/`, le script le prépare tout seul.
2. Dans Unreal : **Outils > Exécuter un script Python** (Tools > Execute Python Script), choisir `Tools/Pokedex/import_pokedex_unreal.py`.
3. Attendre la fin. Le journal (**Fenêtre > Journal de sortie**) doit afficher « Pokédex 3D prêt : 33 pièces sur 33, 16 textures, 10 icônes, 41 lieux sur la carte, 7 sons ».

Le script crée dans `Content/UI/Pokedex/` : les pièces (`Meshes/`), les textures (`Textures/`), les matériaux `M_PokedexTextured` + `MI_Pokedex_Udim0001` à `0005` (les textures de L'amie) et `M_Pokedex` (uni, sous les icônes), les icônes (`Icons/`), la carte (`Map/`), les sons (`Sounds/`) et `DA_PokedexModel`. Il crée aussi `Content/Audio/SC_Music`, `SC_Effects` et `SM_Options` pour les volumes. On peut le relancer, il remplace tout.

Pour changer une icône : remplacer l'image dans `Tools/Pokedex/Assets/icons/` (ou relancer `make_pokedex_icons.py` sur une nouvelle planche) puis relancer le script d'import. La carte se refait avec `make_pokedex_map.py` (elle suit `Tools/Blockout/kanto_v1_layout.py`).

Sans `DA_PokedexModel`, Tab ouvre l'ancien menu à plat (mêmes pages, sans le 3D).

## 3. Tester

Lancer le jeu sur `L_Kanto`, puis ouvrir la console (touche **²** ou **`**) pour se donner de quoi remplir les menus :

| Commande | Effet |
|---|---|
| `DonnerPokemon Bulbasaur 5` | ajoute un Pokémon à l'équipe |
| `DonnerObjet Potion 5` | ajoute un objet au sac |
| `CombatSauvage Pidgey 3` | lance un combat sauvage (écran de combat) |
| `Sauvegarder` | sauvegarde tout de suite |
| `NouvellePartie` | efface la sauvegarde |

Les pièces de l'appareil :

| Pièce | Effet |
|---|---|
| 10 boutons à icônes du volet | ouvrent les menus (voir plus bas) |
| `<` et `>` sur le volet | menu précédent / suivant |
| bouton rond jaune du volet, bouton rond noir | valider |
| petit bouton rouge sous l'écran | retour |
| languette jaune | accueil (retour au volet) |
| languette bleue, objectif bleu | ouvre le Pokédex |
| croix | naviguer |
| charnière | fermer |
| grand écran | cliquable directement (listes, onglets) |

- **Tourner l'appareil** : clic droit maintenu, ou clic gauche sur le boîtier ou le volet. Il revient en place quand on lâche. La molette rapproche ou éloigne (sur l'écran, elle fait défiler la liste).
- **Clavier** : flèches ou ZQSD (grille 5 × 2 sur le volet), Entrée pour valider, Échap pour revenir, Tab pour fermer.
- **Manette** : croix, A, B, gâchettes hautes pour changer de menu, stick droit pour tourner, Start pour fermer.

Les autres écrans : celui du haut du volet donne le nom et la description de ce qui est choisi (menu, Pokémon, espèce, objet, ville). Les petits écrans affichent l'heure et l'argent (sous l'écran), le Pokédex capturés / vus et les badges (sur le volet).

Ce que contient chaque menu (dans l'ordre des boutons) :

1. **Pokémon (L'Équipe)** : l'équipe avec PV et objet tenu, la fiche en 3 onglets (infos, stats, attaques), **Déplacer** pour changer l'ordre de combat, **Donner un objet** / **Reprendre l'objet**.
2. **Pokédex (L'Encyclopédie)** : toutes les espèces par numéro, vu / capturé, fiche avec types, description et **où le trouver** (tables de rencontres).
3. **Sac** : 4 poches, **Soins**, **Capture**, **Baies et matériaux**, **Objets clés** ; utiliser un soin sur un Pokémon.
4. **Radio** : bientôt.
5. **Carte Dresseur** : nom, numéro, argent, temps de jeu, Pokédex, les 8 badges et les **exploits** (premier Pokémon, 10 capturés, 50 vus, badge de Pierre, 8 badges).
6. **Carte** : Kanto vu de haut, le point rouge est le joueur, « Tu es ici » donne le lieu, la liste des villes les montre sur la carte.
7. **Sauvegarder** : enregistre la partie (rechargée au lancement suivant).
8. **En ligne** : mode de la partie (solo, hôte, connecté) et joueurs présents ; échanges et combats entre joueurs : bientôt (SCRUM-69).
9. **Options** : vitesse du texte, volume général, **musique**, **bruitages**, sensibilité et inversion de la caméra, qualité, affichage, synchro verticale, **commandes** (liste des touches), effacer la sauvegarde.
10. **Style** : bientôt.

En ligne (SCRUM-68), le menu ne met pas le jeu en pause et chaque joueur a le sien. Les actions (échanger l'ordre, utiliser ou donner un objet) passent par le serveur.

## 4. Si quelque chose cloche

Les réglages sont dans la classe `PokedexDevice` (créer un Blueprint enfant pour les modifier, puis le mettre dans **Pokedex Device Class** du PlayerController) :

- **Trop sombre ou trop clair** : `Menu Exposure EV100` (plus petit = plus clair), `Key Light Intensity`, `Fill Light Intensity`.
- **Fond pas assez flou** : baisser `Background Blur Fstop`.
- **Trop gros ou trop petit à l'écran** : `Screen Fill`.
- **Volet trop ou pas assez tourné vers soi** : `Open Lid Yaw` (10 degrés par défaut).
- **Appareil à l'envers ou en miroir** : le journal affiche un avertissement. Le code corrige seul l'échelle et le miroir de l'import ; si ça ne suffit pas, me le dire avec une capture.
- **Appareil tout gris** : les textures n'ont pas été importées (dossier `Generated/textures` manquant) ; relancer le script d'import après avoir dézippé.
- **Couleur trop terne ou relief inversé** : ouvrir `MI_Pokedex_Udim000X` et régler `Tint`, ou décocher **Inverser le canal vert** (Flip Green Channel) sur la texture `_Normal`.

## Écran de combat (SCRUM-36)

Il s'ouvre tout seul quand un combat commence : cadres des deux Pokémon (nom, niveau, PV animés, XP), boîte de texte qui s'écrit, menus **Attaque / Sac / Pokémon / Fuite**, choix du remplaçant après un K.O., 5e attaque à oublier et évolution. Il se ferme 1 seconde après la fin du combat.

## Pas encore fait

- **Radio** et **Style** : pages « bientôt ».
- **Changer les touches** : la page Commandes montre les touches sans pouvoir les modifier.
- **Échanges et combats entre joueurs** : SCRUM-69.
- **Objets tenus** : on peut les donner et les reprendre, mais ils n'ont pas encore d'effet en combat.
- **Musique** : le réglage existe ; il agira sur les musiques dès qu'elles utiliseront la classe de son `SC_Music`.
- Le modèle 3D du Pokémon dans la fiche du Pokédex (il n'y a pas encore de modèles importés).
- Les sons sont synthétiques : on pourra les remplacer en gardant les mêmes noms.
- Le nom du joueur est celui donné par Unreal (pas encore d'écran pour le choisir).
