#include "UI/Pokedex/PokedexDevice.h"
#include "UI/Pokedex/PokedexModel.h"
#include "UI/Pokedex/PokedexWidgets.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PokemonNewAge.h"
#include "PokemonNewAgePlayerController.h"

#define LOCTEXT_NAMESPACE "PokedexDevice"

namespace PokedexDevice
{
	/** Pièces du boîtier (accrochées à ModelRoot) et du volet (accrochées à la charnière) */
	const TCHAR* BodyParts[] = { TEXT("Body"), TEXT("TopBand"), TEXT("Bezel"), TEXT("LensRing"), TEXT("Lens"), TEXT("LedRed"),
		TEXT("LedYellow"), TEXT("LedGreen"), TEXT("ButtonRound"), TEXT("PillRed"), TEXT("PillBlue"), TEXT("DPad"), TEXT("Panel"), TEXT("Hinge") };
	const TCHAR* LidParts[] = { TEXT("Lid"), TEXT("LidPanel") };

	/** Centre de l'écran dans le repère de la charnière (cm), voir make_pokedex_model.py */
	const FVector ScreenLocation(0.70f, -6.4f, 1.2f);
	/** Décalage du boîtier fermé pour qu'il reste au centre de l'écran */
	constexpr float ClosedSlide = 6.4f;
	/** Demi-largeur et demi-hauteur de l'appareil ouvert (cm) */
	constexpr float HalfWidth = 12.6f;
	constexpr float HalfHeight = 8.4f;

	/** Couleur du liseré et de l'étiquette des 6 boutons, comme sur l'image de référence */
	const FLinearColor MenuColors[] = {
		FLinearColor(0.25f, 0.55f, 1.f),   // Pokémon : bleu
		FLinearColor(1.f, 0.2f, 0.25f),    // Pokédex : rouge
		FLinearColor(0.2f, 0.8f, 1.f),     // Sac : cyan
		FLinearColor(1.f, 0.8f, 0.15f),    // Dresseur : jaune
		FLinearColor(0.9f, 0.25f, 1.f),    // Sauvegarder : magenta
		FLinearColor(0.2f, 0.95f, 0.45f),  // Options : vert
	};

	float EaseOutCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }
	float EaseOutBack(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		const float C1 = 1.4f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
	}
	float Flash(float Now, float Start, float Decay) { return Now >= Start ? FMath::Exp(-(Now - Start) * Decay) : 0.f; }
}

using namespace PokedexDevice;

APokedexDevice::APokedexDevice()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));
	ModelRoot->SetupAttachment(Pivot);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("HingeAxis"));
	Hinge->SetupAttachment(ModelRoot);

	// Écran : un widget de 680 × 640 px posé sur le cadre noir (1 px = 0,1 mm)
	ScreenComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	ScreenComponent->SetupAttachment(ModelRoot);
	ScreenComponent->SetWidgetSpace(EWidgetSpace::World);
	ScreenComponent->SetDrawSize(FVector2D(UPokedexScreenWidget::ScreenWidth, UPokedexScreenWidget::ScreenHeight));
	ScreenComponent->SetRelativeLocation(ScreenLocation);
	ScreenComponent->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ScreenComponent->SetRelativeScale3D(FVector(0.01f));
	ScreenComponent->SetBlendMode(EWidgetBlendMode::Opaque);
	ScreenComponent->SetTickWhenOffscreen(true);
	ScreenComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ScreenComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ScreenComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ScreenComponent->PrimaryComponentTick.bTickEvenWhenPaused = true;

	// Clics de souris sur l'écran : on lui donne nous-mêmes le point touché
	ScreenInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("ScreenInteraction"));
	ScreenInteraction->SetupAttachment(Root);
	ScreenInteraction->InteractionSource = EWidgetInteractionSource::Custom;
	ScreenInteraction->VirtualUserIndex = 7;
	ScreenInteraction->bShowDebug = false;
	ScreenInteraction->PrimaryComponentTick.bTickEvenWhenPaused = true;

	// Éclairage du Pokédex seul (canal 1) : même rendu dans une grotte ou en plein soleil
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeLocation(FVector(-26.f, -16.f, 18.f));
	KeyLight->SetIntensityUnits(ELightUnits::Candelas);
	KeyLight->SetAttenuationRadius(400.f);
	KeyLight->SetSourceRadius(6.f);
	KeyLight->LightingChannels.bChannel0 = false;
	KeyLight->LightingChannels.bChannel1 = true;
	KeyLight->SetCastShadows(true);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetRelativeLocation(FVector(-20.f, 22.f, -4.f));
	FillLight->SetIntensityUnits(ELightUnits::Candelas);
	FillLight->SetAttenuationRadius(400.f);
	FillLight->SetSourceRadius(10.f);
	FillLight->SetLightColor(FLinearColor(0.85f, 0.92f, 1.f));
	FillLight->LightingChannels.bChannel0 = false;
	FillLight->LightingChannels.bChannel1 = true;
	FillLight->SetCastShadows(false);

	// Décor flouté et exposition fixe pendant le menu
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 50.f;
	PostProcess->BlendWeight = 0.f;

	ModelAsset = TSoftObjectPtr<UPokedexModel>(FSoftObjectPath(TEXT("/Game/UI/Pokedex/DA_PokedexModel.DA_PokedexModel")));
}

// ---------------------------------------------------------------- Construction

bool APokedexDevice::InitializeFor(APokemonNewAgePlayerController* InController)
{
	Model = ModelAsset.LoadSynchronous();
	if (!InController || !Model || !Model->Parts.FindRef(TEXT("Body")))
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : DA_PokedexModel introuvable ou incomplet, menu a plat utilise (voir docs/MENU_POKEDEX.md)"));
		return false;
	}
	Controller = InController;

	for (const TCHAR* Key : BodyParts)
	{
		AddPart(Key, ModelRoot);
	}
	for (const TCHAR* Key : LidParts)
	{
		AddPart(Key, Hinge);
	}
	for (int32 Index = 0; Index < MenuCount; ++Index)
	{
		MenuButtons.Add(AddPart(*FString::Printf(TEXT("MenuButton%d"), Index), Hinge));
	}
	FixImportAxes();
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair : Parts)
	{
		ApplyMaterials(Pair.Value, Pair.Key);
	}

	// Pièces qui s'enfoncent
	for (const TCHAR* Key : { TEXT("ButtonRound"), TEXT("PillRed"), TEXT("PillBlue"), TEXT("DPad"), TEXT("Lens") })
	{
		if (UStaticMeshComponent* Part = Parts.FindRef(Key))
		{
			Pressables.Add({ Part, Part->GetRelativeLocation(), -1.f });
		}
	}
	for (UStaticMeshComponent* Button : MenuButtons)
	{
		if (Button)
		{
			Pressables.Add({ Button, Button->GetRelativeLocation(), -1.f });
		}
	}

	// Écran
	Screen = CreateWidget<UPokedexScreenWidget>(InController, UPokedexScreenWidget::StaticClass());
	Screen->OnFeedback.AddUObject(this, &APokedexDevice::OnScreenFeedback);
	ScreenComponent->SetOwnerPlayer(InController->GetLocalPlayer());
	ScreenComponent->SetWidget(Screen);
	ScreenComponent->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));

	CreateLabels();

	// Calque qui reçoit la souris et le clavier
	InputWidget = CreateWidget<UPokedexInputWidget>(InController, UPokedexInputWidget::StaticClass());
	InputWidget->Device = this;
	InputWidget->AddToPlayerScreen(20);

	PlaceInFrontOfCamera();

	// Lumières et rendu du menu
	const float LightScale = FMath::Square(GetActorScale3D().X);
	KeyLight->SetIntensity(KeyLightIntensity * LightScale);
	FillLight->SetIntensity(FillLightIntensity * LightScale);
	FPostProcessSettings& Settings = PostProcess->Settings;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = ViewDistance * GetActorScale3D().X;
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = BackgroundBlurFstop;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = MenuExposureEV100;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMaxBrightness = MenuExposureEV100;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = 0.f;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.7f;

	// Fermé, sous l'écran
	Hinge->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ModelRoot->SetRelativeLocation(FVector(0.f, ClosedSlide, 0.f));
	UpdateAnimation(0.f);
	return true;
}

UStaticMeshComponent* APokedexDevice::AddPart(FName Key, USceneComponent* Parent)
{
	UStaticMesh* Mesh = Model ? Model->Parts.FindRef(Key) : nullptr;
	if (!Mesh)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : piece %s absente de DA_PokedexModel"), *Key.ToString());
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, FName(*(TEXT("Part_") + Key.ToString())));
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Part->SetCollisionResponseToAllChannels(ECR_Ignore);
	Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Part->SetLightingChannels(false, true, false);
	Part->bReceivesDecals = false;
	Part->bCastDynamicShadow = true;
	Part->RegisterComponent();
	AddInstanceComponent(Part);
	Parts.Add(Key, Part);
	return Part;
}

void APokedexDevice::FixImportAxes()
{
	// Le modèle est fait en cm, face avant vers -X, boîtier à gauche (Y < 0), objectif en haut (Z > 0).
	// Si l'import a changé l'échelle ou retourné un axe, on corrige ici plutôt que de demander un réglage à la main.
	const UStaticMesh* BodyMesh = Model->Parts.FindRef(TEXT("Body"));
	const FBoxSphereBounds Body = BodyMesh->GetBounds();
	float Scale = 1.f;
	if (Body.BoxExtent.Z > 200.f)
	{
		Scale = 0.01f;
	}
	else if (Body.BoxExtent.Z < 1.f)
	{
		Scale = 100.f;
	}
	if (Body.BoxExtent.Z < Body.BoxExtent.X || Body.BoxExtent.Z < Body.BoxExtent.Y)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : le modele semble couche (axes de l'import FBX), voir docs/MENU_POKEDEX.md"));
	}

	FVector Mirror(1.f);
	if (Body.Origin.X < 0.f)
	{
		Mirror.X = -1.f;
	}
	const UStaticMesh* RingMesh = Model->Parts.FindRef(TEXT("LensRing"));
	const FVector Ring = RingMesh ? RingMesh->GetBounds().Origin : Body.Origin + FVector(0.f, 0.f, 1.f);
	if ((RingMesh ? Ring.Y : Body.Origin.Y) > 0.f)
	{
		Mirror.Y = -1.f;
	}
	if (RingMesh && Ring.Z < 0.f)
	{
		Mirror.Z = -1.f;
	}
	if (Scale != 1.f || Mirror != FVector(1.f))
	{
		UE_LOG(LogPokemonNewAge, Log, TEXT("Pokedex 3D : correction de l'import (echelle %.2f, miroir %s)"), Scale, *Mirror.ToString());
	}
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair : Parts)
	{
		Pair.Value->SetRelativeScale3D(Mirror * Scale);
	}
}

void APokedexDevice::ApplyMaterials(UStaticMeshComponent* Part, FName Key)
{
	// Rôle de chaque matériau, reconnu dans le nom de l'emplacement (MAT_RedPlastic...)
	static const FMaterialLook Looks[] = {
		{ TEXT("RedPlastic"), FLinearColor(0.5f, 0.035f, 0.025f), 0.f, 0.42f, FLinearColor::Black, 0.f },
		{ TEXT("RedDark"), FLinearColor(0.28f, 0.02f, 0.015f), 0.f, 0.5f, FLinearColor::Black, 0.f },
		{ TEXT("Black"), FLinearColor(0.012f, 0.012f, 0.014f), 0.f, 0.35f, FLinearColor::Black, 0.f },
		{ TEXT("Metal"), FLinearColor(0.75f, 0.75f, 0.78f), 1.f, 0.2f, FLinearColor::Black, 0.f },
		{ TEXT("LensGlass"), FLinearColor(0.03f, 0.12f, 0.45f), 0.f, 0.05f, FLinearColor(0.1f, 0.45f, 1.f), 0.3f },
		{ TEXT("LedRed"), FLinearColor(0.6f, 0.03f, 0.03f), 0.f, 0.2f, FLinearColor(1.f, 0.05f, 0.04f), 0.3f },
		{ TEXT("LedYellow"), FLinearColor(0.6f, 0.45f, 0.03f), 0.f, 0.2f, FLinearColor(1.f, 0.75f, 0.1f), 0.3f },
		{ TEXT("LedGreen"), FLinearColor(0.05f, 0.5f, 0.12f), 0.f, 0.2f, FLinearColor(0.15f, 1.f, 0.3f), 1.f },
		{ TEXT("PillRed"), FLinearColor(0.75f, 0.06f, 0.2f), 0.f, 0.35f, FLinearColor::Black, 0.f },
		{ TEXT("PillBlue"), FLinearColor(0.15f, 0.45f, 0.9f), 0.f, 0.35f, FLinearColor::Black, 0.f },
		{ TEXT("PanelGlass"), FLinearColor(0.4f, 0.35f, 0.85f), 0.f, 0.15f, FLinearColor(0.4f, 0.35f, 0.85f), 0.15f },
		{ TEXT("Screen"), FLinearColor(0.01f, 0.012f, 0.015f), 0.f, 0.12f, FLinearColor::Black, 0.f },
		{ TEXT("ButtonPlate"), FLinearColor(0.02f, 0.022f, 0.026f), 0.f, 0.3f, FLinearColor::Black, 0.f },
		{ TEXT("Glow"), FLinearColor(0.2f, 0.4f, 0.8f), 0.f, 0.3f, FLinearColor(0.3f, 0.6f, 1.f), 1.5f },
	};

	const int32 MenuIndex = MenuButtons.IndexOfByKey(Part);
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>& Materials = PartMaterials.FindOrAdd(Part);
	const TArray<FName> SlotNames = Part->GetMaterialSlotNames();

	for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
	{
		const FString SlotName = SlotNames.IsValidIndex(Slot) ? SlotNames[Slot].ToString() : FString();
		const FString ImportedName = Part->GetMaterial(Slot) ? Part->GetMaterial(Slot)->GetName() : FString();
		const FMaterialLook* Look = nullptr;
		for (const FMaterialLook& Candidate : Looks)
		{
			if (SlotName.Contains(Candidate.Token) || ImportedName.Contains(Candidate.Token))
			{
				Look = &Candidate;
				break;
			}
		}

		UMaterialInterface* Parent = Model->Material ? Model->Material.Get() : Part->GetMaterial(Slot);
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		if (!Material)
		{
			continue;
		}
		if (Look)
		{
			const bool bGlow = FCString::Strcmp(Look->Token, TEXT("Glow")) == 0;
			const FLinearColor Emissive = (bGlow && MenuIndex >= 0) ? MenuColors[MenuIndex] : Look->Emissive;
			Material->SetVectorParameterValue(TEXT("BaseColor"), bGlow && MenuIndex >= 0 ? Emissive * 0.3f : Look->BaseColor);
			Material->SetScalarParameterValue(TEXT("Metallic"), Look->Metallic);
			Material->SetScalarParameterValue(TEXT("Roughness"), Look->Roughness);
			Material->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
			Material->SetScalarParameterValue(TEXT("EmissiveStrength"), Look->EmissiveStrength);

			if (bGlow && MenuIndex >= 0)
			{
				MenuGlows.SetNum(MenuCount);
				MenuGlows[MenuIndex] = Material;
			}
			else if (Key == TEXT("Lens"))
			{
				LensMaterial = Material;
			}
			else if (Key == TEXT("LedRed"))
			{
				LedRedMaterial = Material;
			}
			else if (Key == TEXT("LedYellow"))
			{
				LedYellowMaterial = Material;
			}
			else if (Key == TEXT("LedGreen"))
			{
				LedGreenMaterial = Material;
			}
		}
		Part->SetMaterial(Slot, Material);
		Materials.Add(Material);
	}
}

void APokedexDevice::CreateLabels()
{
	const APokemonPlayerState* State = Controller.IsValid() ? Controller->GetPlayerState<APokemonPlayerState>() : nullptr;
	const FText Names[MenuCount] = {
		LOCTEXT("Pokemon", "POKÉMON"),
		LOCTEXT("Pokedex", "POKÉDEX"),
		LOCTEXT("Bag", "SAC"),
		State && !State->GetPlayerName().IsEmpty() ? FText::FromString(State->GetPlayerName().ToUpper()) : LOCTEXT("Trainer", "DRESSEUR"),
		LOCTEXT("Save", "SAUVEGARDE"),
		LOCTEXT("Options", "OPTIONS"),
	};

	for (int32 Index = 0; Index < MenuCount; ++Index)
	{
		UStaticMeshComponent* Button = MenuButtons.IsValidIndex(Index) ? MenuButtons[Index].Get() : nullptr;
		if (!Button)
		{
			MenuLabels.Add(nullptr);
			MenuLabelWidgets.Add(nullptr);
			continue;
		}

		// Devant la plaque du bouton, dans le repère de la charnière
		const FBoxSphereBounds Bounds = Button->CalcBounds(Button->GetRelativeTransform());
		const FVector Location(Bounds.Origin.X - Bounds.BoxExtent.X - 0.03f, Bounds.Origin.Y, Bounds.Origin.Z - 0.12f);

		UPokedexLabelWidget* LabelWidget = CreateWidget<UPokedexLabelWidget>(Controller.Get(), UPokedexLabelWidget::StaticClass());
		LabelWidget->SetLabel(Names[Index], MenuColors[Index]);

		UWidgetComponent* Label = NewObject<UWidgetComponent>(this, FName(*FString::Printf(TEXT("Label%d"), Index)));
		Label->SetupAttachment(Hinge);
		Label->SetWidgetSpace(EWidgetSpace::World);
		Label->SetDrawSize(FVector2D(UPokedexLabelWidget::Width, UPokedexLabelWidget::Height));
		Label->SetRelativeLocation(Location);
		Label->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		Label->SetRelativeScale3D(FVector(0.01f));
		Label->SetBlendMode(EWidgetBlendMode::Transparent);
		Label->SetBackgroundColor(FLinearColor::Transparent);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetTickWhenOffscreen(true);
		Label->PrimaryComponentTick.bTickEvenWhenPaused = true;
		Label->RegisterComponent();
		Label->SetOwnerPlayer(Controller->GetLocalPlayer());
		Label->SetWidget(LabelWidget);
		AddInstanceComponent(Label);

		MenuLabels.Add(Label);
		MenuLabelWidgets.Add(LabelWidget);
		Pressables.Add({ Label, Label->GetRelativeLocation(), -1.f });
	}
}

void APokedexDevice::PlaceInFrontOfCamera()
{
	APlayerController* PC = Controller.Get();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	CameraRotation = PC->PlayerCameraManager->GetCameraRotation();

	// Distance pour que l'appareil ouvert remplisse ScreenFill de l'écran, quel que soit le champ de vision
	int32 SizeX = 16, SizeY = 9;
	PC->GetViewportSize(SizeX, SizeY);
	const float Aspect = SizeY > 0 ? float(SizeX) / float(SizeY) : 16.f / 9.f;
	const float TanH = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle() * 0.5f));
	const float TanV = TanH / Aspect;
	ViewDistance = FMath::Max(HalfWidth / (TanH * ScreenFill), HalfHeight / (TanV * ScreenFill));
}

// ---------------------------------------------------------------- Ouverture et fermeture

void APokedexDevice::Open()
{
	bClosing = false;
	OpenAlpha = 0.f;
	PlaySound(Model ? Model->OpenSound : nullptr);
	// Petite séquence de démarrage des voyants
	LedRedFlash = Time + 0.25f;
	LedYellowFlash = Time + 0.4f;
	LensFlash = Time + 0.6f;
	FocusLid();
}

void APokedexDevice::Close(bool bImmediate)
{
	if (!bClosing)
	{
		bClosing = true;
		bDragging = false;
		PlaySound(Model ? Model->CloseSound : nullptr);
		if (Screen)
		{
			ScreenInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
		}
	}
	if (bImmediate)
	{
		OpenAlpha = 0.f;
		UpdateAnimation(0.f);
	}
}

void APokedexDevice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InputWidget)
	{
		InputWidget->RemoveFromParent();
		InputWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- Animation

void APokedexDevice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// En pause, DeltaSeconds est le temps réel : les animations continuent
	UpdateAnimation(DeltaSeconds);
	if (!IsActorBeingDestroyed())
	{
		UpdateLights(DeltaSeconds);
	}
}

void APokedexDevice::UpdateAnimation(float DeltaSeconds)
{
	Time += DeltaSeconds;

	if (bClosing)
	{
		OpenAlpha = FMath::Max(0.f, OpenAlpha - DeltaSeconds / CloseDuration);
		if (OpenAlpha <= 0.f)
		{
			if (APokemonNewAgePlayerController* PC = Controller.Get())
			{
				PC->OnPokedexClosed(this);
			}
			Destroy();
			return;
		}
	}
	else
	{
		OpenAlpha = FMath::Min(1.f, OpenAlpha + DeltaSeconds / OpenDuration);
	}

	// 1. Il monte du bas de l'écran  2. il s'ouvre et glisse pour se recentrer
	const float Rise = EaseOutCubic(OpenAlpha / 0.45f);
	const float LidT = FMath::Clamp((OpenAlpha - 0.3f) / 0.7f, 0.f, 1.f);
	const float LidOpen = bClosing ? FMath::SmoothStep(0.f, 1.f, LidT) : EaseOutBack(LidT);
	Hinge->SetRelativeRotation(FRotator(0.f, FMath::Lerp(180.f, OpenLidYaw, LidOpen), 0.f));
	ModelRoot->SetRelativeLocation(FVector(0.f, FMath::Lerp(ClosedSlide, 0.f, FMath::SmoothStep(0.f, 1.f, LidT)), 0.f));

	// Rotation par le joueur : libre pendant qu'on le tient, retour en douceur ensuite
	UserYaw += StickYaw * 140.f * DeltaSeconds;
	UserPitch += StickPitch * 100.f * DeltaSeconds;
	const bool bHeld = bDragging || !FMath::IsNearlyZero(StickYaw) || !FMath::IsNearlyZero(StickPitch);
	if (!bHeld)
	{
		UserYaw = FMath::FInterpTo(FRotator::NormalizeAxis(UserYaw), 0.f, DeltaSeconds, 5.f);
		UserPitch = FMath::FInterpTo(FRotator::NormalizeAxis(UserPitch), 0.f, DeltaSeconds, 5.f);
	}
	const float Sway = Time - LastInteractTime > 1.5f ? 1.f : 0.f;
	Pivot->SetRelativeRotation(FRotator(UserPitch + Sway * 1.2f * FMath::Sin(Time * 1.1f), UserYaw + Sway * 2.f * FMath::Sin(Time * 0.7f), 0.f));

	// Devant la caméra, tourné vers elle
	const FVector Forward = CameraRotation.Vector();
	const FVector Up = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Z);
	const float Distance = (ViewDistance + ZoomOffset) * GetActorScale3D().X;
	const FVector Location = CameraLocation + Forward * Distance - Up * (1.f - Rise) * Distance * 0.9f;
	SetActorLocationAndRotation(Location, CameraRotation + FRotator((1.f - Rise) * -35.f, 0.f, 0.f));

	// Pièces enfoncées
	for (FPressable& Press : Pressables)
	{
		USceneComponent* Component = Press.Component.Get();
		if (!Component || Press.PressTime < 0.f)
		{
			continue;
		}
		const float T = (Time - Press.PressTime) / PressDuration;
		float Depth = T < 0.3f ? T / 0.3f : 1.f - (T - 0.3f) / 0.7f;
		if (T >= 1.f)
		{
			Depth = 0.f;
			Press.PressTime = -1.f;
		}
		Component->SetRelativeLocation(Press.BaseLocation + FVector(PressDepth * FMath::Clamp(Depth, 0.f, 1.f), 0.f, 0.f));
	}

	// Écran qui s'allume, flou du décor (on l'enlève quand on tourne l'appareil, sinon ses bords seraient flous)
	ScreenComponent->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp((OpenAlpha - 0.65f) / 0.3f, 0.f, 1.f)));
	const float Turned = FMath::Clamp(FMath::Max(FMath::Abs(FRotator::NormalizeAxis(UserYaw)), FMath::Abs(FRotator::NormalizeAxis(UserPitch))) / 25.f, 0.f, 1.f);
	PostProcess->BlendWeight = OpenAlpha * (1.f - Turned);
	PostProcess->Settings.DepthOfFieldFocalDistance = Distance;
}

void APokedexDevice::UpdateLights(float DeltaSeconds)
{
	const float Booting = OpenAlpha < 1.f && !bClosing ? 1.f : 0.f;
	if (LedGreenMaterial)
	{
		LedGreenMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), (1.f - Booting) * (2.5f + 1.5f * FMath::Sin(Time * 2.4f)) + 6.f * Flash(Time, LensFlash, 3.f));
	}
	if (LedYellowMaterial)
	{
		LedYellowMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.2f + 10.f * Flash(Time, LedYellowFlash, 10.f));
	}
	if (LedRedMaterial)
	{
		LedRedMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.2f + 10.f * Flash(Time, LedRedFlash, 6.f) + (bClosing ? 4.f : 0.f));
	}
	if (LensMaterial)
	{
		LensMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.3f + 8.f * Flash(Time, LensFlash, 4.f));
	}

	for (int32 Index = 0; Index < MenuGlows.Num(); ++Index)
	{
		const bool bSelected = Index == LidSelection;
		const bool bHover = Hovered.IsValid() && MenuButtons.IsValidIndex(Index) && Hovered.Get() == MenuButtons[Index].Get();
		float Strength = 1.2f;
		if (bSelected && bLidFocus)
		{
			Strength = 5.f + 2.f * FMath::Sin(Time * 6.f);
		}
		else if (bSelected || bHover)
		{
			Strength = 4.f;
		}
		Strength += 14.f * Flash(Time, MenuFlash[Index], 6.f);
		if (MenuGlows[Index])
		{
			MenuGlows[Index]->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength * OpenAlpha);
		}
		if (MenuLabelWidgets.IsValidIndex(Index) && MenuLabelWidgets[Index])
		{
			MenuLabelWidgets[Index]->SetHighlighted(bSelected || bHover);
		}
	}
}

// ---------------------------------------------------------------- Souris

bool APokedexDevice::TraceParts(const FVector2D& Pixel, FHitResult& OutHit) const
{
	const APlayerController* PC = Controller.Get();
	FVector Origin, Direction;
	if (!PC || !PC->DeprojectScreenPositionToWorld(Pixel.X, Pixel.Y, Origin, Direction))
	{
		return false;
	}
	const FVector End = Origin + Direction * 1000.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PokedexTrace), true);

	bool bFound = false;
	float Best = TNumericLimits<float>::Max();
	auto TryComponent = [&](UPrimitiveComponent* Component)
	{
		FHitResult Hit;
		if (Component && Component->LineTraceComponent(Hit, Origin, End, Params))
		{
			const float Distance = FVector::Dist(Origin, Hit.ImpactPoint);
			if (Distance < Best)
			{
				Best = Distance;
				OutHit = Hit;
				OutHit.Component = Component;
				OutHit.Distance = Distance;
				bFound = true;
			}
		}
	};
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair : Parts)
	{
		TryComponent(Pair.Value.Get());
	}
	TryComponent(ScreenComponent.Get());
	return bFound;
}

FName APokedexDevice::GetPartKey(const UPrimitiveComponent* Component) const
{
	if (Component == ScreenComponent.Get())
	{
		return TEXT("Screen");
	}
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair : Parts)
	{
		if (Pair.Value.Get() == Component)
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

void APokedexDevice::HandlePointerDown(const FKey& Button, const FVector2D& Pixel)
{
	if (bClosing || OpenAlpha < 0.6f)
	{
		return;
	}
	LastPointer = Pixel;
	LastInteractTime = Time;

	if (Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton)
	{
		bDragging = true;
		return;
	}
	if (Button != EKeys::LeftMouseButton)
	{
		return;
	}

	FHitResult Hit;
	if (!TraceParts(Pixel, Hit))
	{
		bDragging = true;
		return;
	}

	const FName Key = GetPartKey(Hit.GetComponent());
	const FString KeyString = Key.ToString();
	if (Key == TEXT("Screen"))
	{
		bLidFocus = false;
		ScreenInteraction->SetCustomHitResult(Hit);
		ScreenInteraction->PressPointerKey(EKeys::LeftMouseButton);
		bScreenPointerDown = true;
	}
	else if (KeyString.StartsWith(TEXT("MenuButton")))
	{
		PressMenuButton(FCString::Atoi(*KeyString.RightChop(10)));
	}
	else if (Key == TEXT("DPad"))
	{
		ClickDPad(Hit.ImpactPoint);
	}
	else if (Key == TEXT("ButtonRound"))
	{
		PressPart(Hit.GetComponent());
		HandleKey(EKeys::Enter);
	}
	else if (Key == TEXT("PillRed"))
	{
		PressPart(Hit.GetComponent());
		HandleKey(EKeys::Escape);
	}
	else if (Key == TEXT("PillBlue"))
	{
		PressPart(Hit.GetComponent());
		PlaySound(Model ? Model->ClickSound : nullptr);
		FocusLid();
	}
	else if (Key == TEXT("Lens") || Key == TEXT("LensRing"))
	{
		// L'objectif « scanne » : il s'allume et ouvre le Pokédex
		PressPart(Parts.FindRef(TEXT("Lens")));
		LensFlash = Time;
		PressMenuButton(int32(EPokedexMenu::Pokedex));
	}
	else if (Key == TEXT("LedRed") || Key == TEXT("LedYellow") || Key == TEXT("LedGreen"))
	{
		LedRedFlash = Time;
		LedYellowFlash = Time + 0.12f;
		LensFlash = Time + 0.24f;
		PlaySound(Model ? Model->BeepSound : nullptr, 1.2f);
	}
	else if (Key == TEXT("Lid") || Key == TEXT("LidPanel"))
	{
		// Toucher le volet le referme
		Close();
	}
	else
	{
		// Boîtier : on l'attrape pour le tourner
		bDragging = true;
	}
}

void APokedexDevice::HandlePointerUp(const FKey& Button, const FVector2D& Pixel)
{
	if (bScreenPointerDown && Button == EKeys::LeftMouseButton)
	{
		ScreenInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
		bScreenPointerDown = false;
	}
	bDragging = false;
	LastInteractTime = Time;
}

void APokedexDevice::HandlePointerMove(const FVector2D& Pixel, const FVector2D& Delta)
{
	if (bDragging)
	{
		// Tirer vers la droite tourne la face avant vers la droite ; vers le bas, le haut vient vers soi
		UserYaw -= Delta.X * DragSensitivity;
		UserPitch += Delta.Y * DragSensitivity;
		LastInteractTime = Time;
		return;
	}

	FHitResult Hit;
	const bool bHit = !bClosing && TraceParts(Pixel, Hit);
	UPrimitiveComponent* Component = bHit ? Hit.GetComponent() : nullptr;
	if (Component == ScreenComponent.Get())
	{
		ScreenInteraction->SetCustomHitResult(Hit);
	}
	else
	{
		ScreenInteraction->SetCustomHitResult(FHitResult());
	}
	SetHovered(Component);
}

void APokedexDevice::HandleWheel(float Delta)
{
	LastInteractTime = Time;
	// Sur l'écran, la molette fait défiler la liste ; ailleurs elle rapproche l'appareil
	if (Hovered.Get() == ScreenComponent.Get() && Screen)
	{
		bLidFocus = false;
		Screen->Navigate(FIntPoint(0, Delta > 0.f ? -1 : 1));
		return;
	}
	ZoomOffset = FMath::Clamp(ZoomOffset - Delta * 1.5f, -ViewDistance * 0.35f, ViewDistance * 0.6f);
}

void APokedexDevice::SetHovered(UPrimitiveComponent* Component)
{
	if (Hovered.Get() == Component)
	{
		return;
	}

	// Surbrillance des petits boutons du boîtier (les boutons du volet, l'objectif et les voyants ont leur propre lumière)
	auto Highlight = [this](UPrimitiveComponent* Part, bool bOn)
	{
		const FName Key = GetPartKey(Part);
		if (Key != TEXT("ButtonRound") && Key != TEXT("PillRed") && Key != TEXT("PillBlue") && Key != TEXT("DPad") && Key != TEXT("LensRing"))
		{
			return;
		}
		if (const TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>* Materials = PartMaterials.Find(Part))
		{
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Material : *Materials)
			{
				if (UMaterialInstanceDynamic* MID = Material.Get())
				{
					MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.f, 0.95f, 0.85f));
					MID->SetScalarParameterValue(TEXT("EmissiveStrength"), bOn ? 0.25f : 0.f);
				}
			}
		}
	};

	if (UPrimitiveComponent* Previous = Hovered.Get())
	{
		Highlight(Previous, false);
	}
	Hovered = Component;
	if (Component)
	{
		Highlight(Component, true);
	}

	const FName Key = GetPartKey(Component);
	const bool bClickable = Component && Key != TEXT("Body") && Key != TEXT("TopBand") && Key != TEXT("Bezel") && Key != TEXT("Panel") && Key != TEXT("Hinge");
	if (InputWidget)
	{
		InputWidget->SetCursor(bClickable ? EMouseCursor::Hand : EMouseCursor::Default);
	}
}

void APokedexDevice::ClickDPad(const FVector& WorldHit)
{
	UStaticMeshComponent* DPad = Parts.FindRef(TEXT("DPad"));
	if (!DPad)
	{
		return;
	}
	// Direction selon l'endroit touché par rapport au centre de la croix (repère de l'appareil)
	const FTransform& Frame = ModelRoot->GetComponentTransform();
	const FVector Local = Frame.InverseTransformPosition(WorldHit) - Frame.InverseTransformPosition(DPad->Bounds.Origin);
	const FIntPoint Direction = FMath::Abs(Local.Y) > FMath::Abs(Local.Z)
		? FIntPoint(Local.Y > 0.f ? 1 : -1, 0)
		: FIntPoint(0, Local.Z > 0.f ? -1 : 1);
	const FKey Key = Direction.X > 0 ? EKeys::Right : (Direction.X < 0 ? EKeys::Left : (Direction.Y < 0 ? EKeys::Up : EKeys::Down));
	HandleKey(Key);
}

// ---------------------------------------------------------------- Clavier et manette

bool APokedexDevice::IsBackKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;
}

bool APokedexDevice::IsConfirmKey(const FKey& Key)
{
	return Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom;
}

FIntPoint APokedexDevice::GetDirection(const FKey& Key)
{
	if (Key == EKeys::Up || Key == EKeys::Z || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		return FIntPoint(0, -1);
	}
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		return FIntPoint(0, 1);
	}
	if (Key == EKeys::Left || Key == EKeys::Q || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		return FIntPoint(-1, 0);
	}
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		return FIntPoint(1, 0);
	}
	return FIntPoint::ZeroValue;
}

bool APokedexDevice::HandleKey(const FKey& Key)
{
	if (Key == EKeys::Tab || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return true;
	}
	if (bClosing)
	{
		return true;
	}
	LastInteractTime = Time;

	// Gâchettes hautes : bouton du volet précédent / suivant
	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
	{
		PressMenuButton((LidSelection + (Key == EKeys::Gamepad_RightShoulder ? 1 : MenuCount - 1)) % MenuCount);
		return true;
	}

	const FIntPoint Direction = GetDirection(Key);
	const bool bConfirm = IsConfirmKey(Key);
	const bool bBack = IsBackKey(Key);
	if (Direction == FIntPoint::ZeroValue && !bConfirm && !bBack)
	{
		return false;
	}

	// La pièce correspondante bouge aussi
	if (Direction != FIntPoint::ZeroValue)
	{
		PressPart(Parts.FindRef(TEXT("DPad")));
	}
	else
	{
		PressPart(Parts.FindRef(bConfirm ? TEXT("ButtonRound") : TEXT("PillRed")));
	}

	if (bLidFocus)
	{
		if (Direction != FIntPoint::ZeroValue)
		{
			MoveLidSelection(Direction);
		}
		else if (bConfirm)
		{
			PressMenuButton(LidSelection);
		}
		else
		{
			Close();
		}
		return true;
	}

	if (bBack)
	{
		if (Screen && !Screen->Back())
		{
			FocusLid();
		}
		return true;
	}
	SendToScreen(Key);
	return true;
}

void APokedexDevice::HandleAnalog(const FKey& Key, float Value)
{
	// Stick droit : tourner l'appareil
	const float Dead = 0.2f;
	const float Filtered = FMath::Abs(Value) > Dead ? Value : 0.f;
	if (Key == EKeys::Gamepad_RightX)
	{
		StickYaw = -Filtered;
	}
	else if (Key == EKeys::Gamepad_RightY)
	{
		StickPitch = -Filtered;
	}
	if (!FMath::IsNearlyZero(Filtered))
	{
		LastInteractTime = Time;
	}
}

void APokedexDevice::SendToScreen(const FKey& Key)
{
	if (Screen)
	{
		Screen->HandleKey(Key);
	}
}

void APokedexDevice::MoveLidSelection(FIntPoint Direction)
{
	// Grille de 2 colonnes × 3 rangées
	int32 Column = LidSelection % 2;
	int32 Row = LidSelection / 2;
	Column = (Column + Direction.X + 2) % 2;
	Row = (Row + Direction.Y + 3) % 3;
	LidSelection = Row * 2 + Column;
	PlayFeedback(EPokedexFeedback::Navigate);
}

void APokedexDevice::PressMenuButton(int32 Index)
{
	if (Index < 0 || Index >= MenuCount)
	{
		return;
	}
	LidSelection = Index;
	bLidFocus = false;
	if (MenuButtons.IsValidIndex(Index))
	{
		PressPart(MenuButtons[Index]);
	}
	if (MenuLabels.IsValidIndex(Index))
	{
		PressPart(MenuLabels[Index]);
	}
	MenuFlash[Index] = Time;
	LensFlash = Time;
	PlaySound(Model ? Model->ClickSound : nullptr);
	if (Screen)
	{
		Screen->OpenMenu(EPokedexMenu(Index));
	}
}

void APokedexDevice::PressPart(USceneComponent* Component)
{
	for (FPressable& Press : Pressables)
	{
		if (Press.Component.Get() == Component)
		{
			Press.PressTime = Time;
		}
	}
}

void APokedexDevice::FocusLid()
{
	bLidFocus = true;
	if (Screen)
	{
		Screen->ShowHome();
	}
}

// ---------------------------------------------------------------- Son et voyants

void APokedexDevice::OnScreenFeedback(EPokedexFeedback Type)
{
	PlayFeedback(Type);
}

void APokedexDevice::PlayFeedback(EPokedexFeedback Type)
{
	if (!Model)
	{
		return;
	}
	switch (Type)
	{
	case EPokedexFeedback::Navigate:
		LedYellowFlash = Time;
		PlaySound(Model->NavSound);
		break;
	case EPokedexFeedback::Confirm:
		LensFlash = Time;
		PlaySound(Model->BeepSound);
		break;
	case EPokedexFeedback::Back:
		LedYellowFlash = Time;
		PlaySound(Model->NavSound, 0.8f);
		break;
	case EPokedexFeedback::Error:
		LedRedFlash = Time;
		PlaySound(Model->ErrorSound);
		break;
	case EPokedexFeedback::Saved:
		LensFlash = Time;
		LedYellowFlash = Time + 0.15f;
		PlaySound(Model->SaveSound);
		break;
	}
}

void APokedexDevice::PlaySound(USoundBase* Sound, float Pitch) const
{
	if (Sound)
	{
		// Son d'interface : il joue même quand le jeu est en pause
		UGameplayStatics::PlaySound2D(this, Sound, 1.f, Pitch, 0.f, nullptr, nullptr, true);
	}
}

#undef LOCTEXT_NAMESPACE
