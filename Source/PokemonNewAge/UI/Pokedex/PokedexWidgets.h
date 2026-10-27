#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PokedexWidgets.generated.h"

class APokedexDevice;
class UTextBlock;

/**
 *  Calque invisible plein écran pendant que le Pokédex 3D est ouvert : il reçoit la souris, le clavier
 *  et la manette et les passe à APokedexDevice (qui fait les traces vers les pièces 3D).
 */
UCLASS()
class UPokedexInputWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	TWeakObjectPtr<APokedexDevice> Device;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;

private:

	/** Position de la souris en pixels du viewport */
	FVector2D ToViewportPixels(const FPointerEvent& InMouseEvent);
};

/** Étiquette lumineuse posée sur un des 6 boutons du volet (« POKÉMON », « SAC »...). */
UCLASS()
class UPokedexLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille en pixels = taille du bouton en centimètres × 100 */
	static constexpr float Width = 470.f;
	static constexpr float Height = 270.f;

	void SetLabel(const FText& Text, const FLinearColor& Color);

	/** Bouton choisi au clavier ou survolé : texte plus clair */
	void SetHighlighted(bool bHighlighted);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	FLinearColor BaseColor = FLinearColor::White;
	FText PendingText;
};
