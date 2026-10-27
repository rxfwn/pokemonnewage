#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 *  Menu en jeu, comme le menu X des jeux Pokémon : Pokédex, Pokémon, Sac, Dresseur,
 *  Sauvegarder, Options, Reprendre, Quitter.
 *  Construit entièrement en C++ (pas besoin de Widget Blueprint) ; on pourra le remplacer
 *  plus tard par un WBP_GameMenu habillé, enfant de cette classe.
 *  Les entrées pas encore codées sont grisées, elles s'activeront avec leurs tickets.
 */
UCLASS()
class UGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Met le focus sur « Reprendre » (clavier et manette) */
	void FocusDefaultEntry();

protected:

	/** Construit l'arbre de widgets la première fois */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Donne le focus à « Reprendre » pour naviguer au clavier ou à la manette */
	virtual void NativeConstruct() override;

	/** Tab, Échap, Retour arrière, B ou Start ferment le menu */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnQuitClicked();

private:

	/** Ajoute une ligne au menu. Sans action, elle est grisée (« bientôt ») */
	UButton* AddEntry(UVerticalBox* List, const FText& Label, bool bAvailable);

	/** Ferme le menu via le PlayerController */
	void CloseMenu();

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeButton;
};
