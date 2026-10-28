#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UTexture2D;
class UOverlay;
class UPokedexScreenWidget;
enum class EPokedexMenu : uint8;

/**
 *  Menu en jeu PC classique (choix de L'amie le 2026-10-28) : plein écran, onglets à icônes en haut
 *  (Pokémon, Pokédex, Sac, Dresseur, Carte, Sauvegarder, En ligne, Options), la page de l'onglet au centre.
 *  Les pages sont celles du Pokédex (UPokedexScreenWidget, à plat), sauf l'onglet Pokédex qui sort le Pokédex 3D
 *  (APokedexDevice). Tab ou Échap ferme, LB/RB ou Page préc./suiv.
 *  change d'onglet, et le raccourci d'un onglet (P Pokédex, B Sac...) y va directement. Voir docs/MENU_POKEDEX.md.
 */
UCLASS()
class UGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Onglets du menu, dans l'ordre */
	static TArray<EPokedexMenu> GetTabs();

	/** Icône d'un menu (planche de L'amie, importée par Tools/Pokedex/import_pokedex_unreal.py), nulle si absente */
	static UTexture2D* GetMenuIcon(EPokedexMenu Menu);

	/** Affiche un onglet */
	void OpenTab(EPokedexMenu Menu);

	/** Rend le clavier et la manette à la page en cours */
	void FocusDefaultEntry();

protected:

	/** Construit l'arbre de widgets la première fois */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeConstruct() override;

	/** Touches que la page n'a pas prises : fermer, changer d'onglet, raccourcis */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void OnResumeClicked();
	UFUNCTION() void OnQuitClicked();

	UFUNCTION() void OnTab0Clicked();
	UFUNCTION() void OnTab1Clicked();
	UFUNCTION() void OnTab2Clicked();
	UFUNCTION() void OnTab3Clicked();
	UFUNCTION() void OnTab4Clicked();
	UFUNCTION() void OnTab5Clicked();
	UFUNCTION() void OnTab6Clicked();
	UFUNCTION() void OnTab7Clicked();

private:

	UButton* AddButton(class UHorizontalBox* Bar, const FText& Label);

	/** Ferme le menu via le PlayerController */
	void CloseMenu();

	/** Onglet précédent (-1) ou suivant (+1) */
	void CycleTab(int32 Delta);

	/** Couleurs des onglets et touches affichées */
	void RefreshTabs();

	UPROPERTY(Transient)
	TObjectPtr<UPokedexScreenWidget> Screen;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> TabButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TabKeys;

	int32 CurrentTab = 0;
};
