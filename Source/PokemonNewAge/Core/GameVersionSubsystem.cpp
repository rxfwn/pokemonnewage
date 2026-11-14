#include "Core/GameVersionSubsystem.h"
#include "PokemonNewAge.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "GameVersion"

namespace
{
	const TCHAR* DevVersion = TEXT("dev");
}

const FString& UGameVersionSubsystem::GetVersion()
{
	static const FString Version = []()
	{
		FString Text;
		// Racine du build packagé : le dossier qui contient PokemonNewAge.exe
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::RootDir(), TEXT("version.txt"))))
		{
			Text.TrimStartAndEndInline();
		}
		// Seulement les caractères qu'écrit le script : la version passe dans l'adresse de connexion
		for (const TCHAR Char : Text)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('.') && Char != TEXT('-') && Char != TEXT('_'))
			{
				return FString(DevVersion);
			}
		}
		return Text.IsEmpty() || Text.Len() > 64 ? FString(DevVersion) : Text;
	}();
	return Version;
}

bool UGameVersionSubsystem::IsCompatible(const FString& OtherVersion)
{
	const FString& Mine = GetVersion();
	return Mine == DevVersion || OtherVersion == DevVersion || OtherVersion == Mine;
}

UGameVersionSubsystem* UGameVersionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameVersionSubsystem>() : nullptr;
}

void UGameVersionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogPokemonNewAge, Log, TEXT("Version du jeu : %s"), *GetVersion());
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UGameVersionSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UGameVersionSubsystem::HandleTravelFailure);
	}
}

void UGameVersionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Deinitialize();
}

void UGameVersionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Le moteur prévient toutes les instances du jeu (fenêtres de test de l'éditeur) : on ne garde que la nôtre
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UE_LOG(LogPokemonNewAge, Warning, TEXT("Connexion refusee ou coupee : %s"), *ErrorString);
	LastNetworkError = ErrorString.IsEmpty()
		? LOCTEXT("ConnectionLost", "Connexion perdue avec le monde.")
		: FText::FromString(ErrorString);
}

void UGameVersionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UE_LOG(LogPokemonNewAge, Warning, TEXT("Voyage impossible : %s"), *ErrorString);
	LastNetworkError = ErrorString.IsEmpty()
		? LOCTEXT("TravelFailed", "Impossible de rejoindre ce monde.")
		: FText::FromString(ErrorString);
}

#undef LOCTEXT_NAMESPACE
