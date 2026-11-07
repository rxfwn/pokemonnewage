# Intégrer le créateur de personnage de l'exercice (inZOI)

Le prof de jeu vidéo de L'amie a fait extraire le menu de création de personnage d'**inZOI** (dossier local
`D:\Dev\SourceArt\CCMenu\Exercice Compilation`, ~50 Go). Cette page explique comment on le branche sur **notre**
éditeur de personnage ([PERSONNAGE_CREATION.md](PERSONNAGE_CREATION.md)).

## À savoir avant de commencer

- **Le menu lui-même ne s'importe pas.** Les écrans d'inZOI sont des Widget Blueprints « cuits » (compilés pour le jeu)
  et leur logique est dans le code C++ du jeu, qu'on ne peut pas lire. L'export ne contient que leur description en
  JSON. C'est pour ça que notre éditeur (`UI/CharacterCreatorWidget`) refait la même mise en page en C++. Ce qu'on
  récupère, ce sont **les ressources** : corps, têtes, vêtements, accessoires, textures, icônes, et les **données**
  de déformation du corps.
- **Rien de tout ça ne va sur GitHub.** Le script importe dans `Content/ImportedAssets/InZOI`, que `.gitignore`
  exclut. Le guide du prof autorise ces ressources pour l'exercice seulement, en privé, et 50 Go ne passeraient de
  toute façon pas dans le dépôt. Conséquence : l'ami(e) qui récupère le projet par Git n'a **pas** ces ressources ;
  le jeu marche quand même (il reprend `DA_TrainerAppearance`, le catalogue de base). Pour qu'il ou elle les voie,
  il faut lancer le même script sur son PC avec une copie du dossier de l'exercice.
- Animations : l'export en contient ~2 000 d'adulte (attentes, émotions, gestes, poses, danses) au format
  `.ueanim`, mais **ni marche ni course** (seulement un jogging sur tapis de course). Elles s'importent avec le
  plugin UEFormat (étape « Animations »). Pour marcher dans Kanto, il faudra retargeter nos animations sur le
  squelette inZOI (section 5).
- Le visage détaillé (91 points, squelette de visage MetaHuman), les cheveux (poils « Groom », non exportés en .glb)
  et le maquillage viendront plus tard.

## Ce que le code sait faire maintenant (PR #46)

| Morceau | Où | Ce que ça fait |
|---|---|---|
| Déformation par os | `Character/TrainerAnimInstance` | Comme inZOI : chaque curseur a une liste d'os « + » et une liste « − » (position, rotation, échelle ajoutées à la pose). Les vêtements suivent (Leader Pose). |
| Muscles / Corpulence | `TrainerAppearanceData::ComputeBoneOffsets` | Deux curseurs globaux qui poussent tous les groupes selon leurs coefficients `delta_muscle` / `delta_masse`. Formule : `valeur du groupe + Muscles × coef muscle + Corpulence × coef masse`, bornée à [-1, 1] (approximation, la formule exacte d'inZOI est dans son code compilé). |
| Pièces modulaires | `FTrainerPartDef`, `FTrainerPartSlotDef` | Un Skeletal Mesh par emplacement (Tête, Haut, Veste, Bas, Tenue complète, Chaussettes, Chaussures, Chapeau, Écharpe, Gants, Bijoux et sacs) en Leader Pose sur le corps. Une tenue complète retire le haut et le bas. |
| Éditeur | `UI/CharacterCreatorWidget` | Onglets **Tenue** et **Accessoires** : puces par emplacement, vignettes, « Aucun ». **Visage > Modèles de visage** : les têtes. **Corps** : les 22 groupes d'os + Muscles + Corpulence. |
| Sauvegarde / réseau | `FTrainerAppearance::Outfit` | Les pièces choisies sont des noms (ids inZOI), donc la sauvegarde et le multijoueur marchent déjà. « -Top » = « Aucun haut ». |
| Catalogue importé | `TrainerAppearanceRender::LoadDefaultData` | Prend `/Game/ImportedAssets/InZOI/DA_TrainerAppearance_InZOI` s'il existe, sinon `DA_TrainerAppearance`. |

## Pas à pas

### 1. Récupérer le code

Dans GitHub Desktop (ou VS Code) : branche `claude/project-thread-jyy19p` (PR #46), **Pull**. Fermer Unreal, puis
compiler avec la tâche « PokemonNewAgeEditor Win64 Development Build ».

### 2. Vérifier le chemin du dossier

Ouvrir `Tools/InZOI/import_inzoi.py` dans VS Code. En haut :

```python
SOURCE = r"D:\Dev\SourceArt\CCMenu\Exercice Compilation"
AGES = ["Adult"]            # ajouter "Youth" pour les bases Garçon / Fille
MAX_MESHES_PER_SLOT = 15    # 0 = tout importer
MAX_VARIANTS_PER_MESH = 4   # variantes de couleur gardées par modèle (0 = toutes)
```

Garder **15** pour le premier essai (un quart d'heure environ). Quand tout marche, mettre **0** et relancer : le
script saute ce qui est déjà importé.

### 3. Lancer l'import

**Conseillé (sans carte graphique)** : fermer Unreal, puis double-cliquer sur `Tools/InZOI/import_inzoi.bat`.
L'import tourne dans une fenêtre noire, sans éditeur ni rendu (`-nullrhi`) : plus de plantage « Le GPU a planté ou
le dispositif D3D a été retiré » (RTX 3070 = 8 Go de mémoire vidéo, les textures 4K d'inZOI la saturent dans
l'éditeur). Le .bat cherche Unreal dans D:\Games\UE_5.8 puis C:\Program Files\Epic Games\UE_5.8 ; sinon, corriger la ligne `set UE=` du .bat.

Autre façon, dans l'éditeur : **Outils > Exécuter un script Python** (Tools > Execute Python Script) > choisir
`Tools/InZOI/import_inzoi.py`. Une barre de progression s'affiche (bouton Annuler possible). À la fin, le
**Journal de sortie** (Output Log, filtre `InZOI`) donne le bilan : textures, modèles, matériaux, pièces, bases, et
les noms des emplacements de matériaux des corps et des têtes.

Résultat dans le Content Browser : `ImportedAssets/InZOI/` avec `DA_TrainerAppearance_InZOI`, le matériau
`Materials/M_InZOI_Part` et les dossiers `Characters/...` et `UI/...` rangés comme dans le jeu d'origine.

### 4. Tester

Lancer le jeu, puis dans la console (touche ²) : `EditeurPerso`. On doit voir :

- la base **Homme** / **Femme** (flèches ‹ › du Profil) avec une tête et une tenue de départ ;
- **Corps** : bouger *Largeur des épaules*, *Ventre*, *Muscles*, *Corpulence* → la silhouette change, les
  vêtements suivent ;
- **Tenue** / **Accessoires** : cliquer une vignette → le vêtement change ; « Aucun » l'enlève ;
- **Visage > Modèles de visage** : changer de tête ;
- **Peau** : la teinte s'applique au corps et à la tête.

Ce qui peut coincer, et quoi m'envoyer :

| Symptôme | Cause probable | À m'envoyer |
|---|---|---|
| « Import raté » dans le journal | L'import glTF (Interchange) n'a pas créé de Skeletal Mesh | Les lignes rouges du journal |
| Personnage 100 fois trop petit ou trop grand | Unités du .glb | Une capture |
| Les curseurs ne déforment rien | Noms d'os différents, ou l'animation du mesh n'est pas `TrainerAnimInstance` | Capture de l'onglet Squelette d'un corps importé |
| Vêtement qui ne suit pas le corps | Squelette du vêtement trop différent | Le nom du vêtement |
| Tout est gris / blanc | Textures absentes de l'export ou mal reconnues | Le bilan « fichiers absents » du journal |
| Déformation trop forte | Les valeurs d'inZOI sont pensées pour sa formule | Dans `DA_TrainerAppearance_InZOI`, baisser **Bone Deformation Scale** (0,5 par exemple) |

### Animations (attente du studio, gestes, émotions)

1. Télécharger le plugin **UEFormat** : <https://github.com/h4lfheart/UEFormat> > bouton vert **Code** >
   **Download ZIP**. Dans le zip, copier le dossier `plugins/unreal/UEFormat` dans le dossier `Plugins/` du projet
   (créer `Plugins/` s'il n'existe pas) : on doit avoir `D:\Dev\pokemonnewage\Plugins\UEFormat\UEFormat.uplugin`.
   Ce dossier est exclu de Git (le plugin n'a pas de licence qui permette de le mettre dans notre dépôt).
2. Supprimer `Plugins/UEFormat/Binaries` (compilé pour une autre version d'Unreal), fermer Unreal, recompiler
   (« PokemonNewAgeEditor Win64 Development Build ») : le plugin se compile avec le projet.
3. Unreal : **Outils > Exécuter un script Python** > `Tools/InZOI/import_inzoi_anims.py`. Pour chaque base, un
   message indique quel squelette choisir ; la fenêtre « Animation Import Options » s'ouvre : choisir ce squelette,
   puis **Import All**.
4. Résultat : `ImportedAssets/InZOI/Anims/Homme/...` et `.../Femme/...`, et l'attente `ANIM_F_NPC_Idle1` jouée en
   boucle dans le studio de l'éditeur. La liste des animations est en haut du script (`ANIMATIONS`), on peut en
   ajouter (noms dans `Index_Complet/Manifeste_Assets.csv`).

Si le plugin ne compile pas avec Unreal 5.8, m'envoyer les erreurs : on passera par Blender (extension UEFormat
pour Blender, puis export FBX).

### 5. Animations en jeu (plus tard)

Pour que le personnage inZOI marche dans Kanto :

1. Créer un **IK Rig** pour le squelette inZOI (`SK_Base` / squelette du corps importé) et un **IK Retargeter** depuis
   le squelette de nos animations actuelles.
2. Clic droit sur notre Animation Blueprint > **Retarget Animation Assets** (Dupliquer et retargeter) vers le corps
   inZOI.
3. Ouvrir l'Animation Blueprint obtenu > **Paramètres de classe** > **Classe parente** = `TrainerAnimInstance`
   (sinon les curseurs « par os » ne s'appliquent pas).
4. Dans `DA_TrainerAppearance_InZOI` > Bases > **Anim Class** = cet Animation Blueprint.

## Menu façon inZOI (2026-11-07)

- **Colonne de gauche** : hors onglet Modèles, une vignette par pièce portée (tête, haut, bas, chaussures…) ; un clic
  ouvre l'onglet et l'emplacement de la pièce. Le Profil (prénom, base, phrase d'accroche) reste dans l'onglet Modèles.
- **Panneau de droite** en trois colonnes : emplacements (Haut, Veste, Bas…), catégories (T-shirts, Sweats… avec
  « Tout »), grille de vignettes. Le personnage est décalé vers la gauche de l'écran (`ScreenShift` dans
  `CharacterStudio.cpp`).
- **Nuancier** sous la grille pour la pièce portée : rangée de couleurs toutes faites, teintes claires, foncées, gris,
  code hexadécimal, Réinitialiser. Zone « Principale » = paramètre `Tint` du matériau `M_InZOI_Part` ; Zone 1 à 3 =
  `Color1` à `Color3` quand le matériau de la pièce a un masque de couleur. Sauvegardé dans
  `FTrainerAppearance::PartColors`.
- **Traits du visage** (point du visage « Traits du visage », ou mode Détaillé) : 140 curseurs créés par le script
  d'après `Morphologie_Visage_Os.csv` (côtés gauche et droit réunis ; X = écart, Y = avancée, Z = hauteur, d'après le
  squelette de la tête). La tête n'est plus en Leader Pose : elle copie la pose du corps (`UTrainerAnimInstance` en
  mode « suit le corps ») et ses os du visage reçoivent les curseurs.
- **Studio** : éclairage « Doux » par défaut, murs plus clairs.

## Ce qui reste à faire

1. Tester dans Unreal le nouveau menu, les traits du visage et le nuancier (L'amie).
2. Cheveux et barbes : les barbes existent en `.glb` (`Groom/Beard/.../SKM_Beard_00X_For_Male_Head_00Y.glb`) ; les
   cheveux n'ont que des textures (`*_Scalp_DIRO.png`, cartes de mèches), pas de mesh.
3. Animations de marche et de course retargetées (section 5) ; émotes et poses de la carte dresseur avec les animations importées.
4. Taches de rousseur, grains de beauté, cicatrices, maquillage : chercher leurs textures dans l'export.
5. Le style verre de L'amie sur l'éditeur, avec les icônes importées (`ImportedAssets/InZOI/UI/...`).
