#include "Follower/PokemonFollowerComponent.h"
#include "Follower/PokemonFollower.h"
#include "Battle/BattleSubsystem.h"
#include "Core/PokemonPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Pokemon/PokemonDataRows.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Pokemon/PokemonDatabase.h"

UPokemonFollowerComponent::UPokemonFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Pas besoin de regarder l'équipe à chaque image
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

void UPokemonFollowerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPokemonFollowerComponent, bFollowerEnabled);
}

void UPokemonFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Refresh();
	}
}

void UPokemonFollowerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyFollower();
	Super::EndPlay(EndPlayReason);
}

bool UPokemonFollowerComponent::IsInBattle() const
{
	const UWorld* World = GetWorld();
	const UBattleSubsystem* Battles = World ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
	return Battles && Battles->GetBattle(Cast<APlayerController>(GetOwner())) != nullptr;
}

void UPokemonFollowerComponent::Refresh()
{
	APlayerController* Player = Cast<APlayerController>(GetOwner());
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const APokemonPlayerState* State = Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;

	// Qui doit sortir : le premier de l'équipe, s'il peut se battre et sait suivre le joueur
	FName Wanted;
	EFollowMode Mode = EFollowMode::Walk;
	if (bFollowerEnabled && Pawn && State && State->Party.Num() > 0 && State->Party[0].CanBattle() && !IsInBattle())
	{
		const FName Species = State->Party[0].Species;
		if (const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this))
		{
			if (const FSpeciesRow* Row = Database->FindSpecies(Species))
			{
				Mode = Row->FollowMode;
			}
		}
		if (Mode != EFollowMode::None)
		{
			Wanted = Species;
		}
	}

	if (Wanted.IsNone())
	{
		DestroyFollower();
		return;
	}
	if (Follower && (Follower->GetSpecies() != Wanted || Follower->GetFollowMode() != Mode))
	{
		// Autre Pokémon en tête, ou il vient d'évoluer
		DestroyFollower();
	}

	if (!Follower)
	{
		FActorSpawnParameters Params;
		Params.Owner = Player;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const TSubclassOf<APokemonFollower> Class = FollowerClass ? FollowerClass : TSubclassOf<APokemonFollower>(APokemonFollower::StaticClass());
		Follower = GetWorld()->SpawnActor<APokemonFollower>(Class, Pawn->GetActorTransform(), Params);
		if (Follower)
		{
			Follower->Setup(Pawn, Wanted, Mode);
		}
	}
	else
	{
		// Le joueur a réapparu (nouveau pion)
		Follower->SetLeader(Pawn);
	}
}

void UPokemonFollowerComponent::DestroyFollower()
{
	if (Follower)
	{
		Follower->Destroy();
		Follower = nullptr;
	}
}

void UPokemonFollowerComponent::SetFollowerEnabled(bool bEnabled)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bFollowerEnabled = bEnabled;
		Refresh();
	}
	else
	{
		ServerSetFollowerEnabled(bEnabled);
	}
}

void UPokemonFollowerComponent::ServerSetFollowerEnabled_Implementation(bool bEnabled)
{
	SetFollowerEnabled(bEnabled);
}

void UPokemonFollowerComponent::PetFollower(APokemonFollower* Target)
{
	if (!Target)
	{
		return;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const APlayerController* Player = Cast<APlayerController>(GetOwner());
		Target->Pet(Player ? Player->GetPawn() : nullptr);
	}
	else
	{
		ServerPetFollower(Target);
	}
}

void UPokemonFollowerComponent::ServerPetFollower_Implementation(APokemonFollower* Target)
{
	// Seulement à portée de main (pas de caresse à distance par un client trop malin)
	const APlayerController* Player = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (Target && Pawn && FVector::DistSquared(Target->GetActorLocation(), Pawn->GetActorLocation()) < FMath::Square(600.f))
	{
		PetFollower(Target);
	}
}
