# Données Pokémon (SCRUM-27, 28, 29)

Toutes les données du jeu vivent dans des Data Tables, jamais en dur dans le code. Ce document explique d'où elles viennent, ce qu'elles contiennent et comment les mettre dans Unreal.

## En bref

| Data Table | Contenu | Lignes |
|---|---|---|
| `DT_Species` | Les 151 Pokémon : nom FR, types, stats de base, EV donnés, talents, taux de capture, XP, courbe d'XP, sexe, taille, poids, fiche Pokédex FR, évolutions, `bAvailableInV1` | 151 |
| `DT_TypeChart` | Table des 18 types (ligne = type qui attaque, colonne = type qui défend) | 18 |
| `DT_Moves` | Attaques des générations 1 à 3 : nom et description FR, type, catégorie, puissance, précision, PP, priorité, cible, contact, effets (statut, changements de stats, coups multiples, drain, soin, critique, peur) | 354 |
| `DT_Learnsets` | Attaques apprises par niveau et par CT/CS, pour chaque espèce (Rouge Feu / Vert Feuille) | 151 |
| `DT_Machines` | CT01 à CT50 et CS01 à CS07 de Rouge Feu / Vert Feuille | 57 |
| `DT_Abilities` | Talents de la 3G (nom et description FR) | 76 |
| `DT_Natures` | 25 natures (stat +10 % / -10 %) | 25 |
| `DT_Experience` | XP totale par niveau pour les 6 courbes | 100 |
| `DT_Encounters` | Rencontres sauvages de Rouge Feu, tout Kanto et îles Sevii, par lieu et méthode (herbes, cannes, surf, Éclate-Roc) | 295 |
| `DT_Trainers` | Dresseurs de la V1 : rival (labo, Route 22), 5 Scouts de la Forêt de Jade, Campeur Liam, Pierre | 13 |
| `DT_Items` | Objets de la V1 : soins, Poké Balls, objets rares (généré par `Tools/Data/build_items.py`, voir `docs/COMBAT.md`) | 20 |

**Pokémon jouables en V1** (20, colonne `bAvailableInV1`) : les 3 starters et leur première évolution, Roucool, Rattata, Piafabec, Férosinge, Chenipan, Chrysacier, Papilusion, Aspicot, Coconfort, Dardargnan, Pikachu, Racaillou, Sabelette, Onix.

## Choix faits

- **Nouvelle génération pour les règles, Rouge Feu pour le contenu.** Les types et les stats sont ceux d'aujourd'hui (18 types avec Fée : Mélofée est Fée, Magnéti est Électrik/Acier). Chaque attaque a sa propre catégorie physique/spéciale, comme depuis Diamant/Perle. Les apprentissages, CT, rencontres et dresseurs sont ceux de Rouge Feu / Vert Feuille.
- **Valeurs actuelles des attaques** (puissance, précision) : quelques-unes diffèrent de Rouge Feu, c'est voulu.
- **Évolutions** : seules celles qui restent dans les 151 sont gardées (pas de Nostenfer, Steelix…). Les déclencheurs sont `Level`, `Item` (pierres) et `Trade`.
- **Rencontres** : version Rouge Feu. Chaque table garde les 12 cases du jeu d'origine avec leurs niveaux exacts ; les `Rate` font 100 au total. Les starters ne sont pas dans les rencontres : on les donnera par un événement au labo.
- **Dresseurs** : équipes de Rouge Feu / Vert Feuille, saisies à la main dans `build_pokemon_data.py`. Les noms français des dresseurs (Rick, Doug…) et l'argent gagné sont à vérifier. Le rival a une ligne par starter du joueur (`RequiredPlayerStarter`) : il prend toujours celui qui a l'avantage. Pierre donne le Badge Roche et la CT39 (Tomberoche).
- **Ectoplasma** garde Lévitation (son talent de la 3G).

## Fichiers

- `Tools/Data/build_pokemon_data.py` : génère `Data/Pokemon/DT_*.json` depuis les CSV de [PokeAPI](https://github.com/PokeAPI/pokeapi) (téléchargés une fois dans `Tools/Data/.cache/`, hors Git). Pour changer un dresseur ou un choix, modifier ce script et le relancer : `python Tools/Data/build_pokemon_data.py`.
- `Data/Pokemon/DT_*.json` : les données, lisibles et versionnées dans Git.
- `Tools/Data/import_data_tables.py` : script Python d'éditeur qui crée les Data Tables dans `Content/Pokemon/Data/` et `DA_PokemonDatabase`.
- `Source/PokemonNewAge/Pokemon/` :
  - `PokemonTypes.h` : énumérations (`EPokemonType`, `EPokemonStat`, `EGrowthRate`, `EMoveCategory`…) et `FPokemonBaseStats`.
  - `PokemonDataRows.h` : une struct par Data Table (`FSpeciesRow`, `FMoveRow`, `FTypeChartRow`…).
  - `PokemonDatabase.h/.cpp` : `UPokemonDatabase` (l'asset `DA_PokemonDatabase`), qui regroupe les tables et donne les recherches courantes.
  - `PokemonDataSubsystem.h/.cpp` : charge `DA_PokemonDatabase` au lancement.

## Mettre les données dans Unreal

1. Fermer Unreal, compiler dans VS Code (tâche « PokemonNewAgeEditor Win64 Development Build »), rouvrir le projet.
2. Dans Unreal : **Outils > Exécuter un script Python** (*Tools > Execute Python Script*), choisir `Tools/Data/import_data_tables.py`.
3. Le **Journal de sortie** (*Output Log*) doit afficher `DT_Species : 151 lignes importées`, etc., puis `DA_PokemonDatabase à jour.`
4. Ouvrir `Content/Pokemon/Data/DT_Species` pour vérifier : une ligne par Pokémon, noms en français.
5. Committer les `.uasset` de `Content/Pokemon/Data/` (Git LFS).

Pour mettre à jour : modifier puis relancer `build_pokemon_data.py`, puis relancer `import_data_tables.py` dans Unreal. Les tables sont re-remplies, les références restent valides.

## Utiliser les données

En Blueprint : nœud **Get Pokemon Database**, puis par exemple :

- `Get Species` (`Bulbasaur`) : fiche complète de l'espèce.
- `Get Move` (`Tackle`) : fiche de l'attaque.
- `Get Type Effectiveness` (Feu contre Plante/Poison) : 2.
- `Get Experience For Level` / `Get Level For Experience`.
- `Get Default Moves` (`Pidgey`, 5) : les 4 dernières attaques apprises, pour un Pokémon sauvage ou de dresseur.
- `Roll Encounter` (`Route1_Walk`) : espèce et niveau d'une rencontre dans les hautes herbes.

En C++ : `UPokemonDataSubsystem::GetDatabase(this)->FindSpecies(TEXT("Bulbasaur"))`.

## Multijoueur (SCRUM-68)

Les Data Tables sont en lecture seule et identiques sur toutes les machines. Les données vivantes (Pokémon du joueur, équipe, combat) ne garderont que des identifiants (`FName` comme `Bulbasaur`, `Tackle`) et des nombres (niveau, PV, IV/EV) : c'est ce qui passe par le réseau, chaque machine relit le reste dans ses tables. L'équipe du joueur ira dans le `PlayerState`. `Roll Encounter` prend un `Random Stream` pour que le serveur fasse le tirage et l'envoie aux clients.

## Identifiants

Les noms de ligne sont les noms anglais officiels en `PascalCase` (`NidoranF`, `MrMime`, `Farfetchd`, `ThunderShock`). Les lieux suivent `Content/World/` (`PalletTown`, `Route1`, `ViridianForest`). On ne renomme jamais un identifiant : les sauvegardes s'y réfèrent.
