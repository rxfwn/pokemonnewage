#include "Capture/PokeBallThrow.h"
#include "Capture/PokeBallModel.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Remplaçants du moteur quand aucune Ball n'est importée */
	const TCHAR* FallbackMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

APokeBallThrow::APokeBallThrow()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Mesh);

	TopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopMesh"));
	TopMesh->SetupAttachment(Hinge);
	TopMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TopMesh->SetCastShadow(true);
	TopMesh->SetVisibility(false);

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Mesh);
	Flash->SetUsingAbsoluteScale(true);
	Flash->IntensityUnits = ELightUnits::Candelas;
	Flash->Intensity = 0.f;
	Flash->AttenuationRadius = 500.f;
	Flash->CastShadows = false;

	BallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Capture/SM_PokeBall.SM_PokeBall")));
	ThrowSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Capture/Sons/SW_Ball_Throw.SW_Ball_Throw")));
	OpenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Capture/Sons/SW_Ball_Open.SW_Ball_Open")));
	ShakeSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Capture/Sons/SW_Ball_Shake.SW_Ball_Shake")));
	CaughtSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Capture/Sons/SW_Ball_Caught.SW_Ball_Caught")));
	BrokeFreeSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Capture/Sons/SW_Ball_BrokeFree.SW_Ball_BrokeFree")));
	BallColors.Add(TEXT("PokeBall"), FLinearColor(0.80f, 0.05f, 0.04f));
	BallColors.Add(TEXT("GreatBall"), FLinearColor(0.08f, 0.25f, 0.85f));
	BallColors.Add(TEXT("UltraBall"), FLinearColor(0.95f, 0.75f, 0.05f));
	BallColors.Add(TEXT("MasterBall"), FLinearColor(0.45f, 0.10f, 0.65f));
}

void APokeBallThrow::SetupMesh(FName Ball)
{
	// Modèles de L'amie : deux moitiés, le couvercle s'ouvre
	const float TwoPartsSize = PokeBallModel::Apply(Ball, Mesh, Hinge, TopMesh);
	bTwoParts = TwoPartsSize > 0.f;
	if (bTwoParts)
	{
		BaseScale = BallDiameter / FMath::Max(TwoPartsSize, 0.01f);
		SetActorScale3D(FVector(BaseScale));
		return;
	}

	// Ancien modèle d'une pièce, teinté selon la Ball
	TopMesh->SetVisibility(false);
	UStaticMesh* Model = BallMesh.LoadSynchronous();
	FName Parameter = ColorParameter;
	if (!Model)
	{
		Model = LoadObject<UStaticMesh>(nullptr, FallbackMeshPath);
		Parameter = TEXT("Color");
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath))
		{
			Mesh->SetMaterial(0, Material);
		}
	}
	if (!Model)
	{
		return;
	}
	Mesh->SetStaticMesh(Model);

	const float ModelSize = FMath::Max(Model->GetBounds().BoxExtent.GetMax() * 2.f, 0.01f);
	BaseScale = BallDiameter / ModelSize;
	SetActorScale3D(FVector(BaseScale));

	const FLinearColor* Color = BallColors.Find(Ball);
	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(Parameter, Color ? *Color : FLinearColor(0.8f, 0.05f, 0.04f));
	}
}

void APokeBallThrow::SetLid(float Degrees, float Glow, const FLinearColor& GlowColor)
{
	if (bTwoParts)
	{
		PokeBallModel::SetOpenAngle(Hinge, Degrees);
		PokeBallModel::SetGlow(Mesh, TopMesh, Glow * GlowStrength, GlowColor);
	}
}

void APokeBallThrow::Play(const FVector& From, const FVector& Target, FName Ball, int32 Shakes, bool bCaught, AActor* IgnoredActor)
{
	SetupMesh(Ball);
	ShakesToShow = FMath::Clamp(Shakes, 0, 3);
	ShakesDone = 0;
	bCaughtResult = bCaught;
	StartPoint = From;

	// Le sol sous la cible (la Ball s'y pose)
	GroundPoint = Target;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PokeBallGround), false, this);
	if (IgnoredActor)
	{
		Query.AddIgnoredActor(IgnoredActor);
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Target + FVector(0.f, 0.f, 300.f), Target - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Query))
	{
		GroundPoint = Hit.ImpactPoint;
	}
	GroundPoint.Z += BallDiameter / 2.f;
	AirPoint = GroundPoint + FVector(0.f, 0.f, OpenHeight);

	// Au sol, le bouton de la Ball regarde le joueur
	RestRotation = FRotator(0.f, (From - GroundPoint).GetSafeNormal2D().Rotation().Yaw + ButtonYawOffset, 0.f);

	SetActorLocation(From);
	EnterStage(EStage::Throw);
}

void APokeBallThrow::EnterStage(EStage NewStage)
{
	Stage = NewStage;
	StageTime = 0.f;
	switch (Stage)
	{
	case EStage::Throw:
		PlaySound(ThrowSound);
		break;
	case EStage::Open:
		Flash->SetLightColor(FLinearColor(1.f, 0.35f, 0.3f));
		PlaySound(OpenSound);
		OnBallOpened();
		break;
	case EStage::Shake:
		++ShakesDone;
		PlaySound(ShakeSound);
		OnBallShake(ShakesDone);
		break;
	case EStage::Result:
		Flash->SetLightColor(bCaughtResult ? FLinearColor(1.f, 0.9f, 0.5f) : FLinearColor(1.f, 1.f, 1.f));
		PlaySound(bCaughtResult ? CaughtSound : BrokeFreeSound);
		OnBallResult(bCaughtResult);
		break;
	default:
		break;
	}
}

void APokeBallThrow::PlaySound(const TSoftObjectPtr<USoundBase>& Sound) const
{
	// Sons courts : les charger au moment voulu ne se voit pas (absents = silence)
	if (USoundBase* Loaded = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Loaded, GetActorLocation());
	}
}

void APokeBallThrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StageTime += DeltaSeconds;

	switch (Stage)
	{
	case EStage::Throw:
	{
		// Cloche : ligne droite + parabole, la Ball tourne sur elle-même
		const float T = FMath::Clamp(StageTime / ThrowDuration, 0.f, 1.f);
		SetActorLocation(FMath::Lerp(StartPoint, AirPoint, T) + FVector(0.f, 0.f, ArcHeight * 4.f * T * (1.f - T)));
		SetActorRotation(RestRotation + FRotator(-720.f * T, 0.f, 0.f));
		if (T >= 1.f)
		{
			EnterStage(EStage::Open);
		}
		break;
	}
	case EStage::Open:
	{
		// Éclair rouge : le Pokémon est aspiré dans la Ball
		const float T = FMath::Clamp(StageTime / OpenDuration, 0.f, 1.f);
		const float Pulse = FMath::Sin(T * PI);
		Flash->SetIntensity(400.f * Pulse);
		SetActorRotation(RestRotation);
		if (bTwoParts)
		{
			// Le couvercle s'ouvre puis se referme sur le Pokémon
			SetLid(OpenAngle * Pulse, Pulse, FLinearColor(1.f, 0.2f, 0.15f));
		}
		else
		{
			SetActorScale3D(FVector(BaseScale * (1.f + 0.25f * Pulse)));
		}
		if (T >= 1.f)
		{
			Flash->SetIntensity(0.f);
			SetActorScale3D(FVector(BaseScale));
			SetLid(0.f, 0.f, FLinearColor::Black);
			EnterStage(EStage::Drop);
		}
		break;
	}
	case EStage::Drop:
	{
		const float T = FMath::Clamp(StageTime / DropDuration, 0.f, 1.f);
		SetActorLocation(FMath::Lerp(AirPoint, GroundPoint, T * T));
		if (T >= 1.f)
		{
			if (ShakesToShow > 0)
			{
				EnterStage(EStage::Shake);
			}
			else
			{
				EnterStage(EStage::Result);
			}
		}
		break;
	}
	case EStage::Shake:
	{
		// Une oscillation sur la première moitié, puis la Ball se tient tranquille
		const float Swing = ShakeDuration * 0.5f;
		const float Roll = StageTime < Swing ? ShakeAngle * FMath::Sin(StageTime / Swing * 2.f * PI) : 0.f;
		SetActorRotation(RestRotation + FRotator(0.f, 0.f, Roll));
		if (StageTime >= ShakeDuration)
		{
			SetActorRotation(RestRotation);
			if (ShakesDone < ShakesToShow)
			{
				EnterStage(EStage::Shake);
			}
			else
			{
				EnterStage(EStage::Result);
			}
		}
		break;
	}
	case EStage::Result:
	{
		if (bCaughtResult)
		{
			// Clic : petit éclat doré, la Ball reste un moment puis disparaît
			const float Glow = FMath::Clamp(1.f - StageTime / 0.4f, 0.f, 1.f);
			Flash->SetIntensity(150.f * Glow);
			SetLid(0.f, 0.3f * Glow, FLinearColor(1.f, 0.85f, 0.4f));
			const float Fade = FMath::Clamp((ResultDuration - StageTime) / 0.3f, 0.f, 1.f);
			SetActorScale3D(FVector(BaseScale * Fade));
		}
		else
		{
			// La Ball s'ouvre dans un éclair blanc et disparaît
			const float T = FMath::Clamp(StageTime / BrokeFreeDuration(), 0.f, 1.f);
			Flash->SetIntensity(500.f * FMath::Sin(T * PI));
			if (bTwoParts)
			{
				// Le couvercle saute, la Ball brille puis disparaît
				SetLid(BrokeFreeAngle * FMath::Min(T * 3.f, 1.f), FMath::Sin(T * PI), FLinearColor::White);
				SetActorScale3D(FVector(BaseScale * (T < 0.7f ? 1.f : FMath::Max(0.f, (1.f - T) / 0.3f))));
			}
			else
			{
				SetActorScale3D(FVector(BaseScale * (T < 0.5f ? 1.f + T : FMath::Max(0.f, 3.f * (1.f - T)))));
			}
		}
		if (StageTime >= (bCaughtResult ? ResultDuration : BrokeFreeDuration()))
		{
			Destroy();
		}
		break;
	}
	default:
		break;
	}
}
