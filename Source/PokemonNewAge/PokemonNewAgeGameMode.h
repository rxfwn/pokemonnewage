// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PokemonNewAgeGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class APokemonNewAgeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	APokemonNewAgeGameMode();

	/** Refuse un joueur qui n'a pas la même version du jeu (docs/MISES_A_JOUR.md) */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};



