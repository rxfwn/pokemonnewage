# Conventions de nommage

Ces règles valent pour tout le projet Unreal. Elles reprennent les conventions les plus courantes de la communauté UE5, pour que les tutoriels et les assets du Marketplace s'intègrent sans surprise.

## Règles générales

- **Noms en anglais**, en `PascalCase`, sans espace, sans accent, sans caractère spécial (`é`, `ç`, `'`, `-`). Unreal et Git gèrent mal les accents dans les chemins.
- Forme d'un asset : `Préfixe_NomDeBase_Variante_Suffixe`. Exemple : `SK_Pikachu`, `T_Pikachu_D`, `MI_Pikachu_Shiny`.
- Le nom de base désigne la chose, pas son usage : `Pikachu`, `PalletTown`, `Pokeball`.
- Les numéros se notent sur deux chiffres : `SM_Tree_01`, `SM_Tree_02`.
- Les noms de Pokémon et de lieux utilisent le **nom anglais officiel** (`Bulbasaur`, `PalletTown`, `ViridianForest`). Le nom français s'affiche en jeu via les traductions, jamais dans le nom d'un asset.
- Un dossier par Pokémon et par lieu : `Content/Pokemon/Pikachu/`, `Content/World/PalletTown/`.

## Préfixes des assets

| Type d'asset | Préfixe | Exemple |
|---|---|---|
| Blueprint (acteur) | `BP_` | `BP_TallGrass` |
| Blueprint Interface | `BPI_` | `BPI_Interactable` |
| Blueprint Function Library | `BPFL_` | `BPFL_BattleMath` |
| Widget (UI) | `WBP_` | `WBP_BattleHUD` |
| Animation Blueprint | `ABP_` | `ABP_Pikachu` |
| Niveau (map) | `L_` | `L_PalletTown` |
| Static Mesh | `SM_` | `SM_PokemonCenter` |
| Skeletal Mesh | `SK_` | `SK_Bulbasaur` |
| Squelette | `SKEL_` | `SKEL_Bulbasaur` |
| Physics Asset | `PHYS_` | `PHYS_Bulbasaur` |
| Animation Sequence | `A_` | `A_Pikachu_Idle` |
| Animation Montage | `AM_` | `AM_Pikachu_Attack` |
| Blend Space | `BS_` | `BS_Player_Locomotion` |
| Matériau | `M_` | `M_Grass` |
| Instance de matériau | `MI_` | `MI_Grass_Dry` |
| Fonction de matériau | `MF_` | `MF_Wind` |
| Texture | `T_` | `T_Pikachu_D` |
| Système Niagara (effet) | `NS_` | `NS_Thunderbolt` |
| Son (Sound Wave) | `SW_` | `SW_Pikachu_Cry` |
| Sound Cue / MetaSound | `SC_` / `MS_` | `SC_Footstep_Grass` |
| Musique | `MUS_` | `MUS_Route1` |
| Data Table | `DT_` | `DT_Species` |
| Data Asset | `DA_` | `DA_Move_Thunderbolt` |
| Structure (Blueprint) | `F_` | `F_EncounterSlot` |
| Énumération (Blueprint) | `E_` | `E_PokemonType` |
| Courbe | `C_` | `C_ExpGrowth_MediumFast` |
| Input Action / Mapping Context | `IA_` / `IMC_` | `IA_Interact`, `IMC_Exploration` |

### Suffixes des textures

| Suffixe | Contenu |
|---|---|
| `_D` | Couleur de base (diffuse / albedo) |
| `_N` | Normal map |
| `_ORM` | Occlusion, Roughness, Metallic regroupés |
| `_E` | Émissive |
| `_M` | Masque |

## Code C++

On suit le standard de code d'Epic.

- Préfixes de classes imposés par Unreal : `A` pour un acteur (`ABattleArena`), `U` pour un UObject ou composant (`UPokemonInstance`, `UPartyComponent`), `F` pour une struct (`FSpeciesRow`), `E` pour un enum (`EPokemonType`), `I` pour une interface (`IInteractable`).
- Booléens préfixés par `b` : `bIsFainted`, `bCanEvolve`.
- Fichiers : un fichier par classe, nommé sans le préfixe (`PokemonInstance.h` pour `UPokemonInstance`).
- Module principal : `PokemonNewAge`. Les sous-dossiers de `Source/PokemonNewAge/` suivent ceux de `Content/` : `Core/`, `Pokemon/`, `Battle/`, `World/`, `UI/`.
- Commentaires en français, noms de code en anglais.

## Données

- Une ligne de Data Table = un identifiant stable en anglais, en `PascalCase` : `Bulbasaur`, `Thunderbolt`, `Route1`.
- Ne jamais renommer un identifiant déjà utilisé dans une sauvegarde : on ajoute, on ne renomme pas.

## Git

- Branches : `feature/<sujet>`, `fix/<sujet>`, ou la branche fournie par Claude. Exemple : `feature/tall-grass-encounters`.
- Messages de commit en français, à l'impératif, avec la clé Jira quand il y en a une : `SCRUM-17 Créer le projet UE5`.
- Les fichiers `.uasset` et `.umap` sont verrouillables (`git lfs lock`) : verrouiller une map avant de la modifier si quelqu'un d'autre travaille dessus.
