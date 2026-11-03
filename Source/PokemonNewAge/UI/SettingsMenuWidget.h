#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsMenuWidget.generated.h"

class UButton;
class UCanvasPanelSlot;
class UImage;
class UScrollBox;
class USlider;
class UTextBlock;
class USettingsMenuWidget;

/** Sorte de ligne du menu Paramètres. */
enum class ESettingRowKind : uint8
{
	/** Curseur avec la valeur dans une case (volume, champ de vision...) */
	Slider,
	/** < valeur > (résolution, qualité, oui/non...) */
	Choice,
	/** Touche d'une commande : clic, puis appuyer sur la nouvelle touche */
	Key,
	/** Bouton d'action (effacer la sauvegarde, quitter...) */
	Action,
	/** Titre de section, pas modifiable */
	Header
};

/**
 *  Une ligne du menu Paramètres : nom à gauche, case de valeur, curseur ou flèches.
 *  Construite par USettingsMenuWidget ; elle lui renvoie chaque changement.
 */
UCLASS()
class USettingsRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	FName Id;
	ESettingRowKind Kind = ESettingRowKind::Header;
	FText Label;

	/** Slider : bornes, pas et valeur ; Format écrit la valeur dans la case */
	float Min = 0.f;
	float Max = 1.f;
	float Step = 0.1f;
	float Value = 0.f;
	TFunction<FText(float)> Format;

	/** Choice : les valeurs possibles et celle choisie */
	TArray<FText> Choices;
	int32 Index = 0;

	/** Key et Action : texte du bouton */
	FText ButtonText;

	TWeakObjectPtr<USettingsMenuWidget> Menu;

	/** Remet le texte de la case et la position du curseur */
	void RefreshValue();

	/** Le widget qui prend le focus clavier / manette sur cette ligne */
	UWidget* GetFocusTarget() const;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleSlider(float NewValue);
	UFUNCTION() void HandlePrevious();
	UFUNCTION() void HandleNext();
	UFUNCTION() void HandleButton();

private:

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueText;
	UPROPERTY(Transient) TObjectPtr<USlider> ValueSlider;
	UPROPERTY(Transient) TObjectPtr<UButton> MainButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ButtonLabel;
	/** Chevrons < > (grossissent au survol) */
	UPROPERTY(Transient) TObjectPtr<UButton> PreviousButton;
	UPROPERTY(Transient) TObjectPtr<UButton> NextButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PreviousLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NextLabel;
};

/**
 *  Menu Échap (demande de L'amie du 2026-10-29) : Paramètres façon jeu PC, onglets Audio, Vidéo, Commandes et Jeu,
 *  boutons Ok, Annuler, Appliquer et Retour. Disposition au pixel d'après sa maquette Options_Menu (2026-11-01) :
 *  panneau en verre, onglets en grand texte à gauche, chevrons < >, curseurs en barre dégradée. Les changements attendent Appliquer ou OK (sauf les touches,
 *  changées tout de suite). Réglages enregistrés dans UPokemonOptionsSave et GameUserSettings.ini.
 *  Voir docs/MENU_POKEDEX.md.
 */
UCLASS()
class USettingsMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Focus clavier / manette sur le premier onglet */
	void FocusFirst();

	/** Appelés par les lignes */
	void OnRowChanged(USettingsRowWidget* Row);
	void OnRowAction(USettingsRowWidget* Row);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Choix d'une touche (avant tout le reste), Échap et B pour revenir au jeu */
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void OnAudioTab();
	UFUNCTION() void OnVideoTab();
	UFUNCTION() void OnControlsTab();
	UFUNCTION() void OnGameTab();
	UFUNCTION() void OnOkClicked();
	UFUNCTION() void OnCancelClicked();
	UFUNCTION() void OnApplyClicked();
	UFUNCTION() void OnBackClicked();

private:

	/** Valeurs en cours de modification (appliquées par Appliquer ou OK) */
	struct FValues
	{
		float MasterVolume = 1.f;
		float MusicVolume = 1.f;
		float EffectsVolume = 1.f;
		float FootstepsVolume = 1.f;
		float WeatherVolume = 1.f;
		int32 TextSpeed = 1;
		float CameraSensitivity = 1.f;
		bool bInvertCameraY = false;
		float FieldOfView = 90.f;
		bool bShowHUD = true;
		bool bAutoSave = true;
		FIntPoint Resolution = FIntPoint(1920, 1080);
		int32 WindowMode = 0;
		bool bVSync = false;
		float FrameRateLimit = 0.f;
		float ResolutionScale = 100.f;
		int32 ViewDistance = 3;
		int32 AntiAliasing = 3;
		int32 Shadows = 3;
		int32 Textures = 3;
		int32 Effects = 3;
		int32 PostProcess = 3;
		int32 Foliage = 3;
	};

	enum class ETab : uint8 { Audio, Video, Controls, Game };

	/** Lit les réglages enregistrés */
	void LoadValues();

	/** Enregistre et applique les valeurs en cours */
	void ApplyValues();

	/** Reconstruit les lignes de l'onglet */
	void ShowTab(ETab NewTab);

	USettingsRowWidget* AddRow(FName Id, ESettingRowKind Kind, const FText& Label);
	void AddHeader(const FText& Label);
	void AddSlider(FName Id, const FText& Label, float Min, float Max, float Step, float Value, TFunction<FText(float)> Format);
	void AddChoice(FName Id, const FText& Label, const TArray<FText>& Choices, int32 Index);
	void AddToggle(FName Id, const FText& Label, bool bValue);

	/** Ascenseur maison (Rectangle 6 de la maquette) : suit la position de la liste */
	void UpdateScrollThumb();

	void SetStatus(const FText& Message);
	void Close();

	FValues Values;
	ETab Tab = ETab::Audio;
	TArray<FIntPoint> Resolutions;

	/** Commande dont on attend la nouvelle touche, None sinon */
	FName CapturingKey;

	/** Effacer la sauvegarde : il faut cliquer deux fois */
	bool bConfirmDelete = false;

	UPROPERTY(Transient) TObjectPtr<UScrollBox> RowList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> TabButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TabLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> BottomButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> BottomLabels;
	UPROPERTY(Transient) TObjectPtr<UImage> ScrollThumb;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> ScrollThumbSlot;
	UPROPERTY(Transient) TArray<TObjectPtr<USettingsRowWidget>> Rows;
};
