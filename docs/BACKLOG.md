# Backlog et sprints

Sprints de **2 semaines**. Région de la V1 : **Kanto**. Ce fichier sert de backlog tant que Jira n'est pas connecté, puis il en restera le miroir. Les identifiants (`EXP-1`, `CMB-3`…) seront repris dans Jira.

## Périmètre de la V1 (MVP)

Une tranche jouable de Kanto, de Bourg Palette jusqu'à la victoire contre Pierre à Argenta :

- Bourg Palette, Route 1, Jadielle, Route 2, Forêt de Jade, Argenta et son arène
- Une quinzaine de Pokémon (les 3 starters et leurs évolutions, Roucool, Rattata, Chenipan, Aspicot, Pikachu, Racaillou, Onix…)
- Combat au tour par tour, capture, Pokédex, expérience, montée de niveau et évolution, sauvegarde

Le reste de la région et les 151 Pokémon viennent après, sprint par sprint.

## Épopées

| Code | Épopée | Contenu |
|------|--------|---------|
| SET | Mise en place | Projet UE5, dépôt, outils, conventions |
| EXP | Exploration | Personnage, caméra, carte, zones, PNJ, transitions |
| DAT | Données Pokémon | Espèces, types, attaques, stats, natures |
| CMB | Combats | Combat au tour par tour, IA, UI de combat |
| CAP | Capture | Rencontres sauvages, Poké Balls, taux de capture |
| DEX | Pokédex | Vu / capturé, fiches, UI |
| CHA | Champions | Dresseurs, arènes, badges |
| LVL | Progression | Expérience, niveaux, apprentissage d'attaques, évolution |
| UI | Menus | Équipe, sac, PC, sauvegarde, options |
| ART | Direction artistique | 3D réaliste, modèles, animations, éclairage |
| AUD | Audio | Musiques, cris, bruitages |

## Sprint 0 : fondations (semaines 1-2)

Objectif : un projet UE5 qui s'ouvre chez tout le monde et un dépôt propre.

- **SET-1** Initialiser le dépôt : README, `.gitignore` UE5, Git LFS, CLAUDE.md
- **SET-2** Créer le projet UE5 (C++ + Blueprints, modèle Third Person) et le committer
- **SET-3** Configurer VS Code pour Unreal (extension C++, génération des fichiers projet)
- **SET-4** Définir l'arborescence `Content/` et les conventions de nommage
- **SET-5** Brancher Jira et importer ce backlog
- **ART-1** Planche d'ambiance « Kanto réaliste » (références visuelles, palette, lumière)

## Sprint 1 : se déplacer dans Bourg Palette (semaines 3-4)

Objectif : marcher dans une version blockout de Bourg Palette et de la Route 1.

- **EXP-1** Personnage joueur : marche, course, animations de base
- **EXP-2** Caméra troisième personne réglable
- **EXP-3** Blockout de Bourg Palette (maisons, labo du Prof. Chen)
- **EXP-4** Blockout de la Route 1 avec zones d'herbes hautes
- **EXP-5** Entrer et sortir des bâtiments (transition de niveau)
- **EXP-6** Dialogue simple avec un PNJ
  - SCRUM-71 : textes de tous les dialogues de la V1 en Data Tables (`docs/DIALOGUES.md`)

## Sprint 2 : les données Pokémon (semaines 5-6)

Objectif : le jeu sait ce qu'est un Pokémon.

- **DAT-1** Data Table des espèces (stats de base, types, taux de capture, courbe d'XP)
- **DAT-2** Table des 18 types et des multiplicateurs d'efficacité
- **DAT-3** Data Table des attaques (puissance, précision, PP, catégorie)
  - DAT-1 à DAT-3 (SCRUM-27, 28, 29) : données générées, plus apprentissages, CT, talents, natures, rencontres et dresseurs V1 (`docs/DONNEES_POKEMON.md`)
- **DAT-4** Instance de Pokémon : niveau, IV/EV, nature, PV, attaques connues
- **DAT-5** Équipe du joueur (6 max) et choix du starter au labo
- **ART-2** Premier modèle 3D réaliste de test (un starter) avec animations idle et attaque
- **ART-3** (SCRUM-65) Modèle temporaire du joueur : le Dresseur de Smash Bros Ultimate, animations du mannequin retargetées (`docs/PERSONNAGE_DRESSEUR.md`)

## Sprint 3 : le combat (semaines 7-8)

Objectif : un combat sauvage complet du début à la fin.

- **CMB-1** Arène de combat : placement des Pokémon et caméra de combat
- **CMB-2** Boucle de tour : choix d'action, ordre selon la Vitesse
- **CMB-3** Formule de dégâts (STAB, types, coups critiques, aléatoire)
- **CMB-4** UI de combat : barres de PV, menu Attaque / Sac / Pokémon / Fuite
- **CMB-5** IA simple de l'adversaire
- **CMB-6** Changement de Pokémon, K.O., fin de combat et fuite

## Sprint 4 : rencontres et capture (semaines 9-10)

Objectif : attraper des Pokémon dans les herbes hautes.

- **CAP-1** Rencontres aléatoires dans les herbes, tables par zone et par niveau
- **CAP-2** Lancer de Poké Ball avec animation et secousses
- **CAP-3** Formule de capture (PV restants, taux de l'espèce, type de Ball)
- **CAP-4** Envoi au PC quand l'équipe est pleine, surnom
- **DEX-1** Statut vu / capturé enregistré à chaque rencontre et capture

## Sprint 5 : faire monter les Pokémon (semaines 11-12)

Objectif : farmer des niveaux et voir son Pokémon évoluer.

- **LVL-1** Gain d'XP après un combat, partage dans l'équipe
- **LVL-2** Montée de niveau et recalcul des stats
- **LVL-3** Apprentissage d'attaques au niveau (remplacer une attaque si 4 déjà connues)
- **LVL-4** Évolution par niveau avec séquence animée
- **CMB-7** Statuts (poison, paralysie, sommeil, brûlure, gel)

## Sprint 6 : menus, Pokédex et sauvegarde (semaines 13-14)

Objectif : le joueur peut gérer son aventure et la reprendre plus tard.

- **DEX-2** UI du Pokédex : liste, fiche, modèle 3D qui tourne, compteur vu / capturé
- **UI-1** Menu équipe (ordre, détails, stats)
- **UI-2** Sac : objets, Poké Balls, Potions
- **UI-3** Sauvegarde et chargement
- **UI-4** Centre Pokémon (soin) et Boutique (achat)

## Sprint 7 : jusqu'à Pierre, fin du MVP (semaines 15-16)

Objectif : une partie jouable de Bourg Palette au premier badge.

- **EXP-7** Jadielle, Route 2, Forêt de Jade, Argenta en blockout
- **CHA-1** Dresseurs sur les routes : champ de vision, combat obligatoire, récompense
- **CHA-2** Arène d'Argenta : dresseurs puis Pierre
- **CHA-3** Badges : obtention, affichage dans la carte dresseur
- **AUD-1** Musiques de ville, de route et de combat (temporaires)
- **SET-6** Version jouable exportée et tests de bout en bout

## Après le MVP (sprints 8 et suivants)

À détailler au fur et à mesure, un sprint par bloc :

- **ART** Passe « 3D réaliste » sur la tranche MVP : modèles, végétation, éclairage Lumen, ciel et météo
- **EXP** Reste de Kanto : Mont Sélénite, Azuria, Carmin sur Mer, Lavanville, Céladopole, Safrania, Parmanie, Cramois'Île, Route Victoire
- **CHA** Les 7 autres champions, puis la Ligue Pokémon (Conseil 4 et Maître)
- **DAT** Les 151 Pokémon et leurs attaques
- **EXP** CS de terrain (Coupe, Surf, Force, Vol) et objets clés
- **AUD** Musiques et cris définitifs
- **UI** Options, carte de la région, pension

## Point d'attention

Faire 151 Pokémon en 3D réaliste avec animations est le plus gros chantier du projet. On commence avec des modèles provisoires (ou achetés sur le Fab Marketplace) et on remplace au fil des sprints.
