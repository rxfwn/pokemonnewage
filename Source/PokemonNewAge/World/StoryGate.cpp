#include "World/StoryGate.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PokemonNewAgePlayerController.h"

#define LOCTEXT_NAMESPACE "StoryGate"

AStoryGate::AStoryGate()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(100.f, 600.f, 300.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(220, 60, 60);
	RootComponent = Box;

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Box);
	Arrow->ArrowColor = FColor(60, 120, 255);
	Arrow->ArrowSize = 3.f;

	Message = LOCTEXT("Locked", "Tu ne peux pas encore aller par là.");

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.2f;
}

bool AStoryGate::CanPass(const APlayerController* Player) const
{
	const APokemonPlayerState* State = Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (!State)
	{
		return false;
	}
	for (const FName& Flag : RequiredFlags)
	{
		if (!Flag.IsNone() && !State->HasFlag(Flag))
		{
			return false;
		}
	}
	for (const FName& Badge : RequiredBadges)
	{
		if (!Badge.IsNone() && !State->Badges.Contains(Badge))
		{
			return false;
		}
	}
	return true;
}

void AStoryGate::BeginPlay()
{
	Super::BeginPlay();

	// Décidé par le serveur, joueur par joueur
	if (HasAuthority())
	{
		Box->OnComponentBeginOverlap.AddDynamic(this, &AStoryGate::OnBoxBegin);
		// Filet de sécurité : un joueur arrivé dans la boîte sans entrer (sauvegarde, téléportation)
		SetActorTickEnabled(true);
	}
}

void AStoryGate::OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	PushBack(Cast<APawn>(OtherActor));
}

void AStoryGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		PushBack(Cast<APawn>(Actor));
	}
}

bool AStoryGate::PushBack(APawn* Pawn)
{
	APlayerController* Player = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	if (!Player || CanPass(Player))
	{
		return false;
	}

	// Côté ouvert = derrière la flèche ; on remet le joueur juste devant la boîte, à la même place dans la largeur
	const FTransform& Transform = Box->GetComponentTransform();
	FVector Local = Transform.InverseTransformPosition(Pawn->GetActorLocation());
	const float Extent = Box->GetUnscaledBoxExtent().X;
	Local.X = -(Extent + PushBackDistance / FMath::Max(Transform.GetScale3D().X, 0.01f));
	FVector Target = Transform.TransformPosition(Local);
	Target.Z = Pawn->GetActorLocation().Z;

	if (ACharacter* Character = Cast<ACharacter>(Pawn); Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	Pawn->TeleportTo(Target, Pawn->GetActorRotation(), false, true);

	// Une scène ou un message, pas plus d'une fois toutes les 3 secondes
	const float Now = GetWorld()->GetTimeSeconds();
	float& Last = LastMessageTime.FindOrAdd(TWeakObjectPtr<APawn>(Pawn), -100.f);
	if (Now - Last >= 3.f)
	{
		Last = Now;
		UDialogueComponent* Dialogue = Player->FindComponentByClass<UDialogueComponent>();
		if (!ConversationId.IsNone() && Dialogue && !Dialogue->IsInConversation())
		{
			Dialogue->PlayConversation(ConversationId);
		}
		else if (APokemonNewAgePlayerController* Trainer = Cast<APokemonNewAgePlayerController>(Player))
		{
			Trainer->ClientShowGateMessage(Message);
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
