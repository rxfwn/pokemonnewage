#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShortcutBarWidget.generated.h"

class UTextBlock;

/**
 *  Barre des icônes de menu en bas à droite de l'écran, avec la touche de chaque raccourci (P Pokédex, B Sac...).
 *  Cachée pendant les combats, quand un menu est ouvert (souris visible), ou si « Icônes de menu à l'écran » est sur Non dans Options.
 */
UCLASS()
class UShortcutBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	/** Visibilité et touches (les touches peuvent changer dans Options) */
	void Refresh();

	/** Le fond et les icônes (la racine reste visible pour que le Tick continue) */
	UPROPERTY(Transient)
	TObjectPtr<class UBorder> Panel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> KeyTexts;

	float RefreshTimer = 0.f;
};
