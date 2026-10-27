#include "Battle/BattleRules.h"

int32 FBattleStatStages::Get(EPokemonStat Stat) const
{
	switch (Stat)
	{
	case EPokemonStat::Attack:         return Attack;
	case EPokemonStat::Defense:        return Defense;
	case EPokemonStat::SpecialAttack:  return SpecialAttack;
	case EPokemonStat::SpecialDefense: return SpecialDefense;
	case EPokemonStat::Speed:          return Speed;
	case EPokemonStat::Accuracy:       return Accuracy;
	case EPokemonStat::Evasion:        return Evasion;
	default:                           return 0;
	}
}

int32 FBattleStatStages::Add(EPokemonStat Stat, int32 Delta)
{
	int32* Value = nullptr;
	switch (Stat)
	{
	case EPokemonStat::Attack:         Value = &Attack; break;
	case EPokemonStat::Defense:        Value = &Defense; break;
	case EPokemonStat::SpecialAttack:  Value = &SpecialAttack; break;
	case EPokemonStat::SpecialDefense: Value = &SpecialDefense; break;
	case EPokemonStat::Speed:          Value = &Speed; break;
	case EPokemonStat::Accuracy:       Value = &Accuracy; break;
	case EPokemonStat::Evasion:        Value = &Evasion; break;
	default:                           return 0;
	}
	const int32 Old = *Value;
	*Value = FMath::Clamp(Old + Delta, -6, 6);
	return *Value - Old;
}

bool FBattleSide::HasPokemonLeft() const
{
	return Party.ContainsByPredicate([](const FPokemonInstance& Pokemon) { return Pokemon.CanBattle(); });
}

int32 FBattleSide::GetItemCount(FName Item) const
{
	const FItemStack* Stack = Inventory.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; });
	return Stack ? Stack->Count : 0;
}

int32 UBattleRules::CalculateDamage(const FDamageInput& Input)
{
	if (Input.TypeEffectiveness <= 0.f || Input.Power <= 0)
	{
		return 0;
	}

	const int32 LevelFactor = 2 * Input.AttackerLevel / 5 + 2;
	int32 Damage = LevelFactor * Input.Power * FMath::Max(Input.Attack, 1) / FMath::Max(Input.Defense, 1) / 50 + 2;

	if (Input.bCritical)
	{
		Damage = Damage * 3 / 2;
	}
	Damage = Damage * FMath::Clamp(Input.RandomPercent, 85, 100) / 100;
	if (Input.bSTAB)
	{
		Damage = Damage * 3 / 2;
	}
	Damage = FMath::FloorToInt(Damage * Input.TypeEffectiveness);
	if (Input.bBurned)
	{
		Damage /= 2;
	}
	return FMath::Max(Damage, 1);
}

float UBattleRules::GetStatStageMultiplier(int32 Stage)
{
	Stage = FMath::Clamp(Stage, -6, 6);
	return Stage >= 0 ? (2.f + Stage) / 2.f : 2.f / (2.f - Stage);
}

float UBattleRules::GetAccuracyStageMultiplier(int32 Stage)
{
	Stage = FMath::Clamp(Stage, -6, 6);
	return Stage >= 0 ? (3.f + Stage) / 3.f : 3.f / (3.f - Stage);
}

float UBattleRules::GetHitChance(int32 MoveAccuracy, int32 AttackerAccuracyStage, int32 DefenderEvasionStage)
{
	if (MoveAccuracy <= 0)
	{
		return 1000.f;
	}
	return MoveAccuracy * GetAccuracyStageMultiplier(AttackerAccuracyStage - DefenderEvasionStage);
}

int32 UBattleRules::GetCriticalHitDenominator(int32 CritStage)
{
	switch (FMath::Max(CritStage, 0))
	{
	case 0:  return 24;
	case 1:  return 8;
	case 2:  return 2;
	default: return 1;
	}
}

int32 UBattleRules::RollHitCount(int32 MinHits, int32 MaxHits, FRandomStream& Stream)
{
	if (MinHits <= 0)
	{
		return 1;
	}
	if (MinHits == 2 && MaxHits == 5)
	{
		const int32 Roll = Stream.RandRange(0, 99);
		return Roll < 35 ? 2 : Roll < 70 ? 3 : Roll < 85 ? 4 : 5;
	}
	return Stream.RandRange(MinHits, FMath::Max(MinHits, MaxHits));
}

int32 UBattleRules::GetEffectiveSpeed(int32 Speed, int32 SpeedStage, bool bParalyzed)
{
	int32 Result = FMath::FloorToInt(Speed * GetStatStageMultiplier(SpeedStage));
	if (bParalyzed)
	{
		Result /= 2;
	}
	return FMath::Max(Result, 1);
}

float UBattleRules::GetCatchValue(int32 MaxHP, int32 CurrentHP, int32 SpeciesCatchRate, float BallMultiplier, EMajorStatus Status)
{
	MaxHP = FMath::Max(MaxHP, 1);
	CurrentHP = FMath::Clamp(CurrentHP, 1, MaxHP);

	float StatusBonus = 1.f;
	switch (Status)
	{
	case EMajorStatus::Sleep:
	case EMajorStatus::Freeze:
		StatusBonus = 2.5f;
		break;
	case EMajorStatus::Paralysis:
	case EMajorStatus::Burn:
	case EMajorStatus::Poison:
	case EMajorStatus::BadlyPoisoned:
		StatusBonus = 1.5f;
		break;
	default:
		break;
	}

	return (3.f * MaxHP - 2.f * CurrentHP) * SpeciesCatchRate * BallMultiplier / (3.f * MaxHP) * StatusBonus;
}

namespace
{
	/** Seuil d'une secousse sur 65536 */
	float ShakeThreshold(float CatchValue)
	{
		return 65536.f / FMath::Pow(255.f / FMath::Max(CatchValue, 0.0001f), 0.1875f);
	}
}

float UBattleRules::GetCatchProbability(float CatchValue)
{
	if (CatchValue >= 255.f)
	{
		return 1.f;
	}
	return FMath::Pow(FMath::Clamp(ShakeThreshold(CatchValue) / 65536.f, 0.f, 1.f), 4.f);
}

bool UBattleRules::RollCapture(float CatchValue, bool bGuaranteed, FRandomStream& Stream, int32& OutShakes)
{
	OutShakes = 3;
	if (bGuaranteed || CatchValue >= 255.f)
	{
		return true;
	}

	const float Threshold = ShakeThreshold(CatchValue);
	for (int32 Check = 0; Check < 4; ++Check)
	{
		if (Stream.RandRange(0, 65535) >= Threshold)
		{
			OutShakes = FMath::Min(Check, 3);
			return false;
		}
	}
	return true;
}

int32 UBattleRules::GetExperienceGain(int32 BaseExperience, int32 FaintedLevel, int32 WinnerLevel, bool bTrainerBattle, bool bParticipated)
{
	const double Base = static_cast<double>(BaseExperience) * FaintedLevel / 5.0;
	const double LevelRatio = FMath::Pow((2.0 * FaintedLevel + 10.0) / (FaintedLevel + WinnerLevel + 10.0), 2.5);
	double Experience = Base * LevelRatio + 1.0;
	if (bTrainerBattle)
	{
		Experience *= 1.5;
	}
	if (!bParticipated)
	{
		Experience *= 0.5;
	}
	return FMath::Max(1, FMath::FloorToInt(Experience));
}

bool UBattleRules::RollEscape(int32 PlayerSpeed, int32 WildSpeed, int32 Attempts, FRandomStream& Stream)
{
	if (PlayerSpeed >= WildSpeed)
	{
		return true;
	}
	const int32 Odds = PlayerSpeed * 128 / FMath::Max(WildSpeed, 1) + 30 * Attempts;
	return Odds > 255 || Stream.RandRange(0, 255) < Odds;
}

int32 UBattleRules::GetMoneyLost(int32 BadgeCount, int32 HighestLevel, int32 PlayerMoney)
{
	static const int32 BasePayout[] = { 8, 16, 24, 36, 48, 64, 80, 100, 120 };
	const int32 Base = BasePayout[FMath::Clamp(BadgeCount, 0, 8)];
	return FMath::Clamp(Base * HighestLevel, 0, FMath::Max(PlayerMoney, 0));
}

bool UBattleRules::IsImmuneToStatus(EMajorStatus Status, EPokemonType Type1, EPokemonType Type2)
{
	auto HasType = [Type1, Type2](EPokemonType Type) { return Type1 == Type || Type2 == Type; };
	switch (Status)
	{
	case EMajorStatus::Burn:          return HasType(EPokemonType::Fire);
	case EMajorStatus::Freeze:        return HasType(EPokemonType::Ice);
	case EMajorStatus::Paralysis:     return HasType(EPokemonType::Electric);
	case EMajorStatus::Poison:
	case EMajorStatus::BadlyPoisoned: return HasType(EPokemonType::Poison) || HasType(EPokemonType::Steel);
	default:                          return false;
	}
}
