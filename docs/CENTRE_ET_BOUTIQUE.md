# Centre Pokémon et Boutique (SCRUM-53)

Ce qui est codé :

- **Centre Pokémon** : Joëlle soigne l'équipe (dialogue existant), et ce Centre devient le **point de retour**. Si toute l'équipe est K.O., écran noir, le joueur réapparaît devant le comptoir, équipe soignée, et Joëlle dit un mot. Avant d'avoir visité un Centre, on revient à la maison et c'est maman qui parle.
- **PC de Léo** : le PC du Centre (et celui de la chambre) ouvre l'écran du PC : équipe à gauche, boîte de 30 à droite, 12 boîtes. Déposer, Retirer, Échanger, Relâcher (avec confirmation).
- **Boutique Pokémon** : Acheter / Vendre / Quitter, prix et argent affichés, choix de la quantité. Stock propre à chaque ville, qui s'agrandit avec les badges. Les achats arrivent dans le **Sac** du Pokédex, rangés dans leur poche.

Tout est décidé par le serveur (multijoueur prêt), comme le combat et les dialogues.

## Règles choisies

| Sujet | Règle |
|---|---|
| Prix d'achat | `Price` de `DT_Items` (prix de Rouge Feu) |
| Revente | la moitié du prix, arrondie en dessous. Objets rares et objets sans prix (Restes, Master Ball...) invendables |
| Quantité | 1 à 99 par achat, 999 au plus par objet dans le sac |
| Argent | 999 999 ₽ au plus (au-delà, perdu, comme dans les jeux) |
| Cadeau | une **Honor Ball** offerte par dizaine de Balls achetées d'un coup |
| Stock | rayon de base de la ville (Rouge Feu), puis un rayon commun qui arrive avec les badges (Let's Go) : Super Potion (1 badge), Super Ball et Rappel (3), Total Soin (4), Hyper Potion et Hyper Ball (5), Potion Max (7), Guérison (8) |
| Point de retour | le `PokemonCenterPoint` le plus proche au moment du soin (Joëlle, maman). Gardé dans la sauvegarde |
| Après un K.O. | argent perdu (déjà fait par le combat), puis retour au dernier point, équipe soignée |

Stock de la V1 :

- **Jadielle** (`ViridianPokeMart`) : Poké Ball, Potion, Antidote, Anti-Para, Anti-Brûle. Le vendeur ne vend qu'après avoir remis le colis de Chen (dialogue).
- **Argenta** (`PewterPokeMart`) : Poké Ball, Potion, Antidote, Anti-Para, Réveil, Anti-Brûle, Antigel.

Pas encore en rayon : Repousse et Corde Sortie (il faudra d'abord pouvoir les utiliser depuis le Sac).

## Où sont les choses

| Quoi | Où |
|---|---|
| Règles d'achat et de vente (testées sans Unreal) | `Source/PokemonNewAge/Town/ShopRules.h` |
| Boutique côté joueur (réseau, ouverture après le dialogue) | `Source/PokemonNewAge/Town/ShopComponent.h` |
| Écran de la Boutique | `Source/PokemonNewAge/UI/ShopWidget.h` |
| Point de retour à poser dans la map | `Source/PokemonNewAge/Town/PokemonCenterPoint.h` |
| Soin, retour après K.O., PC | `Source/PokemonNewAge/Town/PokemonCenterComponent.h` |
| Écran du PC de Léo | `Source/PokemonNewAge/UI/StorageWidget.h` |
| Stock des boutiques | `Tools/Data/build_shops.py` → `Data/Pokemon/DT_Shops.json` |
| Répliques de Joëlle et maman après un K.O. | `Tools/Data/dialogues_v1.py` (`PokemonCenter_Nurse_AfterBlackout`, `PlayerHouse_Mom_AfterBlackout`) |

Les deux composants (`PokemonCenterComponent`, `ShopComponent`) sont déjà sur le PlayerController du jeu. Les dialogues les appellent : `HealParty` retient le point de retour, `ReturnToLastHeal` ramène au Centre, `OpenShop` ouvre la boutique, `OpenPC` allume le PC.

Changer le stock : modifier `Tools/Data/build_shops.py`, lancer `python Tools/Data/build_shops.py`, puis réimporter dans Unreal (ci-dessous). Une nouvelle boutique : une entrée dans `SHOPS`, et un dialogue avec `ev="OpenShop", p="<nom de la boutique>"`.

## Installer et tester dans Unreal

Le code n'a pas pu être compilé dans le cloud : c'est le premier test. Cette branche contient aussi le menu Pokédex (PR #19), la boîte de dialogue (PR #20) et la capture (PR #21) : tout est à compiler ensemble.

1. Fermer Unreal. Dans VS Code, **Terminal > Exécuter la tâche** (*Run Task*) > « PokemonNewAgeEditor Win64 Development Build ». Rouvrir le projet.
2. **Outils > Exécuter un script Python** (*Tools > Execute Python Script*) > `Tools/Data/import_data_tables.py`. Le journal doit dire `DT_Shops : 2 lignes importées` et `DT_Items : 52 lignes importées`.
3. Ouvrir `L_Kanto`, lancer **Jouer** (*Play*), ouvrir la console avec **²** et taper :
   - `Boutique ViridianPokeMart` : l'écran de la Boutique s'ouvre. **Z / S** pour choisir, **E** pour valider, **Échap** pour revenir. Acheter 10 Poké Balls : 2 000 ₽ retirés et une Honor Ball offerte.
   - Vendre une Potion : 150 ₽. Ouvrir ensuite le Sac du Pokédex : les achats sont dans leur poche.
   - `Badge BoulderBadge`, puis rouvrir la boutique : la Super Potion est en rayon (le stock compte les badges).
   - `Drapeau GotPokedex` puis `Parler ViridianPokeMart_Clerk` : le vendeur parle, puis la Boutique de Jadielle s'ouvre quand sa boîte se ferme.
   - `Parler PokemonCenter_PC` : le PC de Léo s'allume. **Q / D** pour passer de l'équipe à la boîte, **Page suiv.** ou **Tab** pour changer de boîte. Il faut au moins deux Pokémon pour en déposer un (`DonnerPokemon Rattata 3`).
   - `RetourCentre` : sans point de retour posé, on revient au Player Start avec maman. Puis le vrai test : `CombatSauvage Onix 30` et perdre exprès. Après « ... et se précipite au dernier Centre Pokémon visité », écran noir et retour.
4. Dans la map, pour chaque Centre (Jadielle, Argenta) :
   - **Placer des acteurs** (*Place Actors*) > chercher **Pokemon Center Point**, le poser au sol devant le comptoir, la **flèche rouge vers la sortie**. Dans **Détails** : **Point Id** `ViridianPokemonCenter` (ou `PewterPokemonCenter`), **Location** `ViridianCity` (ou `PewterCity`). Facultatif : **Heal Sound** = le jingle du soin.
   - Poser un **Dialogue Npc** avec **Npc Id** `PokemonCenterNurse` (Joëlle) et un autre avec `PokemonCenter_PC` (le PC).
   - Dans la maison du joueur, un **Pokemon Center Point** avec **Is Home** coché, **Point Id** `PlayerHouse`, près de maman (`Mom`).
   - Dans chaque Boutique, un **Dialogue Npc** `ViridianPokeMart_Clerk` ou `PewterPokeMart_Clerk`.
   - Puis parler à Joëlle (`NoterCentre` dans la console fait pareil) et taper `RetourCentre` de loin : on doit réapparaître devant le comptoir.

Si le point de retour n'est pas trouvé après un rechargement de la map, vérifier dans **Détails** du Pokemon Center Point que **Is Spatially Loaded** est décoché (il l'est par défaut).

## Commandes de test (console **²**)

| Commande | Effet |
|---|---|
| `Boutique ViridianPokeMart` | Ouvre une boutique sans parler au vendeur |
| `Acheter PokeBall 10`, `Vendre Potion 2` | Achat / vente dans la boutique ouverte (sans l'écran) |
| `Argent 5000` | Fixe l'argent du joueur |
| `Badge BoulderBadge` | Donne un badge (stock des boutiques) |
| `AllumerPC` | Ouvre le PC de Léo |
| `NoterCentre` | Le Centre le plus proche devient le point de retour |
| `RetourCentre` | Retour au dernier Centre, comme après un K.O. |

## Habiller les écrans

Les écrans sont construits en C++ (mêmes couleurs que la boîte de dialogue). Pour les habiller plus tard : créer un Widget Blueprint enfant de `ShopWidget` (ou `StorageWidget`), puis le choisir dans le PlayerController, composant **ShopComponent > Shop Widget Class** (ou **PokemonCenterComponent > Storage Widget Class**). Les événements **On Shop Refreshed** et **On Storage Refreshed** servent à animer l'habillage.

## Tests sans Unreal

`./Tools/BattleSim/run.sh` lance aussi `PokemonNewAge.Town.Shop` (achat, refus, Honor Ball, sac plein, revente, plafond d'argent). Dans Unreal : **Outils > Session Frontend > Automation**, filtre `PokemonNewAge.Town`.
