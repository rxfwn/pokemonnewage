#include "Character/CharacterStudio.h"
#include "Character/TrainerAppearanceData.h"
#include "Character/TrainerAppearanceRender.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "CharacterStudio"

namespace CharacterStudio
{
	// La pièce : 12 x 12 m, 6 m de haut, sol à Z = 0, personnage au centre, caméra côté +X
	constexpr float RoomHalf = 600.f;
	constexpr float RoomHeight = 600.f;
	constexpr float WallThickness = 20.f;

	/** Gris clair doux des captures d'inZOI */
	const FLinearColor WallColor(0.58f, 0.59f, 0.62f);
	const FLinearColor FloorColor(0.48f, 0.49f, 0.52f);

	/** Cadrages : point visé, distance (cm), champ de vision (degrés) */
	// Corps : ~2,1 m visibles en hauteur (16:9) ; visage : ~40 cm
	const FVector BodyTarget(0.f, 0.f, 95.f);
	constexpr float BodyDistance = 450.f;
	constexpr float BodyFov = 45.f;
	constexpr float FaceDistance = 140.f;
	constexpr float FaceFov = 28.f;
	/** Décalage du personnage vers la gauche de l'écran (0 = centré, 0,25 = à 37,5 % de la largeur) */
	constexpr float ScreenShift = 0.25f;
	/** Tête par défaut si l'os n'est pas trouvé */
	constexpr float DefaultHeadHeight = 158.f;

	struct FLighting
	{
		FText Name;
		float Key, Fill, Rim, Top;
		FLinearColor KeyColor, FillColor, RimColor;
	};

	TArray<FLighting> Lightings()
	{
		const FLinearColor White(1.f, 1.f, 1.f);
		return {
			// Doux en premier : lumière de face presque sans ombre dure, comme le studio d'inZOI
			{ LOCTEXT("LightSoft", "Doux"), 20.f, 17.f, 8.f, 12.f, FLinearColor(1.f, 0.97f, 0.94f), FLinearColor(0.97f, 0.98f, 1.f), White },
			{ LOCTEXT("LightStudio", "Studio"), 30.f, 11.f, 22.f, 8.f, White, White, White },
			{ LOCTEXT("LightEvening", "Soirée"), 32.f, 6.f, 28.f, 3.f, FLinearColor(1.f, 0.78f, 0.55f), FLinearColor(0.75f, 0.8f, 1.f), FLinearColor(0.55f, 0.7f, 1.f) },
		};
	}
}

using namespace CharacterStudio;

ACharacterStudio::ACharacterStudio()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Turntable = CreateDefaultSubobject<USceneComponent>(TEXT("Turntable"));
	Turntable->SetupAttachment(Root);

	// Les Skeletal Mesh importés regardent vers +Y : -90° pour qu'il regarde la caméra (+X)
	Preview = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Preview"));
	Preview->SetupAttachment(Turntable);
	Preview->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Preview->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(BodyFov);
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 0.4f;
	Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
	Camera->PostProcessSettings.VignetteIntensity = 0.25f;
	// Pas de flou de mouvement quand le personnage tourne
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Pièce fermée de cubes de 1 m étirés (sol, plafond, 4 murs)
	const float Size = RoomHalf * 2.f;
	const struct { const TCHAR* Name; FVector Location; FVector SizeCm; } WallDefs[] = {
		{ TEXT("Floor"), FVector(0.f, 0.f, -WallThickness * 0.5f), FVector(Size, Size, WallThickness) },
		{ TEXT("Ceiling"), FVector(0.f, 0.f, RoomHeight + WallThickness * 0.5f), FVector(Size, Size, WallThickness) },
		{ TEXT("WallBack"), FVector(-RoomHalf, 0.f, RoomHeight * 0.5f), FVector(WallThickness, Size, RoomHeight) },
		{ TEXT("WallFront"), FVector(RoomHalf, 0.f, RoomHeight * 0.5f), FVector(WallThickness, Size, RoomHeight) },
		{ TEXT("WallLeft"), FVector(0.f, -RoomHalf, RoomHeight * 0.5f), FVector(Size, WallThickness, RoomHeight) },
		{ TEXT("WallRight"), FVector(0.f, RoomHalf, RoomHeight * 0.5f), FVector(Size, WallThickness, RoomHeight) },
	};
	for (const auto& Def : WallDefs)
	{
		UStaticMeshComponent* Wall = MakeWall(Def.Name, Def.Location, Def.SizeCm);
		Wall->SetStaticMesh(CubeMesh.Object);
		if (ShapeMaterial.Object)
		{
			Wall->SetMaterial(0, ShapeMaterial.Object);
		}
	}

	// Éclairage en trois points + douche du plafond, tous dirigés vers le personnage
	KeyLight = MakeLight(TEXT("KeyLight"), FVector(320.f, -260.f, 230.f), 140.f);
	FillLight = MakeLight(TEXT("FillLight"), FVector(320.f, 300.f, 150.f), 180.f);
	RimLight = MakeLight(TEXT("RimLight"), FVector(-260.f, 180.f, 260.f), 100.f);
	TopLight = MakeLight(TEXT("TopLight"), FVector(30.f, 0.f, 520.f), 240.f);
}

UStaticMeshComponent* ACharacterStudio::MakeWall(FName Name, const FVector& Location, const FVector& SizeCm)
{
	UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Wall->SetupAttachment(GetRootComponent());
	Wall->SetRelativeLocation(Location);
	Wall->SetRelativeScale3D(SizeCm / 100.f);
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wall->SetCastShadow(true);
	Walls.Add(Wall);
	return Wall;
}

URectLightComponent* ACharacterStudio::MakeLight(FName Name, const FVector& Location, float Width)
{
	URectLightComponent* Light = CreateDefaultSubobject<URectLightComponent>(Name);
	Light->SetupAttachment(GetRootComponent());
	Light->SetRelativeLocation(Location);
	Light->SetRelativeRotation((BodyTarget + FVector(0.f, 0.f, 40.f) - Location).Rotation());
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetSourceWidth(Width);
	Light->SetSourceHeight(Width);
	Light->SetAttenuationRadius(1500.f);
	Light->SetBarnDoorAngle(60.f);
	return Light;
}

void ACharacterStudio::Init(UTrainerAppearanceData* InData, const FTrainerAppearance& Appearance)
{
	Data = InData;
	for (int32 Index = 0; Index < Walls.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* MID = Walls[Index]->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Index == 0 ? FloorColor : WallColor);
		}
	}
	ApplyLighting();
	ShowAppearance(Appearance);
	bSnapCamera = true;
}

void ACharacterStudio::ShowAppearance(const FTrainerAppearance& Appearance)
{
	TrainerAppearanceRender::Apply(Data, this, Preview, Appearance, AppliedBase);
}

void ACharacterStudio::SetFocus(EStudioFocus NewFocus)
{
	Focus = NewFocus;
	ZoomFactor = 1.f;
}

void ACharacterStudio::Turn(float DeltaYaw)
{
	TargetYaw += DeltaYaw;
}

void ACharacterStudio::Zoom(float Amount)
{
	ZoomFactor = FMath::Clamp(ZoomFactor - Amount * 0.08f, 0.6f, 1.5f);
}

FText ACharacterStudio::NextLighting()
{
	LightingIndex = (LightingIndex + 1) % Lightings().Num();
	ApplyLighting();
	return GetLightingName();
}

FText ACharacterStudio::GetLightingName() const
{
	return Lightings()[LightingIndex].Name;
}

void ACharacterStudio::ApplyLighting()
{
	const FLighting Light = Lightings()[LightingIndex];
	KeyLight->SetIntensity(Light.Key);
	KeyLight->SetLightColor(Light.KeyColor);
	FillLight->SetIntensity(Light.Fill);
	FillLight->SetLightColor(Light.FillColor);
	RimLight->SetIntensity(Light.Rim);
	RimLight->SetLightColor(Light.RimColor);
	TopLight->SetIntensity(Light.Top);
}

bool ACharacterStudio::GetBoneLocation(FName Bone, const FVector& Offset, FVector& OutLocation) const
{
	if (Bone.IsNone() || !Preview->GetSkeletalMeshAsset() || !Preview->DoesSocketExist(Bone))
	{
		return false;
	}
	// Offset dans le repère du personnage : X devant lui (vers la caméra quand il est de face), Z en haut
	const FRotator Facing = Turntable->GetComponentRotation();
	OutLocation = Preview->GetSocketLocation(Bone) + Facing.RotateVector(Offset);
	return true;
}

FVector ACharacterStudio::GetFocusPoint() const
{
	if (Focus == EStudioFocus::Face)
	{
		if (Data)
		{
			for (const FName Bone : Data->HeadBones)
			{
				FVector Head;
				if (GetBoneLocation(Bone, FVector(0.f, 0.f, 4.f), Head))
				{
					return Head;
				}
			}
		}
		return GetActorTransform().TransformPosition(FVector(0.f, 0.f, DefaultHeadHeight));
	}
	return GetActorTransform().TransformPosition(BodyTarget);
}

void ACharacterStudio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Rotation du personnage, en douceur
	// CurrentYaw garde les tours complets : la rotation lue sur le composant repasse à -180 après 180,
	// et l'interpolation ne rattrapait alors jamais la cible (le perso tournait tout seul)
	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, 10.f);
	Turntable->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));

	// Caméra : toujours de face (+X), à la bonne distance du point visé
	const bool bFace = Focus == EStudioFocus::Face;
	const FVector Target = GetFocusPoint();
	const float Distance = (bFace ? FaceDistance : BodyDistance) * ZoomFactor;
	const float WantedFov = bFace ? FaceFov : BodyFov;
	// Comme inZOI : le personnage un peu à gauche de l'écran, la droite reste libre pour le panneau et ses points.
	// On glisse caméra et point visé ensemble vers la droite de la caméra (ScreenShift = part de la demi-largeur).
	const FVector ToCamera = GetActorForwardVector();
	const FVector CameraRight = FVector::CrossProduct(FVector::UpVector, -ToCamera).GetSafeNormal();
	const FVector Shift = CameraRight * (ScreenShift * Distance * FMath::Tan(FMath::DegreesToRadians(WantedFov * 0.5f)));
	const FVector WantedLocation = Target + ToCamera * Distance + Shift;
	const FRotator WantedRotation = (Target + Shift - WantedLocation).Rotation();

	const float Alpha = bSnapCamera ? 1.f : FMath::Clamp(DeltaSeconds * 6.f, 0.f, 1.f);
	Camera->SetWorldLocation(FMath::Lerp(Camera->GetComponentLocation(), WantedLocation, Alpha));
	Camera->SetWorldRotation(FQuat::Slerp(Camera->GetComponentQuat(), WantedRotation.Quaternion(), Alpha));
	Camera->SetFieldOfView(FMath::Lerp(Camera->FieldOfView, WantedFov, Alpha));
	bSnapCamera = false;
}

#undef LOCTEXT_NAMESPACE
