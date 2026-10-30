#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pokemon/PokemonInstance.h"
#include "ShopRules.generated.h"

class UPokemonDatabase;
struct FItemRow;
struct FShopRow;

/** Pourquoi la Boutique refuse un achat ou une vente (texte : UShopRules::GetErrorText). */
UENUM(BlueprintType)
enum class EShopError : uint8
{
	None,
	/** Aucune boutique ouverte, ou boutique inconnue (DT_Shops) */
	NoShop,
	/** Cet objet n'est pas en rayon ici (ou pas encore : badges, drapeau) */
	NotInStock,
	/** Quantité hors de 1 à MaxPerPurchase */
	InvalidCount,
	NotEnoughMoney,
	/** Le sac en contient déjà MaxItemCount */
	BagFull,
	/** Le joueur n'en a pas autant */
	NotEnoughItems,
	/** Objet rare ou sans prix : la Boutique n'en veut pas */
	CannotSell,
	/** Pas pendant un combat */
	InBattle
};

/** Un objet en rayon, tel que l'UI l'affiche. */
USTRUCT(BlueprintType)
struct FShopOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	FName Item;

	/** Prix à l'unité */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	int32 Price = 0;
};

/** Ce qui s'est passé à la caisse (achat ou vente), renvoyé par le serveur au client. */
USTRUCT(BlueprintType)
struct FShopReceipt
{
	GENERATED_BODY()

	/** Vrai : achat, faux : vente */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	bool bPurchase = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	int32 Count = 0;

	/** Payé (achat) ou reçu (vente) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	int32 Money = 0;

	/** Cadeau de la Boutique (Honor Ball pour 10 Balls achetées), None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	FName BonusItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	int32 BonusCount = 0;
};

/**
 *  Règles de la Boutique Pokémon (SCRUM-53), sans dépendre d'un acteur : le PlayerState garde l'argent et le sac,
 *  UShopComponent appelle ces fonctions côté serveur. Testées sans Unreal (Tools/BattleSim).
 *
 *  - Prix d'achat : FItemRow::Price. Revente : la moitié (arrondie en dessous). Objets rares et objets sans prix : invendables.
 *  - Stock : DT_Shops, chaque objet peut attendre un nombre de badges ou un drapeau d'histoire.
 *  - Sac : 999 exemplaires au plus par objet. Argent : 999 999 ₽ au plus.
 *  - Comme depuis la 3G : une Honor Ball offerte pour 10 Balls achetées d'un coup (une par dizaine, depuis la 8G).
 */
UCLASS()
class UShopRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static constexpr int32 MaxMoney = 999999;
	static constexpr int32 MaxItemCount = 999;
	static constexpr int32 MaxPerPurchase = 99;
	static constexpr int32 BallsPerBonus = 10;

	/** Ligne de DT_Items de la Balle offerte */
	static const FName BonusBall;

	/** Objets en rayon pour ce joueur (badges, drapeaux), avec leur prix. Les objets inconnus de DT_Items sont ignorés */
	static TArray<FShopOffer> GetStock(const UPokemonDatabase* Database, const FShopRow& Shop, const TArray<FName>& Flags, int32 BadgeCount);

	/** Prix de revente à l'unité, 0 = invendable */
	static int32 GetSellPrice(const FItemRow& Item);

	/** Nombre d'exemplaires dans le sac */
	static int32 CountItem(const TArray<FItemStack>& Inventory, FName Item);

	/** Combien le joueur peut en acheter au plus (argent, place dans le sac, MaxPerPurchase). 0 = aucun */
	static int32 GetMaxBuyCount(int32 Price, int32 Money, int32 Owned);

	/**
	 *  Achat : vérifie le stock, la quantité, l'argent et la place, puis retire l'argent et ajoute les objets
	 *  (et la Honor Ball offerte). Faux avec OutError si refusé : rien n'est changé.
	 */
	static bool Buy(const UPokemonDatabase* Database, const FShopRow& Shop, const TArray<FName>& Flags, int32 BadgeCount,
		FName Item, int32 Count, int32& Money, TArray<FItemStack>& Inventory, FShopReceipt& OutReceipt, EShopError& OutError);

	/** Vente : retire les objets du sac et donne la moitié de leur prix. Faux avec OutError si refusé : rien n'est changé */
	static bool Sell(const UPokemonDatabase* Database, FName Item, int32 Count, int32& Money, TArray<FItemStack>& Inventory,
		FShopReceipt& OutReceipt, EShopError& OutError);

	/** Phrase de la vendeuse après un achat ou une vente réussi */
	static FText GetReceiptText(const UPokemonDatabase* Database, const FShopReceipt& Receipt);

	UFUNCTION(BlueprintPure, Category="Pokemon|Shop")
	static FText GetErrorText(EShopError Error);

private:

	static void AddToInventory(TArray<FItemStack>& Inventory, FName Item, int32 Count);
};
