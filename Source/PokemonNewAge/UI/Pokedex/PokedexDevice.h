#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Pokedex/PokedexScreenWidget.h"
#include "PokedexDevice.generated.h"

class APokemonNewAgePlayerController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPokedexIconWidget;
class UPokedexInfoWidget;
class UPokedexInputWidget;
class UPokedexMiniWidget;
class UPokedexPanelWidget;
class UPokedexModel;
class UPostProcessComponent;
class USkeletalMeshComponent;
class USoundBase;
class UStaticMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;

/**
 *  Le Pokédex rouge du menu en jeu (SCRUM-64) : le modèle 3D texturé de L'amie, tenu devant la caméra (sans mains).
 *  - Il sort du bas de l'écran, s'ouvre sur sa charnière, le décor se floute derrière.
 *  - Clic droit maintenu (ou clic gauche sur le boîtier, ou stick droit) : on le tourne ; il revient en place au relâché.
 *    Molette : plus près / plus loin (sur l'écran : fait défiler la liste).
 *  - Clic gauche : les 10 boutons du volet ouvrent les menus, < et > passent au menu précédent / suivant, la croix
 *    navigue, le bouton noir et le bouton jaune du volet valident, le petit bouton rouge revient en arrière,
 *    la languette jaune revient à l'accueil, la bleue (ou l'objectif) ouvre le Pokédex, la charnière ferme.
 *    Chaque pièce s'enfonce, clique, et les voyants réagissent. Le grand écran est cliquable lui aussi.
 *  - Les autres écrans : en haut du volet, le nom et la description de ce qui est choisi ; les 3 petits écrans
 *    affichent l'heure et l'argent, le Pokédex, les badges.
 *  - Clavier / manette : flèches ou ZQSD pour choisir un bouton du volet puis naviguer dans l'écran,
 *    Entrée/A pour valider, Échap/B pour revenir, Tab/Start pour fermer.
 *  Local au joueur (jamais répliqué) ; le jeu ne se met en pause qu'en solo (SCRUM-68).
 *  Pièces 3D, textures, icônes et sons : DA_PokedexModel (Tools/Pokedex/import_pokedex_unreal.py), voir docs/MENU_POKEDEX.md.
 */
UCLASS()
class APokedexDevice : public AActor
{
	GENERATED_BODY()

public:

	APokedexDevice();

	/** Construit l'appareil pour ce joueur. Faux si DA_PokedexModel est introuvable (le menu à plat prend le relais) */
	bool InitializeFor(APokemonNewAgePlayerController* InController);

	/** Animation d'ouverture */
	void Open();

	/** Animation d'ouverture, directement sur un menu (raccourci, onglet du menu classique) */
	void OpenOnMenu(EPokedexMenu Menu);

	/** Animation de fermeture, puis le contrôleur rend la main au jeu. bImmediate : sans animation */
	void Close(bool bImmediate = false);

	bool IsClosing() const { return bClosing; }

	UPokedexInputWidget* GetInputWidget() const { return InputWidget; }
	UPokedexScreenWidget* GetScreen() const { return Screen; }

	// Entrées passées par UPokedexInputWidget (positions en pixels du viewport)
	void HandlePointerDown(const FKey& Button, const FVector2D& Pixel);
	void HandlePointerUp(const FKey& Button, const FVector2D& Pixel);
	void HandlePointerMove(const FVector2D& Pixel, const FVector2D& Delta);
	void HandleWheel(float Delta);
	bool HandleKey(const FKey& Key);
	void HandleAnalog(const FKey& Key, float Value);

	virtual void Tick(float DeltaSeconds) override;

	/** Pièces, matériau et sons (créé par import_pokedex_unreal.py) */
	UPROPERTY(EditAnywhere, Category="Pokedex")
	TSoftObjectPtr<UPokedexModel> ModelAsset;

	/** Angle du volet ouvert (0 = à plat ; un peu tourné vers la caméra comme un livre) */
	UPROPERTY(EditAnywhere, Category="Pokedex", meta=(Units="Degrees"))
	float OpenLidYaw = 10.f;

	/** Marge autour de l'appareil ouvert quand on calcule la distance à la caméra (1 = bord à bord) */
	UPROPERTY(EditAnywhere, Category="Pokedex")
	float ScreenFill = 0.86f;

	/** Exposition fixe pendant le menu (EV100) : l'appareil a le même rendu de jour comme de nuit */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float MenuExposureEV100 = 7.f;

	/** Prendre l'exposition qui va avec le soleil de la map (décor visible), au lieu de Menu Exposure EV100 */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	bool bMatchWorldExposure = true;

	/** Correction de l'exposition calculée (EV) : négatif = décor et appareil plus clairs */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float WorldExposureOffset = 0.f;

	/** Luminosité des écrans et des icônes (1 = blanc pur à l'exposition du menu) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float ScreenBrightness = 0.75f;

	/** Intensité des lumières qui éclairent l'appareil (candelas) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float KeyLightIntensity = 700.f;

	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float FillLightIntensity = 250.f;

	/** Contre-jour chaud qui souligne les bords de l'appareil (soleil derrière, comme sur la maquette) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float RimLightIntensity = 450.f;

	/** Résolution des écrans et des icônes (2 = deux fois plus de pixels, texte net) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu", meta=(ClampMin="1", ClampMax="4"))
	float PanelRenderScale = 2.f;

	/** Flou du décor (ouverture de l'objectif : plus petit = plus flou) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float BackgroundBlurFstop = 1.8f;

	/** Hauteur de la figurine du Pokémon (fiche du Pokédex, résumé), en cm */
	UPROPERTY(EditAnywhere, Category="Pokedex|Figurine", meta=(Units="Centimeters"))
	float FigureHeight = 4.5f;

	/** Dossier des modèles : <dossier>/<Espèce>/SK_<Espèce> (ou SM_<Espèce>), animation A_<Espèce>_Idle */
	UPROPERTY(EditAnywhere, Category="Pokedex|Figurine")
	FString FigureFolder = TEXT("/Game/Pokemon/Models");

	/** Vitesse de rotation à la souris (degrés par pixel) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Commandes")
	float DragSensitivity = 0.35f;

	/** Rotation maximale à la souris ou au stick (degrés de chaque côté) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Commandes", meta=(Units="Degrees"))
	float MaxTurnYaw = 30.f;

	UPROPERTY(EditAnywhere, Category="Pokedex|Commandes", meta=(Units="Degrees"))
	float MaxTurnPitch = 20.f;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Une pièce qui s'enfonce quand on appuie dessus */
	struct FPressable
	{
		TWeakObjectPtr<USceneComponent> Component;
		FVector BaseLocation = FVector::ZeroVector;
		float PressTime = -1.f;
		float HoverAlpha = 0.f;
	};

	UStaticMeshComponent* AddPart(FName Key, USceneComponent* Parent);
	void ApplyMaterials(UStaticMeshComponent* Part, FName Key);
	void FixImportAxes();
	/** Taille de l'appareil et recentrage, d'après les pièces */
	void MeasureModel();
	/** Écrans et icônes posés sur les pièces (Widget Components) */
	void CreatePanels();
	UWidgetComponent* AddPanel(FName PartKey, UUserWidget* Widget, FVector2D PixelSize, bool bOpaque);
	/** Pose le widget sur le panneau, rendu PanelRenderScale fois plus fin (même taille à l'écran) */
	void SetPanelWidget(UWidgetComponent* Panel, UUserWidget* Widget, FVector2D PixelSize, float CmPerPixel);
	/** Taille en cm (largeur, hauteur) et centre de la face avant d'une pièce, dans le repère de son parent */
	bool GetPartFace(FName PartKey, FVector& OutCenter, FVector2D& OutSize) const;
	void PlaceInFrontOfCamera();

	/** Allume un voyant ou une pièce (EmissiveStrength mis à l'échelle de l'exposition du menu) */
	void SetGlow(UMaterialInstanceDynamic* Material, float Strength) const;

	/** Distance à la caméra pour que l'appareil ouvert remplisse ScreenFill de la fenêtre (refait si elle change de taille) */
	void UpdateFraming();

	/** EV100 qui affiche la map correctement d'après la lumière de son soleil */
	float EstimateWorldExposure() const;
	void UpdateAnimation(float DeltaSeconds);
	void UpdateLights(float DeltaSeconds);
	void UpdatePanels(float DeltaSeconds);
	/** Figurine 3D du Pokémon choisi, debout sur le haut du volet */
	void UpdateFigure(float DeltaSeconds);
	void LoadFigure(FName Species);

	/** Trace sous la souris, seulement contre les pièces de l'appareil */
	bool TraceParts(const FVector2D& Pixel, FHitResult& OutHit) const;
	FName GetPartKey(const UPrimitiveComponent* Component) const;

	void PressPart(USceneComponent* Component);
	void PressMenuButton(int32 Index);
	void ClickDPad(const FVector& WorldHit);
	void MoveLidSelection(FIntPoint Direction);
	/** Bouton < ou > : menu précédent / suivant */
	void CycleMenu(int32 Step);
	void SendToScreen(const FKey& Key);
	void FocusLid();
	void SetHovered(UPrimitiveComponent* Component);
	void PlayFeedback(EPokedexFeedback Type);
	void PlaySound(USoundBase* Sound, float Pitch = 1.f) const;
	void OnScreenFeedback(EPokedexFeedback Type);

	static bool IsBackKey(const FKey& Key);
	static bool IsConfirmKey(const FKey& Key);
	static FIntPoint GetDirection(const FKey& Key);

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<USceneComponent> Root;

	/** Rotation donnée par le joueur */
	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<USceneComponent> Pivot;

	/** Recentre l'appareil fermé (le boîtier seul) puis ouvert (les deux moitiés) */
	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<USceneComponent> ModelRoot;

	/** Axe de la charnière : le volet et ses boutons y sont accrochés */
	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UWidgetComponent> ScreenComponent;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UWidgetInteractionComponent> ScreenInteraction;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UPointLightComponent> RimLight;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UPostProcessComponent> PostProcess;

	/** Figurine : socle (tourne), et le modèle animé ou fixe */
	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<USceneComponent> FigureRoot;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<USkeletalMeshComponent> FigureSkeletal;

	UPROPERTY(VisibleAnywhere, Category="Pokedex")
	TObjectPtr<UStaticMeshComponent> FigureStatic;

	UPROPERTY(Transient)
	TObjectPtr<UPokedexModel> Model;

	UPROPERTY(Transient)
	TObjectPtr<UPokedexScreenWidget> Screen;

	UPROPERTY(Transient)
	TObjectPtr<UPokedexInputWidget> InputWidget;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MenuButtons;

	/** Icônes posées sur les 10 boutons du volet */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> MenuIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPokedexIconWidget>> MenuIconWidgets;

	/** Icône appareil photo sur le bouton rond noir */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> CameraIconPanel;

	/** Écran du haut du volet */
	UPROPERTY(Transient)
	TObjectPtr<UPokedexInfoWidget> InfoWidget;

	/** Petits écrans : 0 sous l'écran (heure, argent), 1 et 2 sur le volet (Pokédex, badges) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPokedexMiniWidget>> MiniWidgets;

	/** Écrans à allumer avec l'appareil (fondu d'ouverture) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> Panels;

	/** Matériaux animés : chaque bouton du volet */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MenuGlows;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMaterial;

	/** M_PokedexWidget (écrans et icônes) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WidgetMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedRedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedYellowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedGreenMaterial;

	/** Deux petits voyants rouges au-dessus de l'écran */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenLedsMaterial;

	/** Deux voyants verts du volet */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LidLedsMaterial;

	/** Tous les matériaux, pour la surbrillance au survol */
	TMap<TWeakObjectPtr<UPrimitiveComponent>, TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>> PartMaterials;

	TArray<FPressable> Pressables;

	TWeakObjectPtr<APokemonNewAgePlayerController> Controller;
	TWeakObjectPtr<UPrimitiveComponent> Hovered;

	// État
	float Time = 0.f;
	float OpenAlpha = 0.f;
	bool bClosing = false;
	bool bLidFocus = true;
	int32 LidSelection = 0;

	bool bDragging = false;
	bool bScreenPointerDown = false;
	FVector2D LastPointer = FVector2D::ZeroVector;
	float UserYaw = 0.f;
	float UserPitch = 0.f;
	float StickYaw = 0.f;
	float StickPitch = 0.f;
	float ZoomOffset = 0.f;
	float LastInteractTime = 0.f;

	// Placement calculé à l'ouverture
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	float ViewDistance = 32.f;
	float GlowScale = 1.f;
	float ExposureEV100 = 7.f;
	float WidgetBrightness = 1.f;
	FIntPoint FramedViewportSize = FIntPoint::ZeroValue;

	// Taille du modèle (MeasureModel)
	float HalfWidth = 9.3f;
	float HalfHeight = 7.6f;
	float HalfDepth = 1.5f;
	/** Décalage pour garder l'appareil au centre : fermé (boîtier seul) et ouvert */
	FVector ClosedOffset = FVector(0.f, 4.3f, 0.f);
	FVector OpenOffset = FVector::ZeroVector;

	// Voyants : moment du dernier éclair
	float LedRedFlash = -10.f;
	float LedYellowFlash = -10.f;
	float LensFlash = -10.f;
	float ScreenFlash = -10.f;
	float MenuFlash[PokedexMenuCount] = { -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f };

	float PanelTimer = 0.f;

	/** Espèce affichée en figurine, et son apparition (0 à 1) */
	FName FigureSpecies;
	FName WantedFigureSpecies;
	float FigureAlpha = 0.f;
	float FigureYaw = 0.f;
	bool bFigureLoaded = false;

	static constexpr int32 MenuCount = PokedexMenuCount;
	static constexpr int32 MenuColumns = 5;
	static constexpr float PressDuration = 0.2f;
	static constexpr float PressDepth = 0.08f;
	/** Les boutons du volet se soulèvent un peu sous la souris (cm) */
	static constexpr float HoverLift = 0.05f;
	static constexpr float OpenDuration = 0.95f;
	static constexpr float CloseDuration = 0.6f;
};
