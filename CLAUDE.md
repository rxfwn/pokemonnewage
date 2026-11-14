# CLAUDE.md

Jeu Pokémon fan-made en 3D réaliste (Kanto en monde ouvert), **Unreal Engine 5.8.4**, C++ + Blueprints + scripts Python d'éditeur. Projet privé, jamais publié. Code écrit dans VS Code, compilé par le compilateur de Visual Studio. Le jeu tourne sur le PC Windows de L'amie (`D:\Dev\pokemonnewage`) ; une session Claude dans le cloud n'a **pas** Unreal.

## Règles
- Répondre et écrire la doc en français. Unreal est en français chez L'amie : donner les menus en français, l'anglais entre parenthèses (ex. **Outils > Exécuter un script Python** (*Tools > Execute Python Script*)).
- Noms de code et d'assets en anglais, commentaires en français. Conventions complètes : `docs/CONVENTIONS.md`.
- C++ pour les systèmes (combat, stats, Pokédex, sauvegarde) ; Blueprints pour le contenu et le prototypage.
- Les données (espèces, attaques, types, rencontres, dialogues) vivent dans des Data Tables, pas en dur dans le code.
- Commits en français, à l'impératif, avec la clé Jira : `SCRUM-23 Poser les maisons de Bourg Palette`. Jira : projet SCRUM ; mettre à jour le statut du ticket à chaque fin d'étape. `docs/BACKLOG.md` en est un miroir (peut être en retard).
- Ne jamais committer `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`.

## Où est quoi
| Dossier | Contenu |
|---|---|
| `Source/PokemonNewAge/` | Module C++ unique. Sous-dossiers : `Battle/`, `Capture/`, `Core/` (sauvegarde, horloge, client du serveur de saves), `Dialogue/`, `Pokemon/` (données, instances, progression), `Progression/` (évolution 3D), `Town/` (Centre, boutique), `UI/` (HUD, Pokédex, combat), `World/` (portes, intérieurs, hautes herbes, barrières d'histoire), `Tests/`. Le PlayerController porte les commandes de console (`UFUNCTION(Exec)`). |
| `Source/PokemonNewAge/Variant_*` | Restes du gabarit Third Person d'Epic, pas notre jeu : ne pas y toucher. |
| `Data/Pokemon/*.json`, `Data/Dialogues/*.json` | Sources des Data Tables, produites par `Tools/Data/build_*.py`, importées dans Unreal par `Tools/Data/import_data_tables.py`. |
| `Tools/<Thème>/` | Scripts Python. `make_*`/`build_*`/`prepare_*` tournent hors Unreal (Python normal) ; `import_*_unreal.py`, `blockout_*.py`, `place_*.py` tournent DANS l'éditeur. |
| `Tools/Blockout/` | Monde ouvert : plan `kanto_v1_layout.py` (x = nord, mètres), `make_heightmap.py`, `blockout_kanto_v1.py`. |
| `Tools/BattleSim/` | Compile la logique de combat sans Unreal (faux moteur dans `inc/`). |
| `Tools/SaveServer/` | Serveur de saves du Raspberry Pi (Python sans dépendance, Tailscale, port 8787). |
| `Content/Python/init_unreal.py` | Lancé à chaque ouverture de l'éditeur : importe Pokédex, polices, verre, sons si leurs sources ont changé. |
| `Content/World/Kanto/L_Kanto.umap` | La map du jeu (World Partition). `Blockout/` est généré, `Art/` est fait à la main. |
| `docs/` | Un guide par système (`COMBAT.md`, `DIALOGUES.md`, `MONDE_OUVERT_V1.md`, `MENU_POKEDEX.md`, `MULTIJOUEUR.md`, `SERVEUR_SAVES.md`…). Lire le guide du système avant de le modifier. |

## Vérifier le code depuis le cloud
- `./Tools/BattleSim/run.sh` : tests de combat et de progression (`Source/PokemonNewAge/Tests/`). `./Tools/BattleSim/run.sh fuzz` : milliers de combats avec les vraies données. Le lancer après toute modification de `Pokemon/`, `Battle/`, `Capture/PokemonStorage` ou `Town/ShopRules`.
- Le reste du C++ ne se compile que chez L'amie : relire les includes, `.generated.h` en dernier, et donner les étapes de test dans Unreal.

## Pièges connus
- **Compiler** : fermer Unreal d'abord (sinon Live Coding bloque), puis dans VS Code la tâche **PokemonNewAgeEditor Win64 Development Build**. Les tâches Debug échouent avec l'Unreal du Launcher. Après un nouveau `.cpp`/`.h` : **Outils > Actualiser le projet Visual Studio Code**.
- **`bUseUnity = false`** dans `PokemonNewAge.Build.cs` : à garder (sinon conflits de noms entre namespaces anonymes, C4459).
- **Git LFS** : `.uasset`, `.umap`, images, FBX, sons passent par LFS (verrouillables). Le push LFS **depuis le cloud est refusé (403)** : Claude ne peut pas pousser d'assets binaires. Les petits fichiers générés de `Tools/` (PNG, FBX du Pokédex et des Balls) sont exclus de LFS dans `.gitattributes` ; les gros fichiers passent par `/mnt/project-files/` et L'amie les committe depuis son PC.
- **Blockout** : `blockout_kanto_v1.py` efface le dossier `Kanto_Blockout` de l'Outliner à chaque lancement et réassigne `M_Kanto_Sol` et le soleil. Le travail manuel va ailleurs (ex. `Kanto_Art`).
- **Heightmap** : `make_heightmap.py` prend ~11 min (numpy + numba). Le Landscape World Partition ne se supprime pas : on le met à jour par **Paysage > Gérer > Importer** (mode All), puis on revérifie l'**Échelle Z** dans Détails. Voir `docs/MONDE_OUVERT_V1.md`.
- **Vue noire dans L_Kanto** : charger le terrain (**World Partition > Charger la région depuis la sélection**).
- **`init_unreal.py`** ne doit rien faire dans le jeu lancé à part (**Jeu autonome** / Standalone, `-game`) et ne charge aucun asset au démarrage quand rien n'a changé (crash `EXCEPTION_ACCESS_VIOLATION` sinon) : il compare des empreintes dans `Saved/StartupImports.json`.
- **Console** : la console d'Unreal coupe tout ce qui suit `//`, donc pas de `http://` dans une commande.
- **Échap** arrête le jeu dans l'éditeur (PIE) : les Paramètres sont provisoirement sur M.
- Clavier **AZERTY** : déplacements en ZQSD.
- Après un changement de C++ ou de Data Table, donner à L'amie les étapes : fermer Unreal, compiler, rouvrir, relancer le script d'import, quel message attendre dans le **Journal de sortie** (*Output Log*).
