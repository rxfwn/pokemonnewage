# Remplacer le personnage par le Dresseur (SCRUM-65)

Modèle **temporaire** : le Dresseur Pokémon (Red) de Smash Bros Ultimate, ripé par Centrixe (The Models Resource). Leaf (la version fille) est préparée aussi, au cas où. Projet privé, jamais publié.

Guide pas à pas sous Windows, Unreal 5.8. Compte 45 minutes.

## Ce qui est déjà fait

Le modèle d'origine n'est pas utilisable tel quel dans Unreal : toutes les expressions du visage sont empilées (yeux fermés, bouche qui crie…), il mesure 45 cm, il est couché, et ses os ont des noms propres à Smash. Le script [`Tools/Characters/convert_ssbu_trainer.py`](../Tools/Characters/convert_ssbu_trainer.py) corrige tout ça dans Blender, sans interface :

- un seul visage (neutre, yeux ouverts, avec l'iris) ; les Poké Balls en main sont retirées ;
- debout (Z en haut), pieds au sol, **1,65 m** ;
- os renommés comme le mannequin d'Unreal (`root`, `pelvis`, `thigh_l`, `spine_01`, `upperarm_r`, `hand_l`, `head`…), ce qui permet à Unreal de reprendre **seul** les animations de marche, course, saut et repos du modèle Third Person ;
- 4 matériaux (`Body`, `Skin`, `Hair`, `Eye`) et leurs textures déjà prêtes au format Unreal : `_D` couleur, `_N` normal map, `_ORM` (occlusion, rugosité, métal).

Le résultat est dans les fichiers du projet : **`personnage/Dresseur_UE.zip`** (et un aperçu, `personnage/dresseur_apercu.png`). Pas besoin de Blender pour la suite.

> Les `.fbx` et les textures ne vont **pas** dans Git pour l'instant : depuis le cloud, Claude ne peut pas pousser de fichiers LFS. Les `.uasset` créés par l'import, eux, se committent normalement depuis le PC (étape 8).

Pour relancer la conversion (autre modèle, réglage de taille…), avec Blender 4 ou plus :
```
blender --background --python Tools/Characters/convert_ssbu_trainer.py -- "D:\Assets\Pokemon Trainer\ptrainer.fbx" "D:\Assets\Dresseur"
```
La taille se règle avec `TARGET_HEIGHT_M` en haut du script.

## 1. Récupérer les fichiers

1. Télécharger `personnage/Dresseur_UE.zip` depuis les fichiers du projet.
2. Le dézipper **en dehors** du dossier du jeu, par exemple dans `D:\Assets\`. On obtient `D:\Assets\Dresseur\SK_Trainer.fbx` et `D:\Assets\Dresseur\Textures\`.
3. Récupérer la branche du ticket. Unreal fermé, dans un terminal sur `D:\Dev\pokemonnewage` :
   ```
   git fetch origin
   git checkout claude/project-thread-aygw22
   ```
   Pas de C++ dans ce ticket : pas besoin de recompiler.
4. Ouvrir `PokemonNewAge.uproject`.

## 2. Importer le modèle

1. Dans le Content Browser, aller dans `Content/Characters`, clic droit > **Nouveau dossier** : `Trainer`. L'ouvrir.
2. Glisser `D:\Assets\Dresseur\SK_Trainer.fbx` dans le dossier.
3. Dans la fenêtre d'import :
   - **Skeletal Mesh** : coché. **Skeleton** : laisser vide (Unreal crée un nouveau squelette).
   - **Import Animations** : décoché (le modèle n'en a pas).
   - **Materials** : créer les matériaux, **importer les textures** : coché.
   - **Import Uniform Scale** : `1.0`. Rien d'autre à toucher.
4. Cliquer **Importer**. Des avertissements sur les « smoothing groups » ou « bind pose » peuvent s'afficher : sans gravité.
5. Renommer selon les conventions (clic droit > Renommer) :
   - le squelette → `SKEL_Trainer`
   - le Physics Asset → `PHYS_Trainer`
6. Double-cliquer sur `SK_Trainer` : le Dresseur doit être debout, bras à l'horizontale, et mesurer environ 165 cm (grille du viewport). Il regarde dans la même direction que le mannequin.

## 3. Importer les textures `_N` et `_ORM`

L'import du FBX a pris les couleurs (`_D`). Il reste les deux autres.

1. Créer un sous-dossier `Content/Characters/Trainer/Textures` et y glisser **tous** les fichiers de `D:\Assets\Dresseur\Textures\`. Si Unreal propose de remplacer des textures `_D` déjà importées : **Oui à tout**, puis supprimer les doublons qui seraient restés à côté du mesh.
2. Pour chaque `T_Trainer_*_ORM` (double-clic) : **sRGB** décoché, **Compression Settings** = `Masks (no sRGB)`. Enregistrer.
3. Pour chaque `T_Trainer_*_N` : vérifier **Compression Settings** = `Normalmap` (Unreal le met souvent tout seul grâce au `_N`). Enregistrer.

## 4. Faire un matériau maître et ses instances

On fait un seul matériau, réutilisé par les 4 morceaux (et plus tard par les PNJ).

1. Dans `Content/Characters/Trainer`, clic droit > **Material** : `M_Character`. L'ouvrir.
2. Ajouter (clic droit dans le graphe, chercher le nom) :
   - un **TextureSampleParameter2D** nommé `BaseColor` → sortie RGB vers **Base Color** ;
   - un **TextureSampleParameter2D** nommé `Normal`, **Sampler Type** = `Normal` → RGB vers **Normal** ;
   - un **TextureSampleParameter2D** nommé `ORM`, **Sampler Type** = `Masks` → **R** vers **Ambient Occlusion**, **G** vers **Roughness**, **B** vers **Metallic**.
   Donner aux trois une texture par défaut quelconque du dresseur (`T_Trainer_Body_…` correspondante).
3. **Apply**, **Save**.
4. Clic droit sur `M_Character` > **Create Material Instance**, 4 fois : `MI_Trainer_Body`, `MI_Trainer_Skin`, `MI_Trainer_Hair`, `MI_Trainer_Eye`. Dans chacune, cocher les 3 paramètres et mettre les textures du même nom (`T_Trainer_Skin_D`, `_N`, `_ORM` pour Skin, etc.).
   - Pour `MI_Trainer_Eye`, il n'y a qu'une texture `_D` : pour `Normal` et `ORM`, laisser celles de `Skin`, ça ne se voit pas.
5. Ouvrir `SK_Trainer` > panneau **Asset Details** > **Material Slots** : mettre chaque `MI_Trainer_…` sur le slot du même nom. Enregistrer.
6. Les matériaux `M_Trainer_*` créés par l'import ne servent plus : on peut les supprimer (Unreal demande de remplacer les références : choisir les `MI_`).

> Si les reliefs (coutures, plis) semblent éclairés à l'envers : ouvrir la texture `_N` concernée et cocher **Flip Green Channel**.

## 5. Reprendre les animations du mannequin (retargeting)

Unreal sait transférer des animations d'un squelette à un autre. Comme les os du Dresseur portent les noms du mannequin, il génère presque tout seul la correspondance.

1. Aller dans `Content/Characters/Mannequins/Anims/Unarmed`.
2. Clic droit sur **`ABP_Unarmed`** > **Retarget Animations** (« Recibler les animations »).
3. Dans la fenêtre :
   - **Source Skeletal Mesh** : `SKM_Manny_Simple` (normalement déjà rempli) ;
   - **Target Skeletal Mesh** : `SK_Trainer` ;
   - laisser le **Retargeter** en automatique (*Auto Generate Retargeter*).
4. Dans l'aperçu en haut, Manny et le Dresseur bougent côte à côte. Vérifier que le Dresseur marche normalement.
5. Dans la liste, sélectionner `ABP_Unarmed` **et** toutes les animations (`MM_Idle`, `BS_Idle_Walk_Run`, celles de `Walk`, `Jog`, `Jump`…). Cocher l'option d'**export des assets de retargeting**.
6. **Export Animations**, dossier `Content/Characters/Trainer/Anims`. Suffixe proposé : `_Trainer`.
7. On obtient notamment `ABP_Unarmed_Trainer`, les animations retargetées, et un `IK_…`/`RTG_…` générés. Renommer l'AnimBP `ABP_Trainer`.

**Si les bras restent écartés** pendant la marche : c'est la différence de pose de départ (le Dresseur est en T, Manny en A). Ouvrir le `RTG_…` exporté, en haut à gauche passer en mode **Edit Retarget Pose** sur la cible, clic droit sur la racine > **Auto Align All Bones** (ou baisser les deux `upperarm` d'environ 45° à la main), enregistrer, puis refaire les étapes 2 à 6 avec ce retargeter au lieu de l'automatique.

## 6. Mettre le Dresseur dans le personnage joueur

1. Ouvrir `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`.
2. Dans **Components**, cliquer sur **Mesh (CharacterMesh0)**.
3. Dans Détails :
   - **Skeletal Mesh Asset** : `SK_Trainer` ;
   - **Anim Class** : `ABP_Trainer` (le `_C` de la liste) ;
   - **Location** : garder la valeur actuelle (pieds en bas de la capsule), **Rotation** Z : garder `-90`.
4. Dans le viewport du Blueprint, les pieds doivent toucher le bas de la capsule et le Dresseur regarder vers la flèche bleue. Sinon, ajuster **Location Z**.
5. Optionnel : la capsule est faite pour 1,80 m. Pour 1,65 m, dans **Capsule Component**, **Capsule Half Height** = `85`, puis remonter le mesh d'autant (Location Z = valeur actuelle + 11).
6. **Compile**, **Save**.

## 7. Tester

1. Ouvrir `L_Kanto` (ou `Lvl_ThirdPerson`), **Play**.
2. Le Dresseur est au repos, **marche** en avançant, **court** avec Maj, saute avec Espace.
3. La caméra (docs/CAMERA.md) reste derrière lui ; si elle paraît trop haute pour sa taille, baisser **Socket Offset Z** du **Camera Boom**.

## 8. Enregistrer le travail dans Git

Seuls les `.uasset` vont dans Git (par LFS, depuis le PC c'est autorisé) :
```
git add Content/Characters/Trainer Content/ThirdPerson/Blueprints
git commit -m "SCRUM-65 Importer le Dresseur et retargeter les animations"
git push
```
Ensuite, prévenir Claude dans le fil : la PR sera prête à fusionner.

## À savoir

- C'est un **modèle temporaire**, au style de Smash (proportions de jeu, textures 512 à 1024 px). Il ira moins bien avec le décor réaliste que le vrai modèle final.
- Les os en plus (`Bag1`, `Bag2` pour le sac, `HaveL`/`HaveR` pour tenir un objet, `PokeballL`/`PokeballR`) restent immobiles avec les animations du mannequin. `HaveR` servira à placer une Poké Ball dans la main pour le lancer.
- Leaf (`TrainerFemale/SK_TrainerFemale.fbx` dans le zip) se prépare exactement pareil, avec le préfixe `TrainerFemale`. Ses cheveux et sa jupe ont des os (`S_Hair…`, `S_Skirt…`) qu'on pourra animer avec la physique plus tard.
