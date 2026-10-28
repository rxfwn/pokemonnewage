// Copyright Epic Games, Inc. All Rights Reserved.

#include "PokemonNewAgeGameMode.h"
#include "Core/PokemonPlayerState.h"

APokemonNewAgeGameMode::APokemonNewAgeGameMode()
{
	// Équipe, sac, argent et drapeaux de chaque joueur (SCRUM-68)
	PlayerStateClass = APokemonPlayerState::StaticClass();
}
