#include "Character/CharacterStudio.h"
#include "Character/TrainerAppearanceData.h"
#include "Character/TrainerAppearanceRender.h"
#include "Character/TrainerAnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "CharacterStudio"

namespace CharacterStudio
{
	// La pièce : 12 x 12 m, 6 m de haut, sol à Z = 0, personnage au centre, caméra côté +X
	constexpr float RoomHalf = 600.f;
	constexpr float RoomHeight = 600.f;
	constexpr float WallThickness = 20.f;

	/** Cyclorama : mur du fond à X = BackdropWallX, raccordé au sol par un quart de cercle de rayon BackdropRadius */
	constexpr float BackdropWallX = -450.f;
	constexpr float BackdropRadius = 320.f;
	constexpr int32 BackdropArcSegments = 24;
	/** Les autres faces (plafond, côtés, mur derrière la caméra) : un peu plus sombres que le fond */
	constexpr float SideWallShade = 0.8f;

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
	/** Enchaînement des poses : secondes d'attente entre deux poses, durée maximale d'une pose */
	constexpr float IdleBetweenPoses = 8.f;
	constexpr float MaxPoseDuration = 7.f;
	/** Part du mouvement du cou et de la tête gardée pendant l'attente */
	constexpr float IdleHeadMotion = 0.3f;

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

	// Pièce fermée : le fond (sol + courbe + mur) est un cyclorama, le reste des cubes de 1 m étirés
	const float Size = RoomHalf * 2.f;
	const struct { const TCHAR* Name; FVector Location; FVector SizeCm; } WallDefs[] = {
		{ TEXT("Ceiling"), FVector(0.f, 0.f, RoomHeight + WallThickness * 0.5f), FVector(Size, Size, WallThickness) },
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

	Backdrop = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Root);
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop->LightingChannels.bChannel1 = true;
	BuildBackdrop();
	if (ShapeMaterial.Object)
	{
		Backdrop->SetMaterial(0, ShapeMaterial.Object);
	}

	// Dégradé du fond : une lumière douce au-dessus du personnage, tournée vers le bas du mur. Canal 1 seulement :
	// le personnage (canal 0) n'en reçoit rien
	BackdropLight = CreateDefaultSubobject<URectLightComponent>(TEXT("BackdropLight"));
	BackdropLight->SetupAttachment(Root);
	BackdropLight->SetRelativeLocation(FVector(150.f, 0.f, 420.f));
	BackdropLight->SetRelativeRotation((FVector(BackdropWallX + 40.f, 0.f, 110.f) - FVector(150.f, 0.f, 420.f)).Rotation());
	BackdropLight->SetMobility(EComponentMobility::Movable);
	BackdropLight->SetIntensityUnits(ELightUnits::Candelas);
	BackdropLight->SetSourceWidth(200.f);
	BackdropLight->SetSourceHeight(200.f);
	BackdropLight->SetAttenuationRadius(1500.f);
	BackdropLight->SetBarnDoorAngle(35.f);
	BackdropLight->SetBarnDoorLength(60.f);
	BackdropLight->SetIntensity(14.f);
	BackdropLight->SetCastShadows(false);
	BackdropLight->LightingChannels.bChannel0 = false;
	BackdropLight->LightingChannels.bChannel1 = true;

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

void ACharacterStudio::BuildBackdrop()
{
	// Profil dans le plan XZ, de l'arrière de la caméra (+X) vers le mur du fond : sol plat, quart de cercle, mur.
	// Normales tournées vers l'intérieur et continues d'un morceau à l'autre : aucune ligne d'horizon
	struct FProfilePoint { float X, Z, NX, NZ; };
	TArray<FProfilePoint> Profile;
	const float ArcX = BackdropWallX + BackdropRadius;
	Profile.Add({ RoomHalf, 0.f, 0.f, 1.f });
	for (int32 Step = 0; Step <= BackdropArcSegments; ++Step)
	{
		const float Angle = HALF_PI * Step / BackdropArcSegments;
		const float S = FMath::Sin(Angle), C = FMath::Cos(Angle);
		Profile.Add({ ArcX - BackdropRadius * S, BackdropRadius * (1.f - C), S, C });
	}
	Profile.Add({ BackdropWallX, RoomHeight, 1.f, 0.f });

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;
	for (const FProfilePoint& Point : Profile)
	{
		for (const float Y : { -RoomHalf, RoomHalf })
		{
			Vertices.Add(FVector(Point.X, Y, Point.Z));
			Normals.Add(FVector(Point.NX, 0.f, Point.NZ));
			UVs.Add(FVector2D(Y / 100.f, (Point.X + Point.Z) / 100.f));
			Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));
		}
	}
	for (int32 Index = 0; Index + 1 < Profile.Num(); ++Index)
	{
		const int32 A = Index * 2, B = A + 1, C = A + 2, D = A + 3;
		// Deux sens : la face tournée vers le personnage reste visible quel que soit le sens attendu par le moteur
		Triangles.Append({ A, C, B, B, C, D });
		Triangles.Append({ A, B, C, B, D, C });
	}
	Backdrop->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
}

void ACharacterStudio::SetBackdropColor(const FLinearColor& Color)
{
	BackdropColor = Color;
	for (int32 Index = 0; Index < BackdropMaterials.Num(); ++Index)
	{
		if (BackdropMaterials[Index])
		{
			// Le dernier est le cyclorama ; les cubes autour un peu plus sombres
			const bool bBackdrop = Index == BackdropMaterials.Num() - 1;
			BackdropMaterials[Index]->SetVectorParameterValue(TEXT("Color"), bBackdrop ? Color : Color * SideWallShade);
		}
	}
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
	BackdropMaterials.Reset();
	for (UStaticMeshComponent* Wall : Walls)
	{
		BackdropMaterials.Add(Wall->CreateDynamicMaterialInstance(0));
	}
	BackdropMaterials.Add(Backdrop->CreateDynamicMaterialInstance(0));
	SetBackdropColor(BackdropColor);
	ApplyLighting();
	ShowAppearance(Appearance);
	bSnapCamera = true;
}

void ACharacterStudio::ShowAppearance(const FTrainerAppearance& Appearance)
{
	const FName PreviousBase = AppliedBase;
	TrainerAppearanceRender::Apply(Data, this, Preview, Appearance, AppliedBase);
	if (PreviousBase != AppliedBase)
	{
		AnimationIndex = 0;
	}
	ApplyStudioAnimation();
}

void ACharacterStudio::ApplyStudioAnimation()
{
	const FTrainerBaseDef* Base = Data ? Data->FindBase(AppliedBase) : nullptr;
	UTrainerAnimInstance* Anim = Preview ? Cast<UTrainerAnimInstance>(Preview->GetAnimInstance()) : nullptr;
	if (!Base || !Anim)
	{
		return;
	}
	UAnimSequenceBase* Sequence = nullptr;
	if (AnimationIndex > 0 && Base->StudioAnimations.IsValidIndex(AnimationIndex - 1))
	{
		Sequence = Base->StudioAnimations[AnimationIndex - 1].Animation.LoadSynchronous();
	}
	Anim->SetIdleAnimation(Sequence ? Sequence : Base->IdleAnimation.LoadSynchronous());
	// Attente : la tête reste presque droite (visage stable en gros plan) ; les poses gardent tout leur mouvement
	Anim->IdleHeadMotion = Sequence ? 1.f : IdleHeadMotion;
}

FText ACharacterStudio::NextAnimation()
{
	const FTrainerBaseDef* Base = Data ? Data->FindBase(AppliedBase) : nullptr;
	const int32 Count = Base ? Base->StudioAnimations.Num() : 0;
	if (Count == 0)
	{
		AnimationIndex = 0;
		return NSLOCTEXT("CharacterStudio", "NoPoses", "Pas encore d'animation : relance Tools/InZOI/import_inzoi.bat (plugin UEFormat installé).");
	}
	AnimationIndex = (AnimationIndex + 1) % (Count + 1);
	ApplyStudioAnimation();
	if (AnimationIndex == 0)
	{
		return NSLOCTEXT("CharacterStudio", "IdlePose", "Pose : attente");
	}
	return FText::Format(NSLOCTEXT("CharacterStudio", "PoseN", "Pose : {0}"), Base->StudioAnimations[AnimationIndex - 1].Label);
}

void ACharacterStudio::SetFocus(EStudioFocus NewFocus)
{
	Focus = NewFocus;
	ZoomFactor = 1.f;
}

void ACharacterStudio::SetAutoPoses(bool bEnable)
{
	bAutoPoses = bEnable;
	AutoTimer = 0.f;
}

void ACharacterStudio::TickAutoPoses(float DeltaSeconds)
{
	const FTrainerBaseDef* Base = Data ? Data->FindBase(AppliedBase) : nullptr;
	const int32 Count = Base ? Base->StudioAnimations.Num() : 0;
	if (!bAutoPoses || Count == 0)
	{
		return;
	}
	// Gros plan sur le visage : on reste dans l'attente (les gestes cacheraient le visage)
	if (Focus == EStudioFocus::Face)
	{
		if (AnimationIndex != 0)
		{
			AnimationIndex = 0;
			ApplyStudioAnimation();
		}
		AutoTimer = 0.f;
		return;
	}
	if (AnimationIndex == 0)
	{
		AutoTimer += DeltaSeconds;
		if (AutoTimer < IdleBetweenPoses)
		{
			return;
		}
		NextAutoPose = NextAutoPose % Count;
		AnimationIndex = NextAutoPose + 1;
		++NextAutoPose;
		ApplyStudioAnimation();
		const UAnimSequenceBase* Pose = Base->StudioAnimations[AnimationIndex - 1].Animation.Get();
		AutoTimer = FMath::Clamp(Pose ? Pose->GetPlayLength() : 4.f, 2.f, MaxPoseDuration);
		return;
	}
	AutoTimer -= DeltaSeconds;
	if (AutoTimer <= 0.f)
	{
		AnimationIndex = 0;
		AutoTimer = 0.f;
		ApplyStudioAnimation();
	}
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
	TickAutoPoses(DeltaSeconds);

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
