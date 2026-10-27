#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Pokemon/PokemonDataRows.h"
#include "PokemonDatabase.generated.h"

class UDataTable;

/**
 *  DA_PokemonDatabase : regroupe toutes les Data Tables Pokémon et donne les recherches courantes.
 *  Données en lecture seule, identiques sur toutes les machines : en multijoueur, le serveur et
 *  les clients lisent chacun leur copie, seuls les identifiants (FName) passent par le réseau.
 *  On l'obtient partout avec UPokemonDataSubsystem::GetDatabase.
 */
UCLASS(BlueprintType)
class UPokemonDatabase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.SpeciesRow"))
	TObjectPtr<UDataTable> Species;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.MoveRow"))
	TObjectPtr<UDataTable> Moves;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.TypeChartRow"))
	TObjectPtr<UDataTable> TypeChart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.LearnsetRow"))
	TObjectPtr<UDataTable> Learnsets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.MachineRow"))
	TObjectPtr<UDataTable> Machines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.AbilityRow"))
	TObjectPtr<UDataTable> Abilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.NatureRow"))
	TObjectPtr<UDataTable> Natures;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.ExperienceRow"))
	TObjectPtr<UDataTable> Experience;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.EncounterTableRow"))
	TObjectPtr<UDataTable> Encounters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tables", meta=(RequiredAssetDataTags="RowStructure=/Script/PokemonNewAge.TrainerRow"))
	TObjectPtr<UDataTable> Trainers;

	// Recherches par identifiant (C++). Renvoient nullptr si la ligne n'existe pas.
	const FSpeciesRow* FindSpecies(FName Id) const;
	const FMoveRow* FindMove(FName Id) const;
	const FLearnsetRow* FindLearnset(FName SpeciesId) const;
	const FNatureRow* FindNature(FName Id) const;
	const FEncounterTableRow* FindEncounterTable(FName Id) const;
	const FTrainerRow* FindTrainer(FName Id) const;

	/** Copie de la fiche d'une espèce (Blueprint) */
	UFUNCTION(BlueprintCallable, Category="Pokemon|Data")
	bool GetSpecies(FName Id, FSpeciesRow& OutRow) const;

	/** Copie de la fiche d'une attaque (Blueprint) */
	UFUNCTION(BlueprintCallable, Category="Pokemon|Data")
	bool GetMove(FName Id, FMoveRow& OutRow) const;

	/** Multiplicateur de type : 0, 0.25, 0.5, 1, 2 ou 4 */
	UFUNCTION(BlueprintPure, Category="Pokemon|Data")
	float GetTypeEffectiveness(EPokemonType AttackType, EPokemonType DefenderType1, EPokemonType DefenderType2) const;

	/** Expérience totale nécessaire pour atteindre Level (1 à 100) */
	UFUNCTION(BlueprintPure, Category="Pokemon|Data")
	int32 GetExperienceForLevel(EGrowthRate GrowthRate, int32 Level) const;

	/** Niveau correspondant à une expérience totale */
	UFUNCTION(BlueprintPure, Category="Pokemon|Data")
	int32 GetLevelForExperience(EGrowthRate GrowthRate, int32 TotalExperience) const;

	/** Attaques apprises exactement à ce niveau (montée de niveau) */
	UFUNCTION(BlueprintCallable, Category="Pokemon|Data")
	TArray<FName> GetMovesLearnedAtLevel(FName SpeciesId, int32 Level) const;

	/** Les 4 dernières attaques apprises jusqu'à ce niveau (Pokémon sauvage ou de dresseur) */
	UFUNCTION(BlueprintCallable, Category="Pokemon|Data")
	TArray<FName> GetDefaultMoves(FName SpeciesId, int32 Level) const;

	/** Tire une case de la table de rencontre. Stream : pour un tirage reproductible (serveur) */
	UFUNCTION(BlueprintCallable, Category="Pokemon|Data")
	bool RollEncounter(FName EncounterTableId, UPARAM(ref) FRandomStream& Stream, FName& OutSpecies, int32& OutLevel) const;
};
