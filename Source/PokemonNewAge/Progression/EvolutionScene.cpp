#include "Progression/EvolutionScene.h"
#include "Capture/PokeBallModel.h"
#include "Progression/ProgressionComponent.h"
#include "UI/Progression/ProgressionWidget.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "Progression"

namespace EvolutionScene
{
	/** Dossier des modèles de Pokémon (même convention que le Pokédex 3D) */
	const TCHAR* ModelFolder = TEXT("/Game/Pokemon/Models");
	const FName GlowParam(TEXT("Glow"));
	const FName ColorParam(TEXT("Color"));
	/** Durée du flash blanc */
	constexpr float FlashTime = 0.55f;
	/** Retour à la normale quand on annule */
	constexpr float CancelFadeTime = 0.8f;
	/** Lumière de la lueur au plus fort (lumens) */
	constexpr float GlowLightMax = 6000.f;
	constexpr float FlashLightMax = 40000.f;
	/** Son d'ouverture de la Ball (celui du lancer, Tools/Capture/import_pokeball_unreal.py) */
	const TCHAR* BallOpenSound = TEXT("/Game/Capture/Sons/SW_Ball_Open.SW_Ball_Open");
	/** Lumière pleine (rayon, étincelles, boule de lumière) : Tools/Progression/import_progression_unreal.py */
	const TCHAR* LightMaterial = TEXT("/Game/Progression/M_EvolutionLight.M_EvolutionLight");
	/** Ball tournée de 3/4 pour qu'on voie le bouton et la charnière */
	constexpr float BallYaw = 25.f;
	/** Repos de la Ball avant de trembler (s) */
	constexpr float WobbleRest = 0.35f;
	/** La Ball flotte un peu en s'ouvrant (en diamètres) */
	constexpr float BallLift = 0.35f;
	/** Étincelles : nombre maximum, cadence pendant le rayon (par seconde) */
	constexpr int32 MaxSparks = 220;
	constexpr float BeamSparkRate = 110.f;

	UObject* LoadModelAsset(const FString& Species, const FString& Prefix)
	{
		const FString Name = Prefix + Species;
		return StaticLoadObject(UObject::StaticClass(), nullptr,
			*FString::Printf(TEXT("%s/%s/%s.%s"), ModelFolder, *Species, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

AEvolutionScene::AEvolutionScene()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(40.f);
	// Décor assombri et flou autour du Pokémon
	Camera->PostProcessBlendWeight = 1.f;
	FPostProcessSettings& Post = Camera->PostProcessSettings;
	Post.bOverride_VignetteIntensity = true;
	Post.VignetteIntensity = 1.1f;
	Post.bOverride_AutoExposureBias = true;
	Post.AutoExposureBias = -0.7f;
	Post.bOverride_DepthOfFieldFstop = true;
	Post.DepthOfFieldFstop = 1.2f;
	Post.bOverride_DepthOfFieldFocalDistance = true;
	Post.DepthOfFieldFocalDistance = 300.f;
	Post.bOverride_BloomIntensity = true;
	Post.BloomIntensity = 1.5f;

	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetIntensityUnits(ELightUnits::Lumens);
	KeyLight->SetIntensity(2500.f);
	KeyLight->SetAttenuationRadius(1500.f);
	KeyLight->SetOuterConeAngle(28.f);
	KeyLight->SetInnerConeAngle(14.f);
	KeyLight->SetCastShadows(true);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(Root);
	GlowLight->SetIntensityUnits(ELightUnits::Lumens);
	GlowLight->SetIntensity(0.f);
	GlowLight->SetAttenuationRadius(900.f);
	GlowLight->SetCastShadows(false);

	auto MakeSkeletal = [this](const TCHAR* Name)
	{
		USkeletalMeshComponent* Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
		return Mesh;
	};
	auto MakeStatic = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
		return Mesh;
	};
	OldSkeletal = MakeSkeletal(TEXT("OldSkeletal"));
	OldStatic = MakeStatic(TEXT("OldStatic"));
	NewSkeletal = MakeSkeletal(TEXT("NewSkeletal"));
	NewStatic = MakeStatic(TEXT("NewStatic"));

	Music = CreateDefaultSubobject<UAudioComponent>(TEXT("Music"));
	Music->SetupAttachment(Root);
	Music->bAutoActivate = false;
	Music->bIsUISound = true;

	BallBottom = MakeStatic(TEXT("BallBottom"));
	BallHinge = CreateDefaultSubobject<USceneComponent>(TEXT("BallHinge"));
	BallHinge->SetupAttachment(BallBottom);
	BallTop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallTop"));
	BallTop->SetupAttachment(BallHinge);
	BallTop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallTop->SetVisibility(false);

	LightCore = MakeStatic(TEXT("LightCore"));
	Beam = MakeStatic(TEXT("Beam"));
	Shockwave = MakeStatic(TEXT("Shockwave"));
	Sparks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks"));
	Sparks->SetupAttachment(Root);
	Sparks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sparks->SetVisibility(false);
	for (UStaticMeshComponent* Effect : { LightCore.Get(), Beam.Get(), Shockwave.Get(), static_cast<UStaticMeshComponent*>(Sparks.Get()) })
	{
		Effect->SetCastShadow(false);
	}
}

// ---------------------------------------------------------------- Mise en place

void AEvolutionScene::Play(APlayerController* Controller, UProgressionWidget* InWidget, const FText& InPokemonName, FName FromSpecies, FName ToSpecies,
	FName Ball, TFunction<void(bool)> OnFinished)
{
	PlayerController = Controller;
	Widget = InWidget;
	PokemonName = InPokemonName;
	FinishedCallback = MoveTemp(OnFinished);
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const FSpeciesRow* NewRow = Database ? Database->FindSpecies(ToSpecies) : nullptr;
	NewSpeciesName = NewRow ? NewRow->DisplayName : FText::FromName(ToSpecies);

	PlaceInFrontOf(Controller);

	UMeshComponent* LoadedOld = nullptr;
	UMeshComponent* LoadedNew = nullptr;
	const float OldHeight = LoadModel(FromSpecies, OldSkeletal, OldStatic, LoadedOld);
	const float NewHeight = LoadModel(ToSpecies, NewSkeletal, NewStatic, LoadedNew);
	OldMesh = LoadedOld;
	NewMesh = LoadedNew;
	OldBaseScale = OldMesh ? OldMesh->GetRelativeScale3D() : FVector::OneVector;
	NewBaseScale = NewMesh ? NewMesh->GetRelativeScale3D() : FVector::OneVector;
	if (OldMesh)
	{
		OldMesh->SetVisibility(true);
	}

	// Lueur : matériau de surcouche, une instance par modèle
	if (UMaterialInterface* GlowMaterial = UProgressionComponent::LoadGlowMaterial())
	{
		if (OldMesh)
		{
			OldGlow = UMaterialInstanceDynamic::Create(GlowMaterial, this);
			OldMesh->SetOverlayMaterial(OldGlow);
		}
		if (NewMesh)
		{
			NewGlow = UMaterialInstanceDynamic::Create(GlowMaterial, this);
			NewMesh->SetOverlayMaterial(NewGlow);
		}
	}
	SetGlow(OldGlow, 0.f);
	SetGlow(NewGlow, 1.f);

	// Le Pokémon sortira de sa Ball : caché jusqu'à l'explosion de lumière
	PokemonHeight = FMath::Max(OldHeight, 30.f);
	bBallIntro = OldMesh && SetupBall(Ball);
	if (bBallIntro)
	{
		OldMesh->SetVisibility(false);
		SetupEffects();
	}

	// Cadrage : le plus grand des deux Pokémon tient dans l'image ; au début, gros plan sur la Ball
	const float Height = FMath::Max3(OldHeight, NewHeight, 30.f);
	const float Distance = FMath::Max(220.f, Height * 2.8f);
	const FVector Target(0.f, 0.f, Height * 0.5f);
	PokemonShotTarget = Target;
	PokemonShotLocation = FVector(Distance, 0.f, Height * 0.55f + 25.f);
	// Caméra assez basse : le sol fuit vite dans le flou derrière la Ball (son grain ne se voit pas)
	BallShotLocation = BallRest + FVector(BallDiameter * BallShotDistance, 0.f, BallDiameter * 0.8f);
	PokemonFstop = Camera->PostProcessSettings.DepthOfFieldFstop;
	CameraBlend = bBallIntro ? 0.f : 1.f;
	UpdateCamera(CameraBlend);

	const FVector KeyLocation(Distance * 0.6f, Distance * 0.45f, Height * 1.6f + 80.f);
	KeyLight->SetRelativeLocation(KeyLocation);
	KeyLight->SetRelativeRotation((Target - KeyLocation).Rotation());
	GlowLight->SetRelativeLocation(bBallIntro ? BallRest + FVector(BallDiameter, 0.f, 0.f) : FVector(Height * 0.4f, 0.f, Height * 0.5f));
	GlowLight->SetLightColor(bBallIntro ? BallLightColor : GlowColor);
	BaseLightIntensity = KeyLight->Intensity;

	// Le joueur s'efface, la caméra glisse vers la scène
	if (Controller)
	{
		PreviousViewTarget = Controller->GetViewTarget();
		if (APawn* Pawn = Controller->GetPawn())
		{
			bPawnWasHidden = Pawn->IsHidden();
			Pawn->SetActorHiddenInGame(true);
		}
		Controller->SetViewTargetWithBlend(this, 0.8f, VTBlend_Cubic);
	}

	if (bBallIntro)
	{
		// « Quoi ? » pendant que la Ball tremble ; le nom vient quand la transformation commence
		if (Widget)
		{
			Widget->SetCaption(LOCTEXT("What", "Quoi ?"));
		}
		EnterPhase(EPhase::BallWobble);
		return;
	}
	if (Widget)
	{
		Widget->SetCaption(FText::Format(LOCTEXT("Evolving", "Quoi ? {0} évolue !"), PokemonName));
	}
	PlaySound(TEXT("SW_Evolution_Start"));
	EnterPhase(EPhase::Intro);
}

void AEvolutionScene::PlaceInFrontOf(APlayerController* Controller)
{
	FVector Feet = FVector::ZeroVector;
	FRotator Facing = FRotator::ZeroRotator;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (Pawn)
	{
		Feet = Pawn->GetActorLocation() - FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight());
		Facing = FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	}
	else if (Controller)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Feet = ViewLocation;
		Facing = FRotator(0.f, ViewRotation.Yaw, 0.f);
	}

	// Posé sur le sol devant le joueur ; la scène regarde le joueur (son +X pointe vers lui)
	FVector Location = Feet + Facing.Vector() * DistanceFromPlayer;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EvolutionSceneTrace), false, Pawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 300.f), Location - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
	{
		Location = Hit.ImpactPoint;
	}
	SetActorLocationAndRotation(Location, FRotator(0.f, Facing.Yaw + 180.f, 0.f));
}

float AEvolutionScene::LoadModel(FName Species, USkeletalMeshComponent* Skeletal, UStaticMeshComponent* Static, UMeshComponent*& OutMesh)
{
	using namespace EvolutionScene;
	const FString Id = Species.ToString();
	// Les modèles regardent +Y : un quart de tour pour regarder la caméra (+X)
	const FRotator FaceCamera(0.f, -90.f, 0.f);

	if (USkeletalMesh* SkeletalAsset = Cast<USkeletalMesh>(LoadModelAsset(Id, TEXT("SK_"))))
	{
		Skeletal->SetSkeletalMesh(SkeletalAsset);
		Skeletal->SetRelativeRotation(FaceCamera);
		const FString IdleName = FString::Printf(TEXT("A_%s_Idle"), *Id);
		if (UAnimSequence* Idle = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr,
			*FString::Printf(TEXT("%s/%s/%s.%s"), ModelFolder, *Id, *IdleName, *IdleName), nullptr, LOAD_NoWarn | LOAD_Quiet)))
		{
			Skeletal->PlayAnimation(Idle, true);
		}
		OutMesh = Skeletal;
		return SkeletalAsset->GetBounds().BoxExtent.Z * 2.f;
	}

	UStaticMesh* StaticAsset = Cast<UStaticMesh>(LoadModelAsset(Id, TEXT("SM_")));
	float Height = 0.f;
	if (StaticAsset)
	{
		Static->SetStaticMesh(StaticAsset);
		Static->SetRelativeRotation(FaceCamera);
		Height = StaticAsset->GetBounds().BoxExtent.Z * 2.f;
	}
	else if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		// Pas encore de modèle pour cette espèce : une sphère à sa place
		Static->SetStaticMesh(Sphere);
		const float Scale = PlaceholderHeight / 100.f;
		Static->SetRelativeScale3D(FVector(Scale));
		Static->SetRelativeLocation(FVector(0.f, 0.f, PlaceholderHeight * 0.5f));
		Height = PlaceholderHeight;
	}
	OutMesh = Static->GetStaticMesh() ? Static : nullptr;
	return Height;
}

// ---------------------------------------------------------------- Déroulement

void AEvolutionScene::EnterPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.f;

	if (!IsBallPhase() && bBallIntro)
	{
		// Entrée interrompue (annulation) ou finie : plus de Ball, le Pokémon en entier
		HideBall();
		if (OldMesh)
		{
			OldMesh->SetVisibility(true);
		}
		SetModelScale(OldMesh, 1.f);
		if (Widget)
		{
			Widget->SetFlash(0.f);
		}
		GlowLight->SetRelativeLocation(FVector(PokemonHeight * 0.4f, 0.f, PokemonHeight * 0.5f));
		GlowLight->SetLightColor(GlowColor);
		GlowLight->SetIntensity(0.f);
		bBallIntro = false;
	}

	auto StartMusic = [this]()
	{
		if (Music->IsPlaying())
		{
			return;
		}
		if (USoundBase* Loop = UProgressionComponent::LoadSound(TEXT("SW_Evolution_Music")))
		{
			Music->SetSound(Loop);
			Music->Play();
		}
	};

	switch (Phase)
	{
	case EPhase::BallOpen:
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, EvolutionScene::BallOpenSound, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
		LightCore->SetVisibility(LightCore->GetStaticMesh() != nullptr);
		break;

	case EPhase::BallBeam:
		PlaySound(TEXT("SW_Evolution_Start"));
		Beam->SetVisibility(Beam->GetStaticMesh() != nullptr);
		SparkTimer = 0.f;
		break;

	case EPhase::BallBurst:
		PlaySound(TEXT("SW_Evolution_Flash"));
		Shockwave->SetVisibility(Shockwave->GetStaticMesh() != nullptr);
		SpawnSparks(90, 32.f, 0.f);
		break;

	case EPhase::Glow:
		StartMusic();
		break;

	case EPhase::Morph:
		StartMusic();
		MorphPhase = 0.f;
		if (NewMesh)
		{
			NewMesh->SetVisibility(true);
			SetModelScale(NewMesh, 0.f);
		}
		break;

	case EPhase::Flash:
		Music->FadeOut(0.3f, 0.f);
		PlaySound(TEXT("SW_Evolution_Flash"));
		if (OldMesh)
		{
			OldMesh->SetVisibility(false);
		}
		if (NewMesh)
		{
			NewMesh->SetVisibility(true);
			SetModelScale(NewMesh, 1.f);
		}
		SetGlow(NewGlow, 1.f);
		break;

	case EPhase::Reveal:
		PlaySound(TEXT("SW_Evolution_Done"));
		break;

	case EPhase::Cancelled:
		Music->FadeOut(0.3f, 0.f);
		PlaySound(TEXT("SW_Evolution_Cancel"));
		if (NewMesh)
		{
			NewMesh->SetVisibility(false);
		}
		if (OldMesh)
		{
			OldMesh->SetVisibility(true);
			SetModelScale(OldMesh, 1.f);
		}
		break;

	default:
		break;
	}
}

void AEvolutionScene::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PhaseTime += DeltaTime;
	TickSparks(DeltaTime);
	if (!IsBallPhase() && CameraBlend < 1.f)
	{
		// Annulation pendant l'entrée : la caméra recule jusqu'au Pokémon
		CameraBlend = FMath::FInterpConstantTo(CameraBlend, 1.f, DeltaTime, 1.2f);
		UpdateCamera(FMath::SmoothStep(0.f, 1.f, CameraBlend));
	}

	switch (Phase)
	{
	case EPhase::Intro:
		if (bCancelRequested)
		{
			EnterPhase(EPhase::Cancelled);
		}
		else if (PhaseTime >= IntroTime)
		{
			EnterPhase(EPhase::Glow);
		}
		break;

	case EPhase::BallWobble:
	case EPhase::BallOpen:
	case EPhase::BallBeam:
	case EPhase::BallBurst:
		if (bCancelRequested)
		{
			EnterPhase(EPhase::Cancelled);
		}
		else
		{
			TickBallPhase(DeltaTime);
		}
		break;

	case EPhase::Glow:
	{
		const float Alpha = FMath::Clamp(PhaseTime / FMath::Max(GlowTime, 0.01f), 0.f, 1.f);
		SetGlow(OldGlow, Alpha);
		GlowLight->SetIntensity(EvolutionScene::GlowLightMax * Alpha);
		KeyLight->SetIntensity(BaseLightIntensity * (1.f - 0.6f * Alpha));
		if (bCancelRequested)
		{
			EnterPhase(EPhase::Cancelled);
		}
		else if (Alpha >= 1.f)
		{
			EnterPhase(EPhase::Morph);
		}
		break;
	}

	case EPhase::Morph:
	{
		// Les deux silhouettes grandissent et rétrécissent à tour de rôle, de plus en plus vite
		const float Progress = FMath::Clamp(PhaseTime / FMath::Max(MorphTime, 0.01f), 0.f, 1.f);
		const float Rate = FMath::Lerp(MorphStartRate, MorphEndRate, Progress * Progress);
		MorphPhase += Rate * DeltaTime * 2.f * UE_PI;
		const float Old = 0.5f + 0.5f * FMath::Cos(MorphPhase);
		SetModelScale(OldMesh, FMath::Lerp(0.08f, 1.f, Old));
		SetModelScale(NewMesh, FMath::Lerp(0.08f, 1.f, 1.f - Old));
		GlowLight->SetIntensity(EvolutionScene::GlowLightMax * (0.8f + 0.4f * Progress + 0.2f * FMath::Sin(MorphPhase * 2.f)));
		if (bCancelRequested)
		{
			EnterPhase(EPhase::Cancelled);
		}
		else if (Progress >= 1.f)
		{
			EnterPhase(EPhase::Flash);
		}
		break;
	}

	case EPhase::Flash:
	{
		const float Alpha = PhaseTime / EvolutionScene::FlashTime;
		const float Flash = Alpha < 0.25f ? Alpha / 0.25f : FMath::Max(0.f, 1.f - (Alpha - 0.25f) / 0.75f);
		if (Widget)
		{
			Widget->SetFlash(Flash);
		}
		GlowLight->SetIntensity(FMath::Lerp(EvolutionScene::GlowLightMax, EvolutionScene::FlashLightMax, Flash));
		if (Alpha >= 1.f)
		{
			EnterPhase(EPhase::Reveal);
		}
		break;
	}

	case EPhase::Reveal:
	{
		// La lueur s'efface : la nouvelle forme apparaît en couleurs
		const float Alpha = FMath::Clamp(PhaseTime / FMath::Max(RevealTime, 0.01f), 0.f, 1.f);
		SetGlow(NewGlow, 1.f - Alpha);
		GlowLight->SetIntensity(EvolutionScene::GlowLightMax * (1.f - Alpha));
		KeyLight->SetIntensity(BaseLightIntensity * (0.4f + 0.6f * Alpha));
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Done;
			if (Widget)
			{
				TWeakObjectPtr<AEvolutionScene> WeakThis(this);
				Widget->ShowMessage(FText::Format(LOCTEXT("Evolved", "Félicitations ! Ton {0} a évolué en {1} !"), PokemonName, NewSpeciesName), true,
					[WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Finish(true); } });
			}
			else
			{
				Finish(true);
			}
		}
		break;
	}

	case EPhase::Cancelled:
	{
		const float Alpha = FMath::Clamp(PhaseTime / EvolutionScene::CancelFadeTime, 0.f, 1.f);
		SetGlow(OldGlow, 1.f - Alpha);
		GlowLight->SetIntensity(EvolutionScene::GlowLightMax * (1.f - Alpha));
		KeyLight->SetIntensity(BaseLightIntensity * (0.4f + 0.6f * Alpha));
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Done;
			if (Widget)
			{
				TWeakObjectPtr<AEvolutionScene> WeakThis(this);
				Widget->ShowMessage(FText::Format(LOCTEXT("NotEvolved", "Hein ? {0} n'a pas évolué !"), PokemonName), true,
					[WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Finish(false); } });
			}
			else
			{
				Finish(false);
			}
		}
		break;
	}

	default:
		break;
	}
}

bool AEvolutionScene::SetupBall(FName Ball)
{
	const float ModelSize = PokeBallModel::Apply(Ball, BallBottom, BallHinge, BallTop);
	if (ModelSize <= 0.f)
	{
		return false;
	}
	BallBaseScale = BallDiameter / ModelSize;
	// Posée sur l'herbe à la place du Pokémon, le bouton (+X) vers la caméra
	BallRest = FVector(0.f, 0.f, BallDiameter * 0.5f);
	BallBottom->SetRelativeLocationAndRotation(BallRest, FRotator(0.f, EvolutionScene::BallYaw, 0.f));
	BallBottom->SetRelativeScale3D(FVector(BallBaseScale));
	BallBottom->SetVisibility(true);
	BallTop->SetVisibility(true);
	PokeBallModel::SetOpenAngle(BallHinge, 0.f);
	PokeBallModel::SetGlow(BallBottom, BallTop, 0.f, BallLightColor);
	LastWobble = -1;
	bPokemonOut = false;
	return true;
}

void AEvolutionScene::SetupEffects()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Rim = UProgressionComponent::LoadGlowMaterial();
	UMaterialInterface* Full = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
		EvolutionScene::LightMaterial, nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!Full)
	{
		Full = Rim;
	}
	if (!Full || !Sphere || !Cylinder)
	{
		// Matériaux pas encore importés : la Ball s'ouvre sans effets de lumière
		return;
	}

	auto Setup = [this](UStaticMeshComponent* Mesh, UStaticMesh* Shape, UMaterialInterface* Material) -> UMaterialInstanceDynamic*
	{
		Mesh->SetStaticMesh(Shape);
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
		Mesh->SetMaterial(0, Instance);
		Mesh->SetVisibility(false);
		return Instance;
	};
	CoreLight = Setup(LightCore, Sphere, Full);
	BeamLight = Setup(Beam, Cylinder, Full);
	// L'onde de choc est plus claire sur ses bords : des arcs de lumière autour du Pokémon
	ShockLight = Setup(Shockwave, Sphere, Rim ? Rim : Full);
	SparkLight = Setup(Sparks, Sphere, Full);
	SetEffectGlow(SparkLight, 1.f, BallLightColor);
}

void AEvolutionScene::TickBallPhase(float DeltaTime)
{
	using namespace EvolutionScene;
	const float D = BallDiameter;
	const float Radius = D * 0.5f;

	// Pose la Ball : inclinaison (roulis, elle roule un peu sur l'herbe) et hauteur
	auto PlaceBall = [this, Radius](float Roll, float Lift)
	{
		const FVector Right = FRotator(0.f, BallYaw, 0.f).RotateVector(FVector::RightVector);
		BallBottom->SetRelativeLocationAndRotation(BallRest + Right * (-Radius * FMath::DegreesToRadians(Roll)) + FVector(0.f, 0.f, Lift),
			FRotator(0.f, BallYaw, Roll));
	};
	auto Ease = [](float Alpha) { return FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f), 2.f); };
	const FVector BallCenter = BallRest + FVector(0.f, 0.f, D * BallLift);

	switch (Phase)
	{
	case EPhase::BallWobble:
	{
		// 1. La Ball tremble sur l'herbe, comme pendant une capture
		const float Active = FMath::Max(WobbleTime - WobbleRest - 0.25f, 0.1f);
		const float U = (PhaseTime - WobbleRest) / Active;
		float Roll = 0.f;
		if (U > 0.f && U < 1.f)
		{
			const float Cycles = U * FMath::Max(WobbleCount, 1);
			Roll = WobbleAngle * FMath::Sin(Cycles * 2.f * UE_PI);
			const int32 Index = FMath::FloorToInt(Cycles * 2.f);
			if (Index != LastWobble)
			{
				LastWobble = Index;
				PlaySound(TEXT("SW_Evolution_Wobble"));
			}
		}
		PlaceBall(Roll, 0.f);
		if (PhaseTime >= WobbleTime)
		{
			EnterPhase(EPhase::BallOpen);
		}
		break;
	}

	case EPhase::BallOpen:
	{
		// 2. Elle se soulève et s'entrouvre : la lumière passe par l'ouverture
		const float Alpha = FMath::Clamp(PhaseTime / FMath::Max(BallOpenTime, 0.05f), 0.f, 1.f);
		const float Lid = FMath::InterpEaseOut(0.f, 1.f, FMath::Min(Alpha * 2.5f, 1.f), 3.f);
		PokeBallModel::SetOpenAngle(BallHinge, BallOpenAngle * Lid);
		PokeBallModel::SetGlow(BallBottom, BallTop, 1.5f * Alpha, BallLightColor);
		PlaceBall(3.f * FMath::Sin(PhaseTime * 38.f) * (1.f - Alpha), D * BallLift * Ease(Alpha));
		LightCore->SetRelativeLocation(BallRest + FVector(0.f, 0.f, D * BallLift * Ease(Alpha)));
		LightCore->SetRelativeScale3D(FVector(D * (0.25f + 0.45f * Alpha) / 100.f));
		SetEffectGlow(CoreLight, Alpha, BallLightColor);
		GlowLight->SetIntensity(GlowLightMax * 0.8f * Alpha);
		if (Alpha >= 1.f)
		{
			EnterPhase(EPhase::BallBeam);
		}
		break;
	}

	case EPhase::BallBeam:
	{
		// 3. Un rayon de lumière monte de la Ball, les étincelles jaillissent
		const float Alpha = FMath::Clamp(PhaseTime / FMath::Max(BeamTime, 0.05f), 0.f, 1.f);
		PlaceBall(1.5f * FMath::Sin(PhaseTime * 45.f), D * BallLift);
		const float Height = FMath::Max(PokemonHeight * 4.f, 600.f) * FMath::InterpEaseOut(0.f, 1.f, FMath::Min(Alpha * 3.f, 1.f), 2.f);
		const float Width = D * (0.8f + 0.5f * Alpha) * (1.f + 0.12f * FMath::Sin(PhaseTime * 30.f));
		Beam->SetRelativeLocation(BallCenter + FVector(0.f, 0.f, Height * 0.5f));
		Beam->SetRelativeScale3D(FVector(Width / 100.f, Width / 100.f, FMath::Max(Height, 1.f) / 100.f));
		SetEffectGlow(BeamLight, FMath::Min(Alpha * 4.f, 1.f), BallLightColor);
		LightCore->SetRelativeScale3D(FVector(D * (0.7f + 0.4f * Alpha) / 100.f));
		GlowLight->SetIntensity(GlowLightMax * (0.8f + 0.7f * Alpha));

		SparkTimer += DeltaTime * BeamSparkRate;
		const int32 Count = FMath::FloorToInt(SparkTimer);
		SparkTimer -= Count;
		SpawnSparks(Count, 26.f, 0.45f);
		if (Alpha >= 1.f)
		{
			EnterPhase(EPhase::BallBurst);
		}
		break;
	}

	case EPhase::BallBurst:
	{
		// 4. Explosion blanche : la Ball disparaît, le Pokémon est là, en silhouette de lumière ; la caméra recule
		const float Alpha = FMath::Clamp(PhaseTime / FMath::Max(BurstTime, 0.05f), 0.f, 1.f);
		const float Flash = Alpha < 0.15f ? Alpha / 0.15f : FMath::Max(0.f, 1.f - (Alpha - 0.15f) / 0.5f);
		if (Widget)
		{
			Widget->SetFlash(0.85f * Flash);
		}
		if (Alpha >= 0.15f && !bPokemonOut)
		{
			bPokemonOut = true;
			BallBottom->SetVisibility(false);
			BallTop->SetVisibility(false);
			if (OldMesh)
			{
				OldMesh->SetVisibility(true);
				SetModelScale(OldMesh, 1.f);
			}
			SetGlow(OldGlow, 1.f);
		}

		const float Grow = FMath::InterpEaseOut(0.f, 1.f, FMath::Min(Alpha / 0.3f, 1.f), 2.f);
		LightCore->SetRelativeLocation(FMath::Lerp(BallCenter, FVector(0.f, 0.f, PokemonHeight * 0.5f), Grow));
		LightCore->SetRelativeScale3D(FVector(FMath::Lerp(D * 1.1f, PokemonHeight * 1.3f, Grow) / 100.f));
		SetEffectGlow(CoreLight, 1.f - FMath::Clamp((Alpha - 0.2f) / 0.5f, 0.f, 1.f), BallLightColor);

		const float Wave = FMath::InterpEaseOut(0.f, 1.f, Alpha, 3.f);
		Shockwave->SetRelativeLocation(FVector(0.f, 0.f, PokemonHeight * 0.4f));
		Shockwave->SetRelativeScale3D(FVector(FMath::Lerp(D, FMath::Max(PokemonHeight * 2.6f, 180.f), Wave) / 100.f));
		SetEffectGlow(ShockLight, FMath::Pow(1.f - Alpha, 1.5f), FLinearColor::White);

		const float BeamFade = FMath::Clamp(1.f - Alpha * 2.5f, 0.f, 1.f);
		SetEffectGlow(BeamLight, BeamFade, BallLightColor);
		Beam->SetVisibility(BeamFade > 0.f && Beam->GetStaticMesh() != nullptr);

		GlowLight->SetRelativeLocation(FMath::Lerp(BallRest + FVector(D, 0.f, 0.f), FVector(PokemonHeight * 0.4f, 0.f, PokemonHeight * 0.5f), Wave));
		GlowLight->SetLightColor(FMath::Lerp(BallLightColor, GlowColor, Alpha));
		GlowLight->SetIntensity(FMath::Lerp(FlashLightMax * 0.6f, GlowLightMax, Alpha));
		CameraBlend = FMath::SmoothStep(0.1f, 0.9f, Alpha);
		UpdateCamera(CameraBlend);

		if (Alpha >= 1.f)
		{
			if (Widget)
			{
				Widget->SetCaption(FText::Format(LOCTEXT("Evolving", "Quoi ? {0} évolue !"), PokemonName));
			}
			EnterPhase(EPhase::Morph);
		}
		break;
	}

	default:
		break;
	}
}

void AEvolutionScene::SpawnSparks(int32 Count, float Speed, float Up)
{
	if (!SparkLight || Count <= 0)
	{
		return;
	}
	const float D = BallDiameter;
	const FVector Origin = BallRest + FVector(0.f, 0.f, D * EvolutionScene::BallLift);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FSpark Spark;
		FVector Direction = FMath::VRand();
		Direction.Z = FMath::Abs(Direction.Z) * (1.f - Up) + Up * FMath::FRandRange(0.4f, 1.4f);
		Direction = Direction.GetSafeNormal();
		Spark.Location = Origin + Direction * D * 0.3f;
		// Vitesse en diamètres de Ball par seconde
		Spark.Velocity = Direction * Speed * D * FMath::FRandRange(0.5f, 1.2f);
		Spark.Life = FMath::FRandRange(0.35f, 0.8f);
		Spark.Length = D * FMath::FRandRange(0.35f, 1.f);

		// Réserve pleine : on remplace la plus vieille
		if (SparkList.Num() < EvolutionScene::MaxSparks)
		{
			SparkList.Add(Spark);
		}
		else
		{
			int32 Oldest = 0;
			for (int32 Other = 1; Other < SparkList.Num(); ++Other)
			{
				if (SparkList[Other].Age / SparkList[Other].Life > SparkList[Oldest].Age / SparkList[Oldest].Life)
				{
					Oldest = Other;
				}
			}
			SparkList[Oldest] = Spark;
		}
	}
	Sparks->SetVisibility(true);
}

void AEvolutionScene::TickSparks(float DeltaTime)
{
	if (SparkList.Num() == 0 || !Sparks->IsVisible())
	{
		return;
	}
	const float D = BallDiameter;
	const float Drag = FMath::Exp(-2.5f * DeltaTime);
	TArray<FTransform> Transforms;
	Transforms.Reserve(SparkList.Num());
	bool bAnyAlive = false;
	for (FSpark& Spark : SparkList)
	{
		Spark.Age += DeltaTime;
		const float Life = Spark.Age / Spark.Life;
		if (Life >= 1.f)
		{
			Transforms.Add(FTransform(FQuat::Identity, Spark.Location, FVector(0.f)));
			continue;
		}
		bAnyAlive = true;
		Spark.Velocity *= Drag;
		Spark.Velocity.Z -= 4.f * D * DeltaTime;
		Spark.Location += Spark.Velocity * DeltaTime;
		// Un trait étiré dans sa direction, qui s'amincit en mourant
		const float Fade = FMath::Sqrt(1.f - Life);
		const float Width = D * 0.05f * Fade;
		Transforms.Add(FTransform(Spark.Velocity.Rotation().Quaternion(), Spark.Location,
			FVector(Spark.Length * Fade / 100.f, Width / 100.f, Width / 100.f)));
	}

	while (Sparks->GetInstanceCount() < Transforms.Num())
	{
		Sparks->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.f)));
	}
	Sparks->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
	if (!bAnyAlive)
	{
		SparkList.Reset();
		Sparks->SetVisibility(false);
	}
}

void AEvolutionScene::UpdateCamera(float Blend)
{
	// Gros plan : la Ball au milieu de l'image, un peu plus haut pour rester au-dessus de la boîte de texte
	const FVector BallTarget = BallRest - FVector(0.f, 0.f, BallDiameter * BallShotRaise);
	const FVector Location = FMath::Lerp(BallShotLocation, PokemonShotLocation, Blend);
	const FVector Target = FMath::Lerp(BallTarget, PokemonShotTarget, Blend);
	Camera->SetRelativeLocation(Location);
	Camera->SetRelativeRotation((Target - Location).Rotation());
	// Mise au point sur la Ball (son centre), puis sur le Pokémon
	const FVector Focus = FMath::Lerp(BallRest, PokemonShotTarget, Blend);
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = FMath::Max((Focus - Location).Size(), 10.f);
	Camera->PostProcessSettings.DepthOfFieldFstop = FMath::Lerp(BallShotFstop, PokemonFstop, Blend);
}

void AEvolutionScene::SetEffectGlow(UMaterialInstanceDynamic* Material, float Amount, const FLinearColor& Color)
{
	if (Material)
	{
		Material->SetScalarParameterValue(EvolutionScene::GlowParam, FMath::Clamp(Amount, 0.f, 1.f));
		Material->SetVectorParameterValue(EvolutionScene::ColorParam, Color);
	}
}

void AEvolutionScene::HideBall()
{
	for (UStaticMeshComponent* Mesh : { BallBottom.Get(), BallTop.Get(), LightCore.Get(), Beam.Get(), Shockwave.Get() })
	{
		Mesh->SetVisibility(false);
	}
}

bool AEvolutionScene::RequestCancel()
{
	// Jusqu'au flash, y compris pendant l'entrée par la Ball
	if (Phase <= EPhase::Morph)
	{
		bCancelRequested = true;
		return true;
	}
	return false;
}

void AEvolutionScene::SetGlow(UMaterialInstanceDynamic* Material, float Amount)
{
	if (Material)
	{
		Material->SetScalarParameterValue(EvolutionScene::GlowParam, FMath::Clamp(Amount, 0.f, 1.f));
		Material->SetVectorParameterValue(EvolutionScene::ColorParam, GlowColor);
	}
}

void AEvolutionScene::SetModelScale(UMeshComponent* Mesh, float Scale)
{
	if (Mesh)
	{
		const FVector& Base = Mesh == OldMesh ? OldBaseScale : NewBaseScale;
		Mesh->SetRelativeScale3D(Base * FMath::Max(Scale, 0.001f));
	}
}

void AEvolutionScene::PlaySound(const TCHAR* Name)
{
	if (USoundBase* Sound = UProgressionComponent::LoadSound(Name))
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

// ---------------------------------------------------------------- Fin

void AEvolutionScene::Finish(bool bEvolved)
{
	TFunction<void(bool)> Callback = MoveTemp(FinishedCallback);
	FinishedCallback = nullptr;
	if (Callback)
	{
		Callback(bEvolved);
	}
}

void AEvolutionScene::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// La caméra revient au joueur, qui réapparaît
	if (APlayerController* Controller = PlayerController.Get())
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			Pawn->SetActorHiddenInGame(bPawnWasHidden);
		}
		AActor* Back = PreviousViewTarget.IsValid() && PreviousViewTarget.Get() != this ? PreviousViewTarget.Get() : Controller->GetPawn();
		if (Back && EndPlayReason == EEndPlayReason::Destroyed)
		{
			Controller->SetViewTargetWithBlend(Back, 0.6f, VTBlend_Cubic);
		}
	}
	if (Widget)
	{
		Widget->SetFlash(0.f);
	}
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
