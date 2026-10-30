#include "Town/ShopComponent.h"

#include "Battle/BattleComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "PokemonNewAge.h"
#include "Town/TownScreens.h"
#include "UI/ShopWidget.h"

namespace
{
	/** Poche du Sac (même ordre que le menu Sac du Pokédex) */
	int32 GetPocket(const FItemRow& Item)
	{
		switch (Item.Category)
		{
		case EItemCategory::Medicine: return 0;
		case EItemCategory::PokeBall: return 1;
		case EItemCategory::KeyItem:  return 3;
		default:                      return 2;
		}
	}
}

UShopComponent::UShopComponent()
{
	SetIsReplicatedByDefault(true);
	ShopWidgetClass = UShopWidget::StaticClass();
}

void UShopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

APokemonPlayerState* UShopComponent::GetPokemonPlayerState() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	return PlayerController ? PlayerController->GetPlayerState<APokemonPlayerState>() : nullptr;
}

// ---------------------------------------------------------------- Serveur

bool UShopComponent::OpenShop(FName ShopId)
{
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	if (!Database || !Database->FindShop(ShopId))
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Boutique : %s absente de DT_Shops (relancer Tools/Data/import_data_tables.py)"), *ShopId.ToString());
		return false;
	}
	ServerShop = ShopId;
	ClientOpenShop(ShopId);
	return true;
}

const FShopRow* UShopComponent::GetServerShop(EShopError& OutError) const
{
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const FShopRow* Shop = Database && !ServerShop.IsNone() ? Database->FindShop(ServerShop) : nullptr;
	const UBattleComponent* Battle = GetOwner()->FindComponentByClass<UBattleComponent>();
	OutError = !Shop ? EShopError::NoShop : (Battle && Battle->IsInBattle() ? EShopError::InBattle : EShopError::None);
	return OutError == EShopError::None ? Shop : nullptr;
}

void UShopComponent::ServerBuy_Implementation(FName Item, int32 Count)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	EShopError Error = EShopError::NoShop;
	FShopReceipt Receipt;
	const FShopRow* Shop = State ? GetServerShop(Error) : nullptr;
	const bool bOk = Shop && UShopRules::Buy(UPokemonDataSubsystem::GetDatabase(this), *Shop, State->StoryFlags, State->Badges.Num(),
		Item, Count, State->Money, State->Inventory, Receipt, Error);
	ClientShopResult(bOk, Error, Receipt);
}

void UShopComponent::ServerSell_Implementation(FName Item, int32 Count)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	EShopError Error = EShopError::NoShop;
	FShopReceipt Receipt;
	const bool bOk = State && GetServerShop(Error)
		&& UShopRules::Sell(UPokemonDataSubsystem::GetDatabase(this), Item, Count, State->Money, State->Inventory, Receipt, Error);
	ClientShopResult(bOk, Error, Receipt);
}

void UShopComponent::ServerCloseShop_Implementation()
{
	ServerShop = NAME_None;
}

// ---------------------------------------------------------------- Côté joueur

void UShopComponent::Buy(FName Item, int32 Count)
{
	ServerBuy(Item, Count);
}

void UShopComponent::Sell(FName Item, int32 Count)
{
	ServerSell(Item, Count);
}

void UShopComponent::CloseShop()
{
	HideWidget();
	const bool bWasOpen = !ClientShop.IsNone();
	ClientShop = NAME_None;
	bWaitingForDialogue = false;
	ServerCloseShop();
	if (bWasOpen)
	{
		OnShopClosed.Broadcast();
	}
}

TArray<FShopOffer> UShopComponent::GetStock() const
{
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const FShopRow* Shop = Database && !ClientShop.IsNone() ? Database->FindShop(ClientShop) : nullptr;
	const APokemonPlayerState* State = GetPokemonPlayerState();
	return Shop && State ? UShopRules::GetStock(Database, *Shop, State->StoryFlags, State->Badges.Num()) : TArray<FShopOffer>();
}

TArray<FShopOffer> UShopComponent::GetSellableItems() const
{
	TArray<FShopOffer> Offers;
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const APokemonPlayerState* State = GetPokemonPlayerState();
	if (!Database || !State)
	{
		return Offers;
	}
	TArray<TPair<int32, FShopOffer>> Sorted;
	for (const FItemStack& Stack : State->Inventory)
	{
		const FItemRow* Row = Database->FindItem(Stack.Item);
		const int32 Price = Row ? UShopRules::GetSellPrice(*Row) : 0;
		if (Stack.Count > 0 && Price > 0)
		{
			FShopOffer Offer;
			Offer.Item = Stack.Item;
			Offer.Price = Price;
			Sorted.Emplace(GetPocket(*Row), Offer);
		}
	}
	// Par poche, puis dans l'ordre du sac
	Sorted.StableSort([](const TPair<int32, FShopOffer>& A, const TPair<int32, FShopOffer>& B) { return A.Key < B.Key; });
	for (const TPair<int32, FShopOffer>& Pair : Sorted)
	{
		Offers.Add(Pair.Value);
	}
	return Offers;
}

void UShopComponent::ClientOpenShop_Implementation(FName ShopId)
{
	ClientShop = ShopId;
	OnShopOpened.Broadcast(ShopId);

	// Ouverte par le vendeur : on attend que sa boîte de dialogue se ferme (elle rend la main au jeu en partant)
	UDialogueComponent* Dialogue = GetOwner()->FindComponentByClass<UDialogueComponent>();
	if (Dialogue && Dialogue->IsInConversation())
	{
		bWaitingForDialogue = true;
		Dialogue->OnConversationEnded.AddUniqueDynamic(this, &UShopComponent::HandleConversationEnded);
		return;
	}
	ShowWidget();
}

void UShopComponent::HandleConversationEnded()
{
	if (UDialogueComponent* Dialogue = GetOwner()->FindComponentByClass<UDialogueComponent>())
	{
		Dialogue->OnConversationEnded.RemoveDynamic(this, &UShopComponent::HandleConversationEnded);
	}
	if (bWaitingForDialogue && !ClientShop.IsNone())
	{
		bWaitingForDialogue = false;
		ShowWidget();
	}
}

void UShopComponent::ShowWidget()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (Widget || !PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	Widget = CreateWidget<UShopWidget>(PlayerController, ShopWidgetClass ? ShopWidgetClass.Get() : UShopWidget::StaticClass());
	if (!Widget)
	{
		UE_LOG(LogPokemonNewAge, Error, TEXT("Boutique : impossible de creer l'ecran"));
		return;
	}
	Widget->Init(this);
	TownScreens::OpenScreen(PlayerController, Widget);
}

void UShopComponent::HideWidget()
{
	if (Widget)
	{
		TownScreens::CloseScreen(Cast<APlayerController>(GetOwner()), Widget);
		Widget = nullptr;
	}
}

void UShopComponent::ClientShopResult_Implementation(bool bSuccess, EShopError Error, const FShopReceipt& Receipt)
{
	const FText Message = bSuccess ? UShopRules::GetReceiptText(UPokemonDataSubsystem::GetDatabase(this), Receipt) : UShopRules::GetErrorText(Error);
	UE_LOG(LogPokemonNewAge, Log, TEXT("Boutique : %s"), *Message.ToString());
	if (bPrintToScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, bSuccess ? FColor::Green : FColor::Orange, Message.ToString());
	}
	if (Widget)
	{
		Widget->ShowResult(bSuccess, Message);
	}
	OnShopResult.Broadcast(bSuccess, Error, Receipt);
}
