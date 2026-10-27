#include "Core/PokemonSaveGame.h"
#include "Core/PokemonPlayerState.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

const FString UPokemonSaveLibrary::SaveSlot = TEXT("Partie");
const FString UPokemonSaveLibrary::OptionsSlot = TEXT("Options");

bool UPokemonSaveLibrary::SaveGame(APlayerController* Player)
{
	const APokemonPlayerState* State = Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (!State)
	{
		return false;
	}

	UPokemonSaveGame* Save = Cast<UPokemonSaveGame>(UGameplayStatics::CreateSaveGameObject(UPokemonSaveGame::StaticClass()));
	Save->SavedAt = FDateTime::Now();
	Save->MapName = UGameplayStatics::GetCurrentLevelName(Player, true);
	Save->Player = State->MakeSaveData();
	if (const APawn* Pawn = Player->GetPawn())
	{
		Save->Player.bHasTransform = true;
		Save->Player.PlayerLocation = Pawn->GetActorLocation();
		Save->Player.PlayerRotation = Pawn->GetActorRotation();
	}
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

bool UPokemonSaveLibrary::HasSaveGame()
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlot, 0);
}

UPokemonSaveGame* UPokemonSaveLibrary::LoadSaveGame()
{
	if (!HasSaveGame())
	{
		return nullptr;
	}
	return Cast<UPokemonSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
}

bool UPokemonSaveLibrary::DeleteSaveGame()
{
	return UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
}

UPokemonOptionsSave* UPokemonSaveLibrary::GetOptions()
{
	if (UGameplayStatics::DoesSaveGameExist(OptionsSlot, 0))
	{
		if (UPokemonOptionsSave* Options = Cast<UPokemonOptionsSave>(UGameplayStatics::LoadGameFromSlot(OptionsSlot, 0)))
		{
			return Options;
		}
	}
	return Cast<UPokemonOptionsSave>(UGameplayStatics::CreateSaveGameObject(UPokemonOptionsSave::StaticClass()));
}

void UPokemonSaveLibrary::SaveOptions(UPokemonOptionsSave* Options)
{
	if (Options)
	{
		UGameplayStatics::SaveGameToSlot(Options, OptionsSlot, 0);
	}
}

void UPokemonSaveLibrary::ApplyOptions(const UObject* WorldContextObject, const UPokemonOptionsSave* Options)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !Options)
	{
		return;
	}
	if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice())
	{
		AudioDevice->SetTransientPrimaryVolume(FMath::Clamp(Options->MasterVolume, 0.f, 1.f));
	}
}
