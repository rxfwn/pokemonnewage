"""Génère Data/Pokemon/DT_Shops.json : le stock des Boutiques Pokémon de la V1 (SCRUM-53).

Lancer : python Tools/Data/build_shops.py
Importé dans Unreal par Tools/Data/import_data_tables.py (struct FShopRow, voir docs/CENTRE_ET_BOUTIQUE.md).

Nom de ligne = paramètre de l'événement OpenShop des dialogues (ViridianPokeMart, PewterPokeMart).
Chaque boutique a son rayon de base (celui de Rouge Feu / Vert Feuille), puis un rayon commun qui
s'agrandit avec les badges, comme dans Let's Go. Les prix sont ceux de DT_Items (build_items.py).
Pour ajouter un objet : une ligne dans le rayon voulu, puis relancer ce script et l'import.
"""

import json
import os

HERE = os.path.dirname(__file__)
OUT = os.path.join(HERE, "..", "..", "Data", "Pokemon", "DT_Shops.json")
ITEMS = os.path.join(HERE, "..", "..", "Data", "Pokemon", "DT_Items.json")


def entry(item, badges=0, flag=None):
    return {"Item": item, "MinBadges": badges, "RequiredFlag": flag or "None"}


# Rayon commun : arrive avec les badges (Let's Go)
WITH_BADGES = [
    entry("SuperPotion", 1),
    entry("GreatBall", 3),
    entry("Revive", 3),
    entry("FullHeal", 4),
    entry("HyperPotion", 5),
    entry("UltraBall", 5),
    entry("MaxPotion", 7),
    entry("FullRestore", 8),
]

SHOPS = [
    {
        "Name": "ViridianPokeMart",
        "DisplayName": "Boutique de Jadielle",
        "Location": "ViridianCity",
        # Rouge Feu : la boutique n'ouvre qu'après le colis de Chen (le dialogue le vérifie déjà)
        "Stock": [
            entry("PokeBall"),
            entry("Potion"),
            entry("Antidote"),
            entry("ParalyzeHeal"),
            entry("BurnHeal"),
        ] + WITH_BADGES,
    },
    {
        "Name": "PewterPokeMart",
        "DisplayName": "Boutique d'Argenta",
        "Location": "PewterCity",
        "Stock": [
            entry("PokeBall"),
            entry("Potion"),
            entry("Antidote"),
            entry("ParalyzeHeal"),
            entry("Awakening"),
            entry("BurnHeal"),
            entry("IceHeal"),
        ] + WITH_BADGES,
    },
]


def main():
    with open(ITEMS, encoding="utf-8") as f:
        prices = {row["Name"]: row["Price"] for row in json.load(f)}
    for shop in SHOPS:
        for e in shop["Stock"]:
            if e["Item"] not in prices:
                raise SystemExit(f"{shop['Name']} : objet {e['Item']} absent de DT_Items (build_items.py)")
            if prices[e["Item"]] <= 0:
                raise SystemExit(f"{shop['Name']} : {e['Item']} n'a pas de prix (Price = 0)")
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(SHOPS, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print(f"DT_Shops : {len(SHOPS)} boutiques -> {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
