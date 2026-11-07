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
- Pas d'animations d'adulte dans l'export : le personnage est en pose de référence dans le studio. Pour marcher en
  jeu, il faudra retargeter nos animations sur le squelette inZOI (étape 4).
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

Unreal ouvert sur le projet : **Outils > Exécuter un script Python** (Tools > Execute Python Script) > choisir
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

### 5. Animations en jeu (plus tard)

Pour que le personnage inZOI marche dans Kanto :

1. Créer un **IK Rig** pour le squelette inZOI (`SK_Base` / squelette du corps importé) et un **IK Retargeter** depuis
   le squelette de nos animations actuelles.
2. Clic droit sur notre Animation Blueprint > **Retarget Animation Assets** (Dupliquer et retargeter) vers le corps
   inZOI.
3. Ouvrir l'Animation Blueprint obtenu > **Paramètres de classe** > **Classe parente** = `TrainerAnimInstance`
   (sinon les curseurs « par os » ne s'appliquent pas).
4. Dans `DA_TrainerAppearance_InZOI` > Bases > **Anim Class** = cet Animation Blueprint.

## Ce qui reste à faire

1. Premier import et premier test dans Unreal (L'amie), retours sur le journal.
2. Corriger l'import selon les retours (matériaux, échelle, squelettes).
3. Visage détaillé : curseurs « par os » du visage d'après `Morphologie_Visage_Os.csv` (la tête a son propre
   squelette : il faudra une animation pour la tête qui copie la pose du corps).
4. Animations retargetées (section 5).
5. Cheveux : trouver des coiffures en mesh (l'export n'a que les textures des cheveux Groom).
6. Le style verre de L'amie sur l'éditeur, avec les icônes importées (`ImportedAssets/InZOI/UI/...`).
