#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UBorder;
class UOverlay;
class UPokedexScreenWidget;
enum class EPokedexMenu : uint8;

/**
 *  Menu en jeu à plat, comme le menu X des jeux Pokémon : Pokédex, Pokémon, Sac, Dresseur,
 *  Sauvegarder, Options, Reprendre, Quitter.
 *  C'est le menu de secours : le vrai menu est le Pokédex 3D (APokedexDevice, SCRUM-64). Celui-ci sert
 *  tant que DA_PokedexModel n'est pas importé, et ouvre les mêmes pages (UPokedexScreenWidget) à plat.
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

	UFUNCTION() void OnPokedexClicked();
	UFUNCTION() void OnPokemonClicked();
	UFUNCTION() void OnBagClicked();
	UFUNCTION() void OnTrainerClicked();
	UFUNCTION() void OnSaveClicked();
	UFUNCTION() void OnOptionsClicked();

private:

	/** Ajoute une ligne au menu. Sans action, elle est grisée (« bientôt ») */
	UButton* AddEntry(UVerticalBox* List, const FText& Label, bool bAvailable);

	/** Ferme le menu via le PlayerController */
	void CloseMenu();

	/** Affiche une page des menus à la place de la liste */
	void OpenScreen(EPokedexMenu Menu);
	void CloseScreen();

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ListPanel;

	UPROPERTY(Transient)
	TObjectPtr<UPokedexScreenWidget> Screen;
};
