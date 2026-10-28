#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Town/ShopRules.h"
#include "ShopWidget.generated.h"

class UShopComponent;
class UTextBlock;
class UVerticalBox;

/**
 *  Écran de la Boutique Pokémon (SCRUM-53), comme dans Rouge Feu / Vert Feuille :
 *  Acheter / Vendre / Quitter, liste des articles avec leur prix, choix de la quantité, argent du joueur.
 *
 *  Commandes : Haut / Bas (flèches, Z / S, croix) = choisir ; dans la quantité, Haut / Bas = ±1 et Gauche / Droite (Q / D) = ±10.
 *  E, Espace, Entrée, clic ou A = valider. Échap, Retour arrière ou B = revenir (puis quitter). Molette = choisir.
 *
 *  Construite entièrement en C++ comme la boîte de dialogue ; on pourra l'habiller avec un WBP_Shop
 *  enfant de cette classe (réglé dans le ShopComponent du PlayerController).
 */
UCLASS()
class UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(UShopComponent* InOwner);

	/** Réponse du serveur : message de la vendeuse, et retour à la liste */
	void ShowResult(bool bSuccess, const FText& Message);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Hook Blueprint : l'écran change (menu, liste, quantité) pour animer un habillage WBP_Shop */
	UFUNCTION(BlueprintImplementableEvent, Category="Shop")
	void OnShopRefreshed();

private:

	enum class EMode : uint8 { Menu, Buy, Sell, Quantity, Waiting };

	void Confirm();
	void Back();
	void MoveSelection(int32 Delta);
	void ChangeQuantity(int32 Delta);

	/** Reconstruit tout l'écran depuis le PlayerState (argent, sac) et le rayon */
	void Refresh();

	/** Articles de la liste affichée (rayon ou objets à vendre) */
	const TArray<FShopOffer>& GetCurrentList() const { return ListMode == EMode::Buy ? Stock : Sellable; }
	int32 GetMaxQuantity() const;

	/** Somme de l'argent et du sac : change quand le serveur a répondu */
	int32 ComputeSignature() const;

	UPROPERTY(Transient)
	TObjectPtr<UShopComponent> Owner;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MenuList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ListTitle;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ItemList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	TArray<FShopOffer> Stock;
	TArray<FShopOffer> Sellable;

	EMode Mode = EMode::Menu;
	/** Buy ou Sell : la liste derrière la quantité */
	EMode ListMode = EMode::Buy;
	int32 MenuSelection = 0;
	int32 ItemSelection = 0;
	int32 Quantity = 1;
	int32 LastSignature = 0;
	FText Message;
};
