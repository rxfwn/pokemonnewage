#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pokemon/PokemonTypes.h"
#include "PokemonInstance.generated.h"

class UPokemonDatabase;

/**
 *  Un Pokémon qui existe dans la partie (SCRUM-30) : celui du joueur, d'un dresseur ou un sauvage.
 *  Ne contient que des identifiants (FName) et des nombres : c'est léger à répliquer et à sauvegarder,
 *  tout le reste (nom, types, stats de base) se relit dans UPokemonDatabase.
 */

/** Une attaque connue et ses PP restants. */
USTRUCT(BlueprintType)
struct FPokemonMoveSlot
{
	GENERATED_BODY()

	/** Ligne de DT_Moves */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FName Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	int32 CurrentPP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	int32 MaxPP = 0;
};

USTRUCT(BlueprintType)
struct FPokemonInstance
{
	GENERATED_BODY()

	/** Ligne de DT_Species. None = case vide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FName Species;

	/** Surnom, vide = nom de l'espèce */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FString Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	int32 Level = 1;

	/** Expérience totale (voir DT_Experience) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	int32 Experience = 0;

	/** Ligne de DT_Natures */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FName Nature;

	/** Ligne de DT_Abilities */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FName Ability;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	bool bIsFemale = false;

	/** Individuelles, 0 à 31 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	FPokemonBaseStats IVs;

	/** Points d'effort, 252 max par stat et 510 au total */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	FPokemonBaseStats EVs;

	/** Stats calculées (Stats.HP = PV max). Recalculées par UPokemonLibrary::RecalculateStats */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	FPokemonBaseStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	int32 CurrentHP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	EMajorStatus Status = EMajorStatus::None;

	/** Tours de sommeil restants (Status = Sleep) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	int32 SleepTurns = 0;

	/** 4 au maximum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	TArray<FPokemonMoveSlot> Moves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	int32 Friendship = 70;

	/** Poké Ball où il a été capturé (ligne de DT_Items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FName Ball;

	/** Lieu de rencontre (PalletTown, Route1...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	FName MetLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pokemon")
	int32 MetLevel = 1;

	bool IsValid() const { return !Species.IsNone(); }
	bool IsFainted() const { return CurrentHP <= 0; }
	bool CanBattle() const { return IsValid() && !IsFainted(); }
	int32 GetMaxHP() const { return Stats.HP; }
	int32 FindMove(FName MoveId) const;
};

/** Un objet du sac et sa quantité (ligne de DT_Items, ou de DT_Machines pour les CT). */
USTRUCT(BlueprintType)
struct FItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
	int32 Count = 0;
};

/** Fonctions sur les Pokémon, sans état (C++ et Blueprint). */
UCLASS()
class UPokemonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 *  Crée un Pokémon : IV, nature, talent et sexe tirés au hasard, 4 dernières attaques apprises au niveau.
	 *  FixedIV >= 0 : toutes les IV à cette valeur (dresseurs), sinon au hasard.
	 */
	UFUNCTION(BlueprintCallable, Category="Pokemon")
	static bool CreatePokemon(const UPokemonDatabase* Database, FName Species, int32 Level, UPARAM(ref) FRandomStream& Stream,
		FPokemonInstance& OutPokemon, int32 FixedIV = -1);

	/** Une stat à partir de la base, des IV/EV, du niveau et de la nature en % (90, 100 ou 110). Formule depuis la 3G */
	static int32 CalculateStat(EPokemonStat Stat, int32 Base, int32 IV, int32 EV, int32 Level, int32 NaturePercent = 100);

	/** Recalcule Stats. Si les PV max changent, les PV actuels suivent (comme à la montée de niveau) */
	UFUNCTION(BlueprintCallable, Category="Pokemon")
	static void RecalculateStats(const UPokemonDatabase* Database, UPARAM(ref) FPokemonInstance& Pokemon);

	/** Remet PV, PP et statut au maximum (Centre Pokémon) */
	UFUNCTION(BlueprintCallable, Category="Pokemon")
	static void HealFully(UPARAM(ref) FPokemonInstance& Pokemon);

	/** Nom affiché : surnom ou nom de l'espèce */
	UFUNCTION(BlueprintPure, Category="Pokemon")
	static FText GetDisplayName(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon);

	/** Types de l'espèce */
	static void GetTypes(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, EPokemonType& OutType1, EPokemonType& OutType2);

	/** Ajoute des EV en respectant 252 par stat et 510 au total */
	static void AddEVs(FPokemonInstance& Pokemon, const FPokemonBaseStats& Yield);

	/**
	 *  Ajoute de l'expérience. Remplit OutNewLevels avec chaque niveau atteint (vide si aucun).
	 *  Les stats sont recalculées ; les attaques ne sont PAS apprises ici (voir GetMovesLearnedAtLevel).
	 */
	UFUNCTION(BlueprintCallable, Category="Pokemon")
	static void AddExperience(const UPokemonDatabase* Database, UPARAM(ref) FPokemonInstance& Pokemon, int32 Amount, TArray<int32>& OutNewLevels);

	/** Apprend une attaque : dans une case libre, ou à la place de ForgetSlot (0 à 3). Faux si impossible */
	UFUNCTION(BlueprintCallable, Category="Pokemon")
	static bool LearnMove(const UPokemonDatabase* Database, UPARAM(ref) FPokemonInstance& Pokemon, FName Move, int32 ForgetSlot = -1);

	/** Espèce vers laquelle il évolue maintenant par niveau, None sinon */
	UFUNCTION(BlueprintPure, Category="Pokemon")
	static FName GetLevelEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon);

	/** Espèce obtenue avec cet objet (Pierre Feu...), None sinon */
	UFUNCTION(BlueprintPure, Category="Pokemon")
	static FName GetItemEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, FName Item);

	/**
	 *  Fait évoluer : change l'espèce et recalcule les stats (les PV perdus restent perdus).
	 *  OutMovesToLearn : attaques de la nouvelle espèce apprises à ce niveau, à proposer au joueur.
	 */
	UFUNCTION(BlueprintCallable, Category="Pokemon")
	static bool Evolve(const UPokemonDatabase* Database, UPARAM(ref) FPokemonInstance& Pokemon, FName NewSpecies, TArray<FName>& OutMovesToLearn);
};
