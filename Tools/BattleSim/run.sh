#!/usr/bin/env bash
# Compile et lance la logique du combat SANS Unreal (faux moteur minimal dans inc/).
# Sert à vérifier le C++ du combat là où Unreal n'est pas installé (session Claude dans le cloud).
#   ./Tools/BattleSim/run.sh        -> tests de Source/PokemonNewAge/Tests/BattleTests.cpp
#   ./Tools/BattleSim/run.sh fuzz   -> milliers de combats simulés avec les vraies données (Data/Pokemon)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/PokemonNewAge"
OUT="${TMPDIR:-/tmp}/pokemon_battlesim"
mkdir -p "$OUT/gen"
for f in $(grep -rhoE '[A-Za-z]+\.generated\.h' "$SRC/Pokemon" "$SRC/Battle" | sort -u); do : > "$OUT/gen/$f"; done
FLAGS=(-std=c++20 -O1 -Wall -Wshadow -Wno-comment -I "$HERE/inc" -I "$OUT/gen" -I "$SRC" -include CoreMinimal.h)
LOGIC=("$HERE/enums.cpp" "$SRC/Pokemon/PokemonDatabase.cpp" "$SRC/Pokemon/PokemonInstance.cpp" "$SRC/Battle/BattleRules.cpp"
       "$SRC/Battle/BattleSession.cpp" "$SRC/Battle/BattleAI.cpp" "$SRC/Battle/BattleText.cpp")
if [[ "${1:-}" == "fuzz" ]]; then
  python3 "$HERE/gen_realdata.py" "$ROOT/Data/Pokemon" "$OUT/realdata.cpp"
  g++ "${FLAGS[@]}" -o "$OUT/fuzz" "$HERE/fuzz.cpp" "$OUT/realdata.cpp" "${LOGIC[@]}"
  "$OUT/fuzz"
else
  g++ "${FLAGS[@]}" -o "$OUT/battle_tests" "$HERE/main.cpp" "${LOGIC[@]}" "$SRC/Tests/BattleTests.cpp"
  "$OUT/battle_tests"
fi
