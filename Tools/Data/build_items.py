"""Génère Data/Pokemon/DT_Items.json : les objets de la V1 (soins, Poké Balls, objets rares).

Lancer : python Tools/Data/build_items.py
Importé dans Unreal par Tools/Data/import_data_tables.py (struct FItemRow, voir docs/COMBAT.md).
Pour ajouter un objet : une ligne dans ITEMS, puis relancer ce script et l'import.
"""

import json
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "Data", "Pokemon", "DT_Items.json")


def item(name, display, category, price, desc, *, heal=0, heal_full=False, cures=(), revive=0,
         catch=0.0, master=False, battle=True, field=True):
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
