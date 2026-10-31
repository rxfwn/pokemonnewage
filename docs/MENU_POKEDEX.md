# Menu Échap, Pokédex 3D et HUD en jeu (SCRUM-64, SCRUM-36)

Organisation voulue par L'amie le 2026-10-29 :

- **M** (ou Select) ouvre le menu **Paramètres** (provisoire à la place d'Échap, le temps des tests dans l'éditeur ; Échap le referme) façon jeu PC : onglets **Audio**, **Vidéo**, **Commandes**, **Jeu**, et les boutons **OK** (appliquer et fermer), **Annuler** (revenir aux réglages enregistrés), **Appliquer**, **Retour** (fermer sans appliquer). Rien d'autre dans ce menu.
- **Tab** (ou Start) sort le **Pokédex 3D** modélisé par L'amie. C'est lui qui a toutes les fonctions du jeu (voir la liste plus bas), dont **Sauvegarder** ; sinon le jeu sauvegarde tout seul.
- Le **HUD** suit la maquette de L'amie (fichiers du projet `hud/maquette_hud_route1.png`) : icône du Pokédex en haut à droite, nom du lieu en grand à l'arrivée, et en bas à droite le lieu, l'heure, la température, la météo et la boussole.

Pendant un combat, Tab, Échap et les raccourcis ne font rien : c'est l'écran de combat qui s'affiche.

**Pourquoi M et pas Échap :** dans l'éditeur, Échap arrête le jeu (PIE). Pour repasser sur Échap plus tard : remettre `EKeys::Escape` dans `SetupInputComponent` et `IsSettingsKey` du PlayerController. Pour tester Échap dès maintenant : lancer en **Jeu autonome** (Standalone Game, flèche à côté de Jouer), ou changer le raccourci de l'éditeur dans **Modifier > Préférences de l'éditeur > Général > Raccourcis clavier**, chercher **Arrêter** (Stop) dans **Jouer dans l'éditeur** et mettre Maj + Échap. Select à la manette marche aussi.

## Le menu Échap (Paramètres)

| Onglet | Réglages |
|---|---|
| Audio | volume général, musique, bruitages |
| Vidéo | résolution, mode d'affichage (plein écran, plein écran fenêtré, fenêtré), synchro verticale, images par seconde max., champ de vision, échelle de résolution, qualité globale, distance d'affichage, anticrénelage, ombres, textures, effets, post-traitement, végétation |
| Commandes | sensibilité et inversion de la caméra, **toutes les touches** (clavier puis manette : clic sur la touche, puis appuyer sur la nouvelle ; si elle servait déjà, les deux commandes échangent), « Remettre les touches d'origine » |
| Jeu | vitesse du texte, afficher le HUD, sauvegarde automatique, effacer la sauvegarde (deux clics), quitter le jeu |

Les réglages attendent **Appliquer** ou **OK** ; les touches changent tout de suite. Ils sont gardés dans `Saved/SaveGames/Options.sav` et `GameUserSettings.ini`. LB / RB changent d'onglet à la manette.

## Le Pokédex 3D (Tab)

Comme l'image de référence de L'amie (sans les mains) : Tab sort le Pokédex ouvert devant la caméra, il prend presque tout l'écran sans être coupé (`Screen Fill` 0,86, épaisseur comprise, recalculé si la fenêtre change de taille), décor du jeu flouté derrière. On le tourne à la souris jusqu'à 30° de côté et 20° en hauteur (`Max Turn Yaw` / `Max Turn Pitch`). Les voyants du haut et l'objectif restent allumés ; le bouton rond noir sous l'écran (petite icône appareil photo de L'amie) lance l'appareil photo. Le Pokédex suit la caméra pendant l'ouverture : il finit bien en face même si on tournait la caméra en appuyant sur Tab.

**Netteté** : écrans et icônes sont dessinés `Panel Render Scale` fois plus fins (2 par défaut : l'écran principal est rendu en 1920 × 1296).

**Éclairage** (d'après l'image de L'amie, lumière de jour douce) : lumière principale chaude et large, lumière d'appoint bleutée (ciel), contre-jour chaud derrière en haut à droite (`Rim Light Intensity`), décor plus flouté (`Background Blur Fstop` 1,8), léger bloom et couleurs un peu chaudes.

**Heure du jeu** (HUD, petit écran, barre de l'écran) : `Core/GameClock`. Lue dans une variable décimale `Heure`, `Hour`, `TimeOfDay` ou `Time` (0 à 24) de `BP_CycleJourNuit` si elle existe, sinon déduite de la hauteur du soleil (midi en haut, matin quand il monte), sinon l'heure du PC.

**Rendu (réglages dans `BP`/classe `PokedexDevice`, catégorie Rendu)** : exposition fixe calculée d'après le soleil de la map (`Match World Exposure`, correction `World Exposure Offset` : négatif = plus clair ; sinon `Menu Exposure EV100`). La valeur choisie s'écrit dans le journal : « Pokedex 3D : exposition du menu EV100 … ». Les lumières de l'appareil, les voyants et les écrans suivent cette valeur tout seuls. Les écrans et icônes utilisent `M_PokedexWidget` (créé par le script d'import, réimporté tout seul à l'ouverture d'Unreal s'il manque) ; leur luminosité : `Screen Brightness`. À gauche l'écran affiche la fonction choisie, à droite le volet porte les 10 boutons à icônes : ils s'allument et se soulèvent sous la souris ou la sélection, et s'enfoncent au clic.


Les 10 boutons à icônes du volet, dans l'ordre de la grille :

1. **Pokédex** : toutes les espèces par numéro, vu / capturé, fiche avec types, description, **où le trouver** et la figurine 3D du Pokémon.
2. **Carte et GPS** : Kanto vu de haut, zoomé et centré sur la ville choisie (la carte défile quand on change de ville), le point rouge est le joueur, « Tu es ici » donne le lieu. Valider sur une ville en fait la **destination du GPS** (point vert sur la carte, repère jaune et distance sur la boussole du HUD) ; valider encore l'arrête.
3. **Sac** : 4 poches, **Soins**, **Capture**, **Baies et matériaux**, **Objets clés** ; utiliser un soin sur un Pokémon.
4. **Équipe** : PV et objet tenu, la fiche en 3 onglets (infos, stats, attaques), **Déplacer**, **Donner un objet** / **Reprendre l'objet**.
5. **Carte dresseur** : nom, numéro, argent, temps de jeu, Pokédex, les 8 badges et les exploits.
6. **Radio** : bientôt.
7. **Multijoueur** : mode de la partie et joueurs présents ; échanges et combats : bientôt (SCRUM-69).
8. **Appareil photo** : le Pokédex se range, le HUD disparaît ; clic gauche, Entrée ou A prend une photo (sans l'interface, dans `Saved/Screenshots/Windows/Photo_<date>.png`), Échap ou B revient au jeu.
9. **Compteur de pas** : les pas depuis le début (un pas = 75 cm marchés au sol, comptés par le serveur et sauvegardés) et la distance en km.
10. **Sauvegarder** : enregistre la partie tout de suite.

**Raccourcis** (modifiables dans Échap > Commandes) : P Pokédex, G Carte (GPS), B Sac, R Équipe, C Carte dresseur, L Multijoueur, V Appareil photo, F5 Sauvegarder. Ils ouvrent le Pokédex directement sur la fonction (l'appareil photo démarre sans le Pokédex).

Les icônes **Appareil photo** et **Compteur de pas** sont provisoires (faites par `Tools/HUD/make_hud_assets.py`) : remplacer `Tools/Pokedex/Assets/icons/T_PokedexIcon_Camera.png` et `T_PokedexIcon_Steps.png` (256 × 256, fond transparent) puis relancer le script d'import.

Tab ouvre toujours le modèle 3D. Le Pokédex à plat n'est qu'un secours, si le modèle n'est pas installé (voir §2) ou si `Use Pokedex 3D` est décoché dans le PlayerController.

## Sauvegarde automatique

Toutes les 5 minutes et en arrivant dans un nouveau lieu, sauf en combat ou menu ouvert, et jamais deux fois en moins de 30 s. « Partie sauvegardée » s'affiche sous l'icône du Pokédex. Réglages `Auto Save Interval` et `Auto Save Min Gap` dans le PlayerController ; Échap > Jeu > Sauvegarde automatique pour la couper.

## Le HUD

Fait en C++ (`UGameHUDWidget`) aux mesures de la maquette (1920 × 1080, mis à l'échelle par Unreal) :

- **Icône du Pokédex** en haut à droite (image de L'amie), sans texte de touche (Tab l'ouvre).
- **Nom du lieu** en Futura 95, centré en haut, avec le trait blanc dessous : il apparaît 4 s quand on entre dans une ville ou sur une route.
- **En bas à droite** : le lieu (Futura BdCn BT 24, droite, comme la maquette), l'heure du jeu (voir « Heure du jeu »), la température (17 °C de moyenne, plus chaud à 15 h, réglable : `Base Temperature`, `Temperature Swing`) et le picto météo (pictos de L'amie : soleil par défaut, `SetWeather` pour nuage, pluie ou vent en attendant un vrai système météo).
- **Boussole** : disque gris translucide, N en haut (le nord de Kanto), point blanc du joueur, cône de vue qui tourne avec la caméra, repère jaune du GPS sur le bord et distance au centre.
- Il se cache pendant les combats, les menus, la boutique, le PC et l'appareil photo, ou avec Échap > Jeu > Afficher le HUD.

Images et polices : `Tools/HUD/Assets/` (icône du Pokédex, cône, 4 pictos météo de L'amie ; polices Futura en `FF_*.ttf`), importées dans `/Game/UI/HUD` par `Tools/HUD/import_hud_unreal.py`, lancé aussi à la fin du script du Pokédex.

## 1. Compiler (Unreal fermé)

1. Fermer Unreal.
2. Dans VS Code : **Terminal > Exécuter la tâche…** puis **PokemonNewAgeEditor Win64 Development Build**.
3. Rouvrir le projet.

## 2. Mettre le modèle 3D dans le projet

Le modèle de L'amie (`Pokedex.blend`) a été découpé en 33 pièces par `Tools/Pokedex/prepare_pokedex_model.py` (chaque bouton s'enfonce seul, le volet tourne, les écrans reçoivent l'interface). Ses textures sont gardées.

Le modèle découpé est **dans le dépôt** : `Tools/Pokedex/Generated/Pokedex.fbx` et `Tools/Pokedex/Generated/textures/` (16 images). Plus rien à dézipper.

1. `git pull`, compiler (§1), puis ouvrir Unreal.
2. **C'est tout** : à l'ouverture, `Content/Python/init_unreal.py` voit que `DA_PokedexModel` manque (ou n'a pas le modèle 3D) et lance tout seul `Tools/Pokedex/import_pokedex_unreal.py`. Une fenêtre « Import du menu » s'affiche une minute. Les fois suivantes, il ne fait rien.
3. Le journal (**Fenêtre > Journal de sortie**) doit afficher « HUD prêt : 6 images, 3 polices » puis « Menu prêt : 33 pièces 3D sur 33, 16 textures, 10 icônes, 41 lieux sur la carte, 7 sons ».

Pour réimporter de force (nouvelle icône, nouveau modèle) : **Outils > Exécuter un script Python** (Tools > Execute Python Script), choisir `Tools/Pokedex/import_pokedex_unreal.py`. Pour refaire le modèle depuis `Pokedex.blend` : `prepare_pokedex_model.py` dans Blender, qui réécrit `Tools/Pokedex/Generated/`.

Le script crée dans `Content/UI/Pokedex/` : les pièces (`Meshes/`), les textures (`Textures/`), les matériaux `M_PokedexTextured` + `MI_Pokedex_Udim0001` à `0005` (les textures de L'amie) et `M_Pokedex` (uni, sous les icônes), les icônes (`Icons/`), la carte (`Map/`), les sons (`Sounds/`) et `DA_PokedexModel`. Il crée aussi `Content/Audio/SC_Music`, `SC_Effects` et `SM_Options` pour les volumes. On peut le relancer, il remplace tout.

Pour changer une icône : remplacer l'image dans `Tools/Pokedex/Assets/icons/` (ou relancer `make_pokedex_icons.py` sur une nouvelle planche) puis relancer le script d'import. La carte se refait avec `make_pokedex_map.py` (elle suit `Tools/Blockout/kanto_v1_layout.py`).

Si le Pokédex 3D n'est pas installé, Tab ouvre en secours un Pokédex à plat, et un message orange le signale dans l'éditeur : fermer et rouvrir Unreal suffit.

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
| bouton rond noir (icône appareil photo) | appareil photo |
| croix | naviguer |
| grand écran | cliquable directement (listes, onglets) |

Le reste est du décor, **pas cliquable** (demandé par L'amie) : objectif et voyants du haut, languettes jaune et bleue, petit bouton rouge de l'écran, voyants et fentes du volet, bouton rond jaune du volet, petits écrans « CAPTURÉS/VUS » et « BADGES », boîtier, charnière. La main de la souris n'apparaît que sur ce qui est cliquable.

- **Tourner l'appareil** : seulement **clic droit maintenu** en bougeant la souris (ou stick droit). Il ne bouge jamais tout seul et reste où on l'a laissé. Plus il est tourné, plus il recule, pour ne jamais sortir de l'écran. La molette l'éloigne (jamais plus près que le cadrage de départ ; sur l'écran, elle fait défiler la liste).
- **Clavier** : flèches ou ZQSD (grille 5 × 2 sur le volet), Entrée pour valider, Échap ou Retour arrière pour revenir, Tab pour fermer.
- **Manette** : croix, A, B, gâchettes hautes pour changer de menu, stick droit pour tourner, Start pour fermer.

Les autres écrans : celui du haut du volet donne le nom et la description de ce qui est choisi (menu, Pokémon, espèce, objet, ville). Les petits écrans affichent l'heure **du jeu** et l'argent (sous l'écran), le Pokédex capturés / vus et les badges (sur le volet).

Ce que contient chaque fonction : voir « Le Pokédex 3D (Tab) » plus haut.

**Figurine 3D** : dans la fiche du Pokédex (espèce vue), la liste de l'équipe et le résumé d'un Pokémon, une petite figurine du Pokémon tourne au-dessus de l'écran du volet. Le code cherche `/Game/Pokemon/Models/<Espèce>/SK_<Espèce>` (ou `SM_<Espèce>` pour un modèle fixe) et joue `A_<Espèce>_Idle` s'il existe. Sans modèle importé, rien ne s'affiche. Réglages dans `PokedexDevice` : `Figure Height` (4,5 cm), `Figure Folder`.

En ligne (SCRUM-68), le menu ne met pas le jeu en pause et chaque joueur a le sien. Les actions (échanger l'ordre, utiliser ou donner un objet) passent par le serveur.

## 4. Si quelque chose cloche

Les réglages sont dans la classe `PokedexDevice` (créer un Blueprint enfant pour les modifier, puis le mettre dans **Pokedex Device Class** du PlayerController) :

- **Trop sombre ou trop clair** : `Menu Exposure EV100` (plus petit = plus clair), `Key Light Intensity`, `Fill Light Intensity`.
- **Fond pas assez flou** : baisser `Background Blur Fstop`.
- **Trop gros ou trop petit à l'écran** : `Screen Fill`.
- **Volet trop ou pas assez tourné vers soi** : `Open Lid Yaw` (10 degrés par défaut).
- **Appareil à l'envers ou en miroir** : le journal affiche un avertissement. Le code corrige seul l'échelle et le miroir de l'import ; si ça ne suffit pas, me le dire avec une capture.
- **Appareil tout gris** : les textures n'ont pas été importées (dossier `Generated/textures` manquant) ; relancer le script d'import (vérifier que `Tools/Pokedex/Generated/textures` est bien là après le `git pull`).
- **Couleur trop terne ou relief inversé** : ouvrir `MI_Pokedex_Udim000X` et régler `Tint`, ou décocher **Inverser le canal vert** (Flip Green Channel) sur la texture `_Normal`.

## Écran de combat (SCRUM-36)

Il s'ouvre tout seul quand un combat commence : cadres des deux Pokémon (nom, niveau, PV animés, XP), boîte de texte qui s'écrit, menus **Attaque / Sac / Pokémon / Fuite**, choix du remplaçant après un K.O., 5e attaque à oublier et évolution. Il se ferme 1 seconde après la fin du combat.

## Pas encore fait

- **Radio** : page « bientôt ».
- **Échanges et combats entre joueurs** : SCRUM-69.
- **Musique** : le réglage existe ; il agira sur les musiques dès qu'elles utiliseront la classe de son `SC_Music`.
- **Figurine** : elle n'apparaît qu'une fois les modèles importés (fil des modèles 3D) ; la version chromatique (`SK_<Espèce>_Shiny`) n'est pas encore utilisée.
- **Météo** : pas encore de système ; le picto et la température du HUD sont fixes ou calculés selon l'heure.
- **Icônes** Appareil photo et Compteur de pas : provisoires.
- **Boussole** : disque vide comme la maquette ; une vraie mini-carte dedans est possible plus tard.
- Les sons sont synthétiques : on pourra les remplacer en gardant les mêmes noms.
- Le nom du joueur est celui donné par Unreal (pas encore d'écran pour le choisir).

## Police du jeu : Futura penchée partout

Demandé par L'amie le 2026-10-31 : toute l'interface (Pokédex 3D, Paramètres, dialogues, combat, boutique, PC, évolution) passe par `PokemonFonts::Get(Taille, bGras)` (`Source/PokemonNewAge/UI/GameFonts.h`). Texte normal : `FF_Futura_Medium` (Futura Cyrillic Medium fournie par L'amie) ; gras : `FF_Futura_Bold` ; les deux inclinées (`SkewAmount` 0,18, comme l'écran de titre). Pour redresser : `Skew` dans `GameFonts.cpp`.

**Le HUD en jeu** suit la maquette de L'amie : Futura BdCn BT (gras condensé), **droite**, blanche, via `PokemonFonts::GetHUD`. Il prend `FF_Futura_BoldCondensed.ttf` si on l'ajoute dans `Tools/HUD/Assets/fonts/` ; en attendant il prend Futura Bold et la resserre à 82 % en largeur. Si l'une manque, l'autre la remplace ; sans aucune, Roboto.

La Futura d'origine n'avait pas les accents : `Tools/HUD/add_futura_accents.py` (fil de l'écran de titre) les ajoute dans `FF_Futura_Bold.ttf` et `FF_Futura_Medium.ttf` (originaux gardés dans `fonts/original/`). À l'ouverture d'Unreal, `init_unreal.py` réimporte toute police dont le `.ttf` est plus récent que l'asset.

