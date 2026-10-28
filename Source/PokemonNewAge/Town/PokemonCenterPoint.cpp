#include "Town/PokemonCenterPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

APokemonCenterPoint::APokemonCenterPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Root);
	Arrow->ArrowColor = FColor(230, 60, 60);
	Arrow->ArrowSize = 1.5f;

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}
	// World Partition : toujours chargé, on peut y être renvoyé de n'importe où
	bIsSpatiallyLoaded = false;
#endif
}

APokemonCenterPoint* APokemonCenterPoint::FindNearest(const UObject* WorldContext, const FVector& Location)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	APokemonCenterPoint* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<APokemonCenterPoint> It(World); It; ++It)
	{
		const float Distance = FVector::Dist(It->GetActorLocation(), Location);
		if (Distance <= It->HealRadius && Distance < BestDistance)
		{
			Best = *It;
			BestDistance = Distance;
		}
	}
	return Best;
}

APokemonCenterPoint* APokemonCenterPoint::FindById(const UObject* WorldContext, FName PointId)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || PointId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<APokemonCenterPoint> It(World); It; ++It)
	{
		if (It->PointId == PointId)
		{
			return *It;
		}
	}
	return nullptr;
}

APokemonCenterPoint* APokemonCenterPoint::FindHome(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<APokemonCenterPoint> It(World); It; ++It)
	{
		if (It->bIsHome)
		{
			return *It;
		}
	}
	return nullptr;
}
