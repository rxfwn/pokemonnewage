# Créer son personnage (SCRUM-66 / 79)

Le joueur crée son propre dresseur, réaliste, façon Cyberpunk. On le fait avec **Character Creator** (Reallusion) et on l'importe dans Unreal. Ce personnage remplacera le Dresseur provisoire de Smash (SCRUM-65, [PERSONNAGE_DRESSEUR.md](PERSONNAGE_DRESSEUR.md)).

## Ce qu'on a décidé (3 novembre 2026)

| Sujet | Choix |
|---|---|
| Quand | Juste après le discours du Prof. Chen, à la place de « es-tu un garçon ou une fille ? » |
| Liberté | Le plus loin possible, pour que chacun fasse un personnage qui lui ressemble : des **modèles par zone** comme point de départ, puis **beaucoup de curseurs libres**, chacun avec des limites pour qu'on ne puisse pas casser le visage |
| Corps | **2 bases séparées** (homme, femme), chacune avec des curseurs de taille, de muscles, de gras… |
| Changer plus tard | **Tout** est modifiable, visage compris |
| Où | **L'armoire** de la chambre (vêtements) et un menu **Apparence** dans le Pokédex 3D (tout) |
| Vêtements | Pas tous disponibles au départ : **boutiques de vêtements** dans presque chaque ville |
| V1 | Couleur de peau et couleur des yeux. Maquillage, tatouages, piercings, taches de rousseur… viendront après |
| Multijoueur | Les autres joueurs voient ton apparence (elle est dans le PlayerState) |

## Est-ce compliqué d'ajouter des choses après ?

Non, c'est prévu pour :

- **Un nouveau curseur** : tu ajoutes le morph dans CC, tu réexportes la base, puis tu ajoutes une ligne dans `DA_TrainerAppearance`. Pas de code. Les anciennes sauvegardes restent valables (le nouveau curseur vaut 0).
- **Un nouveau vêtement** : un import et une fiche. Pas de code, une fois l'armoire faite.
- **Un nouveau type de détail** (maquillage, tatouages…) : un peu de code une seule fois par type. Les sauvegardes ne gardent que des noms et des nombres, donc rien ne casse.

## Apprendre Character Creator

À regarder dans cet ordre :

1. Les bases (interface, curseurs, vêtements) : [tutoriels officiels](https://www.reallusion.com/character-creator/tutorial.html), commencer par « Getting Started ».
2. CC5 vers Unreal, en français : [tutoriel de la communauté Unreal](https://forums.unrealengine.com/t/community-tutorial-workflow-de-character-creator-5-vers-unreal-engine/2820894).
3. Le guide officiel en deux parties : [1. exporter depuis CC5](https://discussions.reallusion.com/t/ue-new-workflow-1-how-to-export-characters-from-cc5-for-seamless-use-in-unreal/14883), [2. importer avec Auto Setup](https://discussions.reallusion.com/t/ue-new-workflow-2-how-to-properly-import-cc-characters-into-ue5-using-auto-setup/14885).
4. Le plugin gratuit pour Unreal : [CC Auto Setup](https://www.reallusion.com/auto-setup/unreal-engine/download.html).

**ActorMIXER** (plugin de CC) sert à mélanger des visages et des corps existants. Il est parfait pour fabriquer les **modèles de départ** de l'éditeur (nez 1, nez 2…). Il ne sert que dans CC, pas en jeu.

## Comment ça marche dans le jeu

Le point important : **les curseurs de CC n'existent que dans CC.** En jeu, un curseur fait bouger un **morph target** (une déformation enregistrée dans le modèle). Il faut donc que les morphs voulus soient **dans le FBX exporté**.

- Un curseur peut piloter 2 morphs : un vers la droite (nez plus large), un vers la gauche (nez plus fin). Il va de -1 à 1.
- Le même morph est appliqué au corps et aux vêtements qui suivent le corps (sinon le ventre traverserait le t-shirt). CC exporte les vêtements avec les morphs du corps.
- La couleur de peau et celle des yeux sont des paramètres de couleur sur les matériaux.

> **À vérifier pendant l'essai** : certains curseurs de CC déplacent des **os** au lieu de déformer le mesh, surtout la taille et la longueur des membres. Ceux-là ne s'exportent pas tels quels en morph. Pour la taille, on pourra mettre une échelle sur le personnage. On regardera curseur par curseur.

Code : `Source/PokemonNewAge/Character/`

- `TrainerAppearance.h` : ce que le joueur a choisi (base, curseurs, couleurs, vêtements). Sauvegardé et envoyé aux autres joueurs.
- `TrainerAppearanceData.h` : le catalogue `DA_TrainerAppearance` (bases, curseurs, noms des matériaux).
- `TrainerAppearanceComponent.h` : sur le personnage, il l'habille et vérifie les valeurs côté serveur.

## Premier essai, pas à pas

But : une base CC qui marche dans `L_Kanto` à la place du Dresseur, avec 3 curseurs et la couleur de peau. Ça valide tout le chemin avant de faire le vrai éditeur.

### 1. Dans Character Creator

1. Partir d'une base (homme ou femme) et la modifier un peu.
2. Lui mettre une tenue simple fournie avec CC (on décidera plus tard qui fait les vrais vêtements).
3. Choisir 3 curseurs du visage à tester, par exemple largeur du nez, largeur de la mâchoire, taille des yeux. Noter leur **nom exact** dans CC.
4. **Fichier > Exporter > FBX (personnage habillé)**, cible **Unreal**, avec le préréglage du **mannequin UE5** (voir le guide officiel, partie 1).
5. Dans les options d'export, inclure les **morphs** des 3 curseurs choisis. L'intitulé exact change selon la version de CC : prends une capture de la fenêtre d'export si tu ne trouves pas, et on regardera ensemble.

### 2. Dans Unreal

1. Installer le plugin **CC Auto Setup** (lien plus haut), puis redémarrer Unreal.
2. Récupérer cette branche et **recompiler** (Unreal fermé, tâche « PokemonNewAgeEditor Win64 Development Build » dans VS Code).
3. Importer le FBX avec Auto Setup dans `Content/Characters/Player/` (guide officiel, partie 2).
4. Ouvrir le Skeletal Mesh, onglet **Aperçu des morph targets** (Morph Target Preview) : les 3 morphs doivent y être. Noter leurs noms.
5. Retarget des animations : comme pour le Dresseur. Avec le préréglage du mannequin UE5, l'**IK Retargeter** doit reconnaître le squelette tout seul. Créer `ABP_Player` à partir de `ABP_Unarmed`.

### 3. Le catalogue

Clic droit dans `Content/Characters/Player/` > **Divers > Data Asset** > `TrainerAppearanceData`, à nommer **`DA_TrainerAppearance`** (le jeu le cherche à ce nom-là).

- **Bases** : 1 ligne. Id `Femme` ou `Homme`, Mesh = le Skeletal Mesh importé, Anim Class = `ABP_Player`.
- **Common Sliders** : 3 lignes. Par exemple Id `NezLargeur`, Category `Nez`, Morph Positive = le nom du morph, Limit 1.
- **Skin Material Slots** : les emplacements de matériau de la peau (souvent `Std_Skin_Head`, `Std_Skin_Body`, `Std_Skin_Arm`, `Std_Skin_Leg` : regarder dans le Skeletal Mesh).
- **Skin Color Parameter** : le nom d'un paramètre de couleur du matériau de peau d'Auto Setup qui teinte la peau. Ouvrir l'instance de matériau de la tête pour le trouver. S'il n'y en a pas, on en ajoutera un.
- **Eye Material Slots** et **Eye Color Parameter** : pareil pour l'iris.

### 4. Tester

Lancer `L_Kanto`, ouvrir la console (touche **²**) :

| Commande | Effet |
|---|---|
| `Apparence` | Liste les bases et les curseurs avec leurs valeurs |
| `ApparenceBase Femme` | Change de base |
| `Curseur NezLargeur 0.8` | Bouge un curseur (-1 à 1, ou 0 à 1 s'il n'a qu'un morph) |
| `CouleurPeau 0.8 0.6 0.5` | Teinte de peau (0 à 1, blanc = peau d'origine) |
| `CouleurYeux 0.1 0.4 0.2` | Couleur de l'iris |

L'apparence est enregistrée avec la partie (`Sauvegarder`).

Si quelque chose ne bouge pas, envoie une capture de l'onglet Morph Target Preview et du Data Asset.

## Ensuite (après l'essai)

1. Les 2 bases complètes et la liste de tous les curseurs (visage par zone et corps), avec les modèles de départ faits avec ActorMIXER.
2. L'éditeur après le discours du Prof. Chen : caméra sur le visage, onglets par zone, aperçu en direct. On attend tes maquettes Figma pour le dessin.
3. Le menu **Apparence** du Pokédex 3D et l'**armoire** de la chambre.
4. Les vêtements : catalogue, emplacements (haut, bas, chaussures, casquette…), boutiques de vêtements dans les villes, vêtements possédés dans la sauvegarde.
5. Plus tard : maquillage, tatouages, piercings (points d'accroche sur le visage), taches de rousseur.
