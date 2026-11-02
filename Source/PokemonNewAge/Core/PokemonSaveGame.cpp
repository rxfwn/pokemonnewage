#include "Core/PokemonSaveGame.h"
#include "Core/PokemonPlayerState.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "PokemonNewAgePlayerController.h"
#include "PokemonNewAge.h"

const FString UPokemonSaveLibrary::SaveSlot = TEXT("Partie");
const FString UPokemonSaveLibrary::OptionsSlot = TEXT("Options");

FString UPokemonSaveLibrary::GetSaveSlot(const UObject* WorldContextObject)
{
	// Dans l'éditeur, chaque fenêtre de test a son numéro (0 pour la première) : chacune garde sa partie
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const int32 PIEInstance = World ? World->GetOutermost()->GetPIEInstanceID() : INDEX_NONE;
	return PIEInstance > 0 ? FString::Printf(TEXT("%s_PIE%d"), *SaveSlot, PIEInstance) : SaveSlot;
}

bool UPokemonSaveLibrary::SaveGame(APlayerController* Player)
{
	const APokemonPlayerState* State = Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (!State)
	{
		return false;
	}
	// Client qui vient d'arriver : tant que le serveur n'a pas remis sa sauvegarde, son PlayerState est vide
	// et l'écrire effacerait sa vraie partie
	if (const APokemonNewAgePlayerController* Trainer = Cast<APokemonNewAgePlayerController>(Player); Trainer && !Trainer->IsSaveSynced())
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Sauvegarde ignoree : la partie n'est pas encore revenue du serveur"));
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
	if (!UGameplayStatics::SaveGameToSlot(Save, GetSaveSlot(Player), 0))
	{
		return false;
	}
	WriteTrainerCard(Save->Player);
	return true;
}

bool UPokemonSaveLibrary::HasSaveGame(const UObject* WorldContextObject)
{
	return UGameplayStatics::DoesSaveGameExist(GetSaveSlot(WorldContextObject), 0);
}

UPokemonSaveGame* UPokemonSaveLibrary::LoadSaveGame(const UObject* WorldContextObject)
{
	if (!HasSaveGame(WorldContextObject))
	{
		return nullptr;
	}
	return Cast<UPokemonSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlot(WorldContextObject), 0));
}

bool UPokemonSaveLibrary::DeleteSaveGame(const UObject* WorldContextObject)
{
	return UGameplayStatics::DeleteGameInSlot(GetSaveSlot(WorldContextObject), 0);
}

// ---------------------------------------------------------------- Cartes dresseur des amis

namespace PokemonFriendCards
{
	/** Dossier des amis choisi dans les réglages, vide s'il n'existe pas */
	FString GetFolder()
	{
		const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
		const FString Folder = Options ? Options->FriendsFolder.TrimStartAndEnd() : FString();
		return !Folder.IsEmpty() && IFileManager::Get().DirectoryExists(*Folder) ? Folder : FString();
	}

	FString GetFileName(const FString& Name, int32 Number)
	{
		return FPaths::MakeValidFileName(FString::Printf(TEXT("%s_%d"), Name.IsEmpty() ? TEXT("Dresseur") : *Name, Number), TEXT('_')) + TEXT(".json");
	}
}

FTrainerCard UPokemonSaveLibrary::MakeTrainerCard(const FPlayerSaveData& Data)
{
	FTrainerCard Card;
	Card.TrainerName = Data.TrainerName;
	Card.TrainerNumber = Data.TrainerNumber;
	for (const FPokemonInstance& Pokemon : Data.Party)
	{
		FTrainerCardPokemon& Entry = Card.Party.AddDefaulted_GetRef();
		Entry.Species = Pokemon.Species;
		Entry.Nickname = Pokemon.Nickname;
		Entry.Level = Pokemon.Level;
	}
	Card.Badges = Data.Badges;
	Card.PokedexSeen = Data.PokedexSeen.Num();
	Card.PokedexCaught = Data.PokedexCaught.Num();
	Card.CurrentLocation = Data.CurrentLocation;
	Card.PlayTimeSeconds = Data.PlayTimeSeconds;
	Card.SavedAt = FDateTime::UtcNow();
	return Card;
}

bool UPokemonSaveLibrary::WriteTrainerCard(const FPlayerSaveData& Data)
{
	const FString Folder = PokemonFriendCards::GetFolder();
	if (Folder.IsEmpty())
	{
		return false;
	}
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(MakeTrainerCard(Data), Json))
	{
		return false;
	}
	const FString Path = Folder / PokemonFriendCards::GetFileName(Data.TrainerName, Data.TrainerNumber);
	const bool bWritten = FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bWritten)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Carte dresseur non ecrite : %s"), *Path);
	}
	return bWritten;
}

TArray<FTrainerCard> UPokemonSaveLibrary::LoadFriendCards(const FString& OwnName, int32 OwnNumber)
{
	TArray<FTrainerCard> Cards;
	const FString Folder = PokemonFriendCards::GetFolder();
	if (Folder.IsEmpty())
	{
		return Cards;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		FString Json;
		FTrainerCard Card;
		// Un fichier à moitié synchronisé ou d'un autre format est simplement ignoré
		if (!FFileHelper::LoadFileToString(Json, *(Folder / File)) || !FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Card, 0, 0)
			|| Card.TrainerName.IsEmpty())
		{
			continue;
		}
		if (Card.TrainerNumber == OwnNumber && Card.TrainerName == OwnName)
		{
			continue;
		}
		Cards.Add(MoveTemp(Card));
	}
	Cards.Sort([](const FTrainerCard& A, const FTrainerCard& B) { return A.SavedAt > B.SavedAt; });
	return Cards;
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

	// Musique, Vfx, bruits de pas et météo : mélange SM_Options et classes de son créés par Tools/Audio/make_sound_classes.py
	// (lancé tout seul à l'ouverture de l'éditeur ; rien ne se passe sans eux)
	USoundMix* Mix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/SM_Options.SM_Options"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mix)
	{
		return;
	}
	const TPair<const TCHAR*, float> Classes[] = {
		{ TEXT("/Game/Audio/SC_Music.SC_Music"), Options->MusicVolume },
		{ TEXT("/Game/Audio/SC_Effects.SC_Effects"), Options->EffectsVolume },
		{ TEXT("/Game/Audio/SC_Footsteps.SC_Footsteps"), Options->FootstepsVolume },
		{ TEXT("/Game/Audio/SC_Weather.SC_Weather"), Options->WeatherVolume },
	};
	for (const TPair<const TCHAR*, float>& Class : Classes)
	{
		if (USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, Class.Key, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::SetSoundMixClassOverride(WorldContextObject, Mix, SoundClass, FMath::Clamp(Class.Value, 0.f, 1.f), 1.f, 0.f, true);
		}
	}
	UGameplayStatics::PushSoundMixModifier(WorldContextObject, Mix);
}
