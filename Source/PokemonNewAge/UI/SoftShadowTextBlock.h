#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "SoftShadowTextBlock.generated.h"

/**
 *  Texte avec l'ombre floue des maquettes de L'amie (CSS text-shadow: 3px 4px 4px rgba(0, 0, 0, 0.25)).
 *  Slate ne sait faire qu'une ombre nette : on dessine derrière le texte 9 copies noires très transparentes, décalées
 *  autour de (3, 4) sur un petit carré, avec des opacités en cloche. Ensemble elles donnent une ombre douce.
 *  Les copies suivent le texte, la police, l'alignement et le retour à la ligne du texte principal.
 */
UCLASS()
class USoftShadowTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:

	/** Décalage de l'ombre (px de la maquette) */
	FVector2D SoftShadowOffset = FVector2D(3.f, 4.f);

	/** Flou CSS (px) : les copies s'étalent sur la moitié de chaque côté */
	float SoftShadowBlur = 4.f;

	/** Opacité au cœur de l'ombre */
	float SoftShadowOpacity = 0.25f;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnJustificationChanged(ETextJustify::Type InJustification) override;

private:

	// Lus à chaque image par les copies (attributs Slate liés)
	FText GetShadowText() { return GetText(); }
	FSlateFontInfo GetShadowFont() { return GetFont(); }
	ETextJustify::Type GetShadowJustification() { return ShadowJustification; }
	bool GetShadowAutoWrap() { return GetAutoWrapText(); }
	float GetShadowWrapAt() { return GetWrapTextAt(); }

	ETextJustify::Type ShadowJustification = ETextJustify::Left;
	TSharedPtr<class SOverlay> ShadowOverlay;
};
