"""Génère les données Pokémon du jeu (JSON prêts pour les Data Tables UE5).

Source : les CSV de PokeAPI (https://github.com/PokeAPI/pokeapi, dossier
data/v2/csv), téléchargés une fois dans Tools/Data/.cache/ (hors Git).

Choix (voir docs/DONNEES_POKEMON.md) :
- 151 Pokémon de Kanto, stats et types ACTUELS (18 types, Fée comprise).
- Attaques : celles des générations 1 à 3 (354), valeurs actuelles,
  catégorie physique/spéciale par attaque (comme depuis la 4G).
- Apprentissage par niveau et CT/CS : Rouge Feu / Vert Feuille.
- Rencontres sauvages : Rouge Feu, tout Kanto.
- Dresseurs V1 (Bourg Palette -> badge de Pierre) : saisis à la main plus bas.

Usage (Python 3.10+, aucune dépendance) :
    python Tools/Data/build_pokemon_data.py
Le script réécrit Data/Pokemon/*.json. Relançable sans risque.
"""

from __future__ import annotations

import csv
import json
import re
import sys
import urllib.request
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CACHE = Path(__file__).resolve().parent / ".cache"
OUT = ROOT / "Data" / "Pokemon"
POKEAPI_CSV = "https://raw.githubusercontent.com/PokeAPI/pokeapi/master/data/v2/csv/"

FR = "5"
EN = "9"
FRLG = "7"          # version_group firered-leafgreen
FIRERED = "10"      # version firered
MAX_DEX = 151
MAX_MOVE_ID = 354   # dernière attaque de la 3G (Psycho Boost)
V1_MAX_EVOLUTION_LEVEL = 16  # évolutions atteignables avant le badge de Pierre

TABLES = [
    "pokemon", "pokemon_species", "pokemon_species_names", "pokemon_species_flavor_text",
    "pokemon_stats", "pokemon_types", "types", "type_efficacy", "moves", "move_names",
    "move_flavor_text", "move_meta", "move_meta_stat_changes", "move_flag_map",
    "pokemon_moves", "machines", "items", "growth_rates", "experience", "encounters",
    "encounter_slots", "encounter_methods", "location_areas", "locations",
    "location_names", "pokemon_evolution", "pokemon_abilities", "abilities",
    "ability_names", "ability_flavor_text", "natures", "nature_names", "versions",
]

# --- Correspondances PokeAPI -> énumérations C++ (Source/PokemonNewAge/Pokemon/PokemonTypes.h)

TYPE_ORDER = ["Normal", "Fire", "Water", "Electric", "Grass", "Ice", "Fighting", "Poison",
              "Ground", "Flying", "Psychic", "Bug", "Rock", "Ghost", "Dragon", "Dark",
              "Steel", "Fairy"]

GROWTH = {"slow": "Slow", "medium": "MediumFast", "fast": "Fast", "medium-slow": "MediumSlow",
          "slow-then-very-fast": "Erratic", "fast-then-very-slow": "Fluctuating"}

STAT = {"1": "HP", "2": "Attack", "3": "Defense", "4": "SpecialAttack",
        "5": "SpecialDefense", "6": "Speed", "7": "Accuracy", "8": "Evasion"}

CATEGORY = {"1": "Status", "2": "Physical", "3": "Special"}

TARGET = {"1": "SpecificMove", "2": "SelectedPokemonMeFirst", "3": "Ally", "4": "UsersField",
          "5": "UserOrAlly", "6": "OpponentsField", "7": "User", "8": "RandomOpponent",
          "9": "AllOtherPokemon", "10": "SelectedPokemon", "11": "AllOpponents",
          "12": "EntireField", "13": "UserAndAllies", "14": "AllPokemon", "15": "AllAllies",
          "16": "FaintingPokemon"}

AILMENT = {"-1": "Unknown", "0": "None", "1": "Paralysis", "2": "Sleep", "3": "Freeze",
           "4": "Burn", "5": "Poison", "6": "Confusion", "7": "Infatuation", "8": "Trap",
           "9": "Nightmare", "12": "Torment", "13": "Disable", "14": "Yawn",
           "15": "HealBlock", "17": "NoTypeImmunity", "18": "LeechSeed", "19": "Embargo",
           "20": "PerishSong", "21": "Ingrain", "24": "Silence", "42": "TarShot",
           "43": "Protect"}

META_CATEGORY = {"0": "Damage", "1": "Ailment", "2": "NetGoodStats", "3": "Heal",
                 "4": "DamageAilment", "5": "Swagger", "6": "DamageLower", "7": "DamageRaise",
                 "8": "DamageHeal", "9": "OHKO", "10": "WholeFieldEffect", "11": "FieldEffect",
                 "12": "ForceSwitch", "13": "Unique"}

ENCOUNTER_METHOD = {"1": "Walk", "2": "OldRod", "3": "GoodRod", "4": "SuperRod", "5": "Surf",
                    "6": "RockSmash"}

# Noms PascalCase imposés quand la règle automatique donne un résultat bizarre.
SPECIES_ID_OVERRIDES = {"nidoran-f": "NidoranF", "nidoran-m": "NidoranM",
                        "mr-mime": "MrMime", "farfetchd": "Farfetchd"}
# Talents perdus depuis la 3G (Ectoplasma a reçu Corps Maudit en 7G).
ABILITY_OVERRIDES = {"Gengar": ["Levitate"]}
LOCATION_ID_OVERRIDES = {"kanto-route-": "Route", "kanto-sea-route-": "SeaRoute",
                         "kanto-victory-road-2": "VictoryRoad"}


def pascal(identifier: str) -> str:
    if identifier in SPECIES_ID_OVERRIDES:
        return SPECIES_ID_OVERRIDES[identifier]
    parts = [p for p in re.split(r"[-_ ]", identifier) if p]
    # Étages : "1f" -> "1F", "b1f" -> "B1F"
    return "".join(p.upper() if re.fullmatch(r"b?\d+f", p) else p[:1].upper() + p[1:]
                   for p in parts)


def location_id(identifier: str) -> str:
    for prefix, repl in LOCATION_ID_OVERRIDES.items():
        if identifier.startswith(prefix):
            return repl + pascal(identifier[len(prefix):])
    return pascal(identifier)


def clean_text(text: str) -> str:
    text = text.replace("­\n", "").replace("­", "")
    text = re.sub(r"\s+", " ", text.replace("\f", " "))
    return text.replace("POKéMON", "Pokémon").strip()


def load(name: str) -> list[dict]:
    path = CACHE / f"{name}.csv"
    if not path.exists():
        CACHE.mkdir(parents=True, exist_ok=True)
        print(f"Téléchargement {name}.csv")
        urllib.request.urlretrieve(POKEAPI_CSV + name + ".csv", path)
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def names_by(rows, key, lang, field="name"):
    return {r[key]: r[field] for r in rows if r["local_language_id"] == lang}


def latest_text(rows, key, lang, group_field, preferred=()):
    """Texte le plus récent par clé, avec priorité à certaines versions (Let's Go = Kanto)."""
    best: dict[str, tuple[int, str]] = {}
    for r in rows:
        if r["language_id"] != lang:
            continue
        g = r[group_field]
        rank = 1000 + int(g) if g in preferred else int(g)
        if r[key] not in best or rank > best[r[key]][0]:
            best[r[key]] = (rank, clean_text(r["flavor_text"]))
    return {k: v[1] for k, v in best.items()}


def write(name: str, rows: list[dict]) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{name}.json"
    path.write_text(json.dumps(rows, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{path.relative_to(ROOT)} : {len(rows)} lignes")


def main() -> None:
    d = {t: load(t) for t in TABLES}

    types = {r["id"]: pascal(r["identifier"]) for r in d["types"]}
    species = {r["id"]: r for r in d["pokemon_species"] if int(r["id"]) <= MAX_DEX}
    species_key = {sid: pascal(r["identifier"]) for sid, r in species.items()}
    pokemon = {r["species_id"]: r for r in d["pokemon"] if r["is_default"] == "1"}
    moves = {r["id"]: r for r in d["moves"] if int(r["id"]) <= MAX_MOVE_ID}
    move_key = {mid: pascal(r["identifier"]) for mid, r in moves.items()}
    items = {r["id"]: pascal(r["identifier"]) for r in d["items"]}
    abilities = {r["id"]: r for r in d["abilities"] if r["generation_id"] == "3"}
    ability_key = {aid: pascal(r["identifier"]) for aid, r in abilities.items()}
    lets_go = {v["id"] for v in d["versions"] if v["identifier"].startswith("lets-go")}

    # ---------------------------------------------------------------- Types
    chart = defaultdict(dict)
    for r in d["type_efficacy"]:
        a, t = types.get(r["damage_type_id"]), types.get(r["target_type_id"])
        if a in TYPE_ORDER and t in TYPE_ORDER:
            chart[a][t] = int(r["damage_factor"]) / 100
    type_names = {r["type_id"]: r["name"] for r in load("type_names") if r["local_language_id"] == FR}
    type_id = {v: k for k, v in types.items()}
    write("DT_TypeChart", [
        {"Name": a, "DisplayName": type_names.get(type_id[a], a),
         **{t: chart[a].get(t, 1.0) for t in TYPE_ORDER}}
        for a in TYPE_ORDER])

    # ---------------------------------------------------------------- Attaques
    move_fr = names_by(d["move_names"], "move_id", FR)
    move_desc = latest_text(d["move_flavor_text"], "move_id", FR, "version_group_id")
    meta = {r["move_id"]: r for r in d["move_meta"]}
    stat_changes = defaultdict(list)
    for r in d["move_meta_stat_changes"]:
        stat_changes[r["move_id"]].append({"Stat": STAT[r["stat_id"]], "Stages": int(r["change"])})
    contact = {r["move_id"] for r in d["move_flag_map"] if r["move_flag_id"] == "1"}

    def num(v, default=0):
        return int(v) if v not in ("", None) else default

    move_rows = []
    for mid, m in sorted(moves.items(), key=lambda kv: int(kv[0])):
        mm = meta.get(mid, {})
        move_rows.append({
            "Name": move_key[mid], "Id": int(mid),
            "DisplayName": move_fr.get(mid, move_key[mid]),
            "Description": move_desc.get(mid, ""),
            "Type": types[m["type_id"]],
            "Category": CATEGORY[m["damage_class_id"]],
            "Power": num(m["power"]), "Accuracy": num(m["accuracy"]), "PP": num(m["pp"]),
            "Priority": num(m["priority"]),
            "Target": TARGET[m["target_id"]],
            "bMakesContact": mid in contact,
            "EffectChance": num(m["effect_chance"]),
            "MetaCategory": META_CATEGORY[mm.get("meta_category_id", "0")],
            "Ailment": AILMENT[mm.get("meta_ailment_id", "0")],
            "AilmentChance": num(mm.get("ailment_chance")),
            "MinHits": num(mm.get("min_hits")), "MaxHits": num(mm.get("max_hits")),
            "MinTurns": num(mm.get("min_turns")), "MaxTurns": num(mm.get("max_turns")),
            "Drain": num(mm.get("drain")), "Healing": num(mm.get("healing")),
            "CritStage": num(mm.get("crit_rate")), "FlinchChance": num(mm.get("flinch_chance")),
            "StatChance": num(mm.get("stat_chance")),
            "StatChanges": stat_changes.get(mid, []),
        })
    write("DT_Moves", move_rows)

    # ---------------------------------------------------------------- CT / CS (Rouge Feu / Vert Feuille)
    machines = {}
    for r in d["machines"]:
        n = int(r["machine_number"])
        if r["version_group_id"] == FRLG and (n <= 50 or 101 <= n <= 107):
            machines[n] = r["move_id"]
    write("DT_Machines", [
        {"Name": f"TM{n:02d}" if n <= 50 else f"HM{n - 100:02d}",
         "Move": move_key[mid], "bIsHM": n > 100}
        for n, mid in sorted(machines.items())])
    machine_moves = set(machines.values())

    # ---------------------------------------------------------------- Apprentissages
    level_up = defaultdict(list)
    tm_moves = defaultdict(set)
    for r in d["pokemon_moves"]:
        sid = r["pokemon_id"]
        if r["version_group_id"] != FRLG or sid not in species:
            continue
        if r["pokemon_move_method_id"] == "1":
            level_up[sid].append((int(r["level"]), num(r["order"]), move_key[r["move_id"]]))
        elif r["pokemon_move_method_id"] == "4" and r["move_id"] in machine_moves:
            tm_moves[sid].add(r["move_id"])
    learnset_rows = []
    for sid in sorted(species, key=int):
        learnset_rows.append({
            "Name": species_key[sid],
            "LevelUpMoves": [{"Level": lv, "Move": mv} for lv, _, mv in sorted(level_up[sid])],
            "MachineMoves": [move_key[m] for m in sorted(tm_moves[sid], key=int)],
        })
    write("DT_Learnsets", learnset_rows)

    # ---------------------------------------------------------------- Talents
    ability_fr = names_by(d["ability_names"], "ability_id", FR)
    ability_desc = latest_text(d["ability_flavor_text"], "ability_id", FR, "version_group_id")
    write("DT_Abilities", [
        {"Name": ability_key[aid], "DisplayName": ability_fr.get(aid, ability_key[aid]),
         "Description": ability_desc.get(aid, "")}
        for aid in sorted(abilities, key=int)])

    # ---------------------------------------------------------------- Natures
    nature_fr = names_by(d["nature_names"], "nature_id", FR)
    write("DT_Natures", [
        {"Name": pascal(r["identifier"]), "DisplayName": nature_fr.get(r["id"], r["identifier"]),
         "IncreasedStat": STAT[r["increased_stat_id"]],
         "DecreasedStat": STAT[r["decreased_stat_id"]]}
        for r in sorted(d["natures"], key=lambda r: int(r["game_index"]))])

    # ---------------------------------------------------------------- Courbes d'XP
    growth_key = {r["id"]: GROWTH[r["identifier"]] for r in d["growth_rates"]}
    exp = defaultdict(dict)
    for r in d["experience"]:
        exp[int(r["level"])][growth_key[r["growth_rate_id"]]] = int(r["experience"])
    write("DT_Experience", [{"Name": str(lv), **exp[lv]} for lv in range(1, 101)])

    # ---------------------------------------------------------------- Rencontres sauvages (Rouge Feu)
    areas = {r["id"]: r for r in d["location_areas"]}
    locations = {r["id"]: r for r in d["locations"]}
    location_fr = names_by(d["location_names"], "location_id", FR)
    slots = {r["id"]: r for r in d["encounter_slots"]}
    area_counts = defaultdict(int)
    for a in areas.values():
        area_counts[a["location_id"]] += 1
    tables = defaultdict(list)
    for e in d["encounters"]:
        if e["version_id"] != FIRERED or e["pokemon_id"] not in species:
            continue
        s = slots[e["encounter_slot_id"]]
        method = ENCOUNTER_METHOD.get(s["encounter_method_id"])
        if method is None:
            continue
        tables[(e["location_area_id"], method)].append((int(s["slot"] or 0), e, s))
    encounter_rows = []
    for (area_id, method), entries in tables.items():
        area = areas[area_id]
        loc = locations[area["location_id"]]
        loc_key = location_id(loc["identifier"])
        area_key = pascal(area["identifier"]) if area["identifier"] else ""
        encounter_rows.append({
            "Name": "_".join(p for p in (loc_key, area_key, method) if p),
            "Location": loc_key,
            "LocationDisplayName": location_fr.get(loc["id"], loc_key),
            "Area": area_key,
            "Method": method,
            "Slots": [{"Species": species_key[e["pokemon_id"]], "MinLevel": int(e["min_level"]),
                       "MaxLevel": int(e["max_level"]), "Rate": int(s["rarity"])}
                      for _, e, s in sorted(entries, key=lambda x: (x[0], x[1]["id"]))],
        })
    encounter_rows.sort(key=lambda r: r["Name"])
    write("DT_Encounters", encounter_rows)

    # ---------------------------------------------------------------- Dresseurs V1
    trainer_rows = build_trainers(set(species_key.values()), set(move_key.values()))
    write("DT_Trainers", trainer_rows)

    # ---------------------------------------------------------------- Espèces
    names_fr = {r["pokemon_species_id"]: r for r in d["pokemon_species_names"]
                if r["local_language_id"] == FR}
    dex_fr = latest_text(d["pokemon_species_flavor_text"], "species_id", FR, "version_id",
                         preferred=lets_go)
    stats = defaultdict(dict)
    evs = defaultdict(dict)
    for r in d["pokemon_stats"]:
        if r["pokemon_id"] in species:
            stats[r["pokemon_id"]][STAT[r["stat_id"]]] = int(r["base_stat"])
            evs[r["pokemon_id"]][STAT[r["stat_id"]]] = int(r["effort"])
    ptypes = defaultdict(list)
    for r in sorted(d["pokemon_types"], key=lambda r: int(r["slot"])):
        if r["pokemon_id"] in species:
            ptypes[r["pokemon_id"]].append(types[r["type_id"]])
    pabil = defaultdict(list)
    for r in sorted(d["pokemon_abilities"], key=lambda r: int(r["slot"])):
        if r["pokemon_id"] in species and r["is_hidden"] == "0" and r["ability_id"] in ability_key:
            pabil[r["pokemon_id"]].append(ability_key[r["ability_id"]])
    evolutions = defaultdict(list)
    for r in d["pokemon_evolution"]:
        target = r["evolved_species_id"]
        if target not in species:
            continue  # évolutions vers la 2G et après : hors Pokédex V1
        trig = {"1": "Level", "2": "Trade", "3": "Item"}.get(r["evolution_trigger_id"])
        if trig is None:
            continue
        evolutions[species[target]["evolves_from_species_id"]].append({
            "Species": species_key[target], "Trigger": trig,
            "MinLevel": num(r["minimum_level"]),
            "Item": items.get(r["trigger_item_id"], "None") if r["trigger_item_id"] else "None",
        })

    v1 = v1_species(encounter_rows, trainer_rows,
                    {species_key[k]: v for k, v in evolutions.items() if k in species_key})
    species_rows = []
    for sid in sorted(species, key=int):
        s = species[sid]
        p = pokemon[sid]
        t = ptypes[sid] + ["None"]
        gender_rate = int(s["gender_rate"])
        species_rows.append({
            "Name": species_key[sid],
            "NationalDex": int(sid),
            "DisplayName": names_fr[sid]["name"],
            "Genus": names_fr[sid]["genus"],
            "PokedexEntry": dex_fr.get(sid, ""),
            "Type1": t[0], "Type2": t[1],
            "BaseStats": {k: stats[sid][k] for k in STAT.values() if k in stats[sid]},
            "EVYield": {k: evs[sid][k] for k in STAT.values() if k in evs[sid]},
            "Abilities": ABILITY_OVERRIDES.get(species_key[sid], pabil[sid]),
            "CatchRate": int(s["capture_rate"]),
            "BaseExperience": num(p["base_experience"]),
            "BaseFriendship": num(s["base_happiness"]),
            "GrowthRate": growth_key[s["growth_rate_id"]],
            "bGenderless": gender_rate < 0,
            "FemaleChance": 0.0 if gender_rate < 0 else gender_rate / 8,
            "HeightM": int(p["height"]) / 10,
            "WeightKg": int(p["weight"]) / 10,
            "bIsLegendary": s["is_legendary"] == "1",
            "bIsMythical": s["is_mythical"] == "1",
            "bAvailableInV1": species_key[sid] in v1,
            "Evolutions": evolutions.get(sid, []),
        })
    write("DT_Species", species_rows)
    print(f"Pokémon jouables en V1 ({len(v1)}) : {', '.join(sorted(v1))}")


# Lieux de la V1 : de Bourg Palette au badge de Pierre.
V1_LOCATIONS = {"PalletTown", "Route1", "ViridianCity", "Route22", "Route2", "ViridianForest",
                "PewterCity"}
STARTERS = ["Bulbasaur", "Charmander", "Squirtle"]


def v1_species(encounter_rows, trainer_rows, evolutions) -> set[str]:
    found = set(STARTERS)
    for r in encounter_rows:
        if r["Location"] in V1_LOCATIONS and r["Method"] == "Walk":
            found |= {s["Species"] for s in r["Slots"]}
    for t in trainer_rows:
        found |= {p["Species"] for p in t["Party"]}
    # Évolutions par niveau atteignables avant Pierre (Chrysacier, Papilusion, Herbizarre...)
    changed = True
    while changed:
        changed = False
        for pre, evos in evolutions.items():
            for e in evos:
                if (pre in found and e["Species"] not in found and e["Trigger"] == "Level"
                        and 0 < e["MinLevel"] <= V1_MAX_EVOLUTION_LEVEL):
                    found.add(e["Species"])
                    changed = True
    return found


# ---------------------------------------------------------------- Dresseurs (saisie manuelle)
# Équipes de Rouge Feu / Vert Feuille. Moves vide = attaques par défaut
# (les 4 dernières apprises au niveau, comme dans les jeux).
# RequiredPlayerStarter : la ligne ne sert que si le joueur a choisi ce starter.
# Prize = gain de base de la classe x niveau du dernier Pokémon (formule 3G).

RIVAL_COUNTER = {"Bulbasaur": "Charmander", "Charmander": "Squirtle", "Squirtle": "Bulbasaur"}

TRAINER_CLASSES = {
    "Rival": ("Rival", 35),
    "BugCatcher": ("Scout", 16),
    "Camper": ("Campeur", 20),
    "GymLeader": ("Champion d'Arène", 99),
}


def build_trainers(species_ok: set[str], moves_ok: set[str]) -> list[dict]:
    rows = []

    def add(key, cls, name, location, party, required="None", badge="None", reward="None"):
        for p in party:
            assert p["Species"] in species_ok, p["Species"]
            assert all(m in moves_ok for m in p.get("Moves", [])), p
            p.setdefault("Moves", [])
        class_name, base = TRAINER_CLASSES[cls]
        rows.append({
            "Name": key, "TrainerClass": cls, "ClassDisplayName": class_name,
            "DisplayName": name, "Location": location,
            "RequiredPlayerStarter": required,
            "PrizeMoney": base * party[-1]["Level"],
            "bIsGymLeader": cls == "GymLeader", "Badge": badge, "RewardItem": reward,
            "Party": party,
        })

    for starter, rival in RIVAL_COUNTER.items():
        add(f"RivalLab_{starter}", "Rival", "Régis", "PalletTown",
            [{"Species": rival, "Level": 5}], required=starter)
        add(f"RivalRoute22_{starter}", "Rival", "Régis", "Route22",
            [{"Species": "Pidgey", "Level": 9}, {"Species": rival, "Level": 9}], required=starter)

    add("BugCatcherRick", "BugCatcher", "Rick", "ViridianForest",
        [{"Species": "Weedle", "Level": 6}, {"Species": "Caterpie", "Level": 6}])
    add("BugCatcherDoug", "BugCatcher", "Doug", "ViridianForest",
        [{"Species": "Weedle", "Level": 7}, {"Species": "Kakuna", "Level": 7},
         {"Species": "Weedle", "Level": 7}])
    add("BugCatcherAnthony", "BugCatcher", "Anthony", "ViridianForest",
        [{"Species": "Caterpie", "Level": 7}, {"Species": "Caterpie", "Level": 8}])
    add("BugCatcherCharlie", "BugCatcher", "Charlie", "ViridianForest",
        [{"Species": "Metapod", "Level": 7}, {"Species": "Caterpie", "Level": 7},
         {"Species": "Metapod", "Level": 7}])
    add("BugCatcherSammy", "BugCatcher", "Sammy", "ViridianForest",
        [{"Species": "Weedle", "Level": 9}])

    add("CamperLiam", "Camper", "Liam", "PewterCity",
        [{"Species": "Geodude", "Level": 10}, {"Species": "Sandshrew", "Level": 11}])
    add("Brock", "GymLeader", "Pierre", "PewterCity",
        [{"Species": "Geodude", "Level": 12, "Moves": ["Tackle", "DefenseCurl"]},
         {"Species": "Onix", "Level": 14, "Moves": ["Tackle", "Bind", "RockTomb", "Screech"]}],
        badge="BoulderBadge", reward="TM39")
    return rows


if __name__ == "__main__":
    sys.exit(main())
