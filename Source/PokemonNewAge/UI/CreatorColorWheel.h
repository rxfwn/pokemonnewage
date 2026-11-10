#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CreatorColorWheel.generated.h"

class SColorWheel;

/**
 *  Roue de couleur (teinte + saturation) du créateur de personnage, comme celle d'inZOI : à côté des couleurs
 *  prédéfinies du nuancier. La luminosité se règle avec un curseur à part (UCharacterCreatorWidget).
 *  Enveloppe le SColorWheel de Slate.
 */
UCLASS()
class UCreatorColorWheel : public UWidget
{
	GENERATED_BODY()

public:

	/** Couleur affichée, en TSV (R = teinte 0-360, G = saturation, B = luminosité) */
	void SetHSV(const FLinearColor& InHSV);
	FLinearColor GetHSV() const { return HSV; }

	/** Appelé à chaque déplacement sur la roue (couleur en TSV) ; bFinished : bouton relâché */
	void Setup(TFunction<void(const FLinearColor&, bool)> InOnChanged) { OnChanged = MoveTemp(InOnChanged); }

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	FLinearColor HSV = FLinearColor(0.f, 0.f, 1.f, 1.f);
	TFunction<void(const FLinearColor&, bool)> OnChanged;
	TSharedPtr<SColorWheel> Wheel;
};
