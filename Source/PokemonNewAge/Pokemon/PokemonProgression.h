#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pokemon/PokemonInstance.h"
#include "PokemonProgression.generated.h"

class UPokemonDatabase;

/**
 *  Progression d'un Pokémon hors combat (SCRUM-12, docs/PROGRESSION.md) : Super Bonbon, pierres d'évolution,
 *  Fil de Liaison, CT/CS, et ce qui suit une évolution. Règles seules, sans réseau ni affichage :
 *  UProgressionComponent les applique sur le serveur, l'écran de combat garde les siennes (UBattleSession).
 */

/** Un niveau gagné, avec les stats avant et après (panneau « Attaque +3 »). */
USTRUCT(BlueprintType)
struct FLevelUpInfo
{
	GENERATED_BODY()

	/** Niveau atteint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FPokemonBaseStats StatsBefore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FPokemonBaseStats StatsAfter;
};

/** Ce qu'un objet, un gain d'XP ou une évolution a changé, et ce que le joueur doit encore décider. */
USTRUCT(BlueprintType)
struct FProgressionOutcome
{
	GENERATED_BODY()

	/** Pokémon de l'équipe concerné */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	int32 PartyIndex = -1;

	/** Objet utilisé (None pour une évolution confirmée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	TArray<FLevelUpInfo> LevelUps;

	/** Appris tout de suite (il restait une case) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	TArray<FName> MovesLearned;

	/** Il connaît déjà 4 attaques : le joueur choisit laquelle oublier (ou renonce) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	TArray<FName> MovesToChoose;

	/** Espèce vers laquelle il veut évoluer : séquence à jouer, puis à confirmer. None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FName EvolvesInto;

	/** Après une évolution confirmée : l'espèce d'avant et la nouvelle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FName EvolvedFrom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FName EvolvedTo;

	bool HasChoices() const { return MovesToChoose.Num() > 0 || !EvolvesInto.IsNone(); }
};

/** Ce que ferait un objet sur un Pokémon (liste « APTE / PAS APTE » du sac). */
UENUM(BlueprintType)
enum class EItemCompatibility : uint8
{
	/** Pas un objet de progression : les soins ont leurs propres règles */
	NotApplicable,
	Able,
	NotAble,
	/** CT d'une attaque qu'il connaît déjà */
	AlreadyKnown,
	/** Compatible mais sans effet maintenant (Super Bonbon au niveau 100) */
	NoEffect
};

UCLASS()
class UPokemonProgression : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Stats qu'il aurait à ce niveau (mêmes IV, EV et nature) */
	UFUNCTION(BlueprintPure, Category="Progression")
	static FPokemonBaseStats GetStatsAtLevel(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, int32 Level);

	/** Vrai s'il tient une Pierre Stase */
	UFUNCTION(BlueprintPure, Category="Progression")
	static bool IsEvolutionBlocked(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon);

	/** Évolution par niveau possible maintenant (Pierre Stase et K.O. compris), None sinon */
	UFUNCTION(BlueprintPure, Category="Progression")
	static FName GetLevelUpEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon);

	/** Évolution par échange de l'espèce (Kadabra...), None sinon */
	UFUNCTION(BlueprintPure, Category="Progression")
	static FName GetTradeEvolution(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon);

	/** Espèce obtenue avec cet objet (pierre ou Fil de Liaison), None sinon */
	UFUNCTION(BlueprintPure, Category="Progression")
	static FName GetEvolutionForItem(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon, FName Item);

	UFUNCTION(BlueprintPure, Category="Progression")
	static EItemCompatibility GetItemCompatibility(const UPokemonDatabase* Database, FName Item, const FPokemonInstance& Pokemon);

	/** Vrai si l'objet est un objet de progression (DT_Items, FieldEffect) */
	UFUNCTION(BlueprintPure, Category="Progression")
	static bool IsProgressionItem(const UPokemonDatabase* Database, FName Item);

	/**
	 *  Utilise un objet de progression. Ne touche pas au sac : bOutConsumed dit s'il faut le retirer.
	 *  L'évolution n'est PAS faite ici (Outcome.EvolvesInto) : la séquence la propose, le joueur peut l'annuler.
	 */
	static bool UseItem(const UPokemonDatabase* Database, FName Item, FPokemonInstance& Pokemon, FProgressionOutcome& Outcome, bool& bOutConsumed);

	/** Ajoute de l'XP hors combat (ou des niveaux) : stats, attaques du niveau et évolution à proposer */
	static void GainExperience(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, int32 Amount, FProgressionOutcome& Outcome);
	static void GainLevels(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, int32 Levels, FProgressionOutcome& Outcome);

	/** Fait évoluer et apprend les attaques de la nouvelle espèce à ce niveau (ou les met dans MovesToChoose) */
	static bool ApplyEvolution(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, FName NewSpecies, FProgressionOutcome& Outcome);

	/** Apprend dans une case libre, sinon l'ajoute à MovesToChoose (rien si déjà connue) */
	static void LearnOrQueue(const UPokemonDatabase* Database, FPokemonInstance& Pokemon, FName Move, FProgressionOutcome& Outcome);
};
