# CLAUDE.md

Projet : jeu Pokémon fan-made en 3D réaliste, Unreal Engine 5. Éditeur de code : VS Code.

## Règles
- Répondre et écrire la doc en français.
- Le backlog et les sprints sont dans `docs/BACKLOG.md` (miroir des tickets Jira quand Jira est branché).
- Les assets binaires (.uasset, .umap, modèles, textures, sons) passent par Git LFS (voir `.gitattributes`). Ne jamais committer `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`.
- Logique de jeu : C++ pour les systèmes (combat, stats, Pokédex, sauvegarde), Blueprints pour le contenu et le prototypage.
- Conventions de nommage (assets, C++, Git) : `docs/CONVENTIONS.md`.
- Les données (espèces, attaques, types, rencontres) vivent dans des Data Tables / Data Assets, pas en dur dans le code.

## Arborescence prévue (Content/)
- `Content/Core` : GameMode, PlayerController, sauvegarde
- `Content/Pokemon` : données d'espèces, modèles, animations
- `Content/Battle` : système de combat et UI de combat
- `Content/World` : carte de la région, villes, routes
- `Content/UI` : menus, Pokédex, sac, équipe
