#include "Pokemon/PokemonInstance.h"
#include "Pokemon/PokemonDatabase.h"
#include "Engine/DataTable.h"

namespace
{
	constexpr int32 MaxMoves = 4;
	constexpr int32 MaxEVPerStat = 252;
	constexpr int32 MaxEVTotal = 510;

	int32& StatRef(FPokemonBaseStats& Stats, EPokemonStat Stat)
	{
		switch (Stat)
		{
		case EPokemonStat::Attack:         return Stats.Attack;
		case EPokemonStat::Defense:        return Stats.Defense;
		case EPokemonStat::SpecialAttack:  return Stats.SpecialAttack;
		case EPokemonStat::SpecialDefense: return Stats.SpecialDefense;
		case EPokemonStat::Speed:          return Stats.Speed;
		default:                           return Stats.HP;
		}
	}

	int32 StatValue(const FPokemonBaseStats& Stats, EPokemonStat Stat)
	{
		return StatRef(const_cast<FPokemonBaseStats&>(Stats), Stat);
	}

	const EPokemonStat SixStats[] = {
		EPokemonStat::HP, EPokemonStat::Attack, EPokemonStat::Defense,
		EPokemonStat::SpecialAttack, EPokemonStat::SpecialDefense, EPokemonStat::Speed
	};

	FPokemonMoveSlot MakeSlot(const UPokemonDatabase* Database, FName Move)
	{
		FPokemonMoveSlot Slot;
		Slot.Move = Move;
		const FMoveRow* Row = Database ? Database->FindMove(Move) : nullptr;
		Slot.MaxPP = Row ? Row->PP : 10;
		Slot.CurrentPP = Slot.MaxPP;
		return Slot;
	}
}

int32 FPokemonInstance::FindMove(FName MoveId) const
{
	return Moves.IndexOfByPredicate([MoveId](const FPokemonMoveSlot& Slot) { return Slot.Move == MoveId; });
}

bool UPokemonLibrary::CreatePokemon(const UPokemonDatabase* Database, FName Species, int32 Level, FRandomStream& Stream,
	FPokemonInstance& OutPokemon, int32 FixedIV)
{
	const FSpeciesRow* Row = Database ? Database->FindSpecies(Species) : nullptr;
	if (!Row)
	{
		return false;
	}

	OutPokemon = FPokemonInstance();
	OutPokemon.Species = Species;
	OutPokemon.Level = FMath::Clamp(Level, 1, 100);
	OutPokemon.MetLevel = OutPokemon.Level;
	OutPokemon.Experience = Database->GetExperienceForLevel(Row->GrowthRate, OutPokemon.Level);
	OutPokemon.Friendship = Row->BaseFriendship;

	for (EPokemonStat Stat : SixStats)
	{
		StatRef(OutPokemon.IVs, Stat) = FixedIV >= 0 ? FMath::Min(FixedIV, 31) : Stream.RandRange(0, 31);
	}

	if (Database->Natures)
	{
		const TArray<FName> Natures = Database->Natures->GetRowNames();
		if (Natures.Num() > 0)
		{
			OutPokemon.Nature = Natures[Stream.RandRange(0, Natures.Num() - 1)];
		}
	}

	if (Row->Abilities.Num() > 0)
	{
		OutPokemon.Ability = Row->Abilities[Stream.RandRange(0, Row->Abilities.Num() - 1)];
	}

	OutPokemon.bIsFemale = !Row->bGenderless && Stream.FRand() < Row->FemaleChance;

	for (FName Move : Database->GetDefaultMoves(Species, OutPokemon.Level))
	{
		OutPokemon.Moves.Add(MakeSlot(Database, Move));
	}

	RecalculateStats(Database, OutPokemon);
	OutPokemon.CurrentHP = OutPokemon.Stats.HP;
	return true;
}

int32 UPokemonLibrary::CalculateStat(EPokemonStat Stat, int32 Base, int32 IV, int32 EV, int32 Level, int32 NaturePercent)
{
	const int32 Core = (2 * Base + IV + EV / 4) * Level / 100;
	if (Stat == EPokemonStat::HP)
	{
		// Munja (base 1) a toujours 1 PV
		return Base == 1 ? 1 : Core + Level + 10;
	}
	return (Core + 5) * NaturePercent / 100;
}

void UPokemonLibrary::RecalculateStats(const UPokemonDatabase* Database, FPokemonInstance& Pokemon)
{
	const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr;
	if (!Row)
	{
		return;
	}

	const FNatureRow* Nature = Database->FindNature(Pokemon.Nature);
	const int32 OldMaxHP = Pokemon.Stats.HP;

	for (EPokemonStat Stat : SixStats)
	{
		int32 NaturePercent = 100;
		if (Nature && !Nature->IsNeutral())
		{
			if (Nature->IncreasedStat == Stat) NaturePercent = 110;
			if (Nature->DecreasedStat == Stat) NaturePercent = 90;
		}
		StatRef(Pokemon.Stats, Stat) = CalculateStat(Stat, StatValue(Row->BaseStats, Stat),
			StatValue(Pokemon.IVs, Stat), StatValue(Pokemon.EVs, Stat), Pokemon.Level, NaturePercent);
	}

	// Les PV gagnés s'ajoutent aux PV actuels ; un Pokémon K.O. reste K.O.
	if (OldMaxHP > 0 && Pokemon.CurrentHP > 0)
	{
		Pokemon.CurrentHP = FMath::Clamp(Pokemon.CurrentHP + Pokemon.Stats.HP - OldMaxHP, 1, Pokemon.Stats.HP);
	}
	Pokemon.CurrentHP = FMath::Min(Pokemon.CurrentHP, Pokemon.Stats.HP);
}

void UPokemonLibrary::HealFully(FPokemonInstance& Pokemon)
{
	Pokemon.CurrentHP = Pokemon.Stats.HP;
	Pokemon.Status = EMajorStatus::None;
	Pokemon.SleepTurns = 0;
	for (FPokemonMoveSlot& Slot : Pokemon.Moves)
	{
		Slot.CurrentPP = Slot.MaxPP;
	}
}

FText UPokemonLibrary::GetDisplayName(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon)
{
	if (!Pokemon.Nickname.IsEmpty())
	{
		return FText::FromString(Pokemon.Nickname);
	}
	const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr;
	return Row ? Row->DisplayName : FText::FromName(Pokemon.Species);
}

void UPokemonLibrary::GetTypes(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, EPokemonType& OutType1, EPokemonType& OutType2)
{
	const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr;
	OutType1 = Row ? Row->Type1 : EPokemonType::Normal;
	OutType2 = Row ? Row->Type2 : EPokemonType::None;
}

void UPokemonLibrary::AddEVs(FPokemonInstance& Pokemon, const FPokemonBaseStats& Yield)
{
	for (EPokemonStat Stat : SixStats)
	{
		const int32 Room = FMath::Min(MaxEVPerStat - StatValue(Pokemon.EVs, Stat), MaxEVTotal - Pokemon.EVs.Total());
		StatRef(Pokemon.EVs, Stat) += FMath::Clamp(StatValue(Yield, Stat), 0, FMath::Max(Room, 0));
	}
}

void UPokemonLibrary::AddExperience(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, int32 Amount, TArray<int32>& OutNewLevels)
{
	OutNewLevels.Reset();
	const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr;
	if (!Row || Amount <= 0 || Pokemon.Level >= 100)
	{
		return;
	}

	Pokemon.Experience = FMath::Min(Pokemon.Experience + Amount, Database->GetExperienceForLevel(Row->GrowthRate, 100));
	while (Pokemon.Level < 100 && Pokemon.Experience >= Database->GetExperienceForLevel(Row->GrowthRate, Pokemon.Level + 1))
	{
		++Pokemon.Level;
		OutNewLevels.Add(Pokemon.Level);
	}

	if (OutNewLevels.Num() > 0)
	{
		RecalculateStats(Database, Pokemon);
	}
}

bool UPokemonLibrary::LearnMove(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, FName Move, int32 ForgetSlot)
{
	if (Move.IsNone() || Pokemon.FindMove(Move) != INDEX_NONE || (Database && !Database->FindMove(Move)))
	{
		return false;
	}
	if (Pokemon.Moves.Num() < MaxMoves)
	{
		Pokemon.Moves.Add(MakeSlot(Database, Move));
		return true;
	}
	if (Pokemon.Moves.IsValidIndex(ForgetSlot))
	{
		Pokemon.Moves[ForgetSlot] = MakeSlot(Database, Move);
		return true;
	}
	return false;
}

FName UPokemonLibrary::GetLevelEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon)
{
	if (const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr)
	{
		for (const FPokemonEvolution& Evolution : Row->Evolutions)
		{
			if (Evolution.Trigger == EEvolutionTrigger::Level && Evolution.MinLevel > 0 && Pokemon.Level >= Evolution.MinLevel)
			{
				return Evolution.Species;
			}
		}
	}
	return NAME_None;
}

FName UPokemonLibrary::GetItemEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, FName Item)
{
	if (const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr)
	{
		for (const FPokemonEvolution& Evolution : Row->Evolutions)
		{
			if (Evolution.Trigger == EEvolutionTrigger::Item && Evolution.Item == Item)
			{
				return Evolution.Species;
			}
		}
	}
	return NAME_None;
}

bool UPokemonLibrary::Evolve(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, FName NewSpecies, TArray<FName>& OutMovesToLearn)
{
	OutMovesToLearn.Reset();
	if (!Database || !Database->FindSpecies(NewSpecies))
	{
		return false;
	}

	Pokemon.Species = NewSpecies;
	RecalculateStats(Database, Pokemon);

	for (FName Move : Database->GetMovesLearnedAtLevel(NewSpecies, Pokemon.Level))
	{
		if (Pokemon.FindMove(Move) == INDEX_NONE)
		{
			OutMovesToLearn.Add(Move);
		}
	}
	return true;
}
