"""Textes des dialogues de la V1 (Bourg Palette -> badge de Pierre).

C'est ICI qu'on écrit et corrige les dialogues. Ensuite :
    python Tools/Data/build_dialogues.py
génère Data/Dialogues/DT_Npcs.json, DT_Dialogues.json et SCRIPT_V1.md (version lisible),
après avoir vérifié les références (PNJ, conversations, drapeaux, dresseurs, objets).

Écriture d'une réplique :
    "Texte"                    -> dit par le PNJ de la conversation
    "@ProfOak Texte"           -> dit par un autre PNJ (identifiant de NPCS)
    "@Narrator Texte"          -> narration, sans nom affiché
    L("Texte", ev="GiveItem", p="Potion", n=1)        -> réplique + événement
    L("Texte ?", choices=[("Oui", "Conv_Oui"), ("Non", None)])

Le joueur ne parle jamais. Il peut être fille ou garçon : pas d'accord au féminin/masculin
qui le concerne (« Te voilà ! » plutôt que « Tu es arrivé ! »).
Variables remplacées en jeu : {PlayerName}, {StarterName}, {RivalStarterName},
{SeenCount}, {CaughtCount}, {MoneyLost}.
"""

# ------------------------------------------------------------------ Drapeaux d'histoire
# Propres à chaque joueur (sauvegardés et répliqués avec lui). Les drapeaux Beaten_<Dresseur>
# sont posés par le système de combat quand le joueur gagne, ils ne sont pas listés ici.
FLAGS = {
    "MetOak": "Le Prof. Chen a arrêté le joueur au bord des hautes herbes",
    "LabIntroDone": "Scène du labo vue, le joueur peut choisir son starter",
    "GotStarter": "Le joueur a reçu son starter",
    "Starter_Bulbasaur": "Starter choisi : Bulbizarre",
    "Starter_Charmander": "Starter choisi : Salamèche",
    "Starter_Squirtle": "Starter choisi : Carapuce",
    "BattledRivalLab": "Combat du labo fini (gagné ou perdu)",
    "GotPotionSample": "Potion offerte par le vendeur sur la Route 1",
    "GotOaksParcel": "Colis de Chen reçu à la Boutique de Jadielle",
    "GotPokedex": "Colis livré, Pokédex et Poké Balls reçus",
    "GotTownMap": "Carte reçue de Nina",
    "CatchTutorialSeen": "Démonstration de capture du vieil homme de Jadielle vue",
    "BattledRivalRoute22": "Combat contre Régis sur la Route 22 gagné",
    "PaidMuseum": "Entrée du musée d'Argenta payée",
    "GotBoulderBadge": "Badge Roche et CT39 reçus de Pierre",
}

# Objets donnés ou repris (futur DT_Items). Badges à part.
ITEMS = {"Potion", "OaksParcel", "Pokedex", "PokeBall", "TownMap", "TM39"}
BADGES = {"BoulderBadge"}
SHOPS = {"ViridianPokeMart", "PewterPokeMart"}
CUTSCENES = {
    "OakLeadsToLab": "Chen ramène le joueur au labo",
    "CatchTutorial": "Le vieil homme attrape un Roucool devant le joueur",
    "LeadToPewterGym": "Le garçon d'Argenta conduit le joueur à l'arène",
    "JigglypuffSong": "Rondoudou chante, l'écran se brouille un instant",
}

# ------------------------------------------------------------------ PNJ, panneaux, objets
# (identifiant, nom affiché, genre, lieu, dresseur de DT_Trainers)
# Genre : Character (nom affiché au-dessus du texte), Sign, Object, System (narration).
# Lieu None = PNJ générique présent dans plusieurs lieux (infirmière, PC).
NPCS = [
    ("Narrator", "", "System", None, None),
    # Personnages principaux (un seul identifiant, quel que soit le bâtiment)
    ("Mom", "Maman", "Character", "PalletTown", None),
    ("ProfOak", "Prof. Chen", "Character", "PalletTown", None),
    ("Rival", "Régis", "Character", "PalletTown", None),
    ("Daisy", "Nina", "Character", "PalletTown", None),
    # Bourg Palette
    ("PalletTown_Sign", "", "Sign", "PalletTown", None),
    ("PalletTown_PlayerHouseSign", "", "Sign", "PalletTown", None),
    ("PalletTown_RivalHouseSign", "", "Sign", "PalletTown", None),
    ("PalletTown_OaksLabSign", "", "Sign", "PalletTown", None),
    ("PalletTown_Girl", "Fillette", "Character", "PalletTown", None),
    ("PalletTown_TechGuy", "Gros bonhomme", "Character", "PalletTown", None),
    ("PlayerHouse_TV", "", "Object", "PalletTown", None),
    ("PlayerHouse_Console", "", "Object", "PalletTown", None),
    ("PlayerHouse_PC", "", "Object", "PalletTown", None),
    ("PlayerHouse_Bookshelf", "", "Object", "PalletTown", None),
    ("RivalHouse_Painting", "", "Object", "PalletTown", None),
    ("OaksLab_Aide1", "Assistant", "Character", "PalletTown", None),
    ("OaksLab_Aide2", "Assistante", "Character", "PalletTown", None),
    ("OaksLab_Computer", "", "Object", "PalletTown", None),
    ("OaksLab_Bookshelf", "", "Object", "PalletTown", None),
    ("OaksLab_PokeBall_Bulbasaur", "", "Object", "PalletTown", None),
    ("OaksLab_PokeBall_Charmander", "", "Object", "PalletTown", None),
    ("OaksLab_PokeBall_Squirtle", "", "Object", "PalletTown", None),
    # Route 1
    ("Route1_Sign", "", "Sign", "Route1", None),
    ("Route1_MartClerk", "Vendeur", "Character", "Route1", None),
    ("Route1_Boy", "Gamin", "Character", "Route1", None),
    # Jadielle
    ("ViridianCity_Sign", "", "Sign", "ViridianCity", None),
    ("ViridianCity_TrainerTips1", "", "Sign", "ViridianCity", None),
    ("ViridianCity_TrainerTips2", "", "Sign", "ViridianCity", None),
    ("ViridianCity_GymSign", "", "Sign", "ViridianCity", None),
    ("ViridianCity_GymDoor", "", "Object", "ViridianCity", None),
    ("ViridianCity_OldMan", "Vieil homme", "Character", "ViridianCity", None),
    ("ViridianCity_Granddaughter", "Petite-fille", "Character", "ViridianCity", None),
    ("ViridianCity_Youngster", "Gamin", "Character", "ViridianCity", None),
    ("ViridianCity_GymWatcher", "Monsieur", "Character", "ViridianCity", None),
    ("ViridianSchool_Student", "Élève", "Character", "ViridianCity", None),
    ("ViridianSchool_Blackboard", "", "Object", "ViridianCity", None),
    ("ViridianPokeMart_Clerk", "Vendeur", "Character", "ViridianCity", None),
    ("ViridianPokeMart_Customer", "Cliente", "Character", "ViridianCity", None),
    ("ViridianPokemonCenter_Man", "Montagnard", "Character", "ViridianCity", None),
    ("ViridianPokemonCenter_Youngster", "Gamin", "Character", "ViridianCity", None),
    # Centres Pokémon (générique, Jadielle et Argenta)
    ("PokemonCenterNurse", "Infirmière Joëlle", "Character", None, None),
    ("PokemonCenter_PC", "", "Object", None, None),
    # Route 22
    ("Route22_Sign", "", "Sign", "Route22", None),
    ("Route22_LeagueGateGuard", "Garde", "Character", "Route22", None),
    # Route 2 et postes de la Forêt de Jade
    ("Route2_Sign", "", "Sign", "Route2", None),
    ("ViridianForestGateSouth_Guard", "Garde", "Character", "Route2", None),
    ("ViridianForestGateSouth_Girl", "Fillette", "Character", "Route2", None),
    ("ViridianForestGateNorth_Man", "Monsieur", "Character", "Route2", None),
    # Forêt de Jade
    ("ViridianForest_Sign", "", "Sign", "ViridianForest", None),
    ("ViridianForest_TrainerTips1", "", "Sign", "ViridianForest", None),
    ("ViridianForest_TrainerTips2", "", "Sign", "ViridianForest", None),
    ("ViridianForest_TrainerTips3", "", "Sign", "ViridianForest", None),
    ("ViridianForest_ExitSign", "", "Sign", "ViridianForest", None),
    ("ViridianForest_Youngster", "Gamin", "Character", "ViridianForest", None),
    ("ViridianForest_BugCatcherRick", "Scout Rick", "Character", "ViridianForest", "BugCatcherRick"),
    ("ViridianForest_BugCatcherDoug", "Scout Doug", "Character", "ViridianForest", "BugCatcherDoug"),
    ("ViridianForest_BugCatcherAnthony", "Scout Anthony", "Character", "ViridianForest", "BugCatcherAnthony"),
    ("ViridianForest_BugCatcherCharlie", "Scout Charlie", "Character", "ViridianForest", "BugCatcherCharlie"),
    ("ViridianForest_BugCatcherSammy", "Scout Sammy", "Character", "ViridianForest", "BugCatcherSammy"),
    # Argenta
    ("PewterCity_Sign", "", "Sign", "PewterCity", None),
    ("PewterCity_GymSign", "", "Sign", "PewterCity", None),
    ("PewterCity_MuseumSign", "", "Sign", "PewterCity", None),
    ("PewterCity_TrainerTips", "", "Sign", "PewterCity", None),
    ("PewterCity_Youngster", "Gamin", "Character", "PewterCity", None),
    ("PewterCity_Lass", "Fillette", "Character", "PewterCity", None),
    ("PewterCity_OldMan", "Vieil homme", "Character", "PewterCity", None),
    ("PewterCity_Man", "Monsieur", "Character", "PewterCity", None),
    ("PewterMuseum_Receptionist", "Hôtesse", "Character", "PewterCity", None),
    ("PewterMuseum_Scientist", "Scientifique", "Character", "PewterCity", None),
    ("PewterMuseum_OldMan", "Vieil homme", "Character", "PewterCity", None),
    ("PewterMuseum_KabutopsFossil", "", "Object", "PewterCity", None),
    ("PewterMuseum_AerodactylFossil", "", "Object", "PewterCity", None),
    ("PewterMuseum_MoonStone", "", "Object", "PewterCity", None),
    ("PewterMuseum_SpaceShuttle", "", "Object", "PewterCity", None),
    ("PewterGym_Guide", "Guide", "Character", "PewterCity", None),
    ("PewterGym_Statue", "", "Object", "PewterCity", None),
    ("PewterGym_CamperLiam", "Campeur Liam", "Character", "PewterCity", "CamperLiam"),
    ("PewterGym_Brock", "Pierre", "Character", "PewterCity", "Brock"),
    ("PewterPokemonCenter_Gentleman", "Gentleman", "Character", "PewterCity", None),
    ("PewterPokemonCenter_Jigglypuff", "Rondoudou", "Character", "PewterCity", None),
    ("PewterPokeMart_Clerk", "Vendeur", "Character", "PewterCity", None),
    ("PewterPokeMart_Customer", "Gamin", "Character", "PewterCity", None),
]


# ------------------------------------------------------------------ Outils d'écriture
def L(text, who=None, ev=None, p=None, n=0, fail=None, choices=None):
    """Une réplique avec options (voir l'en-tête)."""
    return {"text": text, "who": who, "ev": ev, "p": p, "n": n, "fail": fail, "choices": choices or []}


CONVERSATIONS = []


def conv(name, npc, lines, trigger="Talk", prio=0, req=(), block=(), sets=(), next=None):
    CONVERSATIONS.append({
        "name": name, "npc": npc, "lines": lines, "trigger": trigger, "prio": prio,
        "req": list(req), "block": list(block), "sets": list(sets), "next": next,
    })


def sign(npc, text, **kw):
    conv(f"{npc}_Read", npc, [text], **kw)


def heal_line(text):
    return L(text, ev="HealParty")


# ================================================================== BOURG PALETTE
sign("PalletTown_Sign", "BOURG PALETTE\nDes couleurs pures et paisibles.")
sign("PalletTown_PlayerHouseSign", "Maison de {PlayerName}")
sign("PalletTown_RivalHouseSign", "Maison de Régis")
sign("PalletTown_OaksLabSign", "LABORATOIRE POKÉMON DU PROF. CHEN")

conv("PalletTown_Girl_Talk", "PalletTown_Girl", [
    "Moi aussi, j'élève des Pokémon !",
    "Quand ils seront grands et forts, ce sont eux qui me protégeront.",
])
conv("PalletTown_TechGuy_Talk", "PalletTown_TechGuy", [
    "La technologie, c'est fou !",
    "Aujourd'hui, on peut ranger des Pokémon et des objets dans un PC, comme de simples données !",
])

# ----- Le Prof. Chen arrête le joueur au bord des hautes herbes (volume au nord de la ville)
conv("PalletTown_ProfOak_StopGrass", "ProfOak", [
    "Hé ! Attends ! Ne va pas plus loin !",
    "C'est dangereux ! Des Pokémon sauvages vivent dans les hautes herbes !",
    "Il te faut ton propre Pokémon pour te protéger.",
    L("Allez, suis-moi !", ev="PlayCutscene", p="OakLeadsToLab"),
], trigger="Script", block=["MetOak"], sets=["MetOak"], next="OaksLab_Intro")

# ----- Maison du joueur
conv("PlayerHouse_Mom_Default", "Mom", [
    "Un jour ou l'autre, tout le monde quitte la maison pour partir à l'aventure.",
    "Enfin, c'est ce qu'ils disent à la télé.",
    "Au fait, le Prof. Chen, notre voisin, te cherchait tout à l'heure.",
])
conv("PlayerHouse_Mom_AfterStarter", "Mom", [
    "{PlayerName} ! Le Prof. Chen t'a confié un Pokémon ? C'est merveilleux !",
    "Viens donc te reposer un peu, tous les deux.",
    heal_line("@Narrator {PlayerName} et {StarterName} reprennent des forces."),
    "Voilà ! Ton Pokémon a l'air en pleine forme. Fais bien attention à toi !",
], prio=10, req=["GotStarter"])
conv("PlayerHouse_Mom_AfterPokedex", "Mom", [
    "Le Prof. Chen compte sur toi pour remplir son Pokédex ? Quelle aventure !",
    "Repose-toi avant de repartir.",
    heal_line("@Narrator Toute l'équipe est en pleine forme."),
    "Et repasse me voir de temps en temps, d'accord ?",
], prio=20, req=["GotPokedex"])
conv("PlayerHouse_Mom_AfterBadge", "Mom", [
    "Le Badge Roche ? Tu as battu le Champion d'Argenta ?",
    "Je suis si fière de toi ! Allez, un peu de repos.",
    heal_line("@Narrator Toute l'équipe est en pleine forme."),
], prio=30, req=["GotBoulderBadge"])

conv("PlayerHouse_Mom_AfterBlackout", "Mom", [
    "{PlayerName} ! Te voilà ! Tes Pokémon avaient l'air à bout de forces...",
    "@Narrator Toute l'équipe a repris des forces.",
    "Ne force pas trop, d'accord ? Je suis toujours là si besoin.",
], trigger="Script")

sign("PlayerHouse_TV", "Un film passe à la télé. Quatre garçons marchent le long d'une voie ferrée...\nBon, il est temps d'y aller.")
sign("PlayerHouse_Console", "Une vieille console de jeux.\nPas le temps de jouer, l'aventure attend !")
conv("PlayerHouse_PC_Use", "PlayerHouse_PC", [
    L("@Narrator {PlayerName} allume le PC.", ev="OpenPC"),
])
sign("PlayerHouse_Bookshelf", "Plein de livres sur les Pokémon. Que des classiques !")

# ----- Maison du rival
conv("RivalHouse_Daisy_Default", "Daisy", [
    "Salut, {PlayerName} ! Régis est parti au labo de Grand-père.",
    "Il ne tenait plus en place, ce matin...",
])
conv("RivalHouse_Daisy_AfterStarter", "Daisy", [
    "Grand-père t'a donné un Pokémon ? Je peux te donner un conseil ?",
    "Parle-lui souvent. Un Pokémon sent quand on tient à lui.",
], prio=10, req=["GotStarter"])
conv("RivalHouse_Daisy_GiveTownMap", "Daisy", [
    "Grand-père t'a demandé un service, c'est ça ?",
    "Alors prends ça, ça te sera utile.",
    L("@Narrator {PlayerName} obtient : Carte !", ev="GiveItem", p="TownMap", n=1),
    "C'est une carte de Kanto. Elle te montre toujours où tu te trouves.",
], prio=20, req=["GotPokedex"], block=["GotTownMap"], sets=["GotTownMap"])
conv("RivalHouse_Daisy_AfterTownMap", "Daisy", [
    "La Carte est rangée dans ton Sac, avec les objets rares.",
    "Mon frère est parti vers Jadielle comme une flèche. Il ne changera jamais !",
], prio=30, req=["GotTownMap"])
sign("RivalHouse_Painting", "Un joli tableau. On dirait un paysage de Kanto vu du ciel.")

# ----- Labo du Prof. Chen
conv("OaksLab_Rival_Waiting", "Rival", [
    "Tiens, {PlayerName} ! Pépé n'est pas là.",
    "Il m'a dit de passer, il va me donner un Pokémon. Toi, rentre chez toi !",
], block=["MetOak"])
conv("OaksLab_Intro", "Rival", [
    "Pépé ! J'en ai marre d'attendre !",
    "@ProfOak Régis ? Ah, c'est vrai, je t'avais dit de venir. Patiente un peu.",
    "@ProfOak Bien. {PlayerName}, approche.",
    "@ProfOak Sur cette table, il y a trois Poké Balls. Chacune contient un Pokémon.",
    "@ProfOak Quand j'étais jeune, j'étais un Dresseur redoutable, tu sais.",
    "@ProfOak Aujourd'hui, il ne me reste que ces trois-là. Je t'en confie un. Vas-y, choisis !",
    "Hé ! Pépé ! Et moi, alors ?",
    "@ProfOak Sois patient, Régis ! Tu auras le tien juste après.",
], trigger="Script", req=["MetOak"], block=["LabIntroDone"], sets=["LabIntroDone"])
conv("OaksLab_Rival_BeforeChoice", "Rival", [
    "Vas-y, choisis ! De toute façon, je prendrai le meilleur après toi.",
], prio=5, req=["LabIntroDone"], block=["GotStarter"])
conv("OaksLab_ProfOak_BeforeChoice", "ProfOak", [
    "Alors, {PlayerName} ? Choisis l'un de ces trois Pokémon. Prends ton temps !",
], prio=5, req=["LabIntroDone"], block=["GotStarter"])

STARTERS = [
    ("Bulbasaur", "Bulbizarre", "Plante", "Il est facile à élever et très résistant."),
    ("Charmander", "Salamèche", "Feu", "La flamme au bout de sa queue montre sa force."),
    ("Squirtle", "Carapuce", "Eau", "Il se protège dans sa carapace et crache de l'eau."),
]
# Le rival prend toujours le starter qui a l'avantage (voir RivalLab_* dans DT_Trainers)
LEFT_BALL = {"Bulbasaur": "Starter_Charmander", "Charmander": "Starter_Squirtle", "Squirtle": "Starter_Bulbasaur"}

for species, fr, type_fr, hint in STARTERS:
    ball = f"OaksLab_PokeBall_{species}"
    conv(f"{ball}_Look", ball, [
        "Une Poké Ball. Il y a un Pokémon à l'intérieur !",
        "Le Prof. Chen n'aimerait sans doute pas qu'on y touche...",
    ])
    conv(f"{ball}_Offer", ball, [
        f"@ProfOak Ah ! {fr}, le Pokémon {type_fr} ? {hint}",
        L(f"@ProfOak Tu choisis {fr} ?", choices=[("Oui", f"OaksLab_Choose{species}"), ("Non", None)]),
    ], prio=5, req=["LabIntroDone"], block=["GotStarter"])
    conv(f"OaksLab_Choose{species}", "ProfOak", [
        "Excellent choix ! Ce Pokémon déborde d'énergie.",
        L(f"@Narrator {{PlayerName}} reçoit {fr} de la part du Prof. Chen !", ev="GivePokemon", p=species, n=5),
        L(f"@Narrator Veux-tu donner un surnom à {fr} ?", ev="OfferNickname"),
        "@Rival Alors moi, je prends celui-là !",
        "@Narrator Régis reçoit {RivalStarterName} de la part du Prof. Chen !",
        "@Rival Mon Pokémon a l'air bien plus fort que le tien. Ha !",
    ], trigger="Script", block=["GotStarter"], sets=["GotStarter", f"Starter_{species}"])
    conv(f"{ball}_LastOne", ball, [
        "C'est le dernier Pokémon du Prof. Chen.",
        "Il vaut mieux le laisser à sa place.",
    ], prio=20, req=["GotStarter", LEFT_BALL[species]])
    conv(f"{ball}_Empty", ball, [
        "La Poké Ball est vide.",
    ], prio=10, req=["GotStarter"])

# ----- Premier combat contre Régis (volume devant la porte du labo)
conv("OaksLab_Rival_Challenge", "Rival", [
    "Attends, {PlayerName} !",
    "Pépé nous a donné des Pokémon, alors on va voir ce qu'ils valent !",
    L("Allez, en garde !", ev="StartTrainerBattle", p="RivalLab"),
], trigger="Script", req=["GotStarter"], block=["BattledRivalLab"])
conv("Trainer_RivalLab_PlayerWins", "Rival", [
    "Quoi ?! Pas possible ! J'ai choisi le mauvais Pokémon !",
], trigger="Script", next="OaksLab_Rival_Leaves")
conv("Trainer_RivalLab_PlayerLoses", "Rival", [
    "Ouais ! Mon Pokémon est le meilleur !",
], trigger="Script", next="OaksLab_Rival_Leaves")
conv("OaksLab_Rival_Leaves", "Rival", [
    "Bon, je vais entraîner mon Pokémon et devenir encore plus fort.",
    "{PlayerName} ! Pépé ! À plus tard !",
    "@ProfOak Ce garçon ne tient vraiment pas en place...",
    heal_line("@ProfOak Vos deux Pokémon se sont bien battus. Laisse-moi soigner le tien."),
], trigger="Script", sets=["BattledRivalLab"])

conv("OaksLab_ProfOak_AfterStarter", "ProfOak", [
    "Si un Pokémon sauvage surgit des hautes herbes, ton Pokémon peut le combattre.",
    "Avec lui à tes côtés, tu peux aller jusqu'à la ville voisine, Jadielle.",
], prio=10, req=["GotStarter"])
conv("OaksLab_ProfOak_ParcelDelivery", "ProfOak", [
    "Oh, {PlayerName} ! Comment va ton Pokémon ?",
    "Il a l'air de bien t'aimer. Tu sais t'y prendre avec les Pokémon !",
    "Hmm ? Tu as quelque chose pour moi ?",
    L("@Narrator {PlayerName} remet le Colis de Chen au Prof. Chen.", ev="TakeItem", p="OaksParcel", n=1),
    "Ah ! C'est la Poké Ball spéciale que j'avais commandée. Merci beaucoup !",
    "@Rival Pépé ! Qu'est-ce que tu me voulais ?",
    "Ah, Régis ! Tu tombes bien. J'ai une faveur à vous demander, à tous les deux.",
    "Sur ce bureau, voici ma dernière invention : le Pokédex !",
    "Il enregistre automatiquement chaque Pokémon que tu vois ou que tu attrapes.",
    "C'est une encyclopédie high-tech !",
    L("@Narrator {PlayerName} reçoit le Pokédex du Prof. Chen !", ev="GiveItem", p="Pokedex", n=1),
    "Faire la liste complète des Pokémon du monde... C'était mon rêve.",
    "Mais je suis trop vieux pour ça, maintenant.",
    "Alors je vous confie cette mission. C'est un grand moment de l'histoire des Pokémon !",
    "@Rival T'inquiète, Pépé ! Je m'en occupe !",
    "@Rival {PlayerName}, je n'ai pas besoin de toi. Je vais demander une Carte à ma sœur, et à moi la route !",
    "@Rival Allez, salut !",
    "Ce Régis... Tiens, {PlayerName}, prends ceci aussi.",
    L("@Narrator {PlayerName} obtient 5 Poké Balls !", ev="GiveItem", p="PokeBall", n=5),
    "Quand un Pokémon sauvage est affaibli, lance-lui une Poké Ball pour l'attraper.",
    "Bonne route, {PlayerName} !",
], prio=20, req=["GotOaksParcel"], block=["GotPokedex"], sets=["GotPokedex"])
conv("OaksLab_ProfOak_PokedexRating", "ProfOak", [
    "Ah, {PlayerName} ! Montre-moi ton Pokédex.",
    "Tu as vu {SeenCount} Pokémon et tu en as attrapé {CaughtCount}.",
    "Continue ! Chaque Pokémon que tu rencontres remplit un peu plus le Pokédex.",
], prio=30, req=["GotPokedex"])
conv("OaksLab_ProfOak_AfterBadge", "ProfOak", [
    "Tu as battu Pierre, à Argenta ? Formidable !",
    "Tes Pokémon et toi formez déjà une belle équipe.",
    "Tu as vu {SeenCount} Pokémon et tu en as attrapé {CaughtCount}. Ne t'arrête pas en si bon chemin !",
], prio=40, req=["GotBoulderBadge"])

conv("OaksLab_Aide1_Talk", "OaksLab_Aide1", [
    "Je suis l'assistant du Prof. Chen.",
    "Il étudie les Pokémon depuis toujours. Il en sait plus que n'importe qui !",
])
conv("OaksLab_Aide2_Talk", "OaksLab_Aide2", [
    "Le Prof. Chen est sorti. Il fait souvent des promenades le matin.",
], block=["MetOak"])
conv("OaksLab_Aide2_AfterPokedex", "OaksLab_Aide2", [
    "Le Pokédex fonctionne ! Plus tu vois de Pokémon, plus il se remplit.",
    "Et si tu les attrapes, tu obtiens encore plus d'informations.",
], prio=10, req=["GotPokedex"])
sign("OaksLab_Computer", "Un e-mail est ouvert sur l'écran.\n« Appel à tous les Dresseurs ! Les meilleurs d'entre vous attendent au Plateau Indigo. »")
sign("OaksLab_Bookshelf", "Des livres sur les Pokémon, couverts de notes du Prof. Chen.")

# ================================================================== ROUTE 1
sign("Route1_Sign", "ROUTE 1\nBOURG PALETTE – JADIELLE")
conv("Route1_MartClerk_Sample", "Route1_MartClerk", [
    "Bonjour ! Je travaille à la Boutique Pokémon de Jadielle.",
    "On y trouve tout ce qu'il faut pour partir à l'aventure.",
    "Tenez, un échantillon gratuit !",
    L("@Narrator {PlayerName} obtient : Potion !", ev="GiveItem", p="Potion", n=1),
    "Passez nous voir à Jadielle !",
], block=["GotPotionSample"], sets=["GotPotionSample"])
conv("Route1_MartClerk_After", "Route1_MartClerk", [
    "On vend des Potions et des Poké Balls à la Boutique de Jadielle. Bonne route !",
], prio=10, req=["GotPotionSample"])
conv("Route1_Boy_Talk", "Route1_Boy", [
    "Tu vois ces petites corniches ? On peut sauter par-dessus pour redescendre.",
    "C'est le raccourci pour rentrer à Bourg Palette !",
])

# ================================================================== JADIELLE
sign("ViridianCity_Sign", "JADIELLE\nLe paradis éternellement vert.")
sign("ViridianCity_TrainerTips1", "CONSEIL DE DRESSEUR\nAttrape plusieurs Pokémon : une équipe variée gagne plus facilement !")
sign("ViridianCity_TrainerTips2", "CONSEIL DE DRESSEUR\nAffaiblis un Pokémon sauvage avant de lancer une Poké Ball. Il sera plus facile à attraper.")
sign("ViridianCity_GymSign", "ARÈNE POKÉMON DE JADIELLE\nChampion : ???")
sign("ViridianCity_GymDoor", "La porte de l'Arène est fermée à clé...")

conv("ViridianCity_Youngster_Talk", "ViridianCity_Youngster", [
    "Ces Poké Balls à ta ceinture... Tu as des Pokémon !",
    "C'est génial de pouvoir les emmener partout avec soi, non ?",
])
conv("ViridianCity_GymWatcher_Talk", "ViridianCity_GymWatcher", [
    "L'Arène de Jadielle est fermée depuis longtemps.",
    "Personne ne sait où est passé son Champion...",
])
conv("ViridianCity_GymWatcher_AfterBadge", "ViridianCity_GymWatcher", [
    "On dit que le Champion de Jadielle serait le plus fort de tout Kanto.",
    "Mais tant que la porte reste fermée, impossible de le savoir !",
], prio=10, req=["GotBoulderBadge"])

# ----- Le vieil homme qui bloque la route du nord (Route 2) avant le Pokédex
conv("ViridianCity_OldMan_Blocking", "ViridianCity_OldMan", [
    "Grmbl... On ne passe pas ! C'est une propriété privée !",
    "@ViridianCity_Granddaughter Grand-père ! Ce n'est pas vrai, voyons !",
    "@ViridianCity_Granddaughter Pardon... Il n'a pas encore bu son café, il est de mauvaise humeur.",
], trigger="Script", block=["GotPokedex"])
conv("ViridianCity_OldMan_Grumpy", "ViridianCity_OldMan", [
    "Grmbl... Pas de café, pas de passage.",
], block=["GotPokedex"])
conv("ViridianCity_Granddaughter_Talk", "ViridianCity_Granddaughter", [
    "Grand-père est grognon le matin, tant qu'il n'a pas eu son café.",
    "Repasse un peu plus tard, il sera de meilleure humeur !",
], block=["GotPokedex"])
conv("ViridianCity_Granddaughter_After", "ViridianCity_Granddaughter", [
    "Grand-père a bu son café. Il est tout content, maintenant !",
], prio=10, req=["GotPokedex"])
conv("ViridianCity_OldMan_OfferTutorial", "ViridianCity_OldMan", [
    "Ah, mon café ! Ça va beaucoup mieux.",
    "Hmm ? C'est un Pokédex, ça ? Tu sais attraper des Pokémon, au moins ?",
    L("Alors ?", choices=[("Oui", "ViridianCity_OldMan_KnowsHow"), ("Non", "ViridianCity_OldMan_Tutorial")]),
], prio=10, req=["GotPokedex"], block=["CatchTutorialSeen"])
conv("ViridianCity_OldMan_KnowsHow", "ViridianCity_OldMan", [
    "Bien, bien. Alors je n'ai rien à t'apprendre. Bonne chasse !",
], trigger="Script", sets=["CatchTutorialSeen"])
conv("ViridianCity_OldMan_Tutorial", "ViridianCity_OldMan", [
    "Bon. Je vais te montrer. Regarde bien !",
    L("D'abord, on affaiblit le Pokémon...", ev="PlayCutscene", p="CatchTutorial"),
    "Et voilà ! Ensuite, on lance la Poké Ball. Facile !",
    "Plus le Pokémon est affaibli, plus il est facile à attraper.",
    "S'il te faut des Poké Balls, va voir à la Boutique.",
], trigger="Script", sets=["CatchTutorialSeen"])
conv("ViridianCity_OldMan_After", "ViridianCity_OldMan", [
    "Un bon café le matin, et la journée est réussie !",
    "Attraper des Pokémon, c'est pareil : il faut de la patience.",
], prio=20, req=["CatchTutorialSeen"])

conv("ViridianSchool_Student_Talk", "ViridianSchool_Student", [
    "Je révise mes types ! Le Feu bat la Plante, l'Eau bat le Feu, la Plante bat l'Eau...",
    "Et la Roche résiste au Feu. Pas facile de tout retenir !",
])
sign("ViridianSchool_Blackboard", "Au tableau : les problèmes de statut.\nSommeil, Poison, Paralysie, Brûlure et Gel. Ils durent même après le combat !")

# ----- Boutique de Jadielle (colis du Prof. Chen)
conv("ViridianPokeMart_Clerk_Parcel", "ViridianPokeMart_Clerk", [
    "Bonjour ! Tu viens de Bourg Palette ?",
    "Tu connais le Prof. Chen ? Son colis vient d'arriver.",
    "Tu veux bien le lui apporter ? Merci !",
    L("@Narrator {PlayerName} obtient : Colis de Chen !", ev="GiveItem", p="OaksParcel", n=1),
], block=["GotOaksParcel"], sets=["GotOaksParcel"])
conv("ViridianPokeMart_Clerk_Reminder", "ViridianPokeMart_Clerk", [
    "Le Prof. Chen attend son colis à Bourg Palette. Je compte sur toi !",
], prio=10, req=["GotOaksParcel"], block=["GotPokedex"])
conv("ViridianPokeMart_Clerk_Shop", "ViridianPokeMart_Clerk", [
    L("Bienvenue à la Boutique Pokémon ! Que puis-je faire pour vous ?", ev="OpenShop", p="ViridianPokeMart"),
], prio=20, req=["GotPokedex"])
conv("ViridianPokeMart_Customer_Talk", "ViridianPokeMart_Customer", [
    "Pour remplir un Pokédex, il faut beaucoup de Poké Balls !",
    "Je fais toujours des réserves avant de partir.",
])

# ----- Centres Pokémon (infirmière générique)
conv("PokemonCenter_Nurse_Welcome", "PokemonCenterNurse", [
    "Bienvenue au Centre Pokémon !",
    "Nous soignons vos Pokémon et leur rendons toute leur énergie.",
    L("Voulez-vous que je soigne vos Pokémon ?", choices=[("Oui", "PokemonCenter_Nurse_Heal"), ("Non", "PokemonCenter_Nurse_Decline")]),
])
conv("PokemonCenter_Nurse_Heal", "PokemonCenterNurse", [
    "Très bien. Je prends vos Pokémon un petit instant.",
    heal_line("Merci d'avoir patienté. Vos Pokémon sont en pleine forme !"),
    "À bientôt !",
], trigger="Script")
conv("PokemonCenter_Nurse_Decline", "PokemonCenterNurse", [
    "À bientôt !",
], trigger="Script")
# Retour après un K.O. de toute l'équipe (UPokemonCenterComponent, équipe déjà soignée)
conv("PokemonCenter_Nurse_AfterBlackout", "PokemonCenterNurse", [
    "Oh ! Vos Pokémon étaient à bout de forces...",
    "Nous les avons soignés. Ils sont de nouveau en pleine forme !",
    "Faites bien attention à vous sur la route. À bientôt !",
], trigger="Script")
conv("PokemonCenter_PC_Use", "PokemonCenter_PC", [
    L("@Narrator {PlayerName} allume le PC.", ev="OpenPC"),
])
conv("ViridianPokemonCenter_Man_Talk", "ViridianPokemonCenter_Man", [
    "Ici, on soigne tes Pokémon gratuitement, autant de fois que tu veux.",
    "Profites-en avant de repartir !",
])
conv("ViridianPokemonCenter_Youngster_Talk", "ViridianPokemonCenter_Youngster", [
    "Si tous tes Pokémon sont K.O., tu reviens au dernier Centre Pokémon visité.",
    "Et tu perds un peu d'argent au passage. Fais attention !",
])

# ================================================================== ROUTE 22
sign("Route22_Sign", "ROUTE 22\nPORTE DE LA LIGUE POKÉMON")
conv("Route22_Rival_Challenge", "Rival", [
    "Hé ! {PlayerName} ! Tu vas à la Ligue Pokémon ?",
    "Laisse tomber ! Tu n'as même pas un seul badge !",
    "Le garde ne te laissera jamais passer.",
    L("Mais dis-moi, ton Pokémon est devenu plus fort ? Voyons ça !", ev="StartTrainerBattle", p="RivalRoute22"),
], trigger="Script", req=["GotPokedex"], block=["BattledRivalRoute22"])
conv("Trainer_RivalRoute22_PlayerWins", "Rival", [
    "Pff ! J'ai pas eu de chance, c'est tout !",
], trigger="Script", next="Route22_Rival_Leaves")
conv("Trainer_RivalRoute22_PlayerLoses", "Rival", [
    "Ha ha ! Mes Pokémon sont bien meilleurs que les tiens !",
], trigger="Script", next="Battle_PlayerBlackout")
conv("Route22_Rival_Leaves", "Rival", [
    "Je vais à la Ligue Pokémon pour battre les meilleurs Dresseurs.",
    "Mais avant, je vais attraper plein de Pokémon pour compléter mon équipe.",
    "Salut, {PlayerName} ! À la prochaine !",
], trigger="Script", sets=["BattledRivalRoute22"])
conv("Route22_LeagueGateGuard_NoBadge", "Route22_LeagueGateGuard", [
    "Seuls les Dresseurs de talent peuvent passer par ici.",
    "Montre-moi d'abord le Badge Roche, celui de l'Arène d'Argenta !",
])
conv("Route22_LeagueGateGuard_Badge", "Route22_LeagueGateGuard", [
    "Oh ! C'est bien le Badge Roche !",
    "Mais la route du Plateau Indigo est en travaux. Reviens plus tard !",
], prio=10, req=["GotBoulderBadge"])

# ================================================================== ROUTE 2 ET POSTES
sign("Route2_Sign", "ROUTE 2\nJADIELLE – ARGENTA")
conv("ViridianForestGateSouth_Guard_Talk", "ViridianForestGateSouth_Guard", [
    "Tu vas traverser la Forêt de Jade ?",
    "C'est un vrai labyrinthe naturel. Ne t'y perds pas !",
])
conv("ViridianForestGateSouth_Girl_Talk", "ViridianForestGateSouth_Girl", [
    "Beaucoup de Pokémon Insecte vivent dans la forêt.",
    "Attention à Aspicot : son dard est empoisonné !",
])
conv("ViridianForestGateNorth_Man_Talk", "ViridianForestGateNorth_Man", [
    "Te voilà de l'autre côté de la forêt ! Bravo.",
    "Argenta est juste au nord. Son Arène est réputée pour ses Pokémon Roche.",
])

# ================================================================== FORÊT DE JADE
sign("ViridianForest_Sign", "FORÊT DE JADE\nAttention : la forêt est un vrai labyrinthe !")
sign("ViridianForest_TrainerTips1", "CONSEIL DE DRESSEUR\nUn Pokémon empoisonné perd des PV, même après le combat. Soigne-le vite !")
sign("ViridianForest_TrainerTips2", "CONSEIL DE DRESSEUR\nPour quitter un combat contre un Pokémon sauvage, choisis FUITE.")
sign("ViridianForest_TrainerTips3", "CONSEIL DE DRESSEUR\nNe gaspille pas tes Poké Balls : vise les Pokémon que tu n'as pas encore !")
sign("ViridianForest_ExitSign", "SORTIE DE LA FORÊT\nARGENTA AU NORD")
conv("ViridianForest_Youngster_Talk", "ViridianForest_Youngster", [
    "Je suis venu avec des copains pour attraper des Pokémon Insecte.",
    "Ils n'arrêtent pas de défier tous les Dresseurs qui passent !",
])

BUG_CATCHERS = {
    "Rick": (
        ["Hé ! Tu as des Pokémon ! Allez, on se fait un combat !"],
        ["Non ! Mes Insectes n'ont rien pu faire !"],
        ["Quand ton Pokémon a du mal, n'hésite pas à en changer pendant le combat."],
    ),
    "Doug": (
        ["Ouais ! J'ai attrapé plein de Pokémon Insecte !", "Tu vas voir ce qu'ils valent !"],
        ["Hein ? Déjà fini ?"],
        ["Aspicot devient Coconfort, puis Dardargnan. Les Insectes évoluent super vite !"],
    ),
    "Anthony": (
        ["Des Insectes, j'en vois partout dans cette forêt !", "Et les miens sont les meilleurs !"],
        ["Mes Chenipan n'étaient pas prêts..."],
        ["Je me demande si je ne devrais pas attraper autre chose que des Insectes..."],
    ),
    "Charlie": (
        ["Hé, attends ! Pourquoi tu cours comme ça ?", "Prends le temps de te battre !"],
        ["J'aurais dû te laisser filer..."],
        ["Chrysacier ne sait faire qu'Armure. Il faut être patient pour qu'il évolue !"],
    ),
    "Sammy": (
        ["Je me cache dans les herbes pour surprendre les Dresseurs !", "Tu ne m'avais pas vu, hein ?"],
        ["Raté ! Mon Aspicot n'a pas suffi..."],
        ["Le dard d'Aspicot peut empoisonner. Garde un Antidote sur toi !"],
    ),
}
for name, (intro, wins, after) in BUG_CATCHERS.items():
    trainer = f"BugCatcher{name}"
    npc = f"ViridianForest_{trainer}"
    intro_lines = intro[:-1] + [L(intro[-1], ev="StartTrainerBattle", p=trainer)]
    conv(f"{npc}_Intro", npc, intro_lines, block=[f"Beaten_{trainer}"])
    conv(f"Trainer_{trainer}_PlayerWins", npc, wins, trigger="Script")
    conv(f"{npc}_After", npc, after, prio=10, req=[f"Beaten_{trainer}"])

# ================================================================== ARGENTA
sign("PewterCity_Sign", "ARGENTA\nUne ville grise comme la pierre.")
sign("PewterCity_GymSign", "ARÈNE POKÉMON D'ARGENTA\nChampion : PIERRE\nLe Dresseur solide comme le roc !")
sign("PewterCity_MuseumSign", "MUSÉE DES SCIENCES D'ARGENTA")
sign("PewterCity_TrainerTips", "CONSEIL DE DRESSEUR\nLes Pokémon Roche craignent les attaques Eau et Plante !")

conv("PewterCity_Youngster_Blocking", "PewterCity_Youngster", [
    "Hé ! Tu es Dresseur, non ?",
    "Pierre, le Champion de l'Arène, cherche des adversaires !",
    L("Viens, je t'y emmène !", ev="PlayCutscene", p="LeadToPewterGym"),
    "Voilà, c'est ici ! Bonne chance !",
], trigger="Script", block=["GotBoulderBadge"])
conv("PewterCity_Youngster_Talk", "PewterCity_Youngster", [
    "Pierre est super fort. Ses Pokémon sont durs comme la pierre !",
], block=["GotBoulderBadge"])
conv("PewterCity_Youngster_AfterBadge", "PewterCity_Youngster", [
    "Tu as battu Pierre ?! Incroyable !",
    "La route de l'est est ouverte pour toi, maintenant.",
], prio=10, req=["GotBoulderBadge"])
conv("PewterCity_Lass_Talk", "PewterCity_Lass", [
    "On a trouvé des fossiles près du Mont Sélénite.",
    "Le musée en expose un, il est magnifique !",
])
conv("PewterCity_OldMan_Talk", "PewterCity_OldMan", [
    "Le Mont Sélénite se trouve à l'est de la ville.",
    "On raconte que des Pokémon venus de l'espace s'y réunissent, les nuits de pleine lune...",
])
conv("PewterCity_Man_Talk", "PewterCity_Man", [
    "Les Pokémon de Pierre encaissent très bien les coups.",
    "Les attaques Normal ne leur font presque rien. Pense à l'Eau ou à la Plante !",
])

# ----- Musée
conv("PewterMuseum_Receptionist_Welcome", "PewterMuseum_Receptionist", [
    "Bienvenue au Musée des Sciences d'Argenta !",
    L("L'entrée coûte 50 ₽. Voulez-vous visiter le musée ?",
      choices=[("Oui", "PewterMuseum_Receptionist_Pay"), ("Non", "PewterMuseum_Receptionist_Decline")]),
], block=["PaidMuseum"])
conv("PewterMuseum_Receptionist_Pay", "PewterMuseum_Receptionist", [
    L("@Narrator {PlayerName} paie 50 ₽.", ev="TakeMoney", n=50, fail="PewterMuseum_Receptionist_NoMoney"),
    "Merci ! Bonne visite !",
], trigger="Script", sets=["PaidMuseum"])
conv("PewterMuseum_Receptionist_NoMoney", "PewterMuseum_Receptionist", [
    "Je suis navrée, vous n'avez pas assez d'argent.",
], trigger="Script")
conv("PewterMuseum_Receptionist_Decline", "PewterMuseum_Receptionist", [
    "Revenez quand vous voulez !",
], trigger="Script")
conv("PewterMuseum_Receptionist_Paid", "PewterMuseum_Receptionist", [
    "Bonne visite ! Le fossile de Kabutops est au fond de la salle.",
], prio=10, req=["PaidMuseum"])
sign("PewterMuseum_KabutopsFossil", "Fossile de Kabutops\nUn Pokémon préhistorique qui vivait dans la mer, il y a très longtemps.")
sign("PewterMuseum_AerodactylFossil", "Fossile de Ptéra\nUn Pokémon préhistorique volant, à la mâchoire pleine de crocs.")
sign("PewterMuseum_MoonStone", "Pierre Lune\nUne météorite tombée sur le Mont Sélénite. Elle ferait évoluer certains Pokémon.")
sign("PewterMuseum_SpaceShuttle", "Maquette de navette spatiale\nUne vraie navette comme celle-ci est allée jusque dans l'espace !")
conv("PewterMuseum_Scientist_Talk", "PewterMuseum_Scientist", [
    "Ce fossile vient du Mont Sélénite.",
    "Un jour, la science pourra peut-être ramener ces Pokémon à la vie...",
])
conv("PewterMuseum_OldMan_Talk", "PewterMuseum_OldMan", [
    "Je viens ici tous les jours. Ces vieux os me rappellent ma jeunesse !",
    "Enfin... pas tant que ça. Ha ha !",
])

# ----- Arène d'Argenta
conv("PewterGym_Guide_Welcome", "PewterGym_Guide", [
    "Hé, toi ! Tu veux devenir Champion ?",
    "Je ne suis pas Dresseur, mais je connais des astuces pour gagner.",
    L("Tu veux un conseil ?", choices=[("Oui", "PewterGym_Guide_Advice"), ("Non", "PewterGym_Guide_NoAdvice")]),
], block=["GotBoulderBadge"])
conv("PewterGym_Guide_Advice", "PewterGym_Guide", [
    "Pierre utilise des Pokémon Roche.",
    "Ils résistent aux attaques Normal, Feu et Vol, mais craignent l'Eau et la Plante !",
    "Si ton équipe n'en a pas, entraîne-toi bien dans la Forêt de Jade avant de l'affronter.",
], trigger="Script")
conv("PewterGym_Guide_NoAdvice", "PewterGym_Guide", [
    "D'accord ! Alors, fonce !",
], trigger="Script")
conv("PewterGym_Guide_AfterBadge", "PewterGym_Guide", [
    "Bravo ! Tu as battu Pierre !",
    "Tu as l'étoffe d'un grand Dresseur, crois-moi.",
], prio=10, req=["GotBoulderBadge"])
conv("PewterGym_Statue_Read", "PewterGym_Statue", [
    "ARÈNE POKÉMON D'ARGENTA\nChampion : PIERRE\nDresseurs victorieux : Régis",
], block=["GotBoulderBadge"])
conv("PewterGym_Statue_ReadAfterBadge", "PewterGym_Statue", [
    "ARÈNE POKÉMON D'ARGENTA\nChampion : PIERRE\nDresseurs victorieux : Régis, {PlayerName}",
], prio=10, req=["GotBoulderBadge"])

conv("PewterGym_CamperLiam_Intro", "PewterGym_CamperLiam", [
    "Halte !",
    L("Tu es encore à des années-lumière de pouvoir affronter Pierre !", ev="StartTrainerBattle", p="CamperLiam"),
], block=["Beaten_CamperLiam"])
conv("Trainer_CamperLiam_PlayerWins", "PewterGym_CamperLiam", [
    "Zut ! Les années-lumière, ça mesure une distance, pas le temps !",
], trigger="Script")
conv("PewterGym_CamperLiam_After", "PewterGym_CamperLiam", [
    "Pas mal du tout... Mais Pierre est bien plus coriace que moi. Bonne chance !",
], prio=10, req=["Beaten_CamperLiam"])

conv("PewterGym_Brock_Intro", "PewterGym_Brock", [
    "Alors, te voilà.",
    "Je suis Pierre, le Champion de l'Arène d'Argenta.",
    "Ma volonté est solide comme le roc. Et mes Pokémon aussi !",
    "Ils sont tous de type Roche. Ha ha !",
    "Tu viens me défier en sachant que tu vas perdre ?",
    "C'est l'honneur du Dresseur qui te pousse, n'est-ce pas ? Très bien.",
    L("Montre-moi ce que tu vaux !", ev="StartTrainerBattle", p="Brock"),
], block=["Beaten_Brock"])
conv("Trainer_Brock_PlayerWins", "PewterGym_Brock", [
    "J'ai sous-estimé ta force...",
    "Tu as gagné. Je le reconnais.",
], trigger="Script", next="PewterGym_Brock_Reward")
conv("PewterGym_Brock_Reward", "PewterGym_Brock", [
    "En reconnaissance de ta victoire, voici le Badge Roche de la Ligue Pokémon.",
    L("@Narrator {PlayerName} reçoit le Badge Roche de la part de Pierre !", ev="GiveBadge", p="BoulderBadge"),
    "Il prouve ta victoire à Argenta. Il existe huit badges dans tout Kanto.",
    "Attends, prends aussi ceci.",
    L("@Narrator {PlayerName} obtient : CT39 !", ev="GiveItem", p="TM39", n=1),
    "Une CT, ou Capsule Technique, permet d'apprendre une attaque à un Pokémon.",
    "La CT39 contient Tomberoche. Elle inflige des dégâts et baisse la Vitesse de l'adversaire.",
    "Choisis bien à qui tu l'apprends !",
], trigger="Script", sets=["GotBoulderBadge"])
conv("PewterGym_Brock_After", "PewterGym_Brock", [
    "Le monde est vaste, et il y a beaucoup de Dresseurs plus forts que moi.",
    "Je vais continuer à m'entraîner, moi aussi. On se reverra !",
], prio=10, req=["Beaten_Brock"])

# ----- Centre Pokémon et Boutique d'Argenta
conv("PewterPokemonCenter_Gentleman_Talk", "PewterPokemonCenter_Gentleman", [
    "Si tes Pokémon sont fatigués après l'Arène, reviens ici.",
    "L'infirmière Joëlle s'occupe de tout, et c'est gratuit !",
])
conv("PewterPokemonCenter_Jigglypuff_Talk", "PewterPokemonCenter_Jigglypuff", [
    L("Rondou... Rondouuu ♪", ev="PlayCutscene", p="JigglypuffSong"),
    "@Narrator Rondoudou chante une douce berceuse... On se sent tout à coup un peu somnolent.",
])
conv("PewterPokeMart_Clerk_Shop", "PewterPokeMart_Clerk", [
    L("Bienvenue à la Boutique Pokémon ! Que puis-je faire pour vous ?", ev="OpenShop", p="PewterPokeMart"),
])
conv("PewterPokeMart_Customer_Talk", "PewterPokeMart_Customer", [
    "Un Repousse éloigne les Pokémon sauvages faibles.",
    "Pratique pour traverser la forêt sans être embêté !",
])

# ================================================================== SYSTÈME
conv("Battle_PlayerBlackout", "Narrator", [
    "{PlayerName} n'a plus de Pokémon en état de se battre !",
    "{PlayerName} perd {MoneyLost} ₽...",
    L("... et se précipite au dernier Centre Pokémon visité.", ev="ReturnToLastHeal"),
], trigger="Script")
