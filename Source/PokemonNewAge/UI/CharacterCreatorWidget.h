#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Character/TrainerAppearance.h"
#include "Character/TrainerAppearanceComponent.h"
#include "Character/TrainerAppearanceData.h"
#include "CharacterCreatorWidget.generated.h"

class ACharacterStudio;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UEditableText;
class UScrollBox;
class UTextBlock;
class UTrainerAppearanceData;
class UWidget;

/** Bouton qui appelle une fonction C++ (évite une fonction UFUNCTION par vignette, couleur ou onglet) */
UCLASS()
class UCreatorButton : public UButton
{
	GENERATED_BODY()

public:

	void Setup(TFunction<void()> InOnClick);

private:

	UFUNCTION()
	void HandleClicked();

	TFunction<void()> OnClick;
};

/** Curseur de l'éditeur : appelle une fonction C++ à chaque changement */
UCLASS()
class UCreatorSlider : public USlider
{
	GENERATED_BODY()

public:

	void Setup(TFunction<void(float)> InOnChange);

	FName SliderId;

	/** Valeur affichée à droite du curseur */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;

private:

	UFUNCTION()
	void HandleValue(float Value);

	TFunction<void(float)> OnChange;
};

/**
 *  Éditeur de personnage (SCRUM-66/79), mise en page reprise de « Create a Zoi » d'inZOI (captures de L'amie du
 *  2026-11-05, personnage/references-inzoi/) : personnage au centre dans le studio gris clair, Profil à gauche
 *  (prénom, base, phrase d'accroche de la carte dresseur), barre d'outils en haut (aléatoire, photo, visage, corps,
 *  annuler, refaire), onglets en bas (Modèles, Visage, Corps, Tenue, Accessoires), galerie et réglages à droite,
 *  points cliquables sur le visage, mode Basique / Détaillé (on glisse directement sur le visage), Studio,
 *  Masquer l'interface (U), Terminer. Base de travail : L'amie la redessinera au style verre.
 *  Ouvert par UTrainerAppearanceComponent::OpenEditor ; voir docs/PERSONNAGE_CREATION.md.
 */
UCLASS()
class UCharacterCreatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(UTrainerAppearanceComponent* InOwner, ACharacterStudio* InStudio, UTrainerAppearanceData* InData,
		ETrainerEditorMode InMode, const FTrainerAppearance& Appearance, const FString& TrainerName, const FString& Catchphrase);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleNameChanged(const FText& Text);

	UFUNCTION()
	void HandleCatchphraseChanged(const FText& Text);

private:

	enum class ETab : uint8 { Presets, Face, Body, Outfit, Accessories };

	/** Ce que montre le panneau de droite dans l'onglet Visage (points sur le visage) */
	enum class EFacePart : uint8 { Presets, Hair, Skin, Eyes, Makeup };

	// ------------------------------------------------ Construction
	void BuildLeftColumn();
	void BuildToolbar();
	void BuildTabs();
	void BuildBottomLeft();
	void BuildFaceHotspots();
	void BuildRightPanel();

	/** Remplit le panneau de droite selon l'onglet, la partie du visage et le mode */
	void RefreshRightPanel();
	void AddSection(const FText& Title);
	void AddSliders(ETrainerSliderGroup Group);
	void AddPresetGrid(const TArray<FTrainerPresetDef>& Presets, bool bFull);
	void AddSwatches(const TArray<FLinearColor>& Colors, bool bSkin);
	void AddNote(const FText& Text);

	/** Remet les textes et les styles (onglets, base, curseurs) d'après l'état courant */
	void RefreshAll();
	void RefreshSliderValues();

	// ------------------------------------------------ Actions
	void SelectTab(ETab NewTab);
	void SelectFacePart(EFacePart Part);
	void SetDetailed(bool bNewDetailed);
	void SelectBase(int32 Direction);
	void Randomize();
	void TakePhoto();
	void Undo();
	void Redo();
	void ToggleUI();
	void Finish();
	void Cancel();

	/** Change l'apparence affichée (studio) ; ControlId regroupe les petits pas d'un même réglage pour Annuler */
	void ChangeAppearance(const FTrainerAppearance& NewAppearance, FName ControlId, bool bMerge = true);
	void PushUndo(FName ControlId, bool bMerge);
	void SetStatus(const FText& Text);

	/** Poignées du mode Détaillé : position à l'écran, et laquelle est sous la souris */
	void UpdateHandles();
	int32 FindHandleAt(const FVector2D& CanvasPosition) const;
	FVector2D ToCanvas(const FVector2D& AbsolutePosition) const;

	TWeakObjectPtr<UTrainerAppearanceComponent> Owner;
	TWeakObjectPtr<ACharacterStudio> Studio;

	UPROPERTY(Transient)
	TObjectPtr<UTrainerAppearanceData> Data;

	ETrainerEditorMode Mode = ETrainerEditorMode::Creation;
	FTrainerAppearance Current;
	FTrainerAppearance Original;
	FString Name;
	FString Catch;

	ETab Tab = ETab::Presets;
	EFacePart FacePart = EFacePart::Presets;
	bool bDetailed = false;
	bool bUIHidden = false;

	TArray<FTrainerAppearance> UndoStack;
	TArray<FTrainerAppearance> RedoStack;
	FName LastUndoControl;
	double LastUndoTime = 0.0;

	/** Souris : -1 = rien, -2 = rotation du personnage, sinon index de la poignée attrapée */
	int32 DragTarget = -1;
	FVector2D LastMouse = FVector2D::ZeroVector;

	// ------------------------------------------------ Widgets
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UEditableText> NameBox;
	UPROPERTY(Transient) TObjectPtr<UEditableText> CatchBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BaseText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StudioText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PanelTitle;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> PanelList;
	UPROPERTY(Transient) TObjectPtr<UWidget> RightPanel;
	UPROPERTY(Transient) TObjectPtr<UWidget> ModeBar;
	UPROPERTY(Transient) TObjectPtr<UWidget> Hotspots;
	UPROPERTY(Transient) TObjectPtr<UCreatorButton> FinishButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UCreatorButton>> TabButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TabLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ModeLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> HotspotLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> HotspotDots;
	UPROPERTY(Transient) TArray<TObjectPtr<UCreatorSlider>> Sliders;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> HandleDots;
	TArray<FVector2D> HandlePositions;
	TArray<int32> HandleIndices;
};
