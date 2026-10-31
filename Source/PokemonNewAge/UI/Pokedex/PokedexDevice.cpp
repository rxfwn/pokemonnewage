#include "UI/Pokedex/PokedexDevice.h"
#include "UI/Pokedex/PokedexModel.h"
#include "UI/Pokedex/PokedexWidgets.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PokemonNewAge.h"
#include "PokemonNewAgePlayerController.h"

#define LOCTEXT_NAMESPACE "PokedexDevice"

namespace PokedexDevice
{
	/** Pièces du boîtier (accrochées à ModelRoot) et du volet (accrochées à la charnière), voir prepare_pokedex_model.py */
	const TCHAR* BodyParts[] = { TEXT("Body"), TEXT("Hinge"), TEXT("Lens"), TEXT("LensRing"), TEXT("LedRed"), TEXT("LedYellow"),
		TEXT("LedGreen"), TEXT("ScreenLeds"), TEXT("Screen"), TEXT("MiniScreen0"), TEXT("BackButton"), TEXT("ConfirmButton"),
		TEXT("HomeButton"), TEXT("DexButton"), TEXT("DPad") };
	const TCHAR* LidParts[] = { TEXT("Lid"), TEXT("LidScreen"), TEXT("MiniScreen1"), TEXT("MiniScreen2"), TEXT("LidLeds"),
		TEXT("PrevButton"), TEXT("NextButton"), TEXT("LidButton") };

	/** Boutons qui s'enfoncent */
	const TCHAR* PushButtons[] = { TEXT("BackButton"), TEXT("ConfirmButton"), TEXT("HomeButton"), TEXT("DexButton"), TEXT("DPad"),
		TEXT("Lens"), TEXT("PrevButton"), TEXT("NextButton"), TEXT("LidButton") };

	/** Pièces sur lesquelles on ne clique pas (on les attrape pour tourner l'appareil) */
	const TCHAR* GripParts[] = { TEXT("Body"), TEXT("Lid"), TEXT("LidScreen"), TEXT("MiniScreen0"), TEXT("MiniScreen1"), TEXT("MiniScreen2") };

	/** Couleur des boutons du volet (sous les icônes), allumés quand on les choisit */
	const FLinearColor TileColor(0.62f, 0.86f, 0.93f);
	const FLinearColor TileGlow(0.35f, 0.85f, 1.f);

	/** Petit écart devant une face pour que l'interface ne se mélange pas à la vitre (cm) */
	constexpr float PanelGap = 0.015f;

	template <size_t N>
	bool IsInList(FName Key, const TCHAR* const (&List)[N])
	{
		for (const TCHAR* Name : List)
		{
			if (Key == Name)
			{
				return true;
			}
		}
		return false;
	}

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

	// Grand écran : l'interface des menus (960 × 648 px), posée sur la vitre de la pièce Screen à l'initialisation
	ScreenComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	ScreenComponent->SetupAttachment(ModelRoot);
	ScreenComponent->SetWidgetSpace(EWidgetSpace::World);
	ScreenComponent->SetDrawSize(FVector2D(UPokedexScreenWidget::ScreenWidth, UPokedexScreenWidget::ScreenHeight));
	ScreenComponent->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ScreenComponent->SetRelativeScale3D(FVector(1.f / UPokedexPanelWidget::PixelsPerCm));
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

	// Figurine du Pokémon (accrochée au volet à l'initialisation), éclairée par les lumières du Pokédex
	FigureRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FigureRoot"));
	FigureRoot->SetupAttachment(Hinge);
	FigureSkeletal = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FigureSkeletal"));
	FigureStatic = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FigureStatic"));
	for (UPrimitiveComponent* Figure : { static_cast<UPrimitiveComponent*>(FigureSkeletal), static_cast<UPrimitiveComponent*>(FigureStatic) })
	{
		Figure->SetupAttachment(FigureRoot);
		Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Figure->SetLightingChannels(false, true, false);
		Figure->SetVisibility(false);
		Figure->PrimaryComponentTick.bTickEvenWhenPaused = true;
	}
	FigureSkeletal->SetTickGroup(TG_PrePhysics);

	ModelAsset = TSoftObjectPtr<UPokedexModel>(FSoftObjectPath(UPokedexModel::AssetPath));
}

// ---------------------------------------------------------------- Construction

bool APokedexDevice::InitializeFor(APokemonNewAgePlayerController* InController)
{
	Model = ModelAsset.LoadSynchronous();
	if (!InController || !Model || !Model->Parts.FindRef(TEXT("Body")))
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : DA_PokedexModel introuvable ou incomplet, menu a plat utilise (voir docs/MENU_POKEDEX.md)"));
#if WITH_EDITOR
		// Visible tout de suite dans l'éditeur : le Pokédex 3D s'installe à l'ouverture d'Unreal (Content/Python/init_unreal.py)
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 8.f, FColor::Orange,
				TEXT("Pokedex 3D pas installe : fermer et rouvrir Unreal (installation automatique), ou lancer Tools/Pokedex/import_pokedex_unreal.py"));
		}
#endif
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
	MenuGlows.SetNum(MenuCount);
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair : Parts)
	{
		ApplyMaterials(Pair.Value, Pair.Key);
	}
	MeasureModel();

	// Pièces qui s'enfoncent
	for (const TCHAR* Key : PushButtons)
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

	// Grand écran
	Screen = CreateWidget<UPokedexScreenWidget>(InController, UPokedexScreenWidget::StaticClass());
	Screen->OnFeedback.AddUObject(this, &APokedexDevice::OnScreenFeedback);
	ScreenComponent->SetOwnerPlayer(InController->GetLocalPlayer());
	ScreenComponent->SetWidget(Screen);
	ScreenComponent->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	FVector ScreenCenter;
	FVector2D ScreenSize;
	if (GetPartFace(TEXT("Screen"), ScreenCenter, ScreenSize))
	{
		// Toute la vitre, à la même échelle en largeur et en hauteur
		ScreenComponent->SetRelativeLocation(ScreenCenter);
		const float Scale = FMath::Min(ScreenSize.X / UPokedexScreenWidget::ScreenWidth, ScreenSize.Y / UPokedexScreenWidget::ScreenHeight);
		ScreenComponent->SetRelativeScale3D(FVector(Scale));
	}
	Panels.Add(ScreenComponent);

	CreatePanels();

	// Figurine : debout au-dessus de l'écran du volet, un peu devant
	FVector LidCenter;
	FVector2D LidSize;
	if (GetPartFace(TEXT("LidScreen"), LidCenter, LidSize))
	{
		FigureRoot->SetRelativeLocation(FVector(LidCenter.X - 1.5f, LidCenter.Y, LidCenter.Z + LidSize.Y * 0.5f + 0.3f));
	}

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
	ModelRoot->SetRelativeLocation(ClosedOffset);
	UpdatePanels(1.f);
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
	// Le modèle est en cm, face avant vers -X, boîtier à gauche (Y < 0), objectif en haut (Z > 0).
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
	if (Body.BoxExtent.Z < Body.BoxExtent.X || Body.BoxExtent.Z < Body.BoxExtent.Y * 0.8f)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : le modele semble couche (axes de l'import FBX), voir docs/MENU_POKEDEX.md"));
	}

	// L'objectif dépasse devant (X le plus petit), à gauche (Y < 0) et en haut (Z > 0)
	FVector Mirror(1.f);
	const UStaticMesh* LensMesh = Model->Parts.FindRef(TEXT("Lens"));
	const FVector Lens = LensMesh ? LensMesh->GetBounds().Origin : Body.Origin + FVector(-1.f, -1.f, 1.f);
	if (Lens.X > Body.Origin.X)
	{
		Mirror.X = -1.f;
	}
	if (Lens.Y > 0.f)
	{
		Mirror.Y = -1.f;
	}
	if (Lens.Z < 0.f)
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

void APokedexDevice::MeasureModel()
{
	// Volet ouvert à plat : les pièces du volet sont déjà dans le repère de l'appareil (charnière à l'origine)
	FBox OpenBox(ForceInit);
	FBox ClosedBox(ForceInit);
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair : Parts)
	{
		const FBox Box = Pair.Value->CalcBounds(Pair.Value->GetRelativeTransform()).GetBox();
		OpenBox += Box;
		if (Pair.Value->GetAttachParent() == ModelRoot.Get())
		{
			ClosedBox += Box;
		}
	}
	if (!OpenBox.IsValid || !ClosedBox.IsValid)
	{
		return;
	}
	HalfWidth = OpenBox.GetExtent().Y;
	HalfHeight = OpenBox.GetExtent().Z;
	OpenOffset = FVector(0.f, -OpenBox.GetCenter().Y, -OpenBox.GetCenter().Z);
	ClosedOffset = FVector(0.f, -ClosedBox.GetCenter().Y, -ClosedBox.GetCenter().Z);
}

void APokedexDevice::ApplyMaterials(UStaticMeshComponent* Part, FName Key)
{
	// Le modèle garde ses textures (MI_Pokedex_* posés par le script d'import) : on en fait des copies animables.
	// Les 10 boutons du volet prennent le matériau uni (M_Pokedex) : leurs icônes sont dessinées par-dessus.
	const int32 MenuIndex = MenuButtons.IndexOfByKey(Part);
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>& Materials = PartMaterials.FindOrAdd(Part);

	for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Parent = (MenuIndex >= 0 && Model->Material) ? Model->Material.Get() : Part->GetMaterial(Slot);
		if (!Parent)
		{
			Parent = Model->Material;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		if (!Material)
		{
			continue;
		}
		if (MenuIndex >= 0)
		{
			Material->SetVectorParameterValue(TEXT("BaseColor"), TileColor);
			Material->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			Material->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
			Material->SetVectorParameterValue(TEXT("EmissiveColor"), TileGlow);
			Material->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.f);
			MenuGlows[MenuIndex] = Material;
		}
		else
		{
			// Voyants : leur couleur s'ajoute à la texture quand ils s'allument
			const TPair<const TCHAR*, FLinearColor> Leds[] = {
				{ TEXT("LedRed"), FLinearColor(1.f, 0.05f, 0.04f) },
				{ TEXT("LedYellow"), FLinearColor(1.f, 0.75f, 0.1f) },
				{ TEXT("LedGreen"), FLinearColor(0.15f, 1.f, 0.3f) },
				{ TEXT("Lens"), FLinearColor(0.1f, 0.45f, 1.f) },
				{ TEXT("ScreenLeds"), FLinearColor(1.f, 0.08f, 0.06f) },
				{ TEXT("LidLeds"), FLinearColor(0.1f, 1.f, 0.75f) },
			};
			for (const TPair<const TCHAR*, FLinearColor>& Led : Leds)
			{
				if (Key == Led.Key)
				{
					Material->SetVectorParameterValue(TEXT("EmissiveColor"), Led.Value);
					Material->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.f);
				}
			}
			if (Key == TEXT("Lens"))           { LensMaterial = Material; }
			else if (Key == TEXT("LedRed"))     { LedRedMaterial = Material; }
			else if (Key == TEXT("LedYellow"))  { LedYellowMaterial = Material; }
			else if (Key == TEXT("LedGreen"))   { LedGreenMaterial = Material; }
			else if (Key == TEXT("ScreenLeds")) { ScreenLedsMaterial = Material; }
			else if (Key == TEXT("LidLeds"))    { LidLedsMaterial = Material; }
		}
		Part->SetMaterial(Slot, Material);
		Materials.Add(Material);
	}
}

bool APokedexDevice::GetPartFace(FName PartKey, FVector& OutCenter, FVector2D& OutSize) const
{
	const UStaticMeshComponent* Part = Parts.FindRef(PartKey);
	if (!Part)
	{
		return false;
	}
	// Repère du parent (boîtier ou charnière) : la face avant regarde vers -X
	const FBoxSphereBounds Bounds = Part->CalcBounds(Part->GetRelativeTransform());
	OutCenter = FVector(Bounds.Origin.X - Bounds.BoxExtent.X - PanelGap, Bounds.Origin.Y, Bounds.Origin.Z);
	OutSize = FVector2D(Bounds.BoxExtent.Y * 2.f, Bounds.BoxExtent.Z * 2.f);
	return OutSize.X > 0.1f && OutSize.Y > 0.1f;
}

UWidgetComponent* APokedexDevice::AddPanel(FName PartKey, UUserWidget* Widget, FVector2D PixelSize, bool bOpaque)
{
	UStaticMeshComponent* Part = Parts.FindRef(PartKey);
	FVector Center;
	FVector2D Size;
	if (!Part || !Widget || !GetPartFace(PartKey, Center, Size))
	{
		return nullptr;
	}
	UWidgetComponent* Panel = NewObject<UWidgetComponent>(this, FName(*(TEXT("Panel_") + PartKey.ToString())));
	Panel->SetupAttachment(Part->GetAttachParent());
	Panel->SetWidgetSpace(EWidgetSpace::World);
	Panel->SetDrawSize(PixelSize);
	Panel->SetRelativeLocation(Center);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	Panel->SetRelativeScale3D(FVector(1.f / UPokedexPanelWidget::PixelsPerCm));
	Panel->SetBlendMode(bOpaque ? EWidgetBlendMode::Opaque : EWidgetBlendMode::Transparent);
	Panel->SetBackgroundColor(bOpaque ? FLinearColor::Black : FLinearColor::Transparent);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetTickWhenOffscreen(true);
	Panel->PrimaryComponentTick.bTickEvenWhenPaused = true;
	Panel->RegisterComponent();
	Panel->SetOwnerPlayer(Controller->GetLocalPlayer());
	Panel->SetWidget(Widget);
	Panel->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	AddInstanceComponent(Panel);
	Panels.Add(Panel);
	return Panel;
}

void APokedexDevice::CreatePanels()
{
	APlayerController* PC = Controller.Get();
	FVector Center;
	FVector2D Size;

	// Écran du haut du volet
	if (GetPartFace(TEXT("LidScreen"), Center, Size))
	{
		InfoWidget = CreateWidget<UPokedexInfoWidget>(PC, UPokedexInfoWidget::StaticClass());
		InfoWidget->SetPixelSize(Size * UPokedexPanelWidget::PixelsPerCm);
		AddPanel(TEXT("LidScreen"), InfoWidget, InfoWidget->GetPixelSize(), true);
	}

	// Les 3 petits écrans
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FName Key(*FString::Printf(TEXT("MiniScreen%d"), Index));
		UPokedexMiniWidget* Mini = nullptr;
		if (GetPartFace(Key, Center, Size))
		{
			Mini = CreateWidget<UPokedexMiniWidget>(PC, UPokedexMiniWidget::StaticClass());
			Mini->SetPixelSize(Size * UPokedexPanelWidget::PixelsPerCm);
			AddPanel(Key, Mini, Mini->GetPixelSize(), true);
		}
		MiniWidgets.Add(Mini);
	}

	// Icônes des 10 boutons du volet (planche d'icônes de L'amie)
	for (int32 Index = 0; Index < MenuCount; ++Index)
	{
		const FName Key(*FString::Printf(TEXT("MenuButton%d"), Index));
		UPokedexIconWidget* Icon = nullptr;
		UWidgetComponent* Panel = nullptr;
		if (GetPartFace(Key, Center, Size))
		{
			Icon = CreateWidget<UPokedexIconWidget>(PC, UPokedexIconWidget::StaticClass());
			Icon->SetPixelSize(Size * UPokedexPanelWidget::PixelsPerCm);
			Icon->SetIcon(Model->MenuIcons.IsValidIndex(Index) ? Model->MenuIcons[Index].Get() : nullptr);
			Panel = AddPanel(Key, Icon, Icon->GetPixelSize(), false);
			if (Panel)
			{
				Pressables.Add({ Panel, Panel->GetRelativeLocation(), -1.f });
			}
		}
		MenuIcons.Add(Panel);
		MenuIconWidgets.Add(Icon);
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

	UpdateFraming();
}

void APokedexDevice::UpdateFraming()
{
	APlayerController* PC = Controller.Get();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	// Distance pour que l'appareil ouvert remplisse ScreenFill de l'écran, quel que soit le champ de vision
	int32 SizeX = 16, SizeY = 9;
	PC->GetViewportSize(SizeX, SizeY);
	FramedViewportSize = FIntPoint(SizeX, SizeY);
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

void APokedexDevice::OpenOnMenu(EPokedexMenu Menu)
{
	Open();
	PressMenuButton(int32(Menu));
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
		UpdatePanels(DeltaSeconds);
		UpdateFigure(DeltaSeconds);
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
	ModelRoot->SetRelativeLocation(FMath::Lerp(ClosedOffset, OpenOffset, FMath::SmoothStep(0.f, 1.f, LidT)));

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

	// Le cadrage suit la taille de la fenêtre (plein écran, fenêtré, redimensionné)
	if (APlayerController* PC = Controller.Get())
	{
		FIntPoint ViewportSize(0, 0);
		PC->GetViewportSize(ViewportSize.X, ViewportSize.Y);
		if (ViewportSize != FramedViewportSize)
		{
			UpdateFraming();
		}
	}

	// Pièces enfoncées ; boutons du volet qui se soulèvent sous la souris ou la sélection
	for (FPressable& Press : Pressables)
	{
		USceneComponent* Component = Press.Component.Get();
		if (!Component)
		{
			continue;
		}
		const int32 MenuIndex = MenuButtons.IndexOfByKey(Cast<UStaticMeshComponent>(Component));
		const bool bLifted = MenuIndex != INDEX_NONE && (Component == Hovered.Get() || (MenuIndex == LidSelection && bLidFocus));
		const float PreviousHover = Press.HoverAlpha;
		Press.HoverAlpha = FMath::FInterpTo(Press.HoverAlpha, bLifted ? 1.f : 0.f, DeltaSeconds, 14.f);
		if (Press.PressTime < 0.f && FMath::IsNearlyEqual(PreviousHover, Press.HoverAlpha, 1e-4f))
		{
			continue;
		}
		float Depth = 0.f;
		if (Press.PressTime >= 0.f)
		{
			const float T = (Time - Press.PressTime) / PressDuration;
			Depth = T < 0.3f ? T / 0.3f : 1.f - (T - 0.3f) / 0.7f;
			if (T >= 1.f)
			{
				Depth = 0.f;
				Press.PressTime = -1.f;
			}
		}
		Component->SetRelativeLocation(Press.BaseLocation + FVector(PressDepth * FMath::Clamp(Depth, 0.f, 1.f) - HoverLift * Press.HoverAlpha, 0.f, 0.f));
	}

	// Écrans qui s'allument, flou du décor (on l'enlève quand on tourne l'appareil, sinon ses bords seraient flous)
	const FLinearColor PanelTint(1.f, 1.f, 1.f, FMath::Clamp((OpenAlpha - 0.65f) / 0.3f, 0.f, 1.f));
	for (UWidgetComponent* Panel : Panels)
	{
		if (Panel)
		{
			Panel->SetTintColorAndOpacity(PanelTint);
		}
	}
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
	if (ScreenLedsMaterial)
	{
		// Clignotent quand l'écran change de page
		ScreenLedsMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), OpenAlpha * (0.4f + 8.f * Flash(Time, ScreenFlash, 8.f)));
	}

	float LastMenuFlash = -10.f;
	for (int32 Index = 0; Index < MenuCount; ++Index)
	{
		const bool bSelected = Index == LidSelection;
		const bool bHover = Hovered.IsValid() && MenuButtons.IsValidIndex(Index) && Hovered.Get() == MenuButtons[Index].Get();
		float Strength = 0.f;
		if (bSelected && bLidFocus)
		{
			Strength = 0.6f + 0.3f * FMath::Sin(Time * 6.f);
		}
		else if (bSelected || bHover)
		{
			Strength = 0.4f;
		}
		Strength += 3.f * Flash(Time, MenuFlash[Index], 6.f);
		LastMenuFlash = FMath::Max(LastMenuFlash, MenuFlash[Index]);
		if (MenuGlows.IsValidIndex(Index) && MenuGlows[Index])
		{
			MenuGlows[Index]->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength * OpenAlpha);
		}
		if (MenuIconWidgets.IsValidIndex(Index) && MenuIconWidgets[Index])
		{
			MenuIconWidgets[Index]->SetHighlighted(bSelected || bHover, bSelected && bLidFocus);
		}
	}
	if (LidLedsMaterial)
	{
		// Respirent tant que le volet attend un choix, s'allument quand on appuie sur un bouton du volet
		LidLedsMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"),
			OpenAlpha * ((bLidFocus ? 1.5f + FMath::Sin(Time * 3.f) : 0.5f) + 8.f * Flash(Time, LastMenuFlash, 5.f)));
	}
}

void APokedexDevice::UpdatePanels(float DeltaSeconds)
{
	PanelTimer -= DeltaSeconds;
	if (PanelTimer > 0.f || !Screen)
	{
		return;
	}
	PanelTimer = 0.2f;

	// Écran du volet : le bouton choisi tant qu'on est sur le volet, sinon ce qui est choisi dans l'écran
	if (InfoWidget)
	{
		FText Title, Text;
		int32 MenuIndex = LidSelection;
		for (int32 Index = 0; Index < MenuButtons.Num(); ++Index)
		{
			if (Hovered.IsValid() && Hovered.Get() == MenuButtons[Index].Get())
			{
				MenuIndex = Index;
			}
		}
		if (bLidFocus || Screen->IsAtHome() || MenuIndex != LidSelection)
		{
			Title = UPokedexScreenWidget::GetMenuName(EPokedexMenu(MenuIndex));
			Text = UPokedexScreenWidget::GetMenuDescription(EPokedexMenu(MenuIndex));
		}
		else
		{
			Screen->GetInfo(Title, Text);
		}
		InfoWidget->SetInfo(Title, Text);
	}

	// Petits écrans
	const APokemonPlayerState* State = Controller.IsValid() ? Controller->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (MiniWidgets.IsValidIndex(0) && MiniWidgets[0])
	{
		MiniWidgets[0]->SetValues(FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M"))),
			State ? FText::Format(LOCTEXT("MiniMoney", "{0} P$"), FText::AsNumber(State->Money)) : FText::GetEmpty());
	}
	if (MiniWidgets.IsValidIndex(1) && MiniWidgets[1])
	{
		MiniWidgets[1]->SetValues(State ? FText::FromString(FString::Printf(TEXT("%d / %d"), State->PokedexCaught.Num(), State->PokedexSeen.Num())) : FText::GetEmpty(),
			LOCTEXT("MiniDex", "CAPTURÉS / VUS"));
	}
	if (MiniWidgets.IsValidIndex(2) && MiniWidgets[2])
	{
		MiniWidgets[2]->SetValues(State ? FText::FromString(FString::Printf(TEXT("%d / 8"), State->Badges.Num())) : FText::GetEmpty(),
			LOCTEXT("MiniBadges", "BADGES"));
	}
}

void APokedexDevice::UpdateFigure(float DeltaSeconds)
{
	WantedFigureSpecies = (Screen && !bLidFocus && !bClosing) ? Screen->GetFeaturedSpecies() : NAME_None;

	// L'ancienne figurine rentre avant que la nouvelle sorte
	if (WantedFigureSpecies != FigureSpecies)
	{
		FigureAlpha = FMath::Max(0.f, FigureAlpha - DeltaSeconds * 6.f);
		if (FigureAlpha <= 0.f)
		{
			LoadFigure(WantedFigureSpecies);
		}
	}
	else if (bFigureLoaded)
	{
		FigureAlpha = FMath::Min(1.f, FigureAlpha + DeltaSeconds * 3.f);
	}

	FigureYaw = FMath::Fmod(FigureYaw + DeltaSeconds * 35.f, 360.f);
	const float Pop = EaseOutBack(FigureAlpha) * OpenAlpha;
	FigureRoot->SetRelativeRotation(FRotator(0.f, FigureYaw, 0.f));
	FigureRoot->SetRelativeScale3D(FVector(FMath::Max(Pop, 0.001f)));
	const bool bVisible = bFigureLoaded && FigureAlpha > 0.f;
	FigureSkeletal->SetVisibility(bVisible && FigureSkeletal->GetSkeletalMeshAsset() != nullptr);
	FigureStatic->SetVisibility(bVisible && FigureStatic->GetStaticMesh() != nullptr);
}

void APokedexDevice::LoadFigure(FName Species)
{
	FigureSpecies = Species;
	bFigureLoaded = false;
	FigureSkeletal->SetSkeletalMesh(nullptr);
	FigureStatic->SetStaticMesh(nullptr);
	if (Species.IsNone())
	{
		return;
	}

	// Modèles importés par le fil « modèles 3D » : <dossier>/<Espèce>/SK_<Espèce> ou SM_<Espèce>
	const FString Id = Species.ToString();
	const FString Folder = FigureFolder / Id;
	auto Load = [&Folder](const FString& Name) -> UObject*
	{
		return StaticLoadObject(UObject::StaticClass(), nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};

	FBoxSphereBounds Bounds(ForceInit);
	USceneComponent* Figure = nullptr;
	if (USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Load(TEXT("SK_") + Id)))
	{
		FigureSkeletal->SetSkeletalMesh(Skeletal);
		if (UAnimSequence* Idle = Cast<UAnimSequence>(Load(TEXT("A_") + Id + TEXT("_Idle"))))
		{
			FigureSkeletal->PlayAnimation(Idle, true);
		}
		Bounds = Skeletal->GetBounds();
		Figure = FigureSkeletal;
	}
	else if (UStaticMesh* Static = Cast<UStaticMesh>(Load(TEXT("SM_") + Id)))
	{
		FigureStatic->SetStaticMesh(Static);
		Bounds = Static->GetBounds();
		Figure = FigureStatic;
	}
	if (!Figure)
	{
		return;
	}

	// Même hauteur pour tous, posé sur le socle et centré sur l'axe de rotation
	const float Height = FMath::Max(Bounds.BoxExtent.Z * 2.f, 0.01f);
	const float Scale = FigureHeight / Height;
	Figure->SetRelativeScale3D(FVector(Scale));
	Figure->SetRelativeLocation(FVector(-Bounds.Origin.X, -Bounds.Origin.Y, Bounds.BoxExtent.Z - Bounds.Origin.Z) * Scale);
	bFigureLoaded = true;
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
	if (Key == TEXT("Screen") && Hit.GetComponent() == ScreenComponent.Get())
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
	else if (Key == TEXT("ConfirmButton") || Key == TEXT("LidButton"))
	{
		PressPart(Hit.GetComponent());
		HandleKey(EKeys::Enter);
	}
	else if (Key == TEXT("BackButton"))
	{
		PressPart(Hit.GetComponent());
		HandleKey(EKeys::Escape);
	}
	else if (Key == TEXT("HomeButton"))
	{
		PressPart(Hit.GetComponent());
		PlaySound(Model ? Model->ClickSound : nullptr);
		FocusLid();
	}
	else if (Key == TEXT("PrevButton") || Key == TEXT("NextButton"))
	{
		PressPart(Hit.GetComponent());
		CycleMenu(Key == TEXT("NextButton") ? 1 : -1);
	}
	else if (Key == TEXT("DexButton") || Key == TEXT("Lens") || Key == TEXT("LensRing"))
	{
		// L'objectif « scanne » : il s'allume et ouvre le Pokédex
		PressPart(Key == TEXT("DexButton") ? Hit.GetComponent() : Parts.FindRef(TEXT("Lens")));
		LensFlash = Time;
		PressMenuButton(int32(EPokedexMenu::Pokedex));
	}
	else if (Key == TEXT("LedRed") || Key == TEXT("LedYellow") || Key == TEXT("LedGreen") || Key == TEXT("ScreenLeds") || Key == TEXT("LidLeds"))
	{
		LedRedFlash = Time;
		LedYellowFlash = Time + 0.12f;
		LensFlash = Time + 0.24f;
		ScreenFlash = Time + 0.36f;
		PlaySound(Model ? Model->BeepSound : nullptr, 1.2f);
	}
	else if (Key == TEXT("Hinge"))
	{
		// La charnière referme l'appareil
		Close();
	}
	else
	{
		// Boîtier, volet, petits écrans : on l'attrape pour le tourner
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

	// Surbrillance des boutons du boîtier et du volet (les 10 boutons de menu, l'objectif et les voyants ont leur propre lumière)
	auto Highlight = [this](UPrimitiveComponent* Part, bool bOn)
	{
		const FName Key = GetPartKey(Part);
		if (!IsInList(Key, PushButtons) || Key == TEXT("Lens"))
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
	PanelTimer = 0.f;

	const FName Key = GetPartKey(Component);
	const bool bClickable = Component && !IsInList(Key, GripParts);
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
		CycleMenu(Key == EKeys::Gamepad_RightShoulder ? 1 : -1);
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
		PressPart(Parts.FindRef(bConfirm ? TEXT("ConfirmButton") : TEXT("BackButton")));
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
	// Grille de 5 colonnes × 2 rangées, comme sur le volet
	const int32 Rows = MenuCount / MenuColumns;
	int32 Column = LidSelection % MenuColumns;
	int32 Row = LidSelection / MenuColumns;
	Column = (Column + Direction.X + MenuColumns) % MenuColumns;
	Row = (Row + Direction.Y + Rows) % Rows;
	LidSelection = Row * MenuColumns + Column;
	PanelTimer = 0.f;
	PlayFeedback(EPokedexFeedback::Navigate);
}

void APokedexDevice::CycleMenu(int32 Step)
{
	PressMenuButton((LidSelection + Step + MenuCount) % MenuCount);
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
	if (MenuIcons.IsValidIndex(Index))
	{
		PressPart(MenuIcons[Index]);
	}
	MenuFlash[Index] = Time;
	LensFlash = Time;
	ScreenFlash = Time;
	PanelTimer = 0.f;
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
		PanelTimer = 0.f;
		PlaySound(Model->NavSound);
		break;
	case EPokedexFeedback::Confirm:
		LensFlash = Time;
		ScreenFlash = Time;
		PanelTimer = 0.f;
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
