#include "Pokemon/PokemonTypes.h"
template <> FShimEnum* StaticEnum<EPokemonType>() { static FShimEnum E{{L"None",L"Normal",L"Fire",L"Water",L"Electric",L"Grass",L"Ice",L"Fighting",L"Poison",L"Ground",L"Flying",L"Psychic",L"Bug",L"Rock",L"Ghost",L"Dragon",L"Dark",L"Steel",L"Fairy"}}; return &E; }
#include "Pokemon/PokemonDataSubsystem.h"
UPokemonDatabase* UPokemonDataSubsystem::GetDatabase(const UObject*) { return nullptr; }
