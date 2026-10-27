#include "Pokemon/PokemonDataSubsystem.h"
#include "Pokemon/PokemonDatabase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UPokemonDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (DatabaseAsset.IsNull())
	{
		DatabaseAsset = TSoftObjectPtr<UPokemonDatabase>(FSoftObjectPath(TEXT("/Game/Pokemon/Data/DA_PokemonDatabase.DA_PokemonDatabase")));
	}

	Database = DatabaseAsset.LoadSynchronous();
	if (!Database)
	{
		UE_LOG(LogTemp, Warning, TEXT("PokemonDataSubsystem : %s introuvable (lancer Tools/Data/import_data_tables.py)"), *DatabaseAsset.ToString());
	}
}

UPokemonDatabase* UPokemonDataSubsystem::GetDatabase(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UPokemonDataSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UPokemonDataSubsystem>() : nullptr;
	return Subsystem ? Subsystem->Database.Get() : nullptr;
}
