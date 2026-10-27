import json, sys
D = sys.argv[1]
ENUMS = {"Type1":"EPokemonType","Type2":"EPokemonType","Type":"EPokemonType","Target":"EMoveTarget","MetaCategory":"EMoveMetaCategory",
         "Ailment":"EMoveAilment","GrowthRate":"EGrowthRate","Stat":"EPokemonStat","Trigger":"EEvolutionTrigger",
         "IncreasedStat":"EPokemonStat","DecreasedStat":"EPokemonStat","Method":"EEncounterMethod"}
TEXTS = {"DisplayName","Description","Genus","PokedexEntry","ClassDisplayName","LocationDisplayName"}
STRUCTS = {"Evolutions":"FPokemonEvolution","StatChanges":"FMoveStatChange","LevelUpMoves":"FLevelUpMove","Party":"FTrainerPokemon","Slots":"FEncounterSlot"}
def lit(s): return 'L"' + s.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n') + '"'
def val(key, v, table):
    if isinstance(v, bool): return "true" if v else "false"
    if isinstance(v, (int, float)): return repr(v) + ("f" if isinstance(v, float) else "")
    if isinstance(v, str):
        if key in TEXTS: return f"FText::FromString({lit(v)})"
        if key == "Category": return ("EItemCategory::" if table=="DT_Items" else "EMoveCategory::") + v
        if key in ENUMS: return f"{ENUMS[key]}::{v}"
        return f"FName({lit(v)})"
    raise Exception(key)
def assign(target, obj, table, out, depth=0):
    for k, v in obj.items():
        if k == "Name" and depth == 0: continue
        if isinstance(v, dict):
            for k2, v2 in v.items(): out.append(f"{target}.{k}.{k2} = {val(k2, v2, table)};")
        elif isinstance(v, list):
            for i, e in enumerate(v):
                if isinstance(e, dict):
                    var = f"E{depth}_{i}"
                    out.append(f"{{ {STRUCTS[k]} {var};")
                    assign(var, e, table, out, depth+1)
                    out.append(f"{target}.{k}.Add({var}); }}")
                elif k == "CuresStatus": out.append(f"{target}.{k}.Add(EMajorStatus::{e});")
                else: out.append(f"{target}.{k}.Add({val(k, e, table)});")
        else: out.append(f"{target}.{k} = {val(k, v, table)};")
TABLES = [("DT_Species","FSpeciesRow","Species"),("DT_Moves","FMoveRow","Moves"),("DT_TypeChart","FTypeChartRow","TypeChart"),
          ("DT_Learnsets","FLearnsetRow","Learnsets"),("DT_Natures","FNatureRow","Natures"),("DT_Experience","FExperienceRow","Experience"),
          ("DT_Trainers","FTrainerRow","Trainers"),("DT_Items","FItemRow","Items"),("DT_Encounters","FEncounterTableRow","Encounters")]
out = ['#include "Pokemon/PokemonDatabase.h"', '#include "Engine/DataTable.h"']
for name, struct, prop in TABLES:
    out.append(f"static void Fill_{prop}(UDataTable* T) {{")
    for row in json.load(open(f"{D}/{name}.json", encoding="utf-8")):
        out.append(f"{{ {struct} R;")
        assign("R", row, name, out)
        out.append(f"T->AddRow(FName({lit(row['Name'])}), R); }}")
    out.append("}")
out.append("UPokemonDatabase* MakeRealDatabase() { UPokemonDatabase* Db = NewObject<UPokemonDatabase>();")
for name, struct, prop in TABLES:
    out.append(f"Db->{prop} = NewObject<UDataTable>(); Fill_{prop}(Db->{prop});")
out.append("return Db; }")
open(sys.argv[2], "w", encoding="utf-8").write("\n".join(out))
