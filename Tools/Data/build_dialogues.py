"""Génère les Data Tables de dialogues depuis Tools/Data/dialogues_v1.py.

    python Tools/Data/build_dialogues.py

Écrit :
  Data/Dialogues/DT_Npcs.json       PNJ, panneaux et objets (struct FNpcRow)
  Data/Dialogues/DT_Dialogues.json  conversations (struct FDialogueRow)
  Data/Dialogues/SCRIPT_V1.md       les mêmes textes en version lisible, pour relire

Vérifie avant d'écrire : identifiants uniques, PNJ, conversations suivantes, drapeaux, dresseurs
(DT_Trainers), espèces (DT_Species), objets et variables. S'arrête sur la première série d'erreurs.
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import dialogues_v1 as src  # noqa: E402

OUT_DIR = os.path.join(ROOT, "Data", "Dialogues")
POKEMON_DIR = os.path.join(ROOT, "Data", "Pokemon")

TOKENS = {"PlayerName", "StarterName", "RivalStarterName", "SeenCount", "CaughtCount", "MoneyLost"}
EVENTS = {
    "None", "GiveItem", "TakeItem", "TakeMoney", "GivePokemon", "OfferNickname", "GiveBadge",
    "HealParty", "StartTrainerBattle", "OpenShop", "OpenPC", "PlayCutscene", "ReturnToLastHeal",
}
KINDS = {"Character", "Sign", "Object", "System"}
TRIGGERS = {"Talk", "Script"}
MAX_LINE = 120  # au-delà, la réplique ne tient plus dans la boîte de texte (2 lignes)

LOCATION_NAMES = {
    "PalletTown": "Bourg Palette", "Route1": "Route 1", "ViridianCity": "Jadielle",
    "Route22": "Route 22", "Route2": "Route 2", "ViridianForest": "Forêt de Jade",
    "PewterCity": "Argenta", None: "Partout (PNJ génériques et système)",
}


# Lieu d'une conversation pour SCRIPT_V1.md, d'après le début de son identifiant
PREFIX_LOCATIONS = {
    "PalletTown": "PalletTown", "PlayerHouse": "PalletTown", "RivalHouse": "PalletTown", "OaksLab": "PalletTown",
    "Route1": "Route1", "Route22": "Route22", "Route2": "Route2", "ViridianForestGateSouth": "Route2",
    "ViridianForestGateNorth": "Route2", "ViridianForest": "ViridianForest",
}


def conversation_location(row, npcs, trainers):
    prefix = row["Name"].split("_")[0]
    if prefix == "Trainer":
        group = row["Name"].split("_")[1]
        for t in trainers:
            if t["Name"] == group or t["Name"].startswith(group + "_"):
                return t["Location"]
    if prefix in PREFIX_LOCATIONS:
        return PREFIX_LOCATIONS[prefix]
    if prefix.startswith("Viridian"):
        return "ViridianCity"
    if prefix.startswith("Pewter"):
        return "PewterCity"
    return npcs[row["Npc"]]["Location"]


def french_spacing(text):
    """Espace insécable avant ! ? : ; et dans les guillemets, comme dans les jeux en français."""
    text = re.sub(r" ([!?:;»])", " \\1", text)
    return text.replace("« ", "« ")


def load_rows(name):
    with open(os.path.join(POKEMON_DIR, f"{name}.json"), encoding="utf-8") as f:
        return json.load(f)


def main():
    errors, warnings = [], []
    trainers = load_rows("DT_Trainers")
    species = {r["Name"] for r in load_rows("DT_Species")}
    # Groupe de dresseur : le nom de ligne, ou le préfixe pour le rival (RivalLab_Bulbasaur -> RivalLab)
    trainer_groups = set()
    for r in trainers:
        trainer_groups.add(r["Name"])
        if r["RequiredPlayerStarter"] != "None":
            trainer_groups.add(r["Name"].rsplit("_", 1)[0])
    flags = set(src.FLAGS) | {f"Beaten_{t}" for t in trainer_groups}

    npcs = {}
    for npc_id, display, kind, location, trainer in src.NPCS:
        if npc_id in npcs:
            errors.append(f"PNJ en double : {npc_id}")
        if kind not in KINDS:
            errors.append(f"{npc_id} : genre inconnu {kind}")
        if trainer and trainer not in trainer_groups:
            errors.append(f"{npc_id} : dresseur inconnu {trainer}")
        npcs[npc_id] = {
            "Name": npc_id, "DisplayName": display, "Kind": kind,
            "Location": location or "None", "Trainer": trainer or "None",
        }

    names = [c["name"] for c in src.CONVERSATIONS]
    known = set(names)
    for n in {n for n in names if names.count(n) > 1}:
        errors.append(f"Conversation en double : {n}")

    def check_target(where, target):
        if target and target not in known:
            errors.append(f"{where} : conversation inconnue {target}")

    rows, battles = [], set()
    for c in src.CONVERSATIONS:
        where = c["name"]
        if c["npc"] not in npcs:
            errors.append(f"{where} : PNJ inconnu {c['npc']}")
        if c["trigger"] not in TRIGGERS:
            errors.append(f"{where} : déclencheur inconnu {c['trigger']}")
        for flag in c["req"] + c["block"] + c["sets"]:
            if flag not in flags:
                errors.append(f"{where} : drapeau inconnu {flag} (à ajouter dans FLAGS)")
        check_target(where, c["next"])

        lines = []
        for raw in c["lines"]:
            line = raw if isinstance(raw, dict) else {"text": raw, "who": None, "ev": None, "p": None, "n": 0, "fail": None, "choices": []}
            text, speaker = line["text"], line["who"] or c["npc"]
            m = re.match(r"@(\w+) (.*)", text, re.S)
            if m:
                speaker, text = m.group(1), m.group(2)
            if speaker not in npcs:
                errors.append(f"{where} : orateur inconnu {speaker}")
            for token in re.findall(r"\{(\w+)\}", text):
                if token not in TOKENS:
                    errors.append(f"{where} : variable inconnue {{{token}}}")
            if npcs.get(speaker, {}).get("Kind") == "Character" and len(text) > MAX_LINE:
                warnings.append(f"{where} : réplique longue ({len(text)} car.) : {text[:50]}...")

            event, param = line["ev"] or "None", line["p"] or "None"
            if event not in EVENTS:
                errors.append(f"{where} : événement inconnu {event}")
            valid_params = {
                "GiveItem": src.ITEMS, "TakeItem": src.ITEMS, "GiveBadge": src.BADGES,
                "OpenShop": src.SHOPS, "PlayCutscene": set(src.CUTSCENES),
                "GivePokemon": species, "StartTrainerBattle": trainer_groups,
            }.get(event)
            if valid_params is not None and param not in valid_params:
                errors.append(f"{where} : paramètre {param} invalide pour {event}")
            if event == "StartTrainerBattle":
                battles.add(param)
            check_target(where, line["fail"])
            choices = []
            for choice_text, target in line["choices"]:
                check_target(where, target)
                choices.append({"Text": french_spacing(choice_text), "NextConversation": target or "None"})
            lines.append({
                "Speaker": speaker, "Text": french_spacing(text), "Event": event, "EventParam": param,
                "EventAmount": line["n"], "FailConversation": line["fail"] or "None", "Choices": choices,
            })

        rows.append({
            "Name": c["name"], "Npc": c["npc"], "Trigger": c["trigger"], "Priority": c["prio"],
            "RequiredFlags": c["req"], "BlockedByFlags": c["block"], "Lines": lines,
            "SetFlags": c["sets"], "NextConversation": c["next"] or "None",
        })

    set_somewhere = {f for c in src.CONVERSATIONS for f in c["sets"]} | {f"Beaten_{b}" for b in battles}
    for flag in sorted({f for c in src.CONVERSATIONS for f in c["req"] + c["block"]} - set_somewhere):
        errors.append(f"Le drapeau {flag} est testé mais jamais posé")
    for flag in sorted(set(src.FLAGS) - set_somewhere):
        warnings.append(f"Le drapeau {flag} n'est jamais posé")
    for group in sorted(battles):
        if f"Trainer_{group}_PlayerWins" not in known:
            errors.append(f"Il manque Trainer_{group}_PlayerWins (réplique quand le joueur gagne)")
    talked = {c["npc"] for c in src.CONVERSATIONS} | {l["Speaker"] for r in rows for l in r["Lines"]}
    for npc_id in npcs:
        if npc_id not in talked:
            errors.append(f"{npc_id} n'a aucune réplique")

    for w in warnings:
        print("Attention :", w)
    if errors:
        for e in errors:
            print("ERREUR :", e)
        sys.exit(1)

    os.makedirs(OUT_DIR, exist_ok=True)
    write_json("DT_Npcs.json", list(npcs.values()))
    write_json("DT_Dialogues.json", rows)
    write_script(rows, npcs, trainers)
    line_count = sum(len(r["Lines"]) for r in rows)
    print(f"{len(npcs)} PNJ, {len(rows)} conversations, {line_count} répliques -> {OUT_DIR}")


def write_json(name, rows):
    with open(os.path.join(OUT_DIR, name), "w", encoding="utf-8", newline="\n") as f:
        json.dump(rows, f, ensure_ascii=False, indent=2)
        f.write("\n")


def write_script(rows, npcs, trainers):
    out = [
        "# Dialogues de la V1",
        "",
        "Version lisible de `DT_Dialogues`, générée par `Tools/Data/build_dialogues.py`. "
        "Pour corriger un texte, modifier `Tools/Data/dialogues_v1.py` puis relancer le script.",
        "",
        "Légende : *Parler* = en parlant au PNJ ; *Scène* = déclenchée par le jeu (volume, combat, choix). "
        "« si » = drapeaux nécessaires, « sauf » = drapeaux qui l'empêchent.",
    ]
    by_location = {}
    for r in rows:
        by_location.setdefault(conversation_location(r, npcs, trainers), []).append(r)
    order = ["PalletTown", "Route1", "ViridianCity", "Route22", "Route2", "ViridianForest", "PewterCity", "None"]
    for loc in order:
        if loc not in by_location:
            continue
        out += ["", f"## {LOCATION_NAMES[None if loc == 'None' else loc]}"]
        for r in by_location[loc]:
            cond = ["*Parler*" if r["Trigger"] == "Talk" else "*Scène*"]
            if r["Priority"]:
                cond.append(f"priorité {r['Priority']}")
            if r["RequiredFlags"]:
                cond.append("si " + ", ".join(r["RequiredFlags"]))
            if r["BlockedByFlags"]:
                cond.append("sauf " + ", ".join(r["BlockedByFlags"]))
            out += ["", f"### `{r['Name']}`", "", " · ".join(cond), ""]
            for line in r["Lines"]:
                who = npcs[line["Speaker"]]["DisplayName"]
                text = line["Text"].replace("\n", " / ")
                out.append(f"- **{who}** : {text}" if who else f"- *{text}*")
                if line["Event"] != "None":
                    detail = line["EventParam"] if line["EventParam"] != "None" else ""
                    if line["EventAmount"]:
                        detail += f" ×{line['EventAmount']}"
                    out.append(f"  - ⚙ {line['Event']} {detail}".rstrip())
                for ch in line["Choices"]:
                    out.append(f"  - ▸ « {ch['Text']} » → `{ch['NextConversation']}`")
            if r["SetFlags"]:
                out.append(f"- ⚑ {', '.join(r['SetFlags'])}")
            if r["NextConversation"] != "None":
                out.append(f"- → `{r['NextConversation']}`")
    with open(os.path.join(OUT_DIR, "SCRIPT_V1.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
