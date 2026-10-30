# Système de combat (SCRUM-8, SCRUM-9, SCRUM-12)

Combat au tour par tour façon Rouge Feu, avec les règles des jeux récents. Tout est en C++ et ne dépend d'aucun affichage : l'UI du combat (SCRUM-36) viendra avec les menus et se contentera de rejouer les événements envoyés par le serveur.

## Ce qui est géré

- **Tour** : chaque côté choisit Attaque, Sac, Pokémon ou Fuite. Fuite, puis changements et objets passent d'abord ; ensuite les attaques par priorité (Vive-Attaque +1, Cyclone -6), puis par Vitesse, égalité tirée au sort.
- **Dégâts** : formule actuelle, niveau, puissance, Attaque/Défense ou Attaque Spé./Défense Spé. selon la **catégorie de chaque attaque**, STAB x1,5, table des 18 types (Fée comprise), critique x1,5 (1 chance sur 24, 8, 2 selon le cran), aléatoire 85 à 100 %, brûlure / 2 sur le physique. Arrondis simplifiés.
- **Précision** et **Esquive** (crans), attaques qui ne ratent jamais, attaques K.O. en un coup.
- **PP** : un PP par attaque, Lutte quand il n'y en a plus (contrecoup 1/4 des PV).
- **Statuts** : paralysie (Vitesse / 2, 25 % de ne pas attaquer), sommeil (1 à 3 tours), gel (20 % de dégeler, une attaque Feu dégèle), brûlure (1/16), poison (1/8), Toxik (n/16). Immunités de type (Feu, Électrik, Glace, Poison/Acier, Cage-Éclair sur Sol, poudres sur Plante).
- **Effets volatils** : confusion (2 à 5 tours, 33 % de se blesser), peur, Vampigraine, Ligotage/Étreinte, Abri, Puissance.
- **Changements de stats** -6 à +6 (Rugissement, Armure, Jet de Sable…), avec effets secondaires des attaques (Bulles, Tomberoche, Griffe Acier).
- Attaques spéciales de la V1 : Frappe Atlas, Ombre Nocturne, Sonicboom, Draco-Rage, Croc Fatal, Balayage (poids), Ampleur, Destruction/Explosion, Repos, Buée Noire, Cyclone/Hurlement. Les autres « attaques uniques » affichent « Mais rien ne se passe ! » pour l'instant.
- **Combats sauvages et contre dresseurs**. Fuite (toujours réussie si on est plus rapide, sinon formule de la 3G). Pas de fuite ni de capture contre un dresseur.
- **IA** : un sauvage attaque au hasard ; un dresseur note ses attaques (dégâts estimés, efficacité, précision, K.O. possible, statuts utiles en début de combat) et choisit surtout les meilleures. Il envoie ses Pokémon dans l'ordre.
- **Capture** : formule actuelle avec PV restants, taux de l'espèce, Ball (Poké 1, Super 1,5, Hyper 2, Master sûre) et statut (sommeil/gel x2,5, autres x1,5), 0 à 3 secousses. Le Pokémon va dans l'équipe, ou au PC si elle est pleine (suite de la capture : [CAPTURE.md](CAPTURE.md)).
- **XP** : formule de la 5G et plus (plus d'XP pour les petits niveaux), x1,5 contre un dresseur, **Multi Exp moderne** (ceux qui n'ont pas combattu gagnent la moitié). EV gagnés (252 par stat, 510 en tout).
- **Montée de niveau** : stats recalculées, attaques apprises au niveau ; s'il en connaît déjà 4, le joueur choisit laquelle oublier (ou renonce).
- **Évolution** par niveau proposée en fin de combat (pas après une défaite, ni avec une Pierre Stase) : la séquence 3D se joue une fois l'écran de combat fermé, B l'annule (docs/PROGRESSION.md).
- **Fin** : argent du dresseur gagné, drapeau `Beaten_<Dresseur>` (utilisé par les dialogues), ou argent perdu en cas de défaite (base selon les badges x plus haut niveau).

## Fichiers

| Fichier | Rôle |
|---|---|
| `Pokemon/PokemonInstance.h` | `FPokemonInstance` (SCRUM-30) : espèce, niveau, XP, nature, talent, IV/EV, stats, PV, statut, attaques et PP. `UPokemonLibrary` : créer, calculer les stats, gagner de l'XP, apprendre, évoluer, soigner. |
| `Battle/BattleTypes.h` | Actions, phases, événements, côtés, résultat, instantané pour l'UI. |
| `Battle/BattleRules.h` | Les formules seules (dégâts, crans, précision, capture, XP, fuite, argent). |
| `Battle/BattleSession.h` | Un combat : reçoit les choix, résout le tour, produit les événements. |
| `Battle/BattleAI.h` | IA des adversaires. |
| `Battle/BattleSubsystem.h` | Lance les combats sur le serveur et écrit le résultat dans le PlayerState. |
| `Battle/BattleComponent.h` | Sur le PlayerController : envoie les choix au serveur, reçoit les événements. |
| `Battle/BattleText.h` | Textes français des événements (« Bulbizarre utilise Charge ! »). |
| `Core/PokemonPlayerState.h` | Équipe, PC, sac, argent, badges, drapeaux d'histoire, Pokédex vu/capturé. |
| `Data/Pokemon/DT_Items.json` | Objets de la V1 (soins, Poké Balls, objets rares), généré par `Tools/Data/build_items.py`. |
| `Tests/BattleTests.cpp` | 11 tests de logique. |

## Multijoueur (SCRUM-68)

- Un combat est un objet à part (`UBattleSession`), qui ne vit que sur le serveur : plusieurs joueurs peuvent combattre en même temps, et les deux côtés sont symétriques (un combat entre joueurs marchera pareil).
- Les données du joueur sont dans son `APokemonPlayerState`, répliquées vers lui seul (l'équipe et les badges se voient chez tous).
- Le client n'envoie que ses choix (RPC serveur) ; le serveur valide, calcule avec son propre tirage aléatoire et renvoie les événements (RPC client). Aucun `GetPlayerCharacter(0)`.

## Tester dans Unreal

1. Fermer Unreal, compiler (« PokemonNewAgeEditor Win64 Development Build »), rouvrir.
2. Relancer `Tools/Data/import_data_tables.py` (**Outils > Exécuter un script Python**) : il crée `DT_Items` et le relie à `DA_PokemonDatabase`.
3. Lancer le jeu (PIE) dans `L_Kanto`, ouvrir la console (touche **²**) et taper par exemple :
   - `DonnerPokemon Bulbasaur 5`, `DonnerObjet PokeBall 10`
   - `CombatSauvage Pidgey 3` ou `CombatRencontre Route1_Walk`
   - `Attaque 1`, `Changer 2`, `Objet PokeBall`, `Objet Potion 1`, `Fuite`
   - `Oublier 2` (ou `Oublier 0`) quand il veut apprendre une 5e attaque, `Evoluer 1` après le combat (sans la séquence)
   - `CombatDresseur Brock`, `Equipe`, `Soigner`

   Le texte du combat s'affiche à l'écran et dans le **Journal de sortie** (*Output Log*, catégorie LogPokemonNewAge).
   Si `DonnerPokemon` répond « PlayerState absent » : ouvrir `BP_ThirdPersonGameMode`, **Classe de Player State** (*Player State Class*) = `PokemonPlayerState`.
4. Tests automatiques : **Outils > Session Frontend** (*Tools > Session Frontend*), onglet **Automation**, cocher `PokemonNewAge.Battle`, **Lancer les tests** (*Start Tests*).

Sans Unreal (dans le cloud) : `./Tools/BattleSim/run.sh` lance les mêmes tests avec un faux moteur minimal, et `./Tools/BattleSim/run.sh fuzz` simule plus de 5 000 combats avec les vraies données (tous les dresseurs V1, toutes les tables de rencontre).

## Brancher le reste

- **Dialogues** : l'événement `StartTrainerBattle` appelle `Battle Subsystem > Start Trainer Battle` (groupe `Brock`, `RivalLab`… : le rival prend la ligne qui correspond au drapeau `Starter_<Espèce>`). `On Battle Ended` (serveur) donne le dresseur et le résultat, pour jouer `Get Trainer Outcome Conversation`.
- **Hautes herbes** (SCRUM-39) : fait, acteur `TallGrassZone`, voir [CAPTURE.md](CAPTURE.md).
- **UI du combat** (SCRUM-36) : écouter `On Battle Started`, `On Battle Events` (jouer les événements un par un avec `Get Event Text`, mettre à jour les barres avec `HPAfter`), `On Battle Ended`, `On Action Refused` (`Get Action Error Text`) sur le `BattleComponent` du PlayerController. Mettre `bPrintEventsToScreen` à faux quand l'UI existe.
- **Séquence d'évolution** (SCRUM-47) : faite, voir docs/PROGRESSION.md (`UProgressionComponent`).
- **Objets tenus** (`HeldEffect` dans `DT_Items`) : baies (Oran, Sitrus, Ceriz, Maron, Pêcha, Fraive, Willia, Prine) mangées sous la moitié des PV ou au statut, Restes (1/16 des PV à chaque tour), Grelot Coque, Vive Griffe (agit en premier 1 fois sur 5), Bandeau (tient à 1 PV 1 fois sur 10), objets de type (+20 %), Œuf Chance (XP ×1,5), Pièce Rune (argent ×2). Les baies sont consommées.
- **Pas encore fait** : combats doubles, météo et terrains, talents (Engrais, Brasier…), objets de combat (Attaque +), PV restants transmis à l'écran du PC, surnom à la capture (SCRUM-42, côté UI).
