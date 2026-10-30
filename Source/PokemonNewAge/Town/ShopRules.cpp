#include "Town/ShopRules.h"
#include "Pokemon/PokemonDatabase.h"

const FName UShopRules::BonusBall(TEXT("PremierBall"));

namespace
{
	FString GetItemName(const UPokemonDatabase* Database, FName Item)
	{
		const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
		return Row ? Row->DisplayName.ToString() : Item.ToString();
	}

	/** Le rayon de cette boutique propose-t-il l'objet à ce joueur ? */
	const FShopEntry* FindEntry(const FShopRow& Shop, FName Item, const TArray<FName>& Flags, int32 BadgeCount)
	{
		return Shop.Stock.FindByPredicate([&](const FShopEntry& Entry)
		{
			return Entry.Item == Item && BadgeCount >= Entry.MinBadges && (Entry.RequiredFlag.IsNone() || Flags.Contains(Entry.RequiredFlag));
		});
	}
}

TArray<FShopOffer> UShopRules::GetStock(const UPokemonDatabase* Database, const FShopRow& Shop, const TArray<FName>& Flags, int32 BadgeCount)
{
	TArray<FShopOffer> Offers;
	for (const FShopEntry& Entry : Shop.Stock)
	{
		const FItemRow* Row = Database ? Database->FindItem(Entry.Item) : nullptr;
		if (!Row || Row->Price <= 0 || FindEntry(Shop, Entry.Item, Flags, BadgeCount) != &Entry
			|| Offers.ContainsByPredicate([&Entry](const FShopOffer& O) { return O.Item == Entry.Item; }))
		{
			continue;
		}
		FShopOffer& Offer = Offers.AddDefaulted_GetRef();
		Offer.Item = Entry.Item;
		Offer.Price = Row->Price;
	}
	return Offers;
}

int32 UShopRules::GetSellPrice(const FItemRow& Item)
{
	return Item.Category == EItemCategory::KeyItem ? 0 : FMath::Max(0, Item.Price / 2);
}

int32 UShopRules::CountItem(const TArray<FItemStack>& Inventory, FName Item)
{
	const FItemStack* Stack = Inventory.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; });
	return Stack ? Stack->Count : 0;
}

int32 UShopRules::GetMaxBuyCount(int32 Price, int32 Money, int32 Owned)
{
	if (Price <= 0)
	{
		return 0;
	}
	return FMath::Max(0, FMath::Min(MaxPerPurchase, FMath::Min(Money / Price, MaxItemCount - Owned)));
}

void UShopRules::AddToInventory(TArray<FItemStack>& Inventory, FName Item, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	if (FItemStack* Stack = Inventory.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; }))
	{
		Stack->Count = FMath::Min(MaxItemCount, Stack->Count + Count);
		return;
	}
	FItemStack& Stack = Inventory.AddDefaulted_GetRef();
	Stack.Item = Item;
	Stack.Count = FMath::Min(MaxItemCount, Count);
}

bool UShopRules::Buy(const UPokemonDatabase* Database, const FShopRow& Shop, const TArray<FName>& Flags, int32 BadgeCount,
	FName Item, int32 Count, int32& Money, TArray<FItemStack>& Inventory, FShopReceipt& OutReceipt, EShopError& OutError)
{
	OutError = EShopError::None;
	const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
	if (!Row || Row->Price <= 0 || !FindEntry(Shop, Item, Flags, BadgeCount))
	{
		OutError = EShopError::NotInStock;
		return false;
	}
	if (Count < 1 || Count > MaxPerPurchase)
	{
		OutError = EShopError::InvalidCount;
		return false;
	}
	// Les prix tiennent largement dans un int32 : 99 x 3 000 ₽ au plus
	const int32 Total = Row->Price * Count;
	if (Money < Total)
	{
		OutError = EShopError::NotEnoughMoney;
		return false;
	}
	if (CountItem(Inventory, Item) + Count > MaxItemCount)
	{
		OutError = EShopError::BagFull;
		return false;
	}

	Money -= Total;
	AddToInventory(Inventory, Item, Count);

	OutReceipt = FShopReceipt();
	OutReceipt.bPurchase = true;
	OutReceipt.Item = Item;
	OutReceipt.Count = Count;
	OutReceipt.Money = Total;

	// Honor Ball offerte : une par dizaine de Balls (dans la limite du sac)
	if (Row->IsPokeBall() && Count >= BallsPerBonus && Database->FindItem(BonusBall))
	{
		const int32 Bonus = FMath::Min(Count / BallsPerBonus, MaxItemCount - CountItem(Inventory, BonusBall));
		if (Bonus > 0)
		{
			AddToInventory(Inventory, BonusBall, Bonus);
			OutReceipt.BonusItem = BonusBall;
			OutReceipt.BonusCount = Bonus;
		}
	}
	return true;
}

bool UShopRules::Sell(const UPokemonDatabase* Database, FName Item, int32 Count, int32& Money, TArray<FItemStack>& Inventory,
	FShopReceipt& OutReceipt, EShopError& OutError)
{
	OutError = EShopError::None;
	const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
	const int32 Price = Row ? GetSellPrice(*Row) : 0;
	if (Price <= 0)
	{
		OutError = EShopError::CannotSell;
		return false;
	}
	if (Count < 1 || Count > MaxItemCount)
	{
		OutError = EShopError::InvalidCount;
		return false;
	}
	const int32 Index = Inventory.IndexOfByPredicate([Item](const FItemStack& S) { return S.Item == Item; });
	if (Index == INDEX_NONE || Inventory[Index].Count < Count)
	{
		OutError = EShopError::NotEnoughItems;
		return false;
	}

	Inventory[Index].Count -= Count;
	if (Inventory[Index].Count <= 0)
	{
		Inventory.RemoveAt(Index);
	}
	// Au-delà de 999 999 ₽ l'argent est perdu, comme dans les jeux
	const int32 Earned = Price * Count;
	Money = FMath::Min(MaxMoney, Money + Earned);

	OutReceipt = FShopReceipt();
	OutReceipt.bPurchase = false;
	OutReceipt.Item = Item;
	OutReceipt.Count = Count;
	OutReceipt.Money = Earned;
	return true;
}

FText UShopRules::GetReceiptText(const UPokemonDatabase* Database, const FShopReceipt& Receipt)
{
	const FString Name = GetItemName(Database, Receipt.Item);
	FString Text = Receipt.bPurchase
		? FString::Printf(TEXT("Voici %d x %s pour %d P. Merci !"), Receipt.Count, *Name, Receipt.Money)
		: FString::Printf(TEXT("%d x %s vendu pour %d P."), Receipt.Count, *Name, Receipt.Money);
	if (Receipt.BonusCount > 0)
	{
		Text += FString::Printf(TEXT(" En cadeau, %d x %s !"), Receipt.BonusCount, *GetItemName(Database, Receipt.BonusItem));
	}
	return FText::FromString(Text);
}

FText UShopRules::GetErrorText(EShopError Error)
{
	switch (Error)
	{
	case EShopError::NoShop:         return FText::FromString(TEXT("La Boutique est fermée."));
	case EShopError::NotInStock:     return FText::FromString(TEXT("Nous n'avons pas cet article, désolée."));
	case EShopError::InvalidCount:   return FText::FromString(TEXT("Combien en voulez-vous ?"));
	case EShopError::NotEnoughMoney: return FText::FromString(TEXT("Vous n'avez pas assez d'argent."));
	case EShopError::BagFull:        return FText::FromString(TEXT("Votre Sac est plein pour cet objet."));
	case EShopError::NotEnoughItems: return FText::FromString(TEXT("Vous n'en avez pas autant."));
	case EShopError::CannotSell:     return FText::FromString(TEXT("Oh ! Je ne peux pas vous acheter ça."));
	case EShopError::InBattle:       return FText::FromString(TEXT("Pas pendant un combat !"));
	case EShopError::None:           return FText::GetEmpty();
	}
	return FText::GetEmpty();
}
