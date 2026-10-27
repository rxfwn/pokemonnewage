#include "World/InteriorVolume.h"
#include "Components/BoxComponent.h"
#include "PokemonNewAgeCharacter.h"

AInteriorVolume::AInteriorVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(400.f, 400.f, 150.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
}

void AInteriorVolume::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &AInteriorVolume::OnBoxBegin);
	Box->OnComponentEndOverlap.AddDynamic(this, &AInteriorVolume::OnBoxEnd);
}

void AInteriorVolume::OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APokemonNewAgeCharacter* Character = Cast<APokemonNewAgeCharacter>(OtherActor))
	{
		Character->SetCameraDistanceLimit(InteriorCameraDistance);
	}
}

void AInteriorVolume::OnBoxEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (APokemonNewAgeCharacter* Character = Cast<APokemonNewAgeCharacter>(OtherActor))
	{
		Character->ClearCameraDistanceLimit();
	}
}
