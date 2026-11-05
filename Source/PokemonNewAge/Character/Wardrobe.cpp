#include "Character/Wardrobe.h"
#include "Character/TrainerAppearanceComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AWardrobe::AWardrobe()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractArea"));
	InteractArea->SetBoxExtent(FVector(60.f, 90.f, 100.f));
	InteractArea->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(InteractArea);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(InteractArea);
}

void AWardrobe::Interact_Implementation(APawn* InteractingPawn)
{
	// La touche d'action est lue sur le PC du joueur : on ouvre l'éditeur chez lui
	if (UTrainerAppearanceComponent* Appearance = InteractingPawn ? InteractingPawn->FindComponentByClass<UTrainerAppearanceComponent>() : nullptr)
	{
		Appearance->OpenEditor(ETrainerEditorMode::Wardrobe);
	}
}
