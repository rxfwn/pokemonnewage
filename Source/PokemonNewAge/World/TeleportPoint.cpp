#include "World/TeleportPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATeleportPoint::ATeleportPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Root);
	Arrow->ArrowColor = FColor(60, 160, 255);
	Arrow->ArrowSize = 2.f;

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}
	Label = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (Label)
	{
		Label->SetupAttachment(Root);
		Label->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(80.f);
		Label->SetTextRenderColor(FColor(60, 160, 255));
		Label->SetHiddenInGame(true);
		Label->bIsEditorOnly = true;
	}
	// World Partition : toujours chargé, on peut s'y téléporter de n'importe où
	bIsSpatiallyLoaded = false;
#endif
}

#if WITH_EDITOR
void ATeleportPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Label)
	{
		Label->SetText(FText::FromName(PointName));
	}
}
#endif

TArray<ATeleportPoint*> ATeleportPoint::GetAll(const UObject* WorldContext)
{
	TArray<ATeleportPoint*> Points;
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<ATeleportPoint> It(World); It; ++It)
		{
			Points.Add(*It);
		}
	}
	Points.Sort([](const ATeleportPoint& A, const ATeleportPoint& B)
	{
		return A.PointName.ToString() < B.PointName.ToString();
	});
	return Points;
}

ATeleportPoint* ATeleportPoint::Find(const UObject* WorldContext, const FString& Query)
{
	const TArray<ATeleportPoint*> Points = GetAll(WorldContext);
	const FString Wanted = Query.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return nullptr;
	}
	if (Wanted.IsNumeric())
	{
		const int32 Index = FCString::Atoi(*Wanted) - 1;
		return Points.IsValidIndex(Index) ? Points[Index] : nullptr;
	}
	for (ATeleportPoint* Point : Points)
	{
		if (Point->PointName.ToString().Equals(Wanted, ESearchCase::IgnoreCase))
		{
			return Point;
		}
	}
	for (ATeleportPoint* Point : Points)
	{
		if (Point->PointName.ToString().StartsWith(Wanted, ESearchCase::IgnoreCase))
		{
			return Point;
		}
	}
	return nullptr;
}
