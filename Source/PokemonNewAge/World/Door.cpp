#include "World/Door.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(RootComponent);

	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Hinge);
	Panel->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(RootComponent);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
}

void ADoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Le gond est sur le bord gauche du passage, le battant part de là vers +Y
	Hinge->SetRelativeLocation(FVector(0.f, -DoorWidth / 2.f, 0.f));
	Panel->SetRelativeLocation(FVector(0.f, DoorWidth / 2.f, DoorHeight / 2.f));
	Panel->SetRelativeScale3D(FVector(DoorThickness / 100.f, DoorWidth / 100.f, DoorHeight / 100.f));

	Trigger->SetRelativeLocation(FVector(0.f, 0.f, DoorHeight / 2.f));
	Trigger->SetBoxExtent(FVector(DetectionDepth, DoorWidth / 2.f + 40.f, DoorHeight / 2.f));
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADoor::OnTriggerBegin);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &ADoor::OnTriggerEnd);
}

void ADoor::Open(APawn* FromPawn)
{
	if (bLocked)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CloseTimer);

	// Un angle positif rabat le battant vers -X : on l'ouvre du côté où le joueur n'est pas
	float Side = 1.f;
	if (FromPawn)
	{
		const FVector ToPawn = FromPawn->GetActorLocation() - GetActorLocation();
		Side = FVector::DotProduct(ToPawn, GetActorForwardVector()) >= 0.f ? 1.f : -1.f;
	}

	// Déjà ouverte : on ne la fait pas battre de l'autre côté sous le nez du joueur
	if (!IsOpen())
	{
		TargetAngle = Side * OpenAngle;
		SetActorTickEnabled(true);
	}
}

void ADoor::Close()
{
	TargetAngle = 0.f;
	SetActorTickEnabled(true);
}

void ADoor::Interact_Implementation(APawn* InteractingPawn)
{
	if (IsOpen())
	{
		Close();
	}
	else
	{
		Open(InteractingPawn);
	}
}

void ADoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CurrentAngle = FMath::FInterpTo(CurrentAngle, TargetAngle, DeltaSeconds, OpenSpeed);
	if (FMath::IsNearlyEqual(CurrentAngle, TargetAngle, 0.1f))
	{
		CurrentAngle = TargetAngle;
		SetActorTickEnabled(false);
	}
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
}

void ADoor::OnTriggerBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (bOpenOnApproach && Pawn && Pawn->IsPlayerControlled())
	{
		Open(Pawn);
	}
}

void ADoor::OnTriggerEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (bOpenOnApproach && IsOpen() && !IsPlayerNearby())
	{
		GetWorldTimerManager().SetTimer(CloseTimer, this, &ADoor::Close, FMath::Max(CloseDelay, 0.01f));
	}
}

bool ADoor::IsPlayerNearby() const
{
	TArray<AActor*> Pawns;
	Trigger->GetOverlappingActors(Pawns, APawn::StaticClass());
	for (const AActor* Actor : Pawns)
	{
		if (Cast<APawn>(Actor)->IsPlayerControlled())
		{
			return true;
		}
	}
	return false;
}
