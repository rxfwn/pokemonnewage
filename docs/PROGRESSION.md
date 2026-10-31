# Progression des Pokémon (SCRUM-12)

XP, niveaux, nouvelles attaques et évolutions. Le combat (docs/COMBAT.md) donnait déjà l'XP, la montée de niveau et le choix de l'attaque à oublier **pendant** le combat. Cette partie ajoute tout ce qui se passe **après** ou **hors** du combat.

## Ce qui est géré

- **Écran de combat** : la barre d'XP se remplit en glissant (et repart de zéro à chaque niveau), le son de montée de niveau se joue et un panneau montre les stats (total et gain : « Attaque 24 +3 »).
- **Séquence d'évolution en 3D** (SCRUM-47), une fois l'écran de combat fermé :
  1. la caméra quitte le dresseur et se pose face au Pokémon, juste devant lui ; le décor s'assombrit et se floute ;
  2. « Quoi ? Bulbizarre évolue ! » ; le Pokémon devient une silhouette de lumière ;
  3. sa silhouette et celle de sa nouvelle forme grandissent et rétrécissent à tour de rôle, de plus en plus vite, en musique ;
  4. flash blanc, la nouvelle forme apparaît en couleurs, fanfare : « Félicitations ! Ton Bulbizarre a évolué en Herbizarre ! » ;
  5. ensuite, les attaques de la nouvelle espèce à ce niveau (choix de l'attaque à oublier si besoin).
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
| `Tools/Progression/import_progression_unreal.py` | Importe les sons et crée le matériau de lueur `M_EvolutionGlow`. |
| `Tests/ProgressionTests.cpp` | 4 tests (Super Bonbon, pierres, CT, évolution). |

Assets attendus (créés par le script) : `/Game/Progression/M_EvolutionGlow`, `/Game/Progression/Sounds/SW_LevelUp`, `SW_MoveLearned`, `SW_Evolution_Start`, `SW_Evolution_Music`, `SW_Evolution_Flash`, `SW_Evolution_Done`, `SW_Evolution_Cancel`. S'ils manquent, tout marche quand même, sans lueur ni son.

Modèles : ceux du fil « modèles 3D » (`/Game/Pokemon/Models/<Espèce>/SK_<Espèce>`, animation `A_<Espèce>_Idle`). Une espèce sans modèle (Herbizarre pour l'instant, par exemple) est remplacée par une sphère lumineuse.

## Tester dans Unreal

1. Fermer Unreal, compiler (« PokemonNewAgeEditor Win64 Development Build »), rouvrir.
2. **Outils > Exécuter un script Python** (*Tools > Execute Python Script*) :
   - `Tools/Data/build_items.py` n'est pas à lancer (le JSON est déjà à jour), mais relancer `Tools/Data/import_data_tables.py` : DT_Items reçoit les nouveaux objets et DT_Species les évolutions corrigées ;
   - puis `Tools/Progression/import_progression_unreal.py` (sons et lueur).
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

Créer un Blueprint enfant d'**EvolutionScene** (*Content/Progression/BP_EvolutionScene*), puis dans le Blueprint du PlayerController : composant **ProgressionComponent** > **Evolution Scene Class** = ce Blueprint. Réglages : durées (Intro, Glow, Morph, Reveal), la Poké Ball d'où sort le Pokémon au début (catégorie **Evolution > Ball** : taille, moment d'ouverture, angle du couvercle ; c'est la Ball de sa capture, modèles importés par `Tools/Capture/import_pokeball_unreal.py`, voir `docs/CAPTURE.md`), vitesses de l'alternance, distance devant le joueur, couleur de la lueur, et la caméra / les lumières / le post-process (flou, vignette) dans ses composants. Un Niagara d'étincelles peut s'y ajouter sans code.

## Pas encore fait

- Évolutions par échange entre joueurs (avec SCRUM-69, les échanges) ; le Fil de Liaison les remplace en attendant.
- Bonheur (pas utile pour les 151 : aucune évolution par bonheur), Maître des capacités, attaques apprises à l'évolution façon 7G.
- Vrais sons et vraie musique (épopée Audio).
