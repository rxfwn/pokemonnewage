#include "UI/Pokedex/PokedexDevice.h"
#include "UI/Pokedex/PokedexModel.h"
#include "UI/Pokedex/PokedexWidgets.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Core/GameClock.h"
#include "Core/PokemonPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"
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
	const TCHAR* PushButtons[] = { TEXT("ConfirmButton"), TEXT("DPad"), TEXT("PrevButton"), TEXT("NextButton") };


	/** Couleur des boutons du volet (sous les icônes), allumés quand on les choisit */
	const FLinearColor TileColor(0.62f, 0.86f, 0.93f);
	const FLinearColor TileGlow(0.35f, 0.85f, 1.f);
	/** Fond des icônes du volet (la touche blanc cassé sous l'icône) */
	const FLinearColor IconTileColor(0.8f, 0.84f, 0.86f);
	/** Part de la touche couverte par le panneau de l'icône */
	constexpr float IconPanelFill = 0.8f;

	/** Petit écart devant une face pour que l'interface ne se mélange pas à la vitre (cm) */
	// Écart entre une pièce et l'écran posé dessus : trop petit, l'écran et la pièce se « battent » (texte doublé, icônes pâles)
	constexpr float PanelGap = 0.06f;

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

	/** Luminance qui s'affiche en blanc moyen à cette exposition (EV100) : les émissifs et lumières s'y règlent */
	float ExposureUnit(float EV100) { return 1.2f * FMath::Pow(2.f, EV100); }

	/** Matériau des écrans et icônes, créé par import_pokedex_unreal.py (paramètre Brightness) */
	const TCHAR* WidgetMaterialPath = TEXT("/Game/UI/Pokedex/M_PokedexWidget.M_PokedexWidget");
	const TCHAR* LensMaterialPath = TEXT("/Game/UI/Pokedex/M_PokedexLens.M_PokedexLens");
	const TCHAR* CameraIconPath = TEXT("/Game/UI/Pokedex/Icons/T_PokedexButton_Camera.T_PokedexButton_Camera");
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
	ScreenComponent->SetCastShadow(false);
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
	KeyLight->SetSourceRadius(14.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.94f, 0.84f));
	KeyLight->LightingChannels.bChannel0 = false;
	KeyLight->LightingChannels.bChannel1 = true;
	KeyLight->SetCastShadows(true);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetRelativeLocation(FVector(-20.f, 22.f, -4.f));
	FillLight->SetIntensityUnits(ELightUnits::Candelas);
	FillLight->SetAttenuationRadius(400.f);
	FillLight->SetSourceRadius(10.f);
	FillLight->SetLightColor(FLinearColor(0.78f, 0.88f, 1.f));
	FillLight->LightingChannels.bChannel0 = false;
	FillLight->LightingChannels.bChannel1 = true;
	FillLight->SetCastShadows(false);

	// Contre-jour : soleil chaud derrière l'appareil, en haut à droite (liseré lumineux sur les bords)
	RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RimLight"));
	RimLight->SetupAttachment(Root);
	RimLight->SetRelativeLocation(FVector(22.f, 20.f, 24.f));
	RimLight->SetIntensityUnits(ELightUnits::Candelas);
	RimLight->SetAttenuationRadius(400.f);
	RimLight->SetSourceRadius(8.f);
	RimLight->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));
	RimLight->LightingChannels.bChannel0 = false;
	RimLight->LightingChannels.bChannel1 = true;
	RimLight->SetCastShadows(false);

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
	WidgetMaterial = LoadObject<UMaterialInterface>(nullptr, WidgetMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!WidgetMaterial)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : M_PokedexWidget absent, ecrans sombres (rouvrir Unreal pour relancer l'import)"));
	}
	if (WidgetMaterial)
	{
		// Déjà enregistré : on le réenregistre pour qu'il recrée son matériau à partir de M_PokedexWidget
		ScreenComponent->UnregisterComponent();
		ScreenComponent->SetMaterial(0, WidgetMaterial);
		ScreenComponent->RegisterComponent();
	}
#if WITH_EDITOR
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 10.f, FColor::Orange,
			TEXT("Pokedex 3D : M_PokedexWidget absent, ecrans noirs. Fermer et rouvrir Unreal, ou lancer Tools/Pokedex/import_pokedex_unreal.py"));
	}
#endif
	ScreenComponent->SetOwnerPlayer(InController->GetLocalPlayer());
	const FVector2D ScreenPixels(UPokedexScreenWidget::ScreenWidth, UPokedexScreenWidget::ScreenHeight);
	float ScreenCmPerPixel = 1.f / UPokedexPanelWidget::PixelsPerCm;
	FVector ScreenCenter;
	FVector2D ScreenSize;
	if (GetPartFace(TEXT("Screen"), ScreenCenter, ScreenSize))
	{
		// Toute la vitre, à la même échelle en largeur et en hauteur
		ScreenComponent->SetRelativeLocation(ScreenCenter);
		ScreenCmPerPixel = FMath::Min(ScreenSize.X / ScreenPixels.X, ScreenSize.Y / ScreenPixels.Y);
	}
	SetPanelWidget(ScreenComponent, Screen, ScreenPixels, ScreenCmPerPixel);
	ScreenComponent->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
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

	// Exposition du menu : celle que demande la lumière du soleil de la map (le décor flouté reste visible), sinon le réglage
	ExposureEV100 = bMatchWorldExposure ? EstimateWorldExposure() : MenuExposureEV100;
	UE_LOG(LogPokemonNewAge, Log, TEXT("Pokedex 3D : exposition du menu EV100 %.1f"), ExposureEV100);

	// Lumières et rendu du menu
	// Intensités réglées pour EV100 11 : on les suit quand l'exposition du menu change, pour garder le même rendu
	const float LightScale = FMath::Square(GetActorScale3D().X) * FMath::Pow(2.f, ExposureEV100 - 11.f);
	KeyLight->SetIntensity(KeyLightIntensity * LightScale);
	FillLight->SetIntensity(FillLightIntensity * LightScale);
	RimLight->SetIntensity(RimLightIntensity * LightScale);
	// Reflets doux sur le plastique (lumière de jour diffuse, comme sur la maquette)
	KeyLight->SetSpecularScale(0.45f);
	FillLight->SetSpecularScale(0.15f);
	RimLight->SetSpecularScale(0.6f);
	GlowScale = 0.6f * ExposureUnit(ExposureEV100);
	if (LensMaterial)
	{
		// Lumière propre de la texture de l'objectif, à l'échelle de l'exposition du menu
		LensMaterial->SetScalarParameterValue(TEXT("TextureGlow"), LensTextureGlow * GlowScale);
	}
	WidgetBrightness = ScreenBrightness * ExposureUnit(ExposureEV100);
	FPostProcessSettings& Settings = PostProcess->Settings;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = ViewDistance * GetActorScale3D().X;
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = BackgroundBlurFstop;
	// Histogramme bloqué sur une seule valeur, même si un volume de la map est en exposition manuelle
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = ExposureEV100;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMaxBrightness = ExposureEV100;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = 0.f;
	// Le décor est seulement flouté : ni plus sombre ni plus saturé (demandé par L'amie)
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.f;
	// Pas de flou de mouvement : si la caméra tournait quand on a appuyé sur Tab, l'appareil (qui la suit) était tout étalé
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = 0.f;
	// Bloom faible : sinon chaque lettre claire des écrans a un halo et le texte paraît flou
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = MenuBloom;

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
	HalfDepth = OpenBox.GetExtent().X;
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
		// Voyants et objectif : matériau uni (verre / plastique translucide) plutôt que la texture, plus réaliste
		static const TCHAR* GlassParts[] = { TEXT("Lens"), TEXT("LedRed"), TEXT("LedYellow"), TEXT("LedGreen"), TEXT("ScreenLeds"), TEXT("LidLeds") };
		const bool bGlass = IsInList(Key, GlassParts);
		UMaterialInterface* Parent = ((MenuIndex >= 0 || bGlass) && Model->Material) ? Model->Material.Get() : Part->GetMaterial(Slot);
		// Objectif : vrai verre d'appareil photo (M_PokedexLens, texture projetée de face), s'il est importé
		UMaterialInterface* LensParent = Key == TEXT("Lens")
			? LoadObject<UMaterialInterface>(nullptr, LensMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		if (LensParent)
		{
			Parent = LensParent;
		}
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
			SetGlow(Material, 0.f);
			MenuGlows[MenuIndex] = Material;
		}
		else
		{
			// Voyants : petites bulles de plastique teinté, brillantes ; objectif : verre bleu nuit d'appareil photo
			struct FGlass { const TCHAR* Key; FLinearColor Base; FLinearColor Glow; float Roughness; float Metallic; };
			const FGlass Glasses[] = {
				{ TEXT("LedRed"),     FLinearColor(0.32f, 0.012f, 0.008f), FLinearColor(1.f, 0.08f, 0.04f), 0.12f, 0.f },
				{ TEXT("LedYellow"),  FLinearColor(0.36f, 0.2f, 0.012f),  FLinearColor(1.f, 0.62f, 0.1f),  0.12f, 0.f },
				{ TEXT("LedGreen"),   FLinearColor(0.015f, 0.03f, 0.02f), FLinearColor(0.2f, 1.f, 0.35f),  0.12f, 0.f },
				{ TEXT("Lens"),       FLinearColor(0.006f, 0.012f, 0.03f), FLinearColor(0.15f, 0.4f, 1.f), 0.04f, 0.35f },
				{ TEXT("ScreenLeds"), FLinearColor(0.3f, 0.012f, 0.008f), FLinearColor(1.f, 0.08f, 0.05f), 0.15f, 0.f },
				{ TEXT("LidLeds"),    FLinearColor(0.02f, 0.12f, 0.1f),   FLinearColor(0.15f, 1.f, 0.8f),  0.15f, 0.f },
			};
			for (const FGlass& Glass : Glasses)
			{
				if (Key == Glass.Key)
				{
					Material->SetVectorParameterValue(TEXT("BaseColor"), Glass.Base);
					Material->SetScalarParameterValue(TEXT("Roughness"), Glass.Roughness);
					Material->SetScalarParameterValue(TEXT("Metallic"), Glass.Metallic);
					Material->SetVectorParameterValue(TEXT("EmissiveColor"), Glass.Glow);
					SetGlow(Material, 0.f);
				}
			}
			if (!bGlass)
			{
				// Plastique du boîtier : moins rouge vif et moins brillant (image de référence de L'amie)
				Material->SetVectorParameterValue(TEXT("Tint"), BodyTint);
				Material->SetScalarParameterValue(TEXT("Desaturation"), BodyDesaturation);
				Material->SetScalarParameterValue(TEXT("RoughnessBoost"), BodyRoughnessBoost);
			}
			if (LensParent)
			{
				// Centre et diamètre de l'objectif dans le repère de la pièce (sommets en coordonnées de l'appareil)
				if (const UStaticMesh* Mesh = Part->GetStaticMesh())
				{
					const FBoxSphereBounds Bounds = Mesh->GetBounds();
					Material->SetVectorParameterValue(TEXT("LensCenter"), FLinearColor(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z));
					Material->SetScalarParameterValue(TEXT("LensSize"), 2.f * FMath::Max(Bounds.BoxExtent.Y, Bounds.BoxExtent.Z));
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

UWidgetComponent* APokedexDevice::AddPanel(FName PartKey, UUserWidget* Widget, FVector2D PixelSize, bool bOpaque, FLinearColor Background)
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
	Panel->SetRelativeLocation(Center);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	Panel->SetBlendMode(bOpaque ? EWidgetBlendMode::Opaque : EWidgetBlendMode::Transparent);
	Panel->SetBackgroundColor(bOpaque ? Background : FLinearColor::Transparent);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Pas d'ombre de l'écran sur sa pièce (copie sombre du texte décalée derrière)
	Panel->SetCastShadow(false);
	Panel->SetTickWhenOffscreen(true);
	Panel->PrimaryComponentTick.bTickEvenWhenPaused = true;
	if (WidgetMaterial)
	{
		Panel->SetMaterial(0, WidgetMaterial);
	}
	Panel->RegisterComponent();
	Panel->SetOwnerPlayer(Controller->GetLocalPlayer());
	SetPanelWidget(Panel, Widget, PixelSize, 1.f / UPokedexPanelWidget::PixelsPerCm);
	Panel->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	AddInstanceComponent(Panel);
	Panels.Add(Panel);
	return Panel;
}

void APokedexDevice::SetPanelWidget(UWidgetComponent* Panel, UUserWidget* Widget, FVector2D PixelSize, float CmPerPixel)
{
	// Le widget garde sa mise en page (PixelSize) mais il est dessiné dans une texture RenderScale fois plus grande,
	// et le panneau est réduit d'autant : même taille sur l'appareil, texte et icônes bien plus nets
	const float RenderScale = FMath::Clamp(PanelRenderScale, 1.f, 4.f);
	Panel->SetDrawSize(PixelSize * RenderScale);
	Panel->SetRelativeScale3D(FVector(CmPerPixel / RenderScale));
	if (FMath::IsNearlyEqual(RenderScale, 1.f))
	{
		Panel->SetWidget(Widget);
		return;
	}
	Panel->SetSlateWidget(SNew(SDPIScaler).DPIScale(RenderScale)[Widget->TakeWidget()]);
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
			// Un peu plus petit que la touche : ses bords arrondis restent visibles autour de l'icône
			Icon->SetPixelSize(Size * UPokedexPanelWidget::PixelsPerCm * IconPanelFill);
			// L'appareil photo n'a pas de touche sur le volet : seulement le bouton rond noir (demandé par L'amie)
			Icon->SetIcon(Index != int32(EPokedexMenu::Camera) && Model->MenuIcons.IsValidIndex(Index) ? Model->MenuIcons[Index].Get() : nullptr);
			// Panneau opaque couleur de touche : les icônes de L'amie sont dessinées dessus dans l'écran lui-même,
			// sans transparence (en transparent, leurs couleurs sombres disparaissaient : icônes pâles, globe invisible)
			Panel = AddPanel(Key, Icon, Icon->GetPixelSize(), true, IconTileColor);
			if (Panel)
			{
				Pressables.Add({ Panel, Panel->GetRelativeLocation(), -1.f });
			}
		}
		MenuIcons.Add(Panel);
		MenuIconWidgets.Add(Icon);
	}

	// Petit appareil photo blanc sur le bouton rond noir (icône de L'amie) : il s'enfonce avec le bouton
	if (UTexture2D* CameraIcon = LoadObject<UTexture2D>(nullptr, CameraIconPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		if (GetPartFace(TEXT("ConfirmButton"), Center, Size))
		{
			UPokedexIconWidget* Icon = CreateWidget<UPokedexIconWidget>(PC, UPokedexIconWidget::StaticClass());
			Icon->SetPixelSize(Size * UPokedexPanelWidget::PixelsPerCm * 0.6f);
			Icon->SetIcon(CameraIcon);
			if (UWidgetComponent* Panel = AddPanel(TEXT("ConfirmButton"), Icon, Icon->GetPixelSize(), false))
			{
				Panel->SetRelativeLocation(Panel->GetRelativeLocation() - FVector(0.02f, 0.f, 0.f));
				Pressables.Add({ Panel, Panel->GetRelativeLocation(), -1.f });
				CameraIconPanel = Panel;
			}
		}
	}
	else
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Pokedex 3D : icone appareil photo absente (lancer Tools/Pokedex/import_pokedex_unreal.py)"));
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
	// + la demi-épaisseur : c'est la face avant (plus proche) qui doit tenir dans l'écran
	ViewDistance = FMath::Max(HalfWidth / (TanH * ScreenFill), HalfHeight / (TanV * ScreenFill)) + HalfDepth;
}

// ---------------------------------------------------------------- Ouverture et fermeture

void APokedexDevice::SetMenuAntiAliasing(bool bMenu)
{
	// L'anticrénelage temporel (TSR / TAA) floute le texte des écrans (testé par L'amie : ShowFlag.AntiAliasing 0 = net).
	// Pendant le menu on passe en FXAA (le décor est flouté de toute façon), puis on remet le réglage du jeu.
	IConsoleVariable* Method = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
	if (!Method || !bUseFxaaInMenu)
	{
		return;
	}
	if (bMenu && SavedAntiAliasing < 0)
	{
		SavedAntiAliasing = Method->GetInt();
		Method->Set(1, ECVF_SetByCode);
	}
	else if (!bMenu && SavedAntiAliasing >= 0)
	{
		Method->Set(SavedAntiAliasing, ECVF_SetByCode);
		SavedAntiAliasing = -1;
	}
}

void APokedexDevice::Open()
{
	SetMenuAntiAliasing(true);
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
	SetMenuAntiAliasing(false);
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
				SetMenuAntiAliasing(false);
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

	// Rotation par le joueur : seulement clic droit maintenu (ou stick droit) ; il reste où on l'a laissé, sans bouger tout seul
	UserYaw += StickYaw * 140.f * DeltaSeconds;
	UserPitch += StickPitch * 100.f * DeltaSeconds;
	// Relâché : il revient doucement à sa place (demandé par L'amie)
	if (!bDragging && FMath::IsNearlyZero(StickYaw) && FMath::IsNearlyZero(StickPitch))
	{
		UserYaw = FMath::FInterpTo(UserYaw, 0.f, DeltaSeconds, ReturnSpeed);
		UserPitch = FMath::FInterpTo(UserPitch, 0.f, DeltaSeconds, ReturnSpeed);
	}
	UserYaw = FMath::Clamp(FRotator::NormalizeAxis(UserYaw), -MaxTurnYaw, MaxTurnYaw);
	UserPitch = FMath::Clamp(FRotator::NormalizeAxis(UserPitch), -MaxTurnPitch, MaxTurnPitch);
	// Rotation affichée qui rejoint la souris en douceur, et freine en approchant de la limite (pas d'arrêt sec)
	auto SoftLimit = [](float Angle, float Limit)
	{
		return Limit > 0.f ? Limit * FMath::Sin(FMath::Clamp(Angle / Limit, -1.f, 1.f) * HALF_PI) : 0.f;
	};
	ShownYaw = FMath::FInterpTo(ShownYaw, SoftLimit(UserYaw, MaxTurnYaw), DeltaSeconds, RotationSmoothing);
	ShownPitch = FMath::FInterpTo(ShownPitch, SoftLimit(UserPitch, MaxTurnPitch), DeltaSeconds, RotationSmoothing);
	Pivot->SetRelativeRotation(FRotator(ShownPitch, ShownYaw, 0.f));

	// Suit la caméra à chaque image : si elle tournait encore quand on a appuyé sur Tab (souris en mouvement,
	// retard du bras de caméra), l'appareil finit quand même bien en face
	if (APlayerController* CameraPC = Controller.Get())
	{
		if (CameraPC->PlayerCameraManager)
		{
			CameraLocation = CameraPC->PlayerCameraManager->GetCameraLocation();
			CameraRotation = CameraPC->PlayerCameraManager->GetCameraRotation();
		}
	}

	// Devant la caméra, tourné vers elle (il ne recule pas quand on le tourne : la rotation est limitée à quelques degrés)
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
		// Le bouton et son icône (posée juste devant) montent ensemble, sinon le bouton passe devant l'icône
		int32 MenuIndex = MenuButtons.IndexOfByKey(Cast<UStaticMeshComponent>(Component));
		if (MenuIndex == INDEX_NONE)
		{
			MenuIndex = MenuIcons.IndexOfByKey(Cast<UWidgetComponent>(Component));
		}
		const bool bButtonHovered = MenuIndex != INDEX_NONE && MenuIndex != int32(EPokedexMenu::Camera)
			&& MenuButtons.IsValidIndex(MenuIndex) && Hovered.Get() == MenuButtons[MenuIndex].Get();
		// Les autres boutons (croix, < >, appareil photo et son icône) se soulèvent aussi sous la souris
		UStaticMeshComponent* ConfirmPart = Parts.FindRef(TEXT("ConfirmButton"));
		const bool bOtherHovered = MenuIndex == INDEX_NONE && Hovered.IsValid()
			&& (Hovered.Get() == Component || (Component == CameraIconPanel.Get() && Hovered.Get() == ConfirmPart));
		const bool bLifted = bOtherHovered || (MenuIndex != INDEX_NONE && (bButtonHovered || (MenuIndex == LidSelection && bLidFocus)));
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
		// Survol : le bouton s'enfonce un peu (demandé par L'amie), le clic l'enfonce encore
		Component->SetRelativeLocation(Press.BaseLocation + FVector(FMath::Max(PressDepth * FMath::Clamp(Depth, 0.f, 1.f), HoverLift * Press.HoverAlpha), 0.f, 0.f));
	}

	// Écrans qui s'allument, flou du décor (on l'enlève quand on tourne l'appareil, sinon ses bords seraient flous)
	const FLinearColor PanelTint(1.f, 1.f, 1.f, FMath::Clamp((OpenAlpha - 0.65f) / 0.3f, 0.f, 1.f));
	for (UWidgetComponent* Panel : Panels)
	{
		if (Panel)
		{
			Panel->SetTintColorAndOpacity(PanelTint);
			if (UMaterialInstanceDynamic* PanelMaterial = Panel->GetMaterialInstance())
			{
				PanelMaterial->SetScalarParameterValue(TEXT("Brightness"), WidgetBrightness);
				// Panneau opaque : opacité 1 partout, même au bord des lettres (sinon texte « doublé » sur l'écran du volet)
				PanelMaterial->SetScalarParameterValue(TEXT("OpacityFromTexture"), Panel->GetBlendMode() == EWidgetBlendMode::Opaque ? 0.f : 1.f);
			}
		}
	}
	const float Turned = FMath::Clamp(FMath::Max(FMath::Abs(FRotator::NormalizeAxis(UserYaw)), FMath::Abs(FRotator::NormalizeAxis(UserPitch))) / 25.f, 0.f, 1.f);
	// L'exposition reste celle du menu même tourné ; seul le flou s'en va (sinon les bords de l'appareil seraient flous)
	PostProcess->BlendWeight = OpenAlpha;
	PostProcess->Settings.DepthOfFieldFocalDistance = Distance;
	PostProcess->Settings.DepthOfFieldFstop = FMath::Lerp(BackgroundBlurFstop, 32.f, Turned);
}

void APokedexDevice::UpdateLights(float DeltaSeconds)
{
	const float Booting = OpenAlpha < 1.f && !bClosing ? 1.f : 0.f;
	if (LedGreenMaterial)
	{
		SetGlow(LedGreenMaterial, LedGlow * ((1.f - Booting) * (0.8f + 0.4f * FMath::Sin(Time * 2.4f)) + 4.f * Flash(Time, LensFlash, 3.f)));
	}
	if (LedYellowMaterial)
	{
		SetGlow(LedYellowMaterial, LedGlow * (OpenAlpha * 0.8f + 6.f * Flash(Time, LedYellowFlash, 10.f)));
	}
	if (LedRedMaterial)
	{
		SetGlow(LedRedMaterial, LedGlow * (OpenAlpha * 0.8f + 6.f * Flash(Time, LedRedFlash, 6.f) + (bClosing ? 2.f : 0.f)));
	}
	if (LensMaterial)
	{
		SetGlow(LensMaterial, OpenAlpha * 0.08f + 3.f * Flash(Time, LensFlash, 4.f));
	}
	if (ScreenLedsMaterial)
	{
		// Clignotent quand l'écran change de page
		SetGlow(ScreenLedsMaterial, OpenAlpha * (0.4f + 8.f * Flash(Time, ScreenFlash, 8.f)));
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
		else if (bSelected)
		{
			Strength = 0.25f;
		}
		Strength += 3.f * Flash(Time, MenuFlash[Index], 6.f);
		LastMenuFlash = FMath::Max(LastMenuFlash, MenuFlash[Index]);
		if (MenuGlows.IsValidIndex(Index) && MenuGlows[Index])
		{
			SetGlow(MenuGlows[Index], Strength * OpenAlpha);
		}
		if (MenuIconWidgets.IsValidIndex(Index) && MenuIconWidgets[Index])
		{
			// Survol : l'icône grossit un peu avec son bouton qui se soulève, sans halo (demandé par L'amie)
			MenuIconWidgets[Index]->SetHighlighted(bSelected || bHover, false);
		}
	}
	if (LidLedsMaterial)
	{
		// Respirent tant que le volet attend un choix, s'allument quand on appuie sur un bouton du volet
		SetGlow(LidLedsMaterial,
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
		MiniWidgets[0]->SetValues(PokemonClock::FormatTime(GetWorld()),
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

	// Clic droit maintenu = tourner l'appareil (le clic gauche ne le fait plus tourner)
	if (Button == EKeys::RightMouseButton)
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
		// Clic gauche hors du Pokédex : il se range (demandé par L'amie)
		Close();
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
		const int32 TileIndex = FCString::Atoi(*KeyString.RightChop(10));
		if (TileIndex != int32(EPokedexMenu::Camera))
		{
			PressMenuButton(TileIndex);
		}
	}
	else if (Key == TEXT("DPad"))
	{
		ClickDPad(Hit.ImpactPoint);
	}
	else if (Key == TEXT("ConfirmButton"))
	{
		// Le bouton rond noir sous l'écran : l'appareil photo (demandé par L'amie)
		PressPart(Hit.GetComponent());
		LensFlash = Time;
		PressMenuButton(int32(EPokedexMenu::Camera));
	}
	else if (Key == TEXT("PrevButton") || Key == TEXT("NextButton"))
	{
		PressPart(Hit.GetComponent());
		CycleMenu(Key == TEXT("NextButton") ? 1 : -1);
	}
	// Le reste (objectif, voyants, languettes jaune et bleue, petit bouton rouge, bouton jaune du volet,
	// petits écrans, boîtier, charnière) est du décor : pas cliquable (demandé par L'amie)
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
	// On ne peut que l'éloigner : rapproché, il sortirait de l'écran
	ZoomOffset = FMath::Clamp(ZoomOffset - Delta * 1.5f, 0.f, ViewDistance * 0.6f);
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
		// Plus de surbrillance au survol : les boutons se soulèvent à la place (demandé par L'amie)
		if (bOn || !IsInList(Key, PushButtons))
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
					SetGlow(MID, bOn ? 0.25f : 0.f);
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
	// Main seulement sur ce qu'on peut cliquer : écran, 10 boutons, flèches du volet, croix, bouton appareil photo
	const bool bMenuTile = Key.ToString().StartsWith(TEXT("MenuButton")) && FCString::Atoi(*Key.ToString().RightChop(10)) != int32(EPokedexMenu::Camera);
	const bool bClickable = Component && (Component == ScreenComponent.Get() || bMenuTile || IsInList(Key, PushButtons));
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
	if (LidSelection == int32(EPokedexMenu::Camera))
	{
		// Touche vide (l'appareil photo est sur le bouton rond noir) : on la saute
		Column = (Column + (Direction.X != 0 ? Direction.X : 1) + MenuColumns) % MenuColumns;
		LidSelection = Row * MenuColumns + Column;
	}
	PanelTimer = 0.f;
	PlayFeedback(EPokedexFeedback::Navigate);
}

void APokedexDevice::CycleMenu(int32 Step)
{
	int32 Next = (LidSelection + Step + MenuCount) % MenuCount;
	if (Next == int32(EPokedexMenu::Camera))
	{
		Next = (Next + Step + MenuCount) % MenuCount;
	}
	PressMenuButton(Next);
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
	// Objectif et voyants ne clignotent pas quand on change de menu (demandé par L'amie)
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
		// L'icône appareil photo s'enfonce avec son bouton
		const bool bCameraIcon = Component && Component == Parts.FindRef(TEXT("ConfirmButton")).Get() && Press.Component.Get() == CameraIconPanel.Get();
		if (Press.Component.Get() == Component || bCameraIcon)
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
		PanelTimer = 0.f;
		PlaySound(Model->NavSound);
		break;
	case EPokedexFeedback::Confirm:
		PanelTimer = 0.f;
		PlaySound(Model->BeepSound);
		break;
	case EPokedexFeedback::Back:
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

void APokedexDevice::SetGlow(UMaterialInstanceDynamic* Material, float Strength) const
{
	if (Material)
	{
		// Strength 1 ≈ une lueur nette, quelle que soit l'exposition du menu
		Material->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength * GlowScale);
	}
}

float APokedexDevice::EstimateWorldExposure() const
{
	// Soleil le plus fort de la map (lux) -> luminance d'un sol moyen (18 %) -> EV100 qui l'affiche en gris moyen
	float Lux = 0.f;
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		const UDirectionalLightComponent* Sun = *It;
		if (Sun->GetWorld() == GetWorld() && Sun->IsRegistered() && Sun->IsVisible() && Sun->LightingChannels.bChannel0)
		{
			Lux = FMath::Max(Lux, Sun->Intensity);
		}
	}
	if (Lux <= 0.f)
	{
		return MenuExposureEV100;
	}
	const float Luminance = Lux * 0.18f / PI;
	return FMath::Clamp(FMath::Log2(Luminance * 100.f / 12.5f) + WorldExposureOffset, -2.f, 16.f);
}
