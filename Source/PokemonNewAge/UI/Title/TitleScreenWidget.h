#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleScreenWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UTitleScreenSubsystem;
class UVerticalBox;
class UWidget;

/**
 *  Écran de titre (SCRUM-76), d'après la maquette Figma de L'amie (1920 × 1080) : image de fond, logo en haut
 *  à gauche, menu Continuer / Nouvelle partie / Options à gauche, version et © en bas à droite, tout en Futura.
 *  Fait en C++ comme le HUD ; affiché par UTitleScreenSubsystem par-dessus la map de jeu.
 *  Clavier, souris et manette. Voir docs/ECRAN_TITRE.md.
 */
UCLASS()
class UTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	TWeakObjectPtr<UTitleScreenSubsystem> Owner;

	/** Donne le focus clavier / manette à l'entrée choisie */
	void FocusCurrent();

	/** Relit la sauvegarde (Continuer apparaît ou disparaît) */
	void RefreshSave();

	/** Fondu de sortie quand la partie commence, en secondes */
	void PlayOutro(float Duration);

	/** Vrai s'il y a une image de fond (sinon la vue 3D de la map se voit derrière) */
	bool HasBackground() const { return BackgroundTextures.Num() > 0; }

protected:

	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void OnContinueClicked();
	UFUNCTION() void OnNewGameClicked();
	UFUNCTION() void OnOptionsClicked();
	UFUNCTION() void OnConfirmYes();
	UFUNCTION() void OnConfirmNo();

private:

	enum class EStage : uint8 { Menu, Confirm, Outro };

	/** Une entrée de menu : bouton transparent avec son texte */
	struct FEntry
	{
		TObjectPtr<UButton> Button;
		TObjectPtr<UTextBlock> Label;
	};

	FEntry MakeEntry(const FText& Label, float FontSize);

	/** Entrées visibles de l'étape en cours, dans l'ordre */
	TArray<const FEntry*> VisibleEntries() const;

	void SetStage(EStage NewStage, bool bFocus = true);

	/** Sans image de fond : vignette sombre pour lire le texte sur la vue 3D */
	UTexture2D* MakeVignette();

	EStage Stage = EStage::Menu;
	float Age = 0.f;
	float OutroAge = 0.f;
	float OutroDuration = 1.f;
	int32 Selected = 0;
	int32 LastFocused = INDEX_NONE;

	FEntry ContinueEntry;
	FEntry NewGameEntry;
	FEntry OptionsEntry;
	FEntry YesEntry;
	FEntry NoEntry;

	/** Les widgets des entrées (FEntry n'est pas vu par le ramasse-miettes) */
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> EntryWidgets;

	/** Deux images de fond superposées pour le fondu de l'une à l'autre */
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> BackgroundImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> BackgroundTextures;
	UPROPERTY(Transient) TObjectPtr<UWidget> BackgroundBlock;
	int32 BackgroundIndex = 0;
	float BackgroundAge = 0.f;

	UPROPERTY(Transient) TObjectPtr<UWidget> LogoBlock;
	UPROPERTY(Transient) TObjectPtr<UWidget> MenuBlock;
	UPROPERTY(Transient) TObjectPtr<UWidget> FooterBlock;
	UPROPERTY(Transient) TObjectPtr<UWidget> ConfirmPanel;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> VignetteTexture;
};
