# Blockout de Bourg Palette (SCRUM-23)

Un **blockout**, c'est la ville en formes simples (cubes, cylindres, cônes) aux bonnes dimensions. Il sert à régler les distances, le parcours et la caméra avant de faire les vrais décors. Les vrais modèles 3D remplaceront ces formes plus tard, en suivant `docs/DIRECTION_ARTISTIQUE.md`.

Le blockout est posé par un script Python (`Tools/Blockout/blockout_pallet_town.py`) : pas besoin de placer les cubes à la main.

## Le plan

Vu de dessus, nord en haut (vers la Route 1). Une case ≈ 5 m. Le joueur fait 1,80 m.

```
                 Route 1 (SCRUM-24)
  🌲 🌲 🌲 🌲 🌲 🌲 🌲  ↑  🌲 🌲 🌲 🌲 🌲 🌲 🌲
  🌲                  |  chemin              🌲
  🌲                  |                      🌲
  🌲  ┌────────┐      |      ┌────────┐      🌲
  🌲  │ Maison │==porte|porte==│ Maison │      🌲
  🌲  │ joueur │  ┆  [P]  ┆  │ rival  │      🌲
  🌲  └────────┘  ┆ ✿ | ✿ ┆  └────────┘      🌲
  🌲              ┆ ✿ | ✿ ┆                  🌲
  🌲                  |                      🌲
  🌲                  |                      🌲
  🌲          ┌───────┴───────┐              🌲
  🌲          │  Labo du      │ 📡            🌲
  🌲          │  Prof. Chen   │              🌲
  🌲          └───────────────┘              🌲
  ~~~~~~~~~~~~~~~~~ mer ~~~~~~~~~~~~~~~~~~~~~~~
```

[P] = point de départ du joueur, ✿ = jardins fleuris, ┆ = barrières blanches, 📡 = antenne du labo.

| Élément | Taille (L × l × h) | Remarque |
|---|---|---|
| Ville | 97 m × 100 m | Sortie seulement au nord, murs invisibles ailleurs |
| Maison du joueur / du rival | 10 × 9 × 6 m + toit | Deux niveaux, porte face au chemin |
| Labo du Prof. Chen | 14 × 20 × 7 m | Au bout du chemin, porte au nord, mer derrière |
| Chemin principal | 4 m de large, 76 m de long | Du labo à la Route 1 |
| Jardins | 6 × 5 m | Barrières blanches de 1 m le long du chemin |

Les couleurs viennent de la palette « Bourg Palette » de la direction artistique (tuiles, bois miel, herbe, terre, barrières blanches).

Les portes sont des repères : on pourra entrer dans les bâtiments avec SCRUM-25.

## Pas à pas (15 minutes)

### 1. Récupérer le travail

1. **Fermer Unreal.**
2. Dans le terminal de VS Code :
   ```
   git checkout claude/project-thread-5j8wl2
   git pull
   ```
3. **Ctrl+Maj+B** puis **PokemonNewAgeEditor Win64 Development Build**. Attendre « Succeeded ».
4. Ouvrir `PokemonNewAge.uproject`. Le projet active maintenant les plugins **Python Editor Script Plugin** et **Editor Scripting Utilities** (pour lancer le script).

### 2. Créer la map

1. **Fichier > Nouveau niveau**, choisir **Basic** (ciel, soleil et lumière déjà en place), puis **Créer**.
2. **Fichier > Enregistrer le niveau sous** : créer les dossiers `World` puis `PalletTown` dans `Content`, et nommer la map **`L_PalletTown`**.

### 3. Lancer le script

1. **Outils > Exécuter un script Python…** (ou **Fichier > Exécuter un script Python…** selon la version).
2. Choisir `D:\Dev\pokemonnewage\Tools\Blockout\blockout_pallet_town.py`.
3. Attendre quelques secondes : la ville apparaît, rangée dans le dossier **PalletTown_Blockout** de l'Outliner, et la map est enregistrée.

Le script efface et repose tout à chaque lancement : on peut le relancer après une modification du plan.

### 4. Tester

1. Cliquer sur **Play**. On démarre devant la maison du joueur, face au labo.
2. Vérifier :
   - aller de sa maison au labo à pied, puis en courant ;
   - les maisons et le labo ont une taille crédible à côté du personnage ;
   - on ne peut sortir de la ville que par le nord ;
   - la caméra ne traverse pas les murs (zoom à la molette).
3. Noter ce qui paraît trop grand, trop petit, trop loin : on ajuste les cotes dans le script.

### 5. Enregistrer dans Git

```
git status
git add Content/World
git commit -m "SCRUM-23 Ajouter la map L_PalletTown en blockout"
git push
```

## Si ça ne marche pas

- **Pas de « Exécuter un script Python »** : **Modifier > Plugins**, chercher `Python`, cocher **Python Editor Script Plugin**, redémarrer Unreal.
- **Le script ne fait rien** : ouvrir **Fenêtre > Journal de sortie** et chercher les lignes `LogPython`. Le message « Ouvre d'abord la map L_PalletTown » veut dire qu'une autre map est ouverte.
- **Tout est gris** : la couleur des matériaux n'a pas été appliquée (avertissement dans le journal). Ce n'est pas bloquant pour le blockout, envoie le message à Claude.
