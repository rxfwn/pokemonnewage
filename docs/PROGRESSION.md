# Progression des Pokémon (SCRUM-12)

XP, niveaux, nouvelles attaques et évolutions. Le combat (docs/COMBAT.md) donnait déjà l'XP, la montée de niveau et le choix de l'attaque à oublier **pendant** le combat. Cette partie ajoute tout ce qui se passe **après** ou **hors** du combat.

## Ce qui est géré

- **Écran de combat** : la barre d'XP se remplit en glissant (et repart de zéro à chaque niveau), le son de montée de niveau se joue et un panneau montre les stats (total et gain : « Attaque 24 +3 »).
- **Séquence d'évolution en 3D** (SCRUM-47), une fois l'écran de combat fermé. L'entrée suit la maquette de L'amie (`/mnt/project-files/progression/maquette/Evolution_Pokeball_Anim.png`) :
  1. gros plan de nuit sur la **Poké Ball de sa capture**, posée sur l'herbe : elle tremble trois fois (« Quoi ? ») ;
  2. elle s'ouvre : le Pokémon en sort tout de suite, en silhouette de lumière, pendant que la caméra recule pour le cadrer ;
  3. une **colonne de lumière** monte autour de lui, des étincelles jaillissent de la Ball ;
  4. **explosion blanche** (onde de choc, étincelles) : la Ball disparaît ;
  5. « Quoi ? Bulbizarre évolue ! » : sa silhouette et celle de sa nouvelle forme grandissent et rétrécissent à tour de rôle, de plus en plus vite, en musique ;
  6. flash blanc, la nouvelle forme apparaît en couleurs, fanfare : « Félicitations ! Ton Bulbizarre a évolué en Herbizarre ! » ;
  7. ensuite, les attaques de la nouvelle espèce à ce niveau (choix de l'attaque à oublier si besoin).
  - Sans modèle de Ball importé, l'ancienne entrée est gardée : le Pokémon est là dès le début et se met à briller.
  - La boîte de texte, le panneau des stats et le choix d'attaque sont en **verre** comme les autres fenêtres (`PokemonGlass`, docs/VERRE_UI.md), aux mesures de la maquette, en Futura BdCn.
  - **B** (ou Échap, ou clic droit) avant le flash : « Hein ? Bulbizarre n'a pas évolué ! ». Il le reproposera au prochain niveau.
- **Objets hors combat** (Pokédex > Sac > « Objets, baies et CT ») avec la liste **APTE / PAS APTE / DÉJÀ APPRIS** :
  - **Super Bonbon** : +1 niveau, stats, attaques du niveau, puis évolution s'il y a lieu ;
  - **Pierres** Feu, Eau, Foudre, Plante, Lune : évolution par objet (Pikachu + Pierre Foudre = Raichu). La pierre n'est retirée du sac que si l'évolution va au bout ;
  - **Fil de Liaison** : fait évoluer Kadabra, Machopeur, Gravalanch, Spectrum sans échange (objet officiel des jeux récents) ;
  - **Pierre Stase** (à tenir) : bloque l'évolution par niveau et le Fil de Liaison (pas les pierres) ;
  - **CT et CS** : les 57 de Rouge Feu / Vert Feuille (CT39 Tomberoche de Pierre…), **réutilisables** comme depuis la 5G, seulement pour les espèces compatibles.
  - S'il faut choisir une attaque ou voir une évolution, le Pokédex se referme tout seul et la suite se joue en jeu.
- **Données corrigées** : les évolutions des formes d'Alola et de Galar (Sabelette + Pierre Glace, Ramoloss + Bracelet Galarica) et les doublons ont disparu de DT_Species.
- **Multijoueur** : le serveur applique tout et vérifie chaque choix (évolution proposée, attaque autorisée, objet dans le sac) ; la séquence ne se joue que chez le joueur concerné.

## Fichiers

| Fichier | Rôle |
|---|---|
| `Pokemon/PokemonProgression.h` | Règles seules : objets, niveaux gagnés, évolution, attaques à apprendre, compatibilité. Testées sans Unreal. |
| `Progression/ProgressionComponent.h` | Sur le PlayerController. Serveur : applique les objets et les choix. Client : file des étapes à montrer (niveau, attaque, évolution). |
| `Progression/EvolutionScene.h` | La séquence 3D (caméra, lumières, silhouettes, flash, sons). Durées et couleurs réglables dans un Blueprint enfant. |
| `UI/Progression/ProgressionWidget.h` | Boîte de texte, panneau des stats, choix de l'attaque à oublier, flash blanc. |
| `Tools/Progression/make_progression_sounds.py` | Sons provisoires synthétisés (mélodies originales). |
| `Tools/Progression/import_progression_unreal.py` | Importe les sons et crée les matériaux `M_EvolutionGlow` (lueur) et `M_EvolutionLight` (rayon, étincelles). |
| `Tests/ProgressionTests.cpp` | 4 tests (Super Bonbon, pierres, CT, évolution). |

Assets attendus (créés par le script) : `/Game/Progression/M_EvolutionGlow`, `/Game/Progression/M_EvolutionLight` (rayon, étincelles, boule de lumière), `/Game/Progression/Sounds/SW_LevelUp`, `SW_MoveLearned`, `SW_Evolution_Wobble`, `SW_Evolution_Start`, `SW_Evolution_Music`, `SW_Evolution_Flash`, `SW_Evolution_Done`, `SW_Evolution_Cancel`. S'ils manquent, tout marche quand même, sans lueur ni son.

Modèles : ceux du fil « modèles 3D » (`/Game/Pokemon/Models/<Espèce>/SK_<Espèce>`, animation `A_<Espèce>_Idle`). Une espèce sans modèle (Herbizarre pour l'instant, par exemple) est remplacée par une sphère lumineuse.

## Tester dans Unreal

1. Fermer Unreal, compiler (« PokemonNewAgeEditor Win64 Development Build »), rouvrir.
2. **Outils > Exécuter un script Python** (*Tools > Execute Python Script*) :
   - `Tools/Data/build_items.py` n'est pas à lancer (le JSON est déjà à jour), mais relancer `Tools/Data/import_data_tables.py` : DT_Items reçoit les nouveaux objets et DT_Species les évolutions corrigées ;
   - puis `Tools/Progression/import_progression_unreal.py` (sons, lueur et lumières ; à relancer après chaque mise à jour de la séquence).
3. Lancer le jeu (PIE) dans `L_Kanto`, console (touche **²**) :
   - `DonnerPokemon Bulbasaur 15` puis `SuperBonbon 1` : panneau des stats, puis la séquence (Herbizarre). Appuyer sur **B** pendant l'alternance pour l'annuler, refaire `SuperBonbon 1` pour la revoir jusqu'au bout ;
   - `VoirEvolution Pikachu Raichu` : rejoue la séquence sans rien changer (pratique pour régler l'image) ;
   - `DonnerPokemon Pikachu 10`, `DonnerObjet ThunderStone`, puis Pokédex > Sac > Objets, baies et CT > Pierre Foudre > Pikachu (APTE) ;
   - `DonnerObjet TM39` puis la CT sur un Racaillou ou un Onix qui connaît déjà 4 attaques : choix de l'attaque à oublier ;
   - `DonnerXP 1 2000` : plusieurs niveaux d'un coup hors combat ;
   - en combat : `CombatSauvage Pidgey 20` avec un Pokémon de niveau 15 pour voir la barre d'XP et le panneau des stats.
4. Tests automatiques : **Outils > Session Frontend**, onglet **Automation**, cocher `PokemonNewAge.Progression`.

Sans Unreal (dans le cloud) : `./Tools/BattleSim/run.sh` (tests) et `./Tools/BattleSim/run.sh fuzz` (les 151 espèces montées du niveau 2 au niveau 100 au Super Bonbon, avec évolutions et choix d'attaques, et tous les objets essayés sur toutes les espèces).

## Régler la séquence

Créer un Blueprint enfant d'**EvolutionScene** (*Content/Progression/BP_EvolutionScene*), puis dans le Blueprint du PlayerController : composant **ProgressionComponent** > **Evolution Scene Class** = ce Blueprint. Réglages : durées (Intro, Glow, Morph, Reveal), la Poké Ball d'où sort le Pokémon au début (catégorie **Evolution > Ball** : taille, tremblements (durée, nombre, angle), ouverture (durée, angle du couvercle), durée du rayon et de l'explosion, couleur de la lumière, distance, netteté (Ball Shot Fstop) et hauteur à l'écran (Ball Shot Raise) du gros plan ; c'est la Ball de sa capture, modèles importés par `Tools/Capture/import_pokeball_unreal.py`, voir `docs/CAPTURE.md`), vitesses de l'alternance, distance devant le joueur, couleur de la lueur, et la caméra / les lumières / le post-process (flou, vignette) dans ses composants. Le rayon, les étincelles et l'onde de choc sont faits avec des formes de base et `M_EvolutionLight` (son paramètre **Intensite** règle leur éclat) ; un Niagara peut les remplacer ou s'y ajouter sans code.

## Fond étoilé (2026-11-03)

Demandé par L'amie d'après une référence (ciel bleu, étoiles, lumières floues, traînées), puis refait le même jour
façon évolution de Pokémon Soleil et Lune d'après ses captures : salle bleu vif, piliers de lumière verticaux, gros
bokeh, éclats à quatre branches qui apparaissent et disparaissent, lignes de vitesse qui partent du centre, disque
lumineux avec deux anneaux sous les pieds. À la place du terrain sombre, la scène montre ce ciel animé derrière le
Pokémon et le sol lumineux sous ses pieds.

- **Mise en place** : lancer `Tools/Progression/import_evolution_backdrop.py` dans Unreal (*Outils > Exécuter un
  script Python...*), puis compiler (nouveau composant C++). Test : `VoirEvolution Bulbasaur Ivysaur`.
- **Code** : `UEvolutionBackdropComponent` (`Source/PokemonNewAge/Progression/EvolutionBackdrop.h`), créé par
  `AEvolutionScene`. Le ciel est un panneau accroché à la caméra, derrière tout ce qui est montré ; le sol est un
  disque à la hauteur exacte des pieds et du bas de la Ball, qui va jusqu'au ciel. La scène entière est soulevée de
  `GroundClearance` (30 m) au-dessus du terrain : aucune colline ni arbre ne passe devant le ciel (une pente
  coupait l'image en biais au premier test), et rien ne s'enfonce dans le sol lumineux.
  Devant le Pokémon, deux panneaux transparents accrochés à la caméra : `M_EvolutionFrontNear` (gros bokeh tout
  près, adoucis par le flou de profondeur) et `M_EvolutionFrontMid` (éclats et poussières qui montent juste devant
  lui). Pendant la transformation en points (et le flash), le centre de l'image se dégage ; avant et après, les
  effets repassent devant le Pokémon (paramètre `Degagement`, posé par le C++). Sans les matériaux : ancien fond.
- **Textures** fabriquées par `make_evolution_sky.py` (dans `Tools/Progression/Assets/`) :
  `T_EvolutionSky.png` (R étoiles, G bokeh, B phase du scintillement, A traînées) et `T_EvolutionFx.png`
  (R piliers, G éclats à quatre branches, B phase des éclats, A lignes de vitesse rangées en angle/distance).
- **Réglages** (ouvrir `M_EvolutionSky` ou `M_EvolutionFloor`, cliquer sur le paramètre, *Default Value*,
  *Appliquer* ; relancer le script remet les valeurs d'origine) :

  | Paramètre | Matériau | Effet |
  |---|---|---|
  | `CouleurHaut`, `CouleurHorizon`, `HauteurDegrade` | ciel | Dégradé du ciel. `CouleurHorizon` existe aussi dans le sol : garder la même valeur dans les deux. |
  | `CouleurCentre`, `LueurCentre` | les deux | Grande lueur au centre de l'image. |
  | `IntensiteEtoiles`, `CouleurEtoiles`, `VitesseScintillement`, `Repetition` | ciel | Étoiles (`Repetition` plus grand = plus d'étoiles, plus petites). |
  | `IntensiteBokeh`, `CouleurBokeh`, `VitesseBokeh` | ciel | Lumières floues qui dérivent. |
  | `Piliers`, `CouleurPiliers` | ciel | Piliers de lumière verticaux (ils vacillent doucement). |
  | `Eclats`, `CouleurEclats`, `VitesseEclats` | ciel | Éclats à quatre branches qui s'allument et s'éteignent. |
  | `LignesVitesse`, `CouleurLignes` | ciel | Lignes qui partent du centre de l'image. |
  | `IntensiteTrainees`, `CouleurTrainees` | ciel | Traînées de lumière en diagonale. |
  | `CouleurSol`, `IntensiteSol`, `BokehSol` | sol | Lueur sous le Pokémon. |
  | `Anneaux`, `CouleurAnneaux`, `Anneau1`, `Anneau2`, `LargeurAnneau` | sol | Les deux anneaux qui respirent autour des pieds (taille en part du rayon de la lueur). |
  | `Contraste` | ciel, sol | Bords de l'image assombris (profondeur). 0 = pas d'assombrissement. |
  | `IntensiteBokeh`, `VitesseBokeh` | devant (Near) | Gros bokeh flous au premier plan. |
  | `Eclats`, `Poussieres`, `CouleurPoussieres`, `VitesseEclats`, `VitesseScintillement` | devant (Mid) | Éclats et poussières devant le Pokémon. |
  | `ZoneLibre`, `DansZoneLibre` | devant (les deux) | Taille de la zone dégagée au centre, et ce qui reste des effets dedans (0,15). |
  | `Luminosite` | tous | Tout plus clair ou plus sombre. |

- **Vidéo After Effects** : quand L'amie enverra sa boucle (MP4 H.264, sans son), elle pourra remplacer le ciel
  (Media Player + Media Texture sur le panneau du ciel).
- **Limite** : si le terrain monte fort autour du Pokémon, une bosse peut passer au-dessus du sol lumineux.

## Pas encore fait

- Évolutions par échange entre joueurs (avec SCRUM-69, les échanges) ; le Fil de Liaison les remplace en attendant.
- Bonheur (pas utile pour les 151 : aucune évolution par bonheur), Maître des capacités, attaques apprises à l'évolution façon 7G.
- Vrais sons et vraie musique (épopée Audio).
