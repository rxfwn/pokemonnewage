#include "Progression/EvolutionScene.h"
#include "Progression/ProgressionComponent.h"
#include "UI/Progression/ProgressionWidget.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
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
}

// ---------------------------------------------------------------- Mise en place

void AEvolutionScene::Play(APlayerController* Controller, UProgressionWidget* InWidget, const FText& InPokemonName, FName FromSpecies, FName ToSpecies,
	TFunction<void(bool)> OnFinished)
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

	// Cadrage : le plus grand des deux Pokémon tient dans l'image
	const float Height = FMath::Max3(OldHeight, NewHeight, 30.f);
	const float Distance = FMath::Max(220.f, Height * 2.8f);
	const FVector Target(0.f, 0.f, Height * 0.5f);
	const FVector CameraLocation(Distance, 0.f, Height * 0.55f + 25.f);
	Camera->SetRelativeLocation(CameraLocation);
	Camera->SetRelativeRotation((Target - CameraLocation).Rotation());
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = Distance;

	const FVector KeyLocation(Distance * 0.6f, Distance * 0.45f, Height * 1.6f + 80.f);
	KeyLight->SetRelativeLocation(KeyLocation);
	KeyLight->SetRelativeRotation((Target - KeyLocation).Rotation());
	GlowLight->SetRelativeLocation(FVector(Height * 0.4f, 0.f, Height * 0.5f));
	GlowLight->SetLightColor(GlowColor);
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

	switch (Phase)
	{
	case EPhase::Glow:
		if (USoundBase* Loop = UProgressionComponent::LoadSound(TEXT("SW_Evolution_Music")))
		{
			Music->SetSound(Loop);
			Music->Play();
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
			MorphPhase = 0.f;
			if (NewMesh)
			{
				NewMesh->SetVisibility(true);
				SetModelScale(NewMesh, 0.f);
			}
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

bool AEvolutionScene::RequestCancel()
{
	if (Phase == EPhase::Intro || Phase == EPhase::Glow || Phase == EPhase::Morph)
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
