# Dialogues de la V1

Version lisible de `DT_Dialogues`, générée par `Tools/Data/build_dialogues.py`. Pour corriger un texte, modifier `Tools/Data/dialogues_v1.py` puis relancer le script.

Légende : *Parler* = en parlant au PNJ ; *Scène* = déclenchée par le jeu (volume, combat, choix). « si » = drapeaux nécessaires, « sauf » = drapeaux qui l'empêchent.

## Bourg Palette

### `PalletTown_Sign_Read`

*Parler*

- *BOURG PALETTE / Des couleurs pures et paisibles.*

### `PalletTown_PlayerHouseSign_Read`

*Parler*

- *Maison de {PlayerName}*

### `PalletTown_RivalHouseSign_Read`

*Parler*

- *Maison de Régis*

### `PalletTown_OaksLabSign_Read`

*Parler*

- *LABORATOIRE POKÉMON DU PROF. CHEN*

### `PalletTown_Girl_Talk`

*Parler*

- **Fillette** : Moi aussi, j'élève des Pokémon !
- **Fillette** : Quand ils seront grands et forts, ce sont eux qui me protégeront.

### `PalletTown_TechGuy_Talk`

*Parler*

- **Gros bonhomme** : La technologie, c'est fou !
- **Gros bonhomme** : Aujourd'hui, on peut ranger des Pokémon et des objets dans un PC, comme de simples données !

### `PalletTown_ProfOak_StopGrass`

*Scène* · sauf MetOak

- **Prof. Chen** : Hé ! Attends ! Ne va pas plus loin !
- **Prof. Chen** : C'est dangereux ! Des Pokémon sauvages vivent dans les hautes herbes !
- **Prof. Chen** : Il te faut ton propre Pokémon pour te protéger.
- **Prof. Chen** : Allez, suis-moi !
  - ⚙ PlayCutscene OakLeadsToLab
- ⚑ MetOak
- → `OaksLab_Intro`

### `PlayerHouse_Mom_Default`

*Parler*

- **Maman** : Un jour ou l'autre, tout le monde quitte la maison pour partir à l'aventure.
- **Maman** : Enfin, c'est ce qu'ils disent à la télé.
- **Maman** : Au fait, le Prof. Chen, notre voisin, te cherchait tout à l'heure.

### `PlayerHouse_Mom_AfterStarter`

*Parler* · priorité 10 · si GotStarter

- **Maman** : {PlayerName} ! Le Prof. Chen t'a confié un Pokémon ? C'est merveilleux !
- **Maman** : Viens donc te reposer un peu, tous les deux.
- *{PlayerName} et {StarterName} reprennent des forces.*
  - ⚙ HealParty
- **Maman** : Voilà ! Ton Pokémon a l'air en pleine forme. Fais bien attention à toi !

### `PlayerHouse_Mom_AfterPokedex`

*Parler* · priorité 20 · si GotPokedex

- **Maman** : Le Prof. Chen compte sur toi pour remplir son Pokédex ? Quelle aventure !
- **Maman** : Repose-toi avant de repartir.
- *Toute l'équipe est en pleine forme.*
  - ⚙ HealParty
- **Maman** : Et repasse me voir de temps en temps, d'accord ?

### `PlayerHouse_Mom_AfterBadge`

*Parler* · priorité 30 · si GotBoulderBadge

- **Maman** : Le Badge Roche ? Tu as battu le Champion d'Argenta ?
- **Maman** : Je suis si fière de toi ! Allez, un peu de repos.
- *Toute l'équipe est en pleine forme.*
  - ⚙ HealParty

### `PlayerHouse_Mom_AfterBlackout`

*Scène*

- **Maman** : {PlayerName} ! Te voilà ! Tes Pokémon avaient l'air à bout de forces...
- *Toute l'équipe a repris des forces.*
- **Maman** : Ne force pas trop, d'accord ? Je suis toujours là si besoin.

### `PlayerHouse_TV_Read`

*Parler*

- *Un film passe à la télé. Quatre garçons marchent le long d'une voie ferrée... / Bon, il est temps d'y aller.*

### `PlayerHouse_Console_Read`

*Parler*

- *Une vieille console de jeux. / Pas le temps de jouer, l'aventure attend !*

### `PlayerHouse_PC_Use`

*Parler*

- *{PlayerName} allume le PC.*
  - ⚙ OpenPC

### `PlayerHouse_Bookshelf_Read`

*Parler*

- *Plein de livres sur les Pokémon. Que des classiques !*

### `RivalHouse_Daisy_Default`

*Parler*

- **Nina** : Salut, {PlayerName} ! Régis est parti au labo de Grand-père.
- **Nina** : Il ne tenait plus en place, ce matin...

### `RivalHouse_Daisy_AfterStarter`

*Parler* · priorité 10 · si GotStarter

- **Nina** : Grand-père t'a donné un Pokémon ? Je peux te donner un conseil ?
- **Nina** : Parle-lui souvent. Un Pokémon sent quand on tient à lui.

### `RivalHouse_Daisy_GiveTownMap`

*Parler* · priorité 20 · si GotPokedex · sauf GotTownMap

- **Nina** : Grand-père t'a demandé un service, c'est ça ?
- **Nina** : Alors prends ça, ça te sera utile.
- *{PlayerName} obtient : Carte !*
  - ⚙ GiveItem TownMap ×1
- **Nina** : C'est une carte de Kanto. Elle te montre toujours où tu te trouves.
- ⚑ GotTownMap

### `RivalHouse_Daisy_AfterTownMap`

*Parler* · priorité 30 · si GotTownMap

- **Nina** : La Carte est rangée dans ton Sac, avec les objets rares.
- **Nina** : Mon frère est parti vers Jadielle comme une flèche. Il ne changera jamais !

### `RivalHouse_Painting_Read`

*Parler*

- *Un joli tableau. On dirait un paysage de Kanto vu du ciel.*

### `OaksLab_Rival_Waiting`

*Parler* · sauf MetOak

- **Régis** : Tiens, {PlayerName} ! Pépé n'est pas là.
- **Régis** : Il m'a dit de passer, il va me donner un Pokémon. Toi, rentre chez toi !

### `OaksLab_Intro`

*Scène* · si MetOak · sauf LabIntroDone

- **Régis** : Pépé ! J'en ai marre d'attendre !
- **Prof. Chen** : Régis ? Ah, c'est vrai, je t'avais dit de venir. Patiente un peu.
- **Prof. Chen** : Bien. {PlayerName}, approche.
- **Prof. Chen** : Sur cette table, il y a trois Poké Balls. Chacune contient un Pokémon.
- **Prof. Chen** : Quand j'étais jeune, j'étais un Dresseur redoutable, tu sais.
- **Prof. Chen** : Aujourd'hui, il ne me reste que ces trois-là. Je t'en confie un. Vas-y, choisis !
- **Régis** : Hé ! Pépé ! Et moi, alors ?
- **Prof. Chen** : Sois patient, Régis ! Tu auras le tien juste après.
- ⚑ LabIntroDone

### `OaksLab_Rival_BeforeChoice`

*Parler* · priorité 5 · si LabIntroDone · sauf GotStarter

- **Régis** : Vas-y, choisis ! De toute façon, je prendrai le meilleur après toi.

### `OaksLab_ProfOak_BeforeChoice`

*Parler* · priorité 5 · si LabIntroDone · sauf GotStarter

- **Prof. Chen** : Alors, {PlayerName} ? Choisis l'un de ces trois Pokémon. Prends ton temps !

### `OaksLab_PokeBall_Bulbasaur_Look`

*Parler*

- *Une Poké Ball. Il y a un Pokémon à l'intérieur !*
- *Le Prof. Chen n'aimerait sans doute pas qu'on y touche...*

### `OaksLab_PokeBall_Bulbasaur_Offer`

*Parler* · priorité 5 · si LabIntroDone · sauf GotStarter

- **Prof. Chen** : Ah ! Bulbizarre, le Pokémon Plante ? Il est facile à élever et très résistant.
- **Prof. Chen** : Tu choisis Bulbizarre ?
  - ▸ « Oui » → `OaksLab_ChooseBulbasaur`
  - ▸ « Non » → `None`

### `OaksLab_ChooseBulbasaur`

*Scène* · sauf GotStarter

- **Prof. Chen** : Excellent choix ! Ce Pokémon déborde d'énergie.
- *{PlayerName} reçoit Bulbizarre de la part du Prof. Chen !*
  - ⚙ GivePokemon Bulbasaur ×5
- *Veux-tu donner un surnom à Bulbizarre ?*
  - ⚙ OfferNickname
- **Régis** : Alors moi, je prends celui-là !
- *Régis reçoit {RivalStarterName} de la part du Prof. Chen !*
- **Régis** : Mon Pokémon a l'air bien plus fort que le tien. Ha !
- ⚑ GotStarter, Starter_Bulbasaur

### `OaksLab_PokeBall_Bulbasaur_LastOne`

*Parler* · priorité 20 · si GotStarter, Starter_Charmander

- *C'est le dernier Pokémon du Prof. Chen.*
- *Il vaut mieux le laisser à sa place.*

### `OaksLab_PokeBall_Bulbasaur_Empty`

*Parler* · priorité 10 · si GotStarter

- *La Poké Ball est vide.*

### `OaksLab_PokeBall_Charmander_Look`

*Parler*

- *Une Poké Ball. Il y a un Pokémon à l'intérieur !*
- *Le Prof. Chen n'aimerait sans doute pas qu'on y touche...*

### `OaksLab_PokeBall_Charmander_Offer`

*Parler* · priorité 5 · si LabIntroDone · sauf GotStarter

- **Prof. Chen** : Ah ! Salamèche, le Pokémon Feu ? La flamme au bout de sa queue montre sa force.
- **Prof. Chen** : Tu choisis Salamèche ?
  - ▸ « Oui » → `OaksLab_ChooseCharmander`
  - ▸ « Non » → `None`

### `OaksLab_ChooseCharmander`

*Scène* · sauf GotStarter

- **Prof. Chen** : Excellent choix ! Ce Pokémon déborde d'énergie.
- *{PlayerName} reçoit Salamèche de la part du Prof. Chen !*
  - ⚙ GivePokemon Charmander ×5
- *Veux-tu donner un surnom à Salamèche ?*
  - ⚙ OfferNickname
- **Régis** : Alors moi, je prends celui-là !
- *Régis reçoit {RivalStarterName} de la part du Prof. Chen !*
- **Régis** : Mon Pokémon a l'air bien plus fort que le tien. Ha !
- ⚑ GotStarter, Starter_Charmander

### `OaksLab_PokeBall_Charmander_LastOne`

*Parler* · priorité 20 · si GotStarter, Starter_Squirtle

- *C'est le dernier Pokémon du Prof. Chen.*
- *Il vaut mieux le laisser à sa place.*

### `OaksLab_PokeBall_Charmander_Empty`

*Parler* · priorité 10 · si GotStarter

- *La Poké Ball est vide.*

### `OaksLab_PokeBall_Squirtle_Look`

*Parler*

- *Une Poké Ball. Il y a un Pokémon à l'intérieur !*
- *Le Prof. Chen n'aimerait sans doute pas qu'on y touche...*

### `OaksLab_PokeBall_Squirtle_Offer`

*Parler* · priorité 5 · si LabIntroDone · sauf GotStarter

- **Prof. Chen** : Ah ! Carapuce, le Pokémon Eau ? Il se protège dans sa carapace et crache de l'eau.
- **Prof. Chen** : Tu choisis Carapuce ?
  - ▸ « Oui » → `OaksLab_ChooseSquirtle`
  - ▸ « Non » → `None`

### `OaksLab_ChooseSquirtle`

*Scène* · sauf GotStarter

- **Prof. Chen** : Excellent choix ! Ce Pokémon déborde d'énergie.
- *{PlayerName} reçoit Carapuce de la part du Prof. Chen !*
  - ⚙ GivePokemon Squirtle ×5
- *Veux-tu donner un surnom à Carapuce ?*
  - ⚙ OfferNickname
- **Régis** : Alors moi, je prends celui-là !
- *Régis reçoit {RivalStarterName} de la part du Prof. Chen !*
- **Régis** : Mon Pokémon a l'air bien plus fort que le tien. Ha !
- ⚑ GotStarter, Starter_Squirtle

### `OaksLab_PokeBall_Squirtle_LastOne`

*Parler* · priorité 20 · si GotStarter, Starter_Bulbasaur

- *C'est le dernier Pokémon du Prof. Chen.*
- *Il vaut mieux le laisser à sa place.*

### `OaksLab_PokeBall_Squirtle_Empty`

*Parler* · priorité 10 · si GotStarter

- *La Poké Ball est vide.*

### `OaksLab_Rival_Challenge`

*Scène* · si GotStarter · sauf BattledRivalLab

- **Régis** : Attends, {PlayerName} !
- **Régis** : Pépé nous a donné des Pokémon, alors on va voir ce qu'ils valent !
- **Régis** : Allez, en garde !
  - ⚙ StartTrainerBattle RivalLab

### `Trainer_RivalLab_PlayerWins`

*Scène*

- **Régis** : Quoi ?! Pas possible ! J'ai choisi le mauvais Pokémon !
- → `OaksLab_Rival_Leaves`

### `Trainer_RivalLab_PlayerLoses`

*Scène*

- **Régis** : Ouais ! Mon Pokémon est le meilleur !
- → `OaksLab_Rival_Leaves`

### `OaksLab_Rival_Leaves`

*Scène*

- **Régis** : Bon, je vais entraîner mon Pokémon et devenir encore plus fort.
- **Régis** : {PlayerName} ! Pépé ! À plus tard !
- **Prof. Chen** : Ce garçon ne tient vraiment pas en place...
- **Prof. Chen** : Vos deux Pokémon se sont bien battus. Laisse-moi soigner le tien.
  - ⚙ HealParty
- ⚑ BattledRivalLab

### `OaksLab_ProfOak_AfterStarter`

*Parler* · priorité 10 · si GotStarter

- **Prof. Chen** : Si un Pokémon sauvage surgit des hautes herbes, ton Pokémon peut le combattre.
- **Prof. Chen** : Avec lui à tes côtés, tu peux aller jusqu'à la ville voisine, Jadielle.

### `OaksLab_ProfOak_ParcelDelivery`

*Parler* · priorité 20 · si GotOaksParcel · sauf GotPokedex

- **Prof. Chen** : Oh, {PlayerName} ! Comment va ton Pokémon ?
- **Prof. Chen** : Il a l'air de bien t'aimer. Tu sais t'y prendre avec les Pokémon !
- **Prof. Chen** : Hmm ? Tu as quelque chose pour moi ?
- *{PlayerName} remet le Colis de Chen au Prof. Chen.*
  - ⚙ TakeItem OaksParcel ×1
- **Prof. Chen** : Ah ! C'est la Poké Ball spéciale que j'avais commandée. Merci beaucoup !
- **Régis** : Pépé ! Qu'est-ce que tu me voulais ?
- **Prof. Chen** : Ah, Régis ! Tu tombes bien. J'ai une faveur à vous demander, à tous les deux.
- **Prof. Chen** : Sur ce bureau, voici ma dernière invention : le Pokédex !
- **Prof. Chen** : Il enregistre automatiquement chaque Pokémon que tu vois ou que tu attrapes.
- **Prof. Chen** : C'est une encyclopédie high-tech !
- *{PlayerName} reçoit le Pokédex du Prof. Chen !*
  - ⚙ GiveItem Pokedex ×1
- **Prof. Chen** : Faire la liste complète des Pokémon du monde... C'était mon rêve.
- **Prof. Chen** : Mais je suis trop vieux pour ça, maintenant.
- **Prof. Chen** : Alors je vous confie cette mission. C'est un grand moment de l'histoire des Pokémon !
- **Régis** : T'inquiète, Pépé ! Je m'en occupe !
- **Régis** : {PlayerName}, je n'ai pas besoin de toi. Je vais demander une Carte à ma sœur, et à moi la route !
- **Régis** : Allez, salut !
- **Prof. Chen** : Ce Régis... Tiens, {PlayerName}, prends ceci aussi.
- *{PlayerName} obtient 5 Poké Balls !*
  - ⚙ GiveItem PokeBall ×5
- **Prof. Chen** : Quand un Pokémon sauvage est affaibli, lance-lui une Poké Ball pour l'attraper.
- **Prof. Chen** : Bonne route, {PlayerName} !
- ⚑ GotPokedex

### `OaksLab_ProfOak_PokedexRating`

*Parler* · priorité 30 · si GotPokedex

- **Prof. Chen** : Ah, {PlayerName} ! Montre-moi ton Pokédex.
- **Prof. Chen** : Tu as vu {SeenCount} Pokémon et tu en as attrapé {CaughtCount}.
- **Prof. Chen** : Continue ! Chaque Pokémon que tu rencontres remplit un peu plus le Pokédex.

### `OaksLab_ProfOak_AfterBadge`

*Parler* · priorité 40 · si GotBoulderBadge

- **Prof. Chen** : Tu as battu Pierre, à Argenta ? Formidable !
- **Prof. Chen** : Tes Pokémon et toi formez déjà une belle équipe.
- **Prof. Chen** : Tu as vu {SeenCount} Pokémon et tu en as attrapé {CaughtCount}. Ne t'arrête pas en si bon chemin !

### `OaksLab_Aide1_Talk`

*Parler*

- **Assistant** : Je suis l'assistant du Prof. Chen.
- **Assistant** : Il étudie les Pokémon depuis toujours. Il en sait plus que n'importe qui !

### `OaksLab_Aide2_Talk`

*Parler* · sauf MetOak

- **Assistante** : Le Prof. Chen est sorti. Il fait souvent des promenades le matin.

### `OaksLab_Aide2_AfterPokedex`

*Parler* · priorité 10 · si GotPokedex

- **Assistante** : Le Pokédex fonctionne ! Plus tu vois de Pokémon, plus il se remplit.
- **Assistante** : Et si tu les attrapes, tu obtiens encore plus d'informations.

### `OaksLab_Computer_Read`

*Parler*

- *Un e-mail est ouvert sur l'écran. / « Appel à tous les Dresseurs ! Les meilleurs d'entre vous attendent au Plateau Indigo. »*

### `OaksLab_Bookshelf_Read`

*Parler*

- *Des livres sur les Pokémon, couverts de notes du Prof. Chen.*

## Route 1

### `Route1_Sign_Read`

*Parler*

- *ROUTE 1 / BOURG PALETTE – JADIELLE*

### `Route1_MartClerk_Sample`

*Parler* · sauf GotPotionSample

- **Vendeur** : Bonjour ! Je travaille à la Boutique Pokémon de Jadielle.
- **Vendeur** : On y trouve tout ce qu'il faut pour partir à l'aventure.
- **Vendeur** : Tenez, un échantillon gratuit !
- *{PlayerName} obtient : Potion !*
  - ⚙ GiveItem Potion ×1
- **Vendeur** : Passez nous voir à Jadielle !
- ⚑ GotPotionSample

### `Route1_MartClerk_After`

*Parler* · priorité 10 · si GotPotionSample

- **Vendeur** : On vend des Potions et des Poké Balls à la Boutique de Jadielle. Bonne route !

### `Route1_Boy_Talk`

*Parler*

- **Gamin** : Tu vois ces petites corniches ? On peut sauter par-dessus pour redescendre.
- **Gamin** : C'est le raccourci pour rentrer à Bourg Palette !

## Jadielle

### `ViridianCity_Sign_Read`

*Parler*

- *JADIELLE / Le paradis éternellement vert.*

### `ViridianCity_TrainerTips1_Read`

*Parler*

- *CONSEIL DE DRESSEUR / Attrape plusieurs Pokémon : une équipe variée gagne plus facilement !*

### `ViridianCity_TrainerTips2_Read`

*Parler*

- *CONSEIL DE DRESSEUR / Affaiblis un Pokémon sauvage avant de lancer une Poké Ball. Il sera plus facile à attraper.*

### `ViridianCity_GymSign_Read`

*Parler*

- *ARÈNE POKÉMON DE JADIELLE / Champion : ???*

### `ViridianCity_GymDoor_Read`

*Parler*

- *La porte de l'Arène est fermée à clé...*

### `ViridianCity_Youngster_Talk`

*Parler*

- **Gamin** : Ces Poké Balls à ta ceinture... Tu as des Pokémon !
- **Gamin** : C'est génial de pouvoir les emmener partout avec soi, non ?

### `ViridianCity_GymWatcher_Talk`

*Parler*

- **Monsieur** : L'Arène de Jadielle est fermée depuis longtemps.
- **Monsieur** : Personne ne sait où est passé son Champion...

### `ViridianCity_GymWatcher_AfterBadge`

*Parler* · priorité 10 · si GotBoulderBadge

- **Monsieur** : On dit que le Champion de Jadielle serait le plus fort de tout Kanto.
- **Monsieur** : Mais tant que la porte reste fermée, impossible de le savoir !

### `ViridianCity_OldMan_Blocking`

*Scène* · sauf GotPokedex

- **Vieil homme** : Grmbl... On ne passe pas ! C'est une propriété privée !
- **Petite-fille** : Grand-père ! Ce n'est pas vrai, voyons !
- **Petite-fille** : Pardon... Il n'a pas encore bu son café, il est de mauvaise humeur.

### `ViridianCity_OldMan_Grumpy`

*Parler* · sauf GotPokedex

- **Vieil homme** : Grmbl... Pas de café, pas de passage.

### `ViridianCity_Granddaughter_Talk`

*Parler* · sauf GotPokedex

- **Petite-fille** : Grand-père est grognon le matin, tant qu'il n'a pas eu son café.
- **Petite-fille** : Repasse un peu plus tard, il sera de meilleure humeur !

### `ViridianCity_Granddaughter_After`

*Parler* · priorité 10 · si GotPokedex

- **Petite-fille** : Grand-père a bu son café. Il est tout content, maintenant !

### `ViridianCity_OldMan_OfferTutorial`

*Parler* · priorité 10 · si GotPokedex · sauf CatchTutorialSeen

- **Vieil homme** : Ah, mon café ! Ça va beaucoup mieux.
- **Vieil homme** : Hmm ? C'est un Pokédex, ça ? Tu sais attraper des Pokémon, au moins ?
- **Vieil homme** : Alors ?
  - ▸ « Oui » → `ViridianCity_OldMan_KnowsHow`
  - ▸ « Non » → `ViridianCity_OldMan_Tutorial`

### `ViridianCity_OldMan_KnowsHow`

*Scène*

- **Vieil homme** : Bien, bien. Alors je n'ai rien à t'apprendre. Bonne chasse !
- ⚑ CatchTutorialSeen

### `ViridianCity_OldMan_Tutorial`

*Scène*

- **Vieil homme** : Bon. Je vais te montrer. Regarde bien !
- **Vieil homme** : D'abord, on affaiblit le Pokémon...
  - ⚙ PlayCutscene CatchTutorial
- **Vieil homme** : Et voilà ! Ensuite, on lance la Poké Ball. Facile !
- **Vieil homme** : Plus le Pokémon est affaibli, plus il est facile à attraper.
- **Vieil homme** : S'il te faut des Poké Balls, va voir à la Boutique.
- ⚑ CatchTutorialSeen

### `ViridianCity_OldMan_After`

*Parler* · priorité 20 · si CatchTutorialSeen

- **Vieil homme** : Un bon café le matin, et la journée est réussie !
- **Vieil homme** : Attraper des Pokémon, c'est pareil : il faut de la patience.

### `ViridianSchool_Student_Talk`

*Parler*

- **Élève** : Je révise mes types ! Le Feu bat la Plante, l'Eau bat le Feu, la Plante bat l'Eau...
- **Élève** : Et la Roche résiste au Feu. Pas facile de tout retenir !

### `ViridianSchool_Blackboard_Read`

*Parler*

- *Au tableau : les problèmes de statut. / Sommeil, Poison, Paralysie, Brûlure et Gel. Ils durent même après le combat !*

### `ViridianPokeMart_Clerk_Parcel`

*Parler* · sauf GotOaksParcel

- **Vendeur** : Bonjour ! Tu viens de Bourg Palette ?
- **Vendeur** : Tu connais le Prof. Chen ? Son colis vient d'arriver.
- **Vendeur** : Tu veux bien le lui apporter ? Merci !
- *{PlayerName} obtient : Colis de Chen !*
  - ⚙ GiveItem OaksParcel ×1
- ⚑ GotOaksParcel

### `ViridianPokeMart_Clerk_Reminder`

*Parler* · priorité 10 · si GotOaksParcel · sauf GotPokedex

- **Vendeur** : Le Prof. Chen attend son colis à Bourg Palette. Je compte sur toi !

### `ViridianPokeMart_Clerk_Shop`

*Parler* · priorité 20 · si GotPokedex

- **Vendeur** : Bienvenue à la Boutique Pokémon ! Que puis-je faire pour vous ?
  - ⚙ OpenShop ViridianPokeMart

### `ViridianPokeMart_Customer_Talk`

*Parler*

- **Cliente** : Pour remplir un Pokédex, il faut beaucoup de Poké Balls !
- **Cliente** : Je fais toujours des réserves avant de partir.

### `ViridianPokemonCenter_Man_Talk`

*Parler*

- **Montagnard** : Ici, on soigne tes Pokémon gratuitement, autant de fois que tu veux.
- **Montagnard** : Profites-en avant de repartir !

### `ViridianPokemonCenter_Youngster_Talk`

*Parler*

- **Gamin** : Si tous tes Pokémon sont K.O., tu reviens au dernier Centre Pokémon visité.
- **Gamin** : Et tu perds un peu d'argent au passage. Fais attention !

## Route 22

### `Route22_Sign_Read`

*Parler*

- *ROUTE 22 / PORTE DE LA LIGUE POKÉMON*

### `Route22_Rival_Challenge`

*Scène* · si GotPokedex · sauf BattledRivalRoute22

- **Régis** : Hé ! {PlayerName} ! Tu vas à la Ligue Pokémon ?
- **Régis** : Laisse tomber ! Tu n'as même pas un seul badge !
- **Régis** : Le garde ne te laissera jamais passer.
- **Régis** : Mais dis-moi, ton Pokémon est devenu plus fort ? Voyons ça !
  - ⚙ StartTrainerBattle RivalRoute22

### `Trainer_RivalRoute22_PlayerWins`

*Scène*

- **Régis** : Pff ! J'ai pas eu de chance, c'est tout !
- → `Route22_Rival_Leaves`

### `Trainer_RivalRoute22_PlayerLoses`

*Scène*

- **Régis** : Ha ha ! Mes Pokémon sont bien meilleurs que les tiens !
- → `Battle_PlayerBlackout`

### `Route22_Rival_Leaves`

*Scène*

- **Régis** : Je vais à la Ligue Pokémon pour battre les meilleurs Dresseurs.
- **Régis** : Mais avant, je vais attraper plein de Pokémon pour compléter mon équipe.
- **Régis** : Salut, {PlayerName} ! À la prochaine !
- ⚑ BattledRivalRoute22

### `Route22_LeagueGateGuard_NoBadge`

*Parler*

- **Garde** : Seuls les Dresseurs de talent peuvent passer par ici.
- **Garde** : Montre-moi d'abord le Badge Roche, celui de l'Arène d'Argenta !

### `Route22_LeagueGateGuard_Badge`

*Parler* · priorité 10 · si GotBoulderBadge

- **Garde** : Oh ! C'est bien le Badge Roche !
- **Garde** : Mais la route du Plateau Indigo est en travaux. Reviens plus tard !

## Route 2

### `Route2_Sign_Read`

*Parler*

- *ROUTE 2 / JADIELLE – ARGENTA*

### `ViridianForestGateSouth_Guard_Talk`

*Parler*

- **Garde** : Tu vas traverser la Forêt de Jade ?
- **Garde** : C'est un vrai labyrinthe naturel. Ne t'y perds pas !

### `ViridianForestGateSouth_Girl_Talk`

*Parler*

- **Fillette** : Beaucoup de Pokémon Insecte vivent dans la forêt.
- **Fillette** : Attention à Aspicot : son dard est empoisonné !

### `ViridianForestGateNorth_Man_Talk`

*Parler*

- **Monsieur** : Te voilà de l'autre côté de la forêt ! Bravo.
- **Monsieur** : Argenta est juste au nord. Son Arène est réputée pour ses Pokémon Roche.

## Forêt de Jade

### `ViridianForest_Sign_Read`

*Parler*

- *FORÊT DE JADE / Attention : la forêt est un vrai labyrinthe !*

### `ViridianForest_TrainerTips1_Read`

*Parler*

- *CONSEIL DE DRESSEUR / Un Pokémon empoisonné perd des PV, même après le combat. Soigne-le vite !*

### `ViridianForest_TrainerTips2_Read`

*Parler*

- *CONSEIL DE DRESSEUR / Pour quitter un combat contre un Pokémon sauvage, choisis FUITE.*

### `ViridianForest_TrainerTips3_Read`

*Parler*

- *CONSEIL DE DRESSEUR / Ne gaspille pas tes Poké Balls : vise les Pokémon que tu n'as pas encore !*

### `ViridianForest_ExitSign_Read`

*Parler*

- *SORTIE DE LA FORÊT / ARGENTA AU NORD*

### `ViridianForest_Youngster_Talk`

*Parler*

- **Gamin** : Je suis venu avec des copains pour attraper des Pokémon Insecte.
- **Gamin** : Ils n'arrêtent pas de défier tous les Dresseurs qui passent !

### `ViridianForest_BugCatcherRick_Intro`

*Parler* · sauf Beaten_BugCatcherRick

- **Scout Rick** : Hé ! Tu as des Pokémon ! Allez, on se fait un combat !
  - ⚙ StartTrainerBattle BugCatcherRick

### `Trainer_BugCatcherRick_PlayerWins`

*Scène*

- **Scout Rick** : Non ! Mes Insectes n'ont rien pu faire !

### `ViridianForest_BugCatcherRick_After`

*Parler* · priorité 10 · si Beaten_BugCatcherRick

- **Scout Rick** : Quand ton Pokémon a du mal, n'hésite pas à en changer pendant le combat.

### `ViridianForest_BugCatcherDoug_Intro`

*Parler* · sauf Beaten_BugCatcherDoug

- **Scout Doug** : Ouais ! J'ai attrapé plein de Pokémon Insecte !
- **Scout Doug** : Tu vas voir ce qu'ils valent !
  - ⚙ StartTrainerBattle BugCatcherDoug

### `Trainer_BugCatcherDoug_PlayerWins`

*Scène*

- **Scout Doug** : Hein ? Déjà fini ?

### `ViridianForest_BugCatcherDoug_After`

*Parler* · priorité 10 · si Beaten_BugCatcherDoug

- **Scout Doug** : Aspicot devient Coconfort, puis Dardargnan. Les Insectes évoluent super vite !

### `ViridianForest_BugCatcherAnthony_Intro`

*Parler* · sauf Beaten_BugCatcherAnthony

- **Scout Anthony** : Des Insectes, j'en vois partout dans cette forêt !
- **Scout Anthony** : Et les miens sont les meilleurs !
  - ⚙ StartTrainerBattle BugCatcherAnthony

### `Trainer_BugCatcherAnthony_PlayerWins`

*Scène*

- **Scout Anthony** : Mes Chenipan n'étaient pas prêts...

### `ViridianForest_BugCatcherAnthony_After`

*Parler* · priorité 10 · si Beaten_BugCatcherAnthony

- **Scout Anthony** : Je me demande si je ne devrais pas attraper autre chose que des Insectes...

### `ViridianForest_BugCatcherCharlie_Intro`

*Parler* · sauf Beaten_BugCatcherCharlie

- **Scout Charlie** : Hé, attends ! Pourquoi tu cours comme ça ?
- **Scout Charlie** : Prends le temps de te battre !
  - ⚙ StartTrainerBattle BugCatcherCharlie

### `Trainer_BugCatcherCharlie_PlayerWins`

*Scène*

- **Scout Charlie** : J'aurais dû te laisser filer...

### `ViridianForest_BugCatcherCharlie_After`

*Parler* · priorité 10 · si Beaten_BugCatcherCharlie

- **Scout Charlie** : Chrysacier ne sait faire qu'Armure. Il faut être patient pour qu'il évolue !

### `ViridianForest_BugCatcherSammy_Intro`

*Parler* · sauf Beaten_BugCatcherSammy

- **Scout Sammy** : Je me cache dans les herbes pour surprendre les Dresseurs !
- **Scout Sammy** : Tu ne m'avais pas vu, hein ?
  - ⚙ StartTrainerBattle BugCatcherSammy

### `Trainer_BugCatcherSammy_PlayerWins`

*Scène*

- **Scout Sammy** : Raté ! Mon Aspicot n'a pas suffi...

### `ViridianForest_BugCatcherSammy_After`

*Parler* · priorité 10 · si Beaten_BugCatcherSammy

- **Scout Sammy** : Le dard d'Aspicot peut empoisonner. Garde un Antidote sur toi !

## Argenta

### `PewterCity_Sign_Read`

*Parler*

- *ARGENTA / Une ville grise comme la pierre.*

### `PewterCity_GymSign_Read`

*Parler*

- *ARÈNE POKÉMON D'ARGENTA / Champion : PIERRE / Le Dresseur solide comme le roc !*

### `PewterCity_MuseumSign_Read`

*Parler*

- *MUSÉE DES SCIENCES D'ARGENTA*

### `PewterCity_TrainerTips_Read`

*Parler*

- *CONSEIL DE DRESSEUR / Les Pokémon Roche craignent les attaques Eau et Plante !*

### `PewterCity_Youngster_Blocking`

*Scène* · sauf GotBoulderBadge

- **Gamin** : Hé ! Tu es Dresseur, non ?
- **Gamin** : Pierre, le Champion de l'Arène, cherche des adversaires !
- **Gamin** : Viens, je t'y emmène !
  - ⚙ PlayCutscene LeadToPewterGym
- **Gamin** : Voilà, c'est ici ! Bonne chance !

### `PewterCity_Youngster_Talk`

*Parler* · sauf GotBoulderBadge

- **Gamin** : Pierre est super fort. Ses Pokémon sont durs comme la pierre !

### `PewterCity_Youngster_AfterBadge`

*Parler* · priorité 10 · si GotBoulderBadge

- **Gamin** : Tu as battu Pierre ?! Incroyable !
- **Gamin** : La route de l'est est ouverte pour toi, maintenant.

### `PewterCity_Lass_Talk`

*Parler*

- **Fillette** : On a trouvé des fossiles près du Mont Sélénite.
- **Fillette** : Le musée en expose un, il est magnifique !

### `PewterCity_OldMan_Talk`

*Parler*

- **Vieil homme** : Le Mont Sélénite se trouve à l'est de la ville.
- **Vieil homme** : On raconte que des Pokémon venus de l'espace s'y réunissent, les nuits de pleine lune...

### `PewterCity_Man_Talk`

*Parler*

- **Monsieur** : Les Pokémon de Pierre encaissent très bien les coups.
- **Monsieur** : Les attaques Normal ne leur font presque rien. Pense à l'Eau ou à la Plante !

### `PewterMuseum_Receptionist_Welcome`

*Parler* · sauf PaidMuseum

- **Hôtesse** : Bienvenue au Musée des Sciences d'Argenta !
- **Hôtesse** : L'entrée coûte 50 ₽. Voulez-vous visiter le musée ?
  - ▸ « Oui » → `PewterMuseum_Receptionist_Pay`
  - ▸ « Non » → `PewterMuseum_Receptionist_Decline`

### `PewterMuseum_Receptionist_Pay`

*Scène*

- *{PlayerName} paie 50 ₽.*
  - ⚙ TakeMoney  ×50
- **Hôtesse** : Merci ! Bonne visite !
- ⚑ PaidMuseum

### `PewterMuseum_Receptionist_NoMoney`

*Scène*

- **Hôtesse** : Je suis navrée, vous n'avez pas assez d'argent.

### `PewterMuseum_Receptionist_Decline`

*Scène*

- **Hôtesse** : Revenez quand vous voulez !

### `PewterMuseum_Receptionist_Paid`

*Parler* · priorité 10 · si PaidMuseum

- **Hôtesse** : Bonne visite ! Le fossile de Kabutops est au fond de la salle.

### `PewterMuseum_KabutopsFossil_Read`

*Parler*

- *Fossile de Kabutops / Un Pokémon préhistorique qui vivait dans la mer, il y a très longtemps.*

### `PewterMuseum_AerodactylFossil_Read`

*Parler*

- *Fossile de Ptéra / Un Pokémon préhistorique volant, à la mâchoire pleine de crocs.*

### `PewterMuseum_MoonStone_Read`

*Parler*

- *Pierre Lune / Une météorite tombée sur le Mont Sélénite. Elle ferait évoluer certains Pokémon.*

### `PewterMuseum_SpaceShuttle_Read`

*Parler*

- *Maquette de navette spatiale / Une vraie navette comme celle-ci est allée jusque dans l'espace !*

### `PewterMuseum_Scientist_Talk`

*Parler*

- **Scientifique** : Ce fossile vient du Mont Sélénite.
- **Scientifique** : Un jour, la science pourra peut-être ramener ces Pokémon à la vie...

### `PewterMuseum_OldMan_Talk`

*Parler*

- **Vieil homme** : Je viens ici tous les jours. Ces vieux os me rappellent ma jeunesse !
- **Vieil homme** : Enfin... pas tant que ça. Ha ha !

### `PewterGym_Guide_Welcome`

*Parler* · sauf GotBoulderBadge

- **Guide** : Hé, toi ! Tu veux devenir Champion ?
- **Guide** : Je ne suis pas Dresseur, mais je connais des astuces pour gagner.
- **Guide** : Tu veux un conseil ?
  - ▸ « Oui » → `PewterGym_Guide_Advice`
  - ▸ « Non » → `PewterGym_Guide_NoAdvice`

### `PewterGym_Guide_Advice`

*Scène*

- **Guide** : Pierre utilise des Pokémon Roche.
- **Guide** : Ils résistent aux attaques Normal, Feu et Vol, mais craignent l'Eau et la Plante !
- **Guide** : Si ton équipe n'en a pas, entraîne-toi bien dans la Forêt de Jade avant de l'affronter.

### `PewterGym_Guide_NoAdvice`

*Scène*

- **Guide** : D'accord ! Alors, fonce !

### `PewterGym_Guide_AfterBadge`

*Parler* · priorité 10 · si GotBoulderBadge

- **Guide** : Bravo ! Tu as battu Pierre !
- **Guide** : Tu as l'étoffe d'un grand Dresseur, crois-moi.

### `PewterGym_Statue_Read`

*Parler* · sauf GotBoulderBadge

- *ARÈNE POKÉMON D'ARGENTA / Champion : PIERRE / Dresseurs victorieux : Régis*

### `PewterGym_Statue_ReadAfterBadge`

*Parler* · priorité 10 · si GotBoulderBadge

- *ARÈNE POKÉMON D'ARGENTA / Champion : PIERRE / Dresseurs victorieux : Régis, {PlayerName}*

### `PewterGym_CamperLiam_Intro`

*Parler* · sauf Beaten_CamperLiam

- **Campeur Liam** : Halte !
- **Campeur Liam** : Tu es encore à des années-lumière de pouvoir affronter Pierre !
  - ⚙ StartTrainerBattle CamperLiam

### `Trainer_CamperLiam_PlayerWins`

*Scène*

- **Campeur Liam** : Zut ! Les années-lumière, ça mesure une distance, pas le temps !

### `PewterGym_CamperLiam_After`

*Parler* · priorité 10 · si Beaten_CamperLiam

- **Campeur Liam** : Pas mal du tout... Mais Pierre est bien plus coriace que moi. Bonne chance !

### `PewterGym_Brock_Intro`

*Parler* · sauf Beaten_Brock

- **Pierre** : Alors, te voilà.
- **Pierre** : Je suis Pierre, le Champion de l'Arène d'Argenta.
- **Pierre** : Ma volonté est solide comme le roc. Et mes Pokémon aussi !
- **Pierre** : Ils sont tous de type Roche. Ha ha !
- **Pierre** : Tu viens me défier en sachant que tu vas perdre ?
- **Pierre** : C'est l'honneur du Dresseur qui te pousse, n'est-ce pas ? Très bien.
- **Pierre** : Montre-moi ce que tu vaux !
  - ⚙ StartTrainerBattle Brock

### `Trainer_Brock_PlayerWins`

*Scène*

- **Pierre** : J'ai sous-estimé ta force...
- **Pierre** : Tu as gagné. Je le reconnais.
- → `PewterGym_Brock_Reward`

### `PewterGym_Brock_Reward`

*Scène*

- **Pierre** : En reconnaissance de ta victoire, voici le Badge Roche de la Ligue Pokémon.
- *{PlayerName} reçoit le Badge Roche de la part de Pierre !*
  - ⚙ GiveBadge BoulderBadge
- **Pierre** : Il prouve ta victoire à Argenta. Il existe huit badges dans tout Kanto.
- **Pierre** : Attends, prends aussi ceci.
- *{PlayerName} obtient : CT39 !*
  - ⚙ GiveItem TM39 ×1
- **Pierre** : Une CT, ou Capsule Technique, permet d'apprendre une attaque à un Pokémon.
- **Pierre** : La CT39 contient Tomberoche. Elle inflige des dégâts et baisse la Vitesse de l'adversaire.
- **Pierre** : Choisis bien à qui tu l'apprends !
- ⚑ GotBoulderBadge

### `PewterGym_Brock_After`

*Parler* · priorité 10 · si Beaten_Brock

- **Pierre** : Le monde est vaste, et il y a beaucoup de Dresseurs plus forts que moi.
- **Pierre** : Je vais continuer à m'entraîner, moi aussi. On se reverra !

### `PewterPokemonCenter_Gentleman_Talk`

*Parler*

- **Gentleman** : Si tes Pokémon sont fatigués après l'Arène, reviens ici.
- **Gentleman** : L'infirmière Joëlle s'occupe de tout, et c'est gratuit !

### `PewterPokemonCenter_Jigglypuff_Talk`

*Parler*

- **Rondoudou** : Rondou... Rondouuu ♪
  - ⚙ PlayCutscene JigglypuffSong
- *Rondoudou chante une douce berceuse... On se sent tout à coup un peu somnolent.*

### `PewterPokeMart_Clerk_Shop`

*Parler*

- **Vendeur** : Bienvenue à la Boutique Pokémon ! Que puis-je faire pour vous ?
  - ⚙ OpenShop PewterPokeMart

### `PewterPokeMart_Customer_Talk`

*Parler*

- **Gamin** : Un Repousse éloigne les Pokémon sauvages faibles.
- **Gamin** : Pratique pour traverser la forêt sans être embêté !

## Partout (PNJ génériques et système)

### `PokemonCenter_Nurse_Welcome`

*Parler*

- **Infirmière Joëlle** : Bienvenue au Centre Pokémon !
- **Infirmière Joëlle** : Nous soignons vos Pokémon et leur rendons toute leur énergie.
- **Infirmière Joëlle** : Voulez-vous que je soigne vos Pokémon ?
  - ▸ « Oui » → `PokemonCenter_Nurse_Heal`
  - ▸ « Non » → `PokemonCenter_Nurse_Decline`

### `PokemonCenter_Nurse_Heal`

*Scène*

- **Infirmière Joëlle** : Très bien. Je prends vos Pokémon un petit instant.
- **Infirmière Joëlle** : Merci d'avoir patienté. Vos Pokémon sont en pleine forme !
  - ⚙ HealParty
- **Infirmière Joëlle** : À bientôt !

### `PokemonCenter_Nurse_Decline`

*Scène*

- **Infirmière Joëlle** : À bientôt !

### `PokemonCenter_Nurse_AfterBlackout`

*Scène*

- **Infirmière Joëlle** : Oh ! Vos Pokémon étaient à bout de forces...
- **Infirmière Joëlle** : Nous les avons soignés. Ils sont de nouveau en pleine forme !
- **Infirmière Joëlle** : Faites bien attention à vous sur la route. À bientôt !

### `PokemonCenter_PC_Use`

*Parler*

- *{PlayerName} allume le PC.*
  - ⚙ OpenPC

### `Battle_PlayerBlackout`

*Scène*

- *{PlayerName} n'a plus de Pokémon en état de se battre !*
- *{PlayerName} perd {MoneyLost} ₽...*
- *... et se précipite au dernier Centre Pokémon visité.*
  - ⚙ ReturnToLastHeal
