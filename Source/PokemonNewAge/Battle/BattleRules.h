#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Battle/BattleTypes.h"
#include "BattleRules.generated.h"

/** Entrées de la formule de dégâts. */
USTRUCT(BlueprintType)
struct FDamageInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 AttackerLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 Power = 0;

	/** Attaque (physique) ou Attaque Spé. (spéciale), crans compris */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 Attack = 1;

	/** Défense ou Défense Spé., crans compris */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 Defense = 1;

	/** Même type que le lanceur (x1,5) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	bool bSTAB = false;

	/** 0, 0.25, 0.5, 1, 2 ou 4 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	float TypeEffectiveness = 1.f;

	/** Coup critique (x1,5) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	bool bCritical = false;

	/** Tirage aléatoire, 85 à 100 (%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 RandomPercent = 100;

	/** Lanceur brûlé qui utilise une attaque physique (x0,5) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	bool bBurned = false;
};

/**
 *  Formules du combat, sans état : faciles à tester (Tests/BattleTests.cpp) et à appeler depuis l'UI.
 *  Règles des jeux récents (6G et plus) : critique x1,5, paralysie = Vitesse / 2, brûlure 1/16...
 *  Les arrondis sont simplifiés (arrondi à l'entier inférieur à chaque étape).
 */
UCLASS()
class UBattleRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Dégâts (au moins 1 si le type n'est pas immunisé) */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static int32 CalculateDamage(const FDamageInput& Input);

	/** Multiplicateur d'un cran d'Attaque, Défense... : +1 = x1,5, +2 = x2, -1 = x0,67 */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static float GetStatStageMultiplier(int32 Stage);

	/** Multiplicateur d'un cran de Précision / Esquive : +1 = x1,33, -1 = x0,75 */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static float GetAccuracyStageMultiplier(int32 Stage);

	/** Chance de toucher en % (peut dépasser 100). MoveAccuracy = 0 : touche toujours */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static float GetHitChance(int32 MoveAccuracy, int32 AttackerAccuracyStage, int32 DefenderEvasionStage);

	/** 1 chance sur N d'avoir un critique : cran 0 = 24, 1 = 8, 2 = 2, 3 et plus = 1 */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static int32 GetCriticalHitDenominator(int32 CritStage);

	/** Nombre de coups d'une attaque multiple (2 à 5 : 35 %, 35 %, 15 %, 15 %) */
	static int32 RollHitCount(int32 MinHits, int32 MaxHits, FRandomStream& Stream);

	/** Vitesse en combat : crans et paralysie (x0,5) compris */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static int32 GetEffectiveSpeed(int32 Speed, int32 SpeedStage, bool bParalyzed);

	/**
	 *  Valeur « a » de la formule de capture (3G et plus) : ((3 PVmax - 2 PV) x taux x Ball) / (3 PVmax) x statut.
	 *  Statut : sommeil ou gel x2,5, paralysie, poison ou brûlure x1,5. 255 ou plus = capture sûre.
	 */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static float GetCatchValue(int32 MaxHP, int32 CurrentHP, int32 SpeciesCatchRate, float BallMultiplier, EMajorStatus Status);

	/** Chance de capture (0 à 1) correspondant à GetCatchValue, pour l'UI ou les tests */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static float GetCatchProbability(float CatchValue);

	/** Lance la Ball : 4 tests de secousse. OutShakes = secousses montrées (0 à 3). Vrai si capturé */
	static bool RollCapture(float CatchValue, bool bGuaranteed, FRandomStream& Stream, int32& OutShakes);

	/**
	 *  XP gagnée en battant un Pokémon (formule de la 5G et plus, qui avantage les Pokémon de plus bas niveau).
	 *  x1,5 contre un dresseur ; moitié pour ceux qui n'ont pas combattu (Multi Exp moderne).
	 */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static int32 GetExperienceGain(int32 BaseExperience, int32 FaintedLevel, int32 WinnerLevel, bool bTrainerBattle, bool bParticipated);

	/** Fuite (3G et plus) : réussit toujours si on est plus rapide, sinon (V x 128 / Vsauvage + 30 x essais) / 256 */
	static bool RollEscape(int32 PlayerSpeed, int32 WildSpeed, int32 Attempts, FRandomStream& Stream);

	/** Argent perdu quand toute l'équipe est K.O. : base selon les badges x plus haut niveau, au plus l'argent du joueur */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static int32 GetMoneyLost(int32 BadgeCount, int32 HighestLevel, int32 PlayerMoney);

	/** Ce type est immunisé contre ce statut (Feu ne brûle pas, Électrik n'est pas paralysé...) */
	UFUNCTION(BlueprintPure, Category="Battle|Rules")
	static bool IsImmuneToStatus(EMajorStatus Status, EPokemonType Type1, EPokemonType Type2);
};
