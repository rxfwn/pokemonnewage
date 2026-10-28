"""Génère Data/Pokemon/DT_Items.json : les objets de la V1 (soins, Poké Balls, objets rares).

Lancer : python Tools/Data/build_items.py
Importé dans Unreal par Tools/Data/import_data_tables.py (struct FItemRow, voir docs/COMBAT.md).
Pour ajouter un objet : une ligne dans ITEMS, puis relancer ce script et l'import.
"""

import json
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Data", "Pokemon", "DT_Items.json")


def item(name, display, category, price, desc, *, heal=0, heal_full=False, cures=(), revive=0,
         catch=0.0, master=False, battle=True, field=True, held="None", held_value=0, boost="None"):
    return {
        "Name": name,
        "DisplayName": display,
        "Description": desc,
        "Category": category,
        "Price": price,
        "HealHP": heal,
        "bHealFull": heal_full,
        "CuresStatus": list(cures),
        "ReviveHPPercent": revive,
        "CatchRateMultiplier": catch,
        "bGuaranteedCatch": master,
        "HeldEffect": held,
        "HeldValue": held_value,
        "HeldBoostType": boost,
        "bUsableInBattle": battle,
        "bUsableInField": field,
    }


ALL_STATUS = ["Poison", "BadlyPoisoned", "Burn", "Paralysis", "Sleep", "Freeze"]

ITEMS = [
    # Soins (prix de Rouge Feu)
    item("Potion", "Potion", "Medicine", 300, "Restaure 20 PV d'un Pokémon.", heal=20),
    item("SuperPotion", "Super Potion", "Medicine", 700, "Restaure 50 PV d'un Pokémon.", heal=50),
    item("HyperPotion", "Hyper Potion", "Medicine", 1200, "Restaure 200 PV d'un Pokémon.", heal=200),
    item("MaxPotion", "Potion Max", "Medicine", 2500, "Restaure tous les PV d'un Pokémon.", heal_full=True),
    item("FullRestore", "Guérison", "Medicine", 3000, "Restaure tous les PV et soigne tous les problèmes de statut.",
         heal_full=True, cures=ALL_STATUS),
    item("Antidote", "Antidote", "Medicine", 100, "Soigne un Pokémon empoisonné.", cures=["Poison", "BadlyPoisoned"]),
    item("ParalyzeHeal", "Anti-Para", "Medicine", 200, "Soigne un Pokémon paralysé.", cures=["Paralysis"]),
    item("Awakening", "Réveil", "Medicine", 250, "Réveille un Pokémon endormi.", cures=["Sleep"]),
    item("BurnHeal", "Anti-Brûle", "Medicine", 250, "Soigne un Pokémon brûlé.", cures=["Burn"]),
    item("IceHeal", "Antigel", "Medicine", 250, "Dégèle un Pokémon gelé.", cures=["Freeze"]),
    item("FullHeal", "Total Soin", "Medicine", 600, "Soigne tous les problèmes de statut d'un Pokémon.", cures=ALL_STATUS),
    item("Revive", "Rappel", "Medicine", 1500, "Ranime un Pokémon K.O. avec la moitié de ses PV.", revive=50),
    item("MaxRevive", "Rappel Max", "Medicine", 0, "Ranime un Pokémon K.O. avec tous ses PV.", revive=100),
    # Poké Balls (multiplicateurs actuels)
    item("PokeBall", "Poké Ball", "PokeBall", 200, "Un objet pour capturer les Pokémon sauvages.", catch=1.0, field=False),
    item("GreatBall", "Super Ball", "PokeBall", 600, "Une Ball plus efficace que la Poké Ball.", catch=1.5, field=False),
    item("UltraBall", "Hyper Ball", "PokeBall", 1200, "Une Ball très efficace, meilleure que la Super Ball.", catch=2.0, field=False),
    item("MasterBall", "Master Ball", "PokeBall", 0, "La meilleure Ball : elle capture à coup sûr.", catch=255.0, master=True, field=False),
    # Objets à tenir (menu Pokémon > Donner un objet) : poche « Baies et matériaux »
    item("OranBerry", "Baie Oran", "Other", 100, "À tenir. Le Pokémon la mange quand ses PV tombent à la moitié : rend 10 PV.",
         held="BerryHeal", held_value=10, battle=False, field=False),
    item("SitrusBerry", "Baie Sitrus", "Other", 300, "À tenir. Le Pokémon la mange quand ses PV tombent à la moitié : rend un quart de ses PV.",
         held="BerryHealPercent", held_value=25, battle=False, field=False),
    item("CheriBerry", "Baie Ceriz", "Other", 100, "À tenir. Soigne la paralysie en combat.",
         cures=["Paralysis"], held="BerryCure", battle=False, field=False),
    item("ChestoBerry", "Baie Maron", "Other", 100, "À tenir. Réveille le Pokémon en combat.",
         cures=["Sleep"], held="BerryCure", battle=False, field=False),
    item("PechaBerry", "Baie Pêcha", "Other", 100, "À tenir. Soigne l'empoisonnement en combat.",
         cures=["Poison", "BadlyPoisoned"], held="BerryCure", battle=False, field=False),
    item("RawstBerry", "Baie Fraive", "Other", 100, "À tenir. Soigne la brûlure en combat.",
         cures=["Burn"], held="BerryCure", battle=False, field=False),
    item("AspearBerry", "Baie Willia", "Other", 100, "À tenir. Dégèle le Pokémon en combat.",
         cures=["Freeze"], held="BerryCure", battle=False, field=False),
    item("LumBerry", "Baie Prine", "Other", 500, "À tenir. Soigne tous les problèmes de statut en combat.",
         cures=ALL_STATUS, held="BerryCure", battle=False, field=False),
    item("Leftovers", "Restes", "Other", 0, "À tenir. Rend un peu de PV à la fin de chaque tour.",
         held="EndTurnHeal", held_value=6, battle=False, field=False),
    item("ShellBell", "Grelot Coque", "Other", 0, "À tenir. Rend des PV à chaque attaque qui touche (1/8 des dégâts).",
         held="DamageHeal", held_value=12, battle=False, field=False),
    item("QuickClaw", "Vive Griffe", "Other", 0, "À tenir. Permet parfois d'attaquer en premier.",
         held="QuickClaw", held_value=20, battle=False, field=False),
    item("FocusBand", "Bandeau", "Other", 0, "À tenir. Permet parfois de tenir avec 1 PV au lieu d'être K.O.",
         held="FocusBand", held_value=10, battle=False, field=False),
    item("LuckyEgg", "Œuf Chance", "Other", 0, "À tenir. Le Pokémon gagne plus d'expérience.",
         held="ExpBoost", held_value=150, battle=False, field=False),
    item("AmuletCoin", "Pièce Rune", "Other", 0, "À tenir. Double l'argent gagné contre un dresseur si le Pokémon a combattu.",
         held="MoneyBoost", held_value=200, battle=False, field=False),
] + [
    # Objets qui renforcent un type (+20 %)
    item(name, display, "Other", 1000, "À tenir. Augmente la puissance des attaques de type %s." % type_fr,
         held="TypeBoost", held_value=120, boost=type_id, battle=False, field=False)
    for name, display, type_id, type_fr in [
        ("SilkScarf", "Mouchoir Soie", "Normal", "Normal"),
        ("Charcoal", "Charbon", "Fire", "Feu"),
        ("MysticWater", "Eau Mystique", "Water", "Eau"),
        ("MiracleSeed", "Grain Miracle", "Grass", "Plante"),
        ("Magnet", "Aimant", "Electric", "Électrik"),
        ("NeverMeltIce", "Glace Éternelle", "Ice", "Glace"),
        ("BlackBelt", "Ceinture Noire", "Fighting", "Combat"),
        ("PoisonBarb", "Pic Venin", "Poison", "Poison"),
        ("SoftSand", "Sable Doux", "Ground", "Sol"),
        ("SharpBeak", "Bec Pointu", "Flying", "Vol"),
        ("TwistedSpoon", "Cuillère Tordue", "Psychic", "Psy"),
        ("SilverPowder", "Poudre Argentée", "Bug", "Insecte"),
        ("HardStone", "Pierre Dure", "Rock", "Roche"),
        ("SpellTag", "Rune Sort", "Ghost", "Spectre"),
        ("DragonFang", "Croc Dragon", "Dragon", "Dragon"),
        ("BlackGlasses", "Lunettes Noires", "Dark", "Ténèbres"),
        ("MetalCoat", "Peau Métal", "Steel", "Acier"),
    ]
] + [
    # Objets rares de la V1 (donnés par les dialogues)
    item("OaksParcel", "Colis de Chen", "KeyItem", 0, "Un colis à remettre au Professeur Chen.", battle=False, field=False),
    item("Pokedex", "Pokédex", "KeyItem", 0, "Une encyclopédie high-tech qui enregistre les Pokémon.", battle=False),
    item("TownMap", "Carte", "KeyItem", 0, "Une carte pratique de la région de Kanto.", battle=False),
]


def main():
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(ITEMS, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print(f"DT_Items : {len(ITEMS)} objets -> {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
