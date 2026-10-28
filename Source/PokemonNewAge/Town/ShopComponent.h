#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Town/ShopRules.h"
#include "ShopComponent.generated.h"

class APokemonPlayerState;
class UShopWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShopOpened, FName, ShopId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShopClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShopResult, bool, bSuccess, EShopError, Error, const FShopReceipt&, Receipt);

/**
 *  La Boutique Pokémon d'un joueur (SCRUM-53), sur son PlayerController.
 *
 *  - Le vendeur ouvre la boutique par son dialogue (événement OpenShop, paramètre = ligne de DT_Shops).
 *    Le serveur retient la boutique ouverte : un client ne peut acheter que là, et seulement ce qui est en rayon pour lui.
 *  - Le client affiche UShopWidget (Acheter / Vendre / Quitter) dès que la boîte de dialogue s'est fermée,
 *    puis envoie chaque achat ou vente au serveur, qui applique UShopRules sur l'argent et le sac du PlayerState.
 *  - Les objets achetés arrivent dans le Sac du Pokédex (même inventaire), rangés par poche.
 *  Voir docs/CENTRE_ET_BOUTIQUE.md.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UShopComponent();

	// ------------------------------------------------ Serveur

	/** Ouvre la boutique ShopId pour ce joueur (dialogue du vendeur, commande Boutique). Faux si elle n'existe pas */
	bool OpenShop(FName ShopId);

	// ------------------------------------------------ Côté joueur (UI)

	UFUNCTION(BlueprintCallable, Category="Shop")
	void Buy(FName Item, int32 Count);

	UFUNCTION(BlueprintCallable, Category="Shop")
	void Sell(FName Item, int32 Count);

	/** Ferme la boutique (bouton Quitter) */
	UFUNCTION(BlueprintCallable, Category="Shop")
	void CloseShop();

	/** Boutique affichée chez ce joueur, None si fermée */
	UFUNCTION(BlueprintPure, Category="Shop")
	FName GetOpenShop() const { return ClientShop; }

	/** Rayon pour ce joueur (ses badges et drapeaux), avec les prix */
	UFUNCTION(BlueprintCallable, Category="Shop")
	TArray<FShopOffer> GetStock() const;

	/** Objets du sac que la boutique rachète, par poche du Sac (soins, Balls, baies et matériaux) */
	UFUNCTION(BlueprintCallable, Category="Shop")
	TArray<FShopOffer> GetSellableItems() const;

	APokemonPlayerState* GetPokemonPlayerState() const;

	UPROPERTY(BlueprintAssignable, Category="Shop")
	FOnShopOpened OnShopOpened;

	UPROPERTY(BlueprintAssignable, Category="Shop")
	FOnShopClosed OnShopClosed;

	/** Réponse du serveur à un achat ou une vente */
	UPROPERTY(BlueprintAssignable, Category="Shop")
	FOnShopResult OnShopResult;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Écran de la boutique (un Widget Blueprint enfant de ShopWidget pour l'habiller) */
	UPROPERTY(EditAnywhere, Category="Shop|UI")
	TSubclassOf<UShopWidget> ShopWidgetClass;

	/** Écrit les achats et ventes à l'écran et dans le journal (tests sans UI) */
	UPROPERTY(EditAnywhere, Category="Shop")
	bool bPrintToScreen = false;

	UFUNCTION(Server, Reliable)
	void ServerBuy(FName Item, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerSell(FName Item, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerCloseShop();

	UFUNCTION(Client, Reliable)
	void ClientOpenShop(FName ShopId);

	UFUNCTION(Client, Reliable)
	void ClientShopResult(bool bSuccess, EShopError Error, const FShopReceipt& Receipt);

private:

	/** Serveur : refus si pas de boutique ouverte ou en combat (prévient le client) */
	const FShopRow* GetServerShop(EShopError& OutError) const;

	/** Client : affiche l'écran, une fois la boîte de dialogue du vendeur fermée */
	void ShowWidget();
	void HideWidget();

	UFUNCTION()
	void HandleConversationEnded();

	/** Boutique ouverte côté serveur */
	FName ServerShop;

	/** Boutique affichée côté client */
	FName ClientShop;

	bool bWaitingForDialogue = false;

	UPROPERTY(Transient)
	TObjectPtr<UShopWidget> Widget;
};
