#include "Dialogue/DialogueNpc.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ADialogueNpc::ADialogueNpc()
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(40.f, 90.f);
	Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Capsule;

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(Capsule);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.8f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PlaceholderMesh->SetStaticMesh(CylinderMesh.Object);
	}

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(Capsule);
	SkeletalMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	SkeletalMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	EditorLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EditorLabel"));
	EditorLabel->SetupAttachment(Capsule);
	EditorLabel->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	EditorLabel->SetHorizontalAlignment(EHTA_Center);
	EditorLabel->SetWorldSize(24.f);
	EditorLabel->SetHiddenInGame(true);
}

void ADialogueNpc::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EditorLabel->SetText(FText::FromName(NpcId));
}

void ADialogueNpc::Interact_Implementation(APawn* InteractingPawn)
{
	APlayerController* PlayerController = InteractingPawn ? InteractingPawn->GetController<APlayerController>() : nullptr;
	UDialogueComponent* Dialogue = PlayerController ? PlayerController->FindComponentByClass<UDialogueComponent>() : nullptr;
	if (!Dialogue || Dialogue->IsInConversation())
	{
		return;
	}

	if (bFacePlayer)
	{
		FVector ToPawn = InteractingPawn->GetActorLocation() - GetActorLocation();
		ToPawn.Z = 0.f;
		if (!ToPawn.IsNearlyZero())
		{
			SetActorRotation(ToPawn.Rotation());
		}
	}

	Dialogue->TalkTo(NpcId);
}

TArray<FString> ADialogueNpc::GetNpcIdOptions() const
{
	TArray<FString> Options;
	if (const UDataTable* Npcs = LoadObject<UDataTable>(nullptr, TEXT("/Game/World/Dialogues/DT_Npcs.DT_Npcs")))
	{
		for (const FName& Row : Npcs->GetRowNames())
		{
			Options.Add(Row.ToString());
		}
	}
	return Options;
}
