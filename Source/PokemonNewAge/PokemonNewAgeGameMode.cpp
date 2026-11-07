// Copyright Epic Games, Inc. All Rights Reserved.

#include "PokemonNewAgeGameMode.h"
#include "Core/PokemonPlayerState.h"
#include "Core/GameVersionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "PokemonNewAge.h"

APokemonNewAgeGameMode::APokemonNewAgeGameMode()
{
	// Équipe, sac, argent et drapeaux de chaque joueur (SCRUM-68)
	PlayerStateClass = APokemonPlayerState::StaticClass();
}

void APokemonNewAgeGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	// Un ancien build n'envoie pas de version : il est refusé lui aussi
	const FString ClientVersion = UGameplayStatics::ParseOption(Options, TEXT("Version"));
	if (!UGameVersionSubsystem::IsCompatible(ClientVersion))
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Joueur refuse (%s) : version %s, le monde est en %s"),
			*Address, ClientVersion.IsEmpty() ? TEXT("inconnue") : *ClientVersion, *UGameVersionSubsystem::GetVersion());
		ErrorMessage = FString::Printf(TEXT("Version différente : toi %s, le monde %s. Fermez le jeu et relancez-le avec Pokemon.exe pour le mettre à jour."),
			ClientVersion.IsEmpty() ? TEXT("ancienne") : *ClientVersion, *UGameVersionSubsystem::GetVersion());
	}
}
