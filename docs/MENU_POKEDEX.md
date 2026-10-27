# Menu Pokédex 3D (SCRUM-64) et écran de combat (SCRUM-36)

Quand on appuie sur **Tab**, un vrai Pokédex rouge sort du bas de l'écran, s'ouvre sur sa charnière et le décor se floute derrière. Ses 6 boutons ouvrent les menus : **Pokémon, Pokédex, Sac, Dresseur, Sauvegarder, Options**. Tout ce qui s'affiche vient des vraies données (équipe, sac, Pokédex et badges du PlayerState, Data Tables des espèces, attaques et objets).

Pendant un combat, Tab ne fait rien : c'est l'écran de combat qui s'affiche.

## 1. Compiler (Unreal fermé)

1. Fermer Unreal.
2. Dans VS Code : **Terminal > Exécuter la tâche…** puis **PokemonNewAgeEditor Win64 Development Build**.
3. Rouvrir le projet.

## 2. Mettre le modèle 3D dans le projet

Le modèle est fabriqué par un script Blender (`Tools/Pokedex/make_pokedex_model.py`). Pas besoin de savoir modéliser.

1. Prendre `Pokedex.fbx` dans les fichiers du projet (dossier **pokedex/**) et le copier dans `Tools/Pokedex/Generated/` (créer le dossier).
   Si Blender est installé dans `C:\Program Files\Blender Foundation\`, cette étape est faite toute seule.
2. Dans Unreal : **Outils > Exécuter un script Python** (Tools > Execute Python Script), choisir `Tools/Pokedex/import_pokedex_unreal.py`.
3. Attendre la fin. Le journal (**Fenêtre > Journal de sortie**) doit afficher « Pokédex 3D prêt : 22 pièces sur 22, 7 sons ».

Le script crée dans `Content/UI/Pokedex/` : les pièces (`Meshes/`), le matériau `M_Pokedex`, les sons (`Sounds/SW_Pokedex_*`) et `DA_PokedexModel`. On peut le relancer, il remplace tout.

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

Commandes du Pokédex :

- **Souris** : clic gauche sur un bouton du volet, sur la croix, le bouton rond (valider), le bouton rouge (retour) ou le bleu (accueil). Cliquer sur l'objectif ouvre le Pokédex, cliquer sur le volet ferme. On peut aussi cliquer directement dans l'écran.
- **Tourner l'appareil** : clic droit maintenu (ou clic gauche dans le vide). Il revient en place quand on lâche. La molette rapproche ou éloigne.
- **Clavier** : flèches ou ZQSD, Entrée pour valider, Échap pour revenir, Tab pour fermer.
- **Manette** : croix, A, B, gâchettes hautes pour changer de menu, stick droit pour tourner, Start pour fermer.

Ce que contient chaque menu :

- **Pokémon** : l'équipe, la fiche en 3 onglets (infos, stats, attaques), échanger l'ordre.
- **Pokédex** : toutes les espèces par numéro, vu / capturé, fiche.
- **Sac** : 4 poches, utiliser un objet sur un Pokémon (Potion…).
- **Dresseur** : carte avec nom, numéro, argent, temps de jeu, Pokédex et les 8 badges.
- **Sauvegarder** : enregistre la partie (rechargée au lancement suivant).
- **Options** : vitesse du texte, volume, sensibilité et inversion de la caméra, qualité, mode fenêtre, synchro verticale, effacer la sauvegarde.

En ligne (SCRUM-68), le menu ne met pas le jeu en pause et chaque joueur a le sien. Les actions (échanger, utiliser un objet) passent par le serveur.

## 4. Si quelque chose cloche

Les réglages sont dans la classe `PokedexDevice` (créer un Blueprint enfant pour les modifier, puis le mettre dans **Pokedex Device Class** du PlayerController) :

- **Trop sombre ou trop clair** : `Menu Exposure EV100` (plus petit = plus clair), `Key Light Intensity`, `Fill Light Intensity`.
- **Fond pas assez flou** : baisser `Background Blur Fstop`.
- **Trop gros ou trop petit à l'écran** : `Screen Fill`.
- **Appareil à l'envers ou en miroir** : le journal affiche un avertissement. Le code corrige seul l'échelle et le miroir de l'import ; si ça ne suffit pas, me le dire avec une capture.

## Écran de combat (SCRUM-36)

Il s'ouvre tout seul quand un combat commence : cadres des deux Pokémon (nom, niveau, PV animés, XP), boîte de texte qui s'écrit, menus **Attaque / Sac / Pokémon / Fuite**, choix du remplaçant après un K.O., 5e attaque à oublier et évolution. Il se ferme 1 seconde après la fin du combat.

## Pas encore fait

- Le modèle 3D du Pokémon dans la fiche du Pokédex (il n'y a pas encore de modèles importés).
- Les sons sont synthétiques : on pourra les remplacer en gardant les mêmes noms.
- Le nom du joueur est celui donné par Unreal (pas encore d'écran pour le choisir).
