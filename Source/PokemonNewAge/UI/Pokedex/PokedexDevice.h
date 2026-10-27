#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Pokedex/PokedexScreenWidget.h"
#include "PokedexDevice.generated.h"

class APokemonNewAgePlayerController;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPokedexInputWidget;
class UPokedexLabelWidget;
class UPokedexModel;
class UPostProcessComponent;
class USoundBase;
class UStaticMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;

/**
 *  Le Pokédex rouge du menu en jeu (SCRUM-64) : un vrai objet 3D tenu devant la caméra.
 *  - Il sort du bas de l'écran, s'ouvre sur sa charnière, le décor se floute derrière.
 *  - Clic droit maintenu (ou clic gauche dans le vide, ou stick droit) : on le tourne ; il revient en place au relâché.
 *    Molette : plus près / plus loin.
 *  - Clic gauche sur une pièce : les 6 boutons du volet ouvrent les menus, la croix navigue, le bouton rond valide,
 *    le bouton rouge revient en arrière, le bleu revient à l'accueil, l'objectif ouvre le Pokédex, le volet le ferme.
 *    Chaque pièce s'enfonce, clique, et les voyants réagissent. L'écran est cliquable lui aussi.
 *  - Clavier / manette : flèches ou ZQSD pour choisir un bouton du volet puis naviguer dans l'écran,
 *    Entrée/A pour valider, Échap/B pour revenir, Tab/Start pour fermer.
 *  Local au joueur (jamais répliqué) ; le jeu ne se met en pause qu'en solo (SCRUM-68).
 *  Pièces 3D et sons : DA_PokedexModel (Tools/Pokedex/import_pokedex_unreal.py), voir docs/MENU_POKEDEX.md.
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
	float OpenLidYaw = 14.f;

	/** Marge autour de l'appareil ouvert quand on calcule la distance à la caméra (1 = bord à bord) */
	UPROPERTY(EditAnywhere, Category="Pokedex")
	float ScreenFill = 0.82f;

	/** Exposition fixe pendant le menu (EV100) : l'appareil a le même rendu de jour comme de nuit */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float MenuExposureEV100 = 11.f;

	/** Intensité des lumières qui éclairent l'appareil (candelas) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float KeyLightIntensity = 700.f;

	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float FillLightIntensity = 250.f;

	/** Flou du décor (ouverture de l'objectif : plus petit = plus flou) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Rendu")
	float BackgroundBlurFstop = 2.8f;

	/** Vitesse de rotation à la souris (degrés par pixel) */
	UPROPERTY(EditAnywhere, Category="Pokedex|Commandes")
	float DragSensitivity = 0.35f;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Une pièce qui s'enfonce quand on appuie dessus */
	struct FPressable
	{
		TWeakObjectPtr<USceneComponent> Component;
		FVector BaseLocation = FVector::ZeroVector;
		float PressTime = -1.f;
	};

	/** Couleur d'un rôle de matériau (MAT_RedPlastic...) */
	struct FMaterialLook
	{
		const TCHAR* Token;
		FLinearColor BaseColor;
		float Metallic;
		float Roughness;
		FLinearColor Emissive;
		float EmissiveStrength;
	};

	UStaticMeshComponent* AddPart(FName Key, USceneComponent* Parent);
	void ApplyMaterials(UStaticMeshComponent* Part, FName Key);
	void FixImportAxes();
	void CreateLabels();
	void PlaceInFrontOfCamera();
	void UpdateAnimation(float DeltaSeconds);
	void UpdateLights(float DeltaSeconds);

	/** Trace sous la souris, seulement contre les pièces de l'appareil */
	bool TraceParts(const FVector2D& Pixel, FHitResult& OutHit) const;
	FName GetPartKey(const UPrimitiveComponent* Component) const;

	void PressPart(USceneComponent* Component);
	void PressMenuButton(int32 Index);
	void ClickDPad(const FVector& WorldHit);
	void MoveLidSelection(FIntPoint Direction);
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
	TObjectPtr<UPostProcessComponent> PostProcess;

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> MenuLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPokedexLabelWidget>> MenuLabelWidgets;

	/** Matériaux animés : liseré de chaque bouton du volet */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MenuGlows;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedRedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedYellowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LedGreenMaterial;

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

	// Voyants : moment du dernier éclair
	float LedRedFlash = -10.f;
	float LedYellowFlash = -10.f;
	float LensFlash = -10.f;
	float MenuFlash[6] = { -10.f, -10.f, -10.f, -10.f, -10.f, -10.f };

	static constexpr int32 MenuCount = 6;
	static constexpr float PressDuration = 0.2f;
	static constexpr float PressDepth = 0.32f;
	static constexpr float OpenDuration = 0.95f;
	static constexpr float CloseDuration = 0.6f;
};
