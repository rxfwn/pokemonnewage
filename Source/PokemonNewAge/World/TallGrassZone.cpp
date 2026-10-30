#include "World/TallGrassZone.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSubsystem.h"
#include "Capture/CaptureComponent.h"
#include "Components/BoxComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "PokemonNewAge.h"

ATallGrassZone::ATallGrassZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.1f;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(500.f, 500.f, 100.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(80, 200, 60);
}

void ATallGrassZone::BeginPlay()
{
	Super::BeginPlay();

	Stream.Initialize(FMath::Rand());
	if (HasAuthority())
	{
		Box->OnComponentBeginOverlap.AddDynamic(this, &ATallGrassZone::OnBoxBegin);
		Box->OnComponentEndOverlap.AddDynamic(this, &ATallGrassZone::OnBoxEnd);
	}
}

void ATallGrassZone::OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* Player = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	if (!Player || Walkers.ContainsByPredicate([Pawn](const FGrassWalker& W) { return W.Pawn == Pawn; }))
	{
		return;
	}

	FGrassWalker& Walker = Walkers.AddDefaulted_GetRef();
	Walker.Pawn = Pawn;
	Walker.LastLocation = Pawn->GetActorLocation();
	SetActorTickEnabled(true);

	// Le lieu des herbes devient celui du joueur (lieu de rencontre des Pokémon capturés)
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const FEncounterTableRow* Table = Database ? Database->FindEncounterTable(EncounterTable) : nullptr;
	APokemonPlayerState* State = Player->GetPlayerState<APokemonPlayerState>();
	if (Table && State && !Table->Location.IsNone())
	{
		State->CurrentLocation = Table->Location;
	}
}

void ATallGrassZone::OnBoxEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	Walkers.RemoveAll([OtherActor](const FGrassWalker& W) { return !W.Pawn.IsValid() || W.Pawn.Get() == OtherActor; });
	SetActorTickEnabled(Walkers.Num() > 0);
}

void ATallGrassZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Walkers.RemoveAll([](const FGrassWalker& W) { return !W.Pawn.IsValid(); });
	for (FGrassWalker& Walker : Walkers)
	{
		APawn* Pawn = Walker.Pawn.Get();
		APlayerController* Player = Pawn->GetController<APlayerController>();
		if (Player && TakeStep(Walker, Player))
		{
			// Un combat par tick au plus : les autres joueurs reprennent au tick suivant
			break;
		}
	}
	if (Walkers.Num() == 0)
	{
		SetActorTickEnabled(false);
	}
}

bool ATallGrassZone::TakeStep(FGrassWalker& Walker, APlayerController* Player)
{
	APawn* Pawn = Walker.Pawn.Get();
	const FVector Location = Pawn->GetActorLocation();
	const float Moved = FVector::Dist2D(Location, Walker.LastLocation);
	Walker.LastLocation = Location;

	UBattleSubsystem* Battles = GetWorld()->GetSubsystem<UBattleSubsystem>();
	if (!Battles || Battles->GetBattle(Player))
	{
		Walker.bWasInBattle = true;
		return false;
	}
	if (Walker.bWasInBattle)
	{
		Walker.bWasInBattle = false;
		Walker.SafeSteps = SafeStepsAfterBattle;
		Walker.Distance = 0.f;
	}

	// En l'air (saut, chute) ou téléporté : ça ne compte pas comme des pas
	const UCharacterMovementComponent* Movement = Pawn->FindComponentByClass<UCharacterMovementComponent>();
	if ((Movement && Movement->IsFalling()) || Moved > StepLength * 4.f)
	{
		return false;
	}

	Walker.Distance += Moved;
	while (Walker.Distance >= StepLength)
	{
		Walker.Distance -= StepLength;
		if (Walker.SafeSteps > 0)
		{
			--Walker.SafeSteps;
			continue;
		}
		if (!UBattleRules::RollEncounterStep(EncounterRate, Stream))
		{
			continue;
		}

		const UCaptureComponent* Capture = Player->FindComponentByClass<UCaptureComponent>();
		const APokemonPlayerState* State = Player->GetPlayerState<APokemonPlayerState>();
		if ((Capture && !Capture->AreEncountersEnabled()) || !State || !State->HasPokemonAbleToBattle())
		{
			return false;
		}
		Walker.Distance = 0.f;
		if (Battles->StartWildEncounter(Player, EncounterTable))
		{
			return true;
		}
		UE_LOG(LogPokemonNewAge, Warning, TEXT("%s : rencontre impossible, table %s introuvable dans DT_Encounters ?"),
			*GetActorNameOrLabel(), *EncounterTable.ToString());
		return false;
	}
	return false;
}
