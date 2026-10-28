#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PokedexWidgets.generated.h"

class APokedexDevice;
class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;

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

/** Taille en pixels d'un écran du Pokédex (1 cm du modèle = PixelsPerCm pixels). */
UCLASS(Abstract)
class UPokedexPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	static constexpr float PixelsPerCm = 150.f;

	/** À appeler avant de donner le widget au Widget Component */
	void SetPixelSize(const FVector2D& InSize) { PixelSize = InSize; }
	FVector2D GetPixelSize() const { return PixelSize; }

protected:

	FVector2D PixelSize = FVector2D(400.f, 200.f);
};

/** Icône posée sur un des 10 boutons du volet (Poké Ball, Pokédex, sac...). */
UCLASS()
class UPokedexIconWidget : public UPokedexPanelWidget
{
	GENERATED_BODY()

public:

	void SetIcon(UTexture2D* Icon);

	/** Bouton choisi au clavier ou survolé : halo et icône un peu plus grande */
	void SetHighlighted(bool bHighlighted, bool bFocused);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Halo;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PendingIcon;
};

/** Écran du haut du volet : nom du bouton choisi, description de la fiche ou de l'objet. */
UCLASS()
class UPokedexInfoWidget : public UPokedexPanelWidget
{
	GENERATED_BODY()

public:

	void SetInfo(const FText& Title, const FText& Text);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	FText PendingTitle;
	FText PendingText;
};

/** Petit écran : une valeur en gros et son nom (heure, Pokédex, badges). */
UCLASS()
class UPokedexMiniWidget : public UPokedexPanelWidget
{
	GENERATED_BODY()

public:

	void SetValues(const FText& Value, const FText& Label);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	FText PendingValue;
	FText PendingLabel;
};
