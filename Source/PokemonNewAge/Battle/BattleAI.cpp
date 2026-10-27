#include "Battle/BattleAI.h"
#include "Battle/BattleSession.h"
#include "Battle/BattleRules.h"
#include "Pokemon/PokemonDatabase.h"

float FBattleAI::ScoreMove(const UBattleSession& Session, int32 Side, int32 MoveSlot)
{
	const UPokemonDatabase* Database = Session.GetDatabase();
	const FBattleSide& Self = Session.GetSide(Side);
	const FBattleSide& Foe = Session.GetSide(1 - Side);
	const FPokemonInstance* User = Self.GetActive();
	const FPokemonInstance* Target = Foe.GetActive();
	if (!Database || !User || !Target || !User->Moves.IsValidIndex(MoveSlot) || User->Moves[MoveSlot].CurrentPP <= 0)
	{
		return 0.f;
	}
	const FMoveRow* Move = Database->FindMove(User->Moves[MoveSlot].Move);
	if (!Move)
	{
		return 0.f;
	}

	EPokemonType UserType1, UserType2, TargetType1, TargetType2;
	UPokemonLibrary::GetTypes(Database, *User, UserType1, UserType2);
	UPokemonLibrary::GetTypes(Database, *Target, TargetType1, TargetType2);
	const float Effectiveness = Database->GetTypeEffectiveness(Move->Type, TargetType1, TargetType2);
	const float Accuracy = Move->Accuracy > 0 ? FMath::Min(UBattleRules::GetHitChance(Move->Accuracy,
		Self.Volatile.Stages.Accuracy, Foe.Volatile.Stages.Evasion), 100.f) / 100.f : 1.f;

	// Attaque qui fait des dégâts : dégâts moyens estimés, en % des PV restants de la cible
	if (Move->Category != EMoveCategory::Status && Move->MetaCategory != EMoveMetaCategory::OHKO)
	{
		if (Effectiveness <= 0.f)
		{
			return 0.f;
		}
		const bool bPhysical = Move->Category == EMoveCategory::Physical;
		FDamageInput Input;
		Input.AttackerLevel = User->Level;
		Input.Power = Move->Power > 0 ? Move->Power : FMath::Max(User->Level, 40);
		Input.Attack = FMath::FloorToInt((bPhysical ? User->Stats.Attack : User->Stats.SpecialAttack)
			* UBattleRules::GetStatStageMultiplier(Self.Volatile.Stages.Get(bPhysical ? EPokemonStat::Attack : EPokemonStat::SpecialAttack)));
		Input.Defense = FMath::FloorToInt((bPhysical ? Target->Stats.Defense : Target->Stats.SpecialDefense)
			* UBattleRules::GetStatStageMultiplier(Foe.Volatile.Stages.Get(bPhysical ? EPokemonStat::Defense : EPokemonStat::SpecialDefense)));
		Input.bSTAB = Move->Type == UserType1 || Move->Type == UserType2;
		Input.TypeEffectiveness = Effectiveness;
		Input.RandomPercent = 92;
		Input.bBurned = bPhysical && User->Status == EMajorStatus::Burn;

		const float Hits = Move->MinHits > 0 ? (Move->MinHits + FMath::Max(Move->MinHits, Move->MaxHits)) * 0.5f : 1.f;
		const float Damage = UBattleRules::CalculateDamage(Input) * Hits;
		float Score = FMath::Min(Damage / FMath::Max(Target->CurrentHP, 1), 1.f) * 100.f * Accuracy;
		if (Damage >= Target->CurrentHP)
		{
			Score += 40.f * Accuracy; // met K.O. : on la préfère
		}
		if (Move->Priority > 0 && Damage >= Target->CurrentHP)
		{
			Score += 20.f;
		}
		if (Move->Drain < 0 || Move->Healing < 0)
		{
			Score *= 0.8f; // contrecoup
		}
		return FMath::Max(Score, 1.f);
	}

	// Attaques de statut : utiles surtout au début, inutiles si l'effet est déjà là
	const bool bEarly = Self.Volatile.Stages.Attack + Self.Volatile.Stages.Defense < 2 && Foe.Volatile.Stages.Attack + Foe.Volatile.Stages.Defense > -2;
	switch (Move->MetaCategory)
	{
	case EMoveMetaCategory::Ailment:
	{
		const bool bMajor = Move->Ailment == EMoveAilment::Paralysis || Move->Ailment == EMoveAilment::Sleep
			|| Move->Ailment == EMoveAilment::Poison || Move->Ailment == EMoveAilment::Burn || Move->Ailment == EMoveAilment::Freeze;
		if (bMajor && Target->Status != EMajorStatus::None) return 0.f;
		if (Move->Ailment == EMoveAilment::Confusion && Foe.Volatile.ConfusionTurns > 0) return 0.f;
		if (Move->Ailment == EMoveAilment::LeechSeed && (Foe.Volatile.bLeechSeeded || TargetType1 == EPokemonType::Grass || TargetType2 == EPokemonType::Grass)) return 0.f;
		if (Move->Type == EPokemonType::Electric && Effectiveness <= 0.f) return 0.f;
		return (Move->Ailment == EMoveAilment::Sleep ? 55.f : 40.f) * Accuracy;
	}
	case EMoveMetaCategory::NetGoodStats:
	{
		if (Move->StatChanges.Num() == 0) return 0.f;
		const bool bSelf = Move->Target == EMoveTarget::User;
		const FMoveStatChange& Change = Move->StatChanges[0];
		const int32 Stage = (bSelf ? Self : Foe).Volatile.Stages.Get(Change.Stat);
		if ((Change.Stages > 0 && Stage >= 6) || (Change.Stages < 0 && Stage <= -6)) return 0.f;
		// Une fois ou deux en début de combat, ensuite on attaque
		if (FMath::Abs(Stage) >= 2) return 2.f;
		return (bEarly ? 25.f : 8.f) * Accuracy / (1.f + 2.f * FMath::Abs(Stage));
	}
	case EMoveMetaCategory::Heal:
		return User->CurrentHP * 2 < User->GetMaxHP() ? 70.f : 0.f;
	case EMoveMetaCategory::OHKO:
		return User->Level >= Target->Level && Effectiveness > 0.f ? 30.f * Accuracy : 0.f;
	default:
		return bEarly ? 10.f : 3.f;
	}
}

FBattleAction FBattleAI::ChooseAction(const UBattleSession& Session, int32 Side, FRandomStream& Stream)
{
	if (Session.MustStruggle(Side))
	{
		return FBattleAction::MakeMove(0);
	}

	const FPokemonInstance* User = Session.GetSide(Side).GetActive();
	TArray<int32> Usable;
	for (int32 Slot = 0; User && Slot < User->Moves.Num(); ++Slot)
	{
		if (User->Moves[Slot].CurrentPP > 0)
		{
			Usable.Add(Slot);
		}
	}
	if (Usable.Num() == 0)
	{
		return FBattleAction::MakeMove(0);
	}

	// Pokémon sauvage : au hasard
	if (Session.GetBattleType() == EBattleType::Wild)
	{
		return FBattleAction::MakeMove(Usable[Stream.RandRange(0, Usable.Num() - 1)]);
	}

	// Dresseur : tirage pondéré par le carré de la note (les meilleures attaques sortent le plus souvent)
	TArray<float> Weights;
	float Total = 0.f;
	for (int32 Slot : Usable)
	{
		const float Score = ScoreMove(Session, Side, Slot);
		Weights.Add(Score * Score);
		Total += Score * Score;
	}
	if (Total <= 0.f)
	{
		return FBattleAction::MakeMove(Usable[Stream.RandRange(0, Usable.Num() - 1)]);
	}

	float Roll = Stream.FRand() * Total;
	for (int32 I = 0; I < Usable.Num(); ++I)
	{
		Roll -= Weights[I];
		if (Roll <= 0.f)
		{
			return FBattleAction::MakeMove(Usable[I]);
		}
	}
	return FBattleAction::MakeMove(Usable.Last());
}
