# Écran de titre (SCRUM-76)

D'après la maquette Figma de L'amie du 2026-10-31 (1920 × 1080) :

- **Fond** : son image (Pikachu et le dresseur au-dessus du village), qui remplit l'écran avec un zoom très lent.
- **Logo** de L'amie (« Plan de travail 10 », `Tools/Title/Assets/T_Title_Logo.png`) en haut à gauche, à (34, 34), 638 × 323, ombre portée noire à 60 %.
- **Menu** à gauche, x = 160 : **Continuer** (y 621), **Nouvelle partie** (694), **Options** (767). Futura Bold penchée 32 px (choix de L'amie : « c'est la même » que Futura Md BT italique), blanc, ombre (3, 4) noire à 60 %. Sans sauvegarde, Continuer disparaît et les deux autres remontent.
- **En bas à droite** : « v.1.0.0 » et « © 2026 », alignés à droite à 1832 px.
- **Toutes les typos en Futura Bold penchée** (`Tools/HUD/Assets/fonts/FF_Futura_Bold.ttf`, penchée par Unreal).

## Ce que fait chaque entrée

| Entrée | Effet |
|---|---|
| Continuer | l'écran s'efface en fondu sur le jeu, à l'endroit sauvegardé (la sauvegarde est déjà chargée dessous) |
| Nouvelle partie | s'il y a une sauvegarde : « Commencer une nouvelle partie ? » (Non par défaut, Échap ou B = Non). Oui efface la sauvegarde et recharge la map, sans repasser par le titre. Sans sauvegarde : on commence tout de suite |
| Options | ouvre le menu Paramètres (le même que M en jeu) ; en le fermant, on revient au titre |

Souris, flèches, Entrée, ou manette (stick/croix, A). Le HUD, le Pokédex et le personnage ne répondent pas tant que le titre est là. « Quitter le jeu » est dans Options > Jeu.

## Comment c'est fait

Pas de map à part : l'écran s'affiche **par-dessus L_Kanto** au lancement (le monde ouvert se charge pendant ce temps). Tout est en C++ dans `Source/PokemonNewAge/UI/Title/` :

- `UTitleScreenSubsystem` : s'allume tout seul sur les maps de la liste (L_Kanto), cache le HUD, bloque les commandes du jeu, puis rend la main.
- `UTitleScreenWidget` : l'écran lui-même, aux mesures de la maquette.
- `UTitleScreenSettings` : réglages dans **Modifier > Paramètres du projet > Jeu > Écran de titre** (Edit > Project Settings > Game) : activer ou non, l'afficher aussi en Jouer dans l'éditeur, maps, images de fond (plusieurs = fondu toutes les 15 s), logo, textes du bas, musique et bruits.

Sans image de fond importée, le titre montre la map en 3D : la caméra tourne lentement au-dessus du joueur (ou prend la vue d'un acteur tagué `TitleCamera` dans la map), puis vole jusque derrière lui.

La map de lancement du jeu final (**Map par défaut du jeu**, Game Default Map) est maintenant L_Kanto.

Pour sauter le titre : lancer avec `-NoTitle`, ou décocher **Afficher dans l'éditeur** (Show In Editor) dans les réglages.

## Installer et tester (une seule fois, Unreal fermé puis ouvert)

1. `git pull`, puis compiler dans VS Code (tâche **PokemonNewAgeEditor Win64 Development Build**, Unreal fermé).
2. Ouvrir Unreal, puis **Outils > Exécuter un script Python** (Tools > Execute Python Script) → `Tools/Title/import_title_unreal.py`. Il crée `/Game/UI/Title/T_Title_Background` et `T_Title_Logo`.
3. Ouvrir L_Kanto et cliquer sur **Jouer**. À vérifier :
   - l'écran ressemble à la maquette (fond, titre en haut à gauche, menu à gauche, version en bas à droite) ;
   - flèches et souris changent l'entrée choisie (elle se décale un peu à droite) ;
   - **Options** ouvre les Paramètres, Échap/Retour revient au titre ;
   - **Continuer** fait disparaître le titre en fondu, le HUD revient et le nom du lieu s'affiche ;
   - relancer, **Nouvelle partie** → la question apparaît ; **Non** revient ; **Oui** recharge la map au point de départ.

## Pour ajouter d'autres fonds

Les autres fonds de la maquette (Route 1, sas) peuvent défiler : `Tools/Title/Assets/T_Title_Background_2.png`, `_3`..., relancer le script d'import, puis les ajouter dans les réglages (Backgrounds).
