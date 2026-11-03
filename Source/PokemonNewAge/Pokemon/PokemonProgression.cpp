#include "Pokemon/PokemonProgression.h"
#include "Pokemon/PokemonDatabase.h"

FPokemonBaseStats UPokemonProgression::GetStatsAtLevel(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, int32 Level)
{
	FPokemonInstance Copy = Pokemon;
	Copy.Level = FMath::Clamp(Level, 1, 100);
	UPokemonLibrary::RecalculateStats(Database, Copy);
	return Copy.Stats;
}

bool UPokemonProgression::IsEvolutionBlocked(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon)
{
	const FItemRow* Held = Database && !Pokemon.HeldItem.IsNone() ? Database->FindItem(Pokemon.HeldItem) : nullptr;
	return Held && Held->HeldEffect == EHeldItemEffect::PreventEvolution;
}

FName UPokemonProgression::GetLevelUpEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon)
{
	if (!Pokemon.CanBattle() || IsEvolutionBlocked(Database, Pokemon))
	{
		return NAME_None;
	}
	return UPokemonLibrary::GetLevelEvolution(Database, Pokemon);
}

FName UPokemonProgression::GetTradeEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon)
{
	if (const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr)
	{
		for (const FPokemonEvolution& Evolution : Row->Evolutions)
		{
			if (Evolution.Trigger == EEvolutionTrigger::Trade)
			{
				return Evolution.Species;
			}
		}
	}
	return NAME_None;
}

FName UPokemonProgression::GetEvolutionForItem(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, FName Item)
{
	const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
	if (!Row)
	{
		return NAME_None;
	}
	switch (Row->FieldEffect)
	{
	case EFieldItemEffect::EvolutionItem:
		// Les pierres marchent même avec une Pierre Stase, comme dans les jeux
		return UPokemonLibrary::GetItemEvolution(Database, Pokemon, Item);
	case EFieldItemEffect::TradeEvolution:
		return IsEvolutionBlocked(Database, Pokemon) ? NAME_None : GetTradeEvolution(Database, Pokemon);
	default:
		return NAME_None;
	}
}

bool UPokemonProgression::IsProgressionItem(const UPokemonDatabase* Database, FName Item)
{
	const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
	return Row && Row->FieldEffect != EFieldItemEffect::None;
}

EItemCompatibility UPokemonProgression::GetItemCompatibility(const UPokemonDatabase* Database, FName Item, const FPokemonInstance& Pokemon)
{
	const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
	if (!Row || Row->FieldEffect == EFieldItemEffect::None)
	{
		return EItemCompatibility::NotApplicable;
	}
	if (!Pokemon.IsValid())
	{
		return EItemCompatibility::NotAble;
	}

	switch (Row->FieldEffect)
	{
	case EFieldItemEffect::LevelUp:
		return Pokemon.Level >= 100 ? EItemCompatibility::NoEffect : EItemCompatibility::Able;

	case EFieldItemEffect::EvolutionItem:
	case EFieldItemEffect::TradeEvolution:
		return GetEvolutionForItem(Database, Pokemon, Item).IsNone() ? EItemCompatibility::NotAble : EItemCompatibility::Able;

	case EFieldItemEffect::TeachMove:
	{
		if (Pokemon.FindMove(Row->TeachesMove) != INDEX_NONE)
		{
			return EItemCompatibility::AlreadyKnown;
		}
		const FLearnsetRow* Learnset = Database->FindLearnset(Pokemon.Species);
		return Learnset && Learnset->MachineMoves.Contains(Row->TeachesMove) ? EItemCompatibility::Able : EItemCompatibility::NotAble;
	}

	default:
		return EItemCompatibility::NotAble;
	}
}

bool UPokemonProgression::UseItem(const UPokemonDatabase* Database, FName Item, FPokemonInstance& Pokemon, FProgressionOutcome& Outcome, bool& bOutConsumed)
{
	bOutConsumed = false;
	Outcome.Item = Item;
	if (GetItemCompatibility(Database, Item, Pokemon) != EItemCompatibility::Able)
	{
		return false;
	}

	const FItemRow& Row = *Database->FindItem(Item);
	switch (Row.FieldEffect)
	{
	case EFieldItemEffect::LevelUp:
		GainLevels(Database, Pokemon, FMath::Max(1, Row.LevelsGained), Outcome);
		break;
	case EFieldItemEffect::EvolutionItem:
	case EFieldItemEffect::TradeEvolution:
		Outcome.EvolvesInto = GetEvolutionForItem(Database, Pokemon, Item);
		break;
	case EFieldItemEffect::TeachMove:
		LearnOrQueue(Database, Pokemon, Row.TeachesMove, Outcome);
		break;
	default:
		return false;
	}

	// Une pierre n'est consommée que si l'évolution va au bout (UProgressionComponent::ConfirmEvolution)
	const bool bEvolutionItem = Row.FieldEffect == EFieldItemEffect::EvolutionItem || Row.FieldEffect == EFieldItemEffect::TradeEvolution;
	bOutConsumed = !Row.bReusable && !bEvolutionItem;
	return true;
}

void UPokemonProgression::GainExperience(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, int32 Amount, FProgressionOutcome& Outcome)
{
	const FPokemonBaseStats Before = Pokemon.Stats;
	TArray<int32> NewLevels;
	UPokemonLibrary::AddExperience(Database, Pokemon, Amount, NewLevels);

	FPokemonBaseStats Previous = Before;
	for (int32 Level : NewLevels)
	{
		FLevelUpInfo& Info = Outcome.LevelUps.AddDefaulted_GetRef();
		Info.Level = Level;
		Info.StatsBefore = Previous;
		Info.StatsAfter = GetStatsAtLevel(Database, Pokemon, Level);
		Previous = Info.StatsAfter;

		for (FName Move : Database->GetMovesLearnedAtLevel(Pokemon.Species, Level))
		{
			LearnOrQueue(Database, Pokemon, Move, Outcome);
		}
	}

	if (NewLevels.Num() > 0 && Outcome.EvolvesInto.IsNone())
	{
		Outcome.EvolvesInto = GetLevelUpEvolution(Database, Pokemon);
	}
}

void UPokemonProgression::GainLevels(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, int32 Levels, FProgressionOutcome& Outcome)
{
	const FSpeciesRow* Row = Database ? Database->FindSpecies(Pokemon.Species) : nullptr;
	if (!Row || Levels <= 0 || Pokemon.Level >= 100)
	{
		return;
	}
	// Comme le Super Bonbon : juste assez d'XP pour le début du niveau visé
	const int32 Target = FMath::Min(Pokemon.Level + Levels, 100);
	const int32 Needed = Database->GetExperienceForLevel(Row->GrowthRate, Target) - Pokemon.Experience;
	GainExperience(Database, Pokemon, FMath::Max(Needed, 1), Outcome);
}

bool UPokemonProgression::ApplyEvolution(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, FName NewSpecies, FProgressionOutcome& Outcome)
{
	const FName OldSpecies = Pokemon.Species;
	TArray<FName> NewMoves;
	if (!UPokemonLibrary::Evolve(Database, Pokemon, NewSpecies, NewMoves))
	{
		return false;
	}
	Outcome.EvolvedFrom = OldSpecies;
	Outcome.EvolvedTo = NewSpecies;
	for (FName Move : NewMoves)
	{
		LearnOrQueue(Database, Pokemon, Move, Outcome);
	}
	return true;
}

void UPokemonProgression::LearnOrQueue(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, FName Move, FProgressionOutcome& Outcome)
{
	if (Move.IsNone() || Pokemon.FindMove(Move) != INDEX_NONE || Outcome.MovesToChoose.Contains(Move) || (Database && !Database->FindMove(Move)))
	{
		return;
	}
	// Une fois qu'un choix attend, les suivantes attendent aussi (même ordre que dans les jeux)
	if (Outcome.MovesToChoose.Num() == 0 && UPokemonLibrary::LearnMove(Database, Pokemon, Move))
	{
		Outcome.MovesLearned.Add(Move);
	}
	else
	{
		Outcome.MovesToChoose.Add(Move);
	}
}
