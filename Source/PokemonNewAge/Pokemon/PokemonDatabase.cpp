#include "Pokemon/PokemonDatabase.h"
#include "Engine/DataTable.h"

namespace
{
	const TCHAR* LookupContext = TEXT("PokemonDatabase");

	template <typename RowType>
	const RowType* FindIn(const UDataTable* Table, FName Id)
	{
		return (Table && !Id.IsNone()) ? Table->FindRow<RowType>(Id, LookupContext, false) : nullptr;
	}
}

float FTypeChartRow::Against(EPokemonType Defender) const
{
	switch (Defender)
	{
	case EPokemonType::Normal:   return Normal;
	case EPokemonType::Fire:     return Fire;
	case EPokemonType::Water:    return Water;
	case EPokemonType::Electric: return Electric;
	case EPokemonType::Grass:    return Grass;
	case EPokemonType::Ice:      return Ice;
	case EPokemonType::Fighting: return Fighting;
	case EPokemonType::Poison:   return Poison;
	case EPokemonType::Ground:   return Ground;
	case EPokemonType::Flying:   return Flying;
	case EPokemonType::Psychic:  return Psychic;
	case EPokemonType::Bug:      return Bug;
	case EPokemonType::Rock:     return Rock;
	case EPokemonType::Ghost:    return Ghost;
	case EPokemonType::Dragon:   return Dragon;
	case EPokemonType::Dark:     return Dark;
	case EPokemonType::Steel:    return Steel;
	case EPokemonType::Fairy:    return Fairy;
	default:                     return 1.f;
	}
}

int32 FExperienceRow::For(EGrowthRate Rate) const
{
	switch (Rate)
	{
	case EGrowthRate::Slow:        return Slow;
	case EGrowthRate::Fast:        return Fast;
	case EGrowthRate::MediumSlow:  return MediumSlow;
	case EGrowthRate::Erratic:     return Erratic;
	case EGrowthRate::Fluctuating: return Fluctuating;
	default:                       return MediumFast;
	}
}

const FSpeciesRow* UPokemonDatabase::FindSpecies(FName Id) const { return FindIn<FSpeciesRow>(Species, Id); }
const FMoveRow* UPokemonDatabase::FindMove(FName Id) const { return FindIn<FMoveRow>(Moves, Id); }
const FLearnsetRow* UPokemonDatabase::FindLearnset(FName SpeciesId) const { return FindIn<FLearnsetRow>(Learnsets, SpeciesId); }
const FNatureRow* UPokemonDatabase::FindNature(FName Id) const { return FindIn<FNatureRow>(Natures, Id); }
const FEncounterTableRow* UPokemonDatabase::FindEncounterTable(FName Id) const { return FindIn<FEncounterTableRow>(Encounters, Id); }
const FTrainerRow* UPokemonDatabase::FindTrainer(FName Id) const { return FindIn<FTrainerRow>(Trainers, Id); }

bool UPokemonDatabase::GetSpecies(FName Id, FSpeciesRow& OutRow) const
{
	if (const FSpeciesRow* Row = FindSpecies(Id))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

bool UPokemonDatabase::GetMove(FName Id, FMoveRow& OutRow) const
{
	if (const FMoveRow* Row = FindMove(Id))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

float UPokemonDatabase::GetTypeEffectiveness(EPokemonType AttackType, EPokemonType DefenderType1, EPokemonType DefenderType2) const
{
	if (AttackType == EPokemonType::None || !TypeChart)
	{
		return 1.f;
	}

	// Nom de ligne = nom de la valeur d'enum (Fire, Water...)
	const FName RowName(*StaticEnum<EPokemonType>()->GetNameStringByValue(static_cast<int64>(AttackType)));
	const FTypeChartRow* Row = FindIn<FTypeChartRow>(TypeChart, RowName);
	if (!Row)
	{
		return 1.f;
	}

	float Multiplier = Row->Against(DefenderType1);
	if (DefenderType2 != DefenderType1)
	{
		Multiplier *= Row->Against(DefenderType2);
	}
	return Multiplier;
}

int32 UPokemonDatabase::GetExperienceForLevel(EGrowthRate GrowthRate, int32 Level) const
{
	const int32 ClampedLevel = FMath::Clamp(Level, 1, 100);
	const FExperienceRow* Row = FindIn<FExperienceRow>(Experience, FName(*FString::FromInt(ClampedLevel)));
	return Row ? Row->For(GrowthRate) : 0;
}

int32 UPokemonDatabase::GetLevelForExperience(EGrowthRate GrowthRate, int32 TotalExperience) const
{
	int32 Level = 1;
	while (Level < 100 && GetExperienceForLevel(GrowthRate, Level + 1) <= TotalExperience)
	{
		++Level;
	}
	return Level;
}

TArray<FName> UPokemonDatabase::GetMovesLearnedAtLevel(FName SpeciesId, int32 Level) const
{
	TArray<FName> Result;
	if (const FLearnsetRow* Row = FindLearnset(SpeciesId))
	{
		for (const FLevelUpMove& Entry : Row->LevelUpMoves)
		{
			if (Entry.Level == Level)
			{
				Result.AddUnique(Entry.Move);
			}
		}
	}
	return Result;
}

TArray<FName> UPokemonDatabase::GetDefaultMoves(FName SpeciesId, int32 Level) const
{
	TArray<FName> Result;
	if (const FLearnsetRow* Row = FindLearnset(SpeciesId))
	{
		// Comme dans les jeux : on apprend dans l'ordre et on oublie la plus ancienne au-delà de 4
		for (const FLevelUpMove& Entry : Row->LevelUpMoves)
		{
			if (Entry.Level > Level)
			{
				break;
			}
			if (!Result.Contains(Entry.Move))
			{
				Result.Add(Entry.Move);
				if (Result.Num() > 4)
				{
					Result.RemoveAt(0);
				}
			}
		}
	}
	return Result;
}

bool UPokemonDatabase::RollEncounter(FName EncounterTableId, FRandomStream& Stream, FName& OutSpecies, int32& OutLevel) const
{
	const FEncounterTableRow* Table = FindEncounterTable(EncounterTableId);
	if (!Table || Table->Slots.IsEmpty())
	{
		return false;
	}

	int32 Total = 0;
	for (const FEncounterSlot& Slot : Table->Slots)
	{
		Total += FMath::Max(Slot.Rate, 0);
	}

	int32 Roll = Stream.RandRange(0, FMath::Max(Total, 1) - 1);
	for (const FEncounterSlot& Slot : Table->Slots)
	{
		Roll -= FMath::Max(Slot.Rate, 0);
		if (Roll < 0)
		{
			OutSpecies = Slot.Species;
			OutLevel = Stream.RandRange(Slot.MinLevel, FMath::Max(Slot.MinLevel, Slot.MaxLevel));
			return true;
		}
	}
	return false;
}
