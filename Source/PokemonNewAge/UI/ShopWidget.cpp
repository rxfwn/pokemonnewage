#include "UI/ShopWidget.h"
#include "UI/GameFonts.h"
#include "UI/GlassPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Town/ShopComponent.h"

#define LOCTEXT_NAMESPACE "ShopWidget"

namespace
{
	// Mêmes couleurs que la boîte de dialogue
	// Boîtes en verre (maquettes Figma de L'amie, 2026-11-01) : texte blanc avec ombre
	const FLinearColor InkColor = PokemonGlass::TextColor;
	const FLinearColor DimColor(0.78f, 0.79f, 0.82f);
	const FLinearColor TitleColor(0.13f, 0.25f, 0.45f, 0.97f);
	const FLinearColor AccentColor(1.f, 0.62f, 0.12f);

	/** Lignes visibles de la liste (elle défile autour de la sélection) */
	constexpr int32 VisibleRows = 8;

	UTextBlock* MakeText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color, FName Name = NAME_None)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(PokemonFonts::Get(Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		PokemonGlass::AddTextShadow(Text);
		return Text;
	}

	UBorder* MakeBox(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrushColor(Color);
		Box->SetPadding(Padding);
		return Box;
	}

	/** Boîte en verre : rend la bordure transparente qui reçoit le contenu ; OutFrame = le panneau à placer */
	UBorder* MakeGlassBox(UWidgetTree* Tree, const FMargin& Padding, UWidget*& OutFrame)
	{
		UBorder* Inner = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Inner->SetBrushColor(FLinearColor::Transparent);
		Inner->SetPadding(Padding);
		OutFrame = PokemonGlass::Make(Tree, Inner, FMargin(0.f));
		return Inner;
	}

	FText Money(int32 Amount)
	{
		return FText::Format(LOCTEXT("Money", "{0} P"), FText::AsNumber(Amount));
	}

	const FItemRow* FindItem(const UObject* Context, FName Item)
	{
		const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(Context);
		return Database ? Database->FindItem(Item) : nullptr;
	}

	FText GetItemName(const UObject* Context, FName Item)
	{
		const FItemRow* Row = FindItem(Context, Item);
		return Row ? Row->DisplayName : FText::FromName(Item);
	}
}

void UShopWidget::Init(UShopComponent* InOwner)
{
	Owner = InOwner;
	Mode = EMode::Menu;
	Message = LOCTEXT("Welcome", "Bienvenue ! Que puis-je faire pour vous ?");
	SetIsFocusable(true);
}

TSharedRef<SWidget> UShopWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
		ColumnSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnSlot->SetVerticalAlignment(VAlign_Center);

		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
		Width->SetWidthOverride(1100.f);
		Column->AddChildToVerticalBox(Width);
		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Inside"));
		Width->SetContent(Inside);

		// Haut : nom de la boutique et argent
		UBorder* Header = MakeBox(WidgetTree, TitleColor, FMargin(28.f, 12.f));
		Inside->AddChildToVerticalBox(Header);
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Header->SetContent(HeaderRow);
		TitleText = MakeText(WidgetTree, 26, FLinearColor::White, TEXT("TitleText"));
		HeaderRow->AddChildToHorizontalBox(TitleText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MoneyText = MakeText(WidgetTree, 26, FLinearColor::White, TEXT("MoneyText"));
		HeaderRow->AddChildToHorizontalBox(MoneyText);

		// Milieu : menu à gauche, liste à droite
		UHorizontalBox* Middle = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Middle"));
		UVerticalBoxSlot* MiddleSlot = Inside->AddChildToVerticalBox(Middle);
		MiddleSlot->SetPadding(FMargin(0.f, 10.f));

		UWidget* MenuBoxFrame = nullptr;
		UBorder* MenuBox = MakeGlassBox(WidgetTree, FMargin(24.f, 16.f), MenuBoxFrame);
		UHorizontalBoxSlot* MenuSlot = Middle->AddChildToHorizontalBox(MenuBoxFrame);
		MenuSlot->SetVerticalAlignment(VAlign_Top);
		MenuSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		MenuList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuList"));
		MenuBox->SetContent(MenuList);

		USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ListSize->SetMinDesiredHeight(420.f);
		Middle->AddChildToHorizontalBox(ListSize)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UWidget* ListBoxFrame = nullptr;
		UBorder* ListBox = MakeGlassBox(WidgetTree, FMargin(28.f, 16.f), ListBoxFrame);
		ListSize->SetContent(ListBoxFrame);
		UVerticalBox* ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ListBox->SetContent(ListColumn);
		// En-tête aux mêmes largeurs de colonnes que les lignes de la liste
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ListColumn->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		ListTitle = MakeText(WidgetTree, 20, DimColor, TEXT("ListTitle"));
		TitleRow->AddChildToHorizontalBox(ListTitle)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* OwnedTitle = MakeText(WidgetTree, 20, DimColor);
		OwnedTitle->SetText(LOCTEXT("OwnedTitle", "Sac"));
		USizeBox* OwnedTitleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		OwnedTitleSize->SetWidthOverride(110.f);
		OwnedTitleSize->SetContent(OwnedTitle);
		TitleRow->AddChildToHorizontalBox(OwnedTitleSize);
		ListPriceTitle = MakeText(WidgetTree, 20, DimColor, TEXT("ListPriceTitle"));
		ListPriceTitle->SetJustification(ETextJustify::Right);
		USizeBox* PriceTitleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PriceTitleSize->SetWidthOverride(140.f);
		PriceTitleSize->SetContent(ListPriceTitle);
		TitleRow->AddChildToHorizontalBox(PriceTitleSize);
		ItemList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemList"));
		ListColumn->AddChildToVerticalBox(ItemList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		QuantityText = MakeText(WidgetTree, 26, AccentColor, TEXT("QuantityText"));
		UVerticalBoxSlot* QuantitySlot = ListColumn->AddChildToVerticalBox(QuantityText);
		QuantitySlot->SetHorizontalAlignment(HAlign_Right);
		QuantitySlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

		// Bas : ce que dit la vendeuse, ou la description de l'objet
		USizeBox* MessageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MessageSize->SetMinDesiredHeight(120.f);
		Inside->AddChildToVerticalBox(MessageSize);
		UWidget* MessageBoxFrame = nullptr;
		UBorder* MessageBox = MakeGlassBox(WidgetTree, FMargin(32.f, 20.f), MessageBoxFrame);
		MessageSize->SetContent(MessageBoxFrame);
		MessageText = MakeText(WidgetTree, 26, InkColor, TEXT("MessageText"));
		MessageText->SetAutoWrapText(true);
		MessageBox->SetContent(MessageText);
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

int32 UShopWidget::ComputeSignature() const
{
	const APokemonPlayerState* State = Owner ? Owner->GetPokemonPlayerState() : nullptr;
	if (!State)
	{
		return 0;
	}
	uint32 Signature = ::GetTypeHash(State->Money) ^ ::GetTypeHash(State->Badges.Num());
	for (const FItemStack& Stack : State->Inventory)
	{
		Signature = HashCombine(Signature, HashCombine(GetTypeHash(Stack.Item), ::GetTypeHash(Stack.Count)));
	}
	return static_cast<int32>(Signature);
}

void UShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Argent et sac répliqués : on redessine quand ils changent
	const int32 Signature = ComputeSignature();
	if (Signature != LastSignature)
	{
		Refresh();
	}
}

int32 UShopWidget::GetMaxQuantity() const
{
	const APokemonPlayerState* State = Owner ? Owner->GetPokemonPlayerState() : nullptr;
	const TArray<FShopOffer>& List = GetCurrentList();
	if (!State || !List.IsValidIndex(ItemSelection))
	{
		return 0;
	}
	const FShopOffer& Offer = List[ItemSelection];
	const int32 Owned = State->GetItemCount(Offer.Item);
	return ListMode == EMode::Buy ? UShopRules::GetMaxBuyCount(Offer.Price, State->Money, Owned) : FMath::Min(Owned, UShopRules::MaxPerPurchase);
}

void UShopWidget::Refresh()
{
	if (!Owner || !TitleText)
	{
		return;
	}
	LastSignature = ComputeSignature();
	const APokemonPlayerState* State = Owner->GetPokemonPlayerState();
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const FShopRow* Shop = Database ? Database->FindShop(Owner->GetOpenShop()) : nullptr;

	Stock = Owner->GetStock();
	Sellable = Owner->GetSellableItems();
	ItemSelection = FMath::Clamp(ItemSelection, 0, FMath::Max(0, GetCurrentList().Num() - 1));
	if (Mode == EMode::Quantity)
	{
		const int32 MaxQuantity = GetMaxQuantity();
		if (MaxQuantity <= 0)
		{
			Mode = ListMode;
		}
		Quantity = FMath::Clamp(Quantity, 1, FMath::Max(1, MaxQuantity));
	}

	TitleText->SetText(Shop ? Shop->DisplayName : LOCTEXT("Shop", "Boutique Pokémon"));
	MoneyText->SetText(FText::Format(LOCTEXT("MoneyLine", "Argent : {0}"), Money(State ? State->Money : 0)));

	// Menu
	static const FText MenuEntries[] = { LOCTEXT("Buy", "Acheter"), LOCTEXT("Sell", "Vendre"), LOCTEXT("Quit", "Quitter") };
	MenuList->ClearChildren();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(MenuEntries); ++Index)
	{
		const bool bSelected = Index == MenuSelection;
		const bool bActive = Mode == EMode::Menu;
		UTextBlock* Entry = MakeText(WidgetTree, 26, bSelected && bActive ? AccentColor : (bSelected ? InkColor : DimColor));
		Entry->SetText(FText::Format(LOCTEXT("MenuEntry", "{0} {1}"), FText::FromString(bSelected ? TEXT("▶") : TEXT("   ")), MenuEntries[Index]));
		MenuList->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 4.f));
	}

	// Liste : le rayon, ou ce que la boutique rachète
	const bool bShowList = Mode != EMode::Menu || MenuSelection < 2;
	const EMode Shown = Mode == EMode::Menu ? (MenuSelection == 1 ? EMode::Sell : EMode::Buy) : ListMode;
	const TArray<FShopOffer>& List = Shown == EMode::Buy ? Stock : Sellable;
	const bool bListActive = Mode == EMode::Buy || Mode == EMode::Sell || Mode == EMode::Quantity;
	ListTitle->SetText(Shown == EMode::Buy ? LOCTEXT("BuyTitle", "Article") : LOCTEXT("SellTitle", "Objet"));
	ListPriceTitle->SetText(Shown == EMode::Buy ? LOCTEXT("PriceTitle", "Prix") : LOCTEXT("BuybackTitle", "Reprise"));
	ItemList->ClearChildren();
	if (bShowList && List.Num() == 0)
	{
		UTextBlock* Empty = MakeText(WidgetTree, 24, DimColor);
		Empty->SetText(Shown == EMode::Buy ? LOCTEXT("NoStock", "Rien en rayon pour l'instant.") : LOCTEXT("NothingToSell", "Rien à vendre dans votre Sac."));
		ItemList->AddChildToVerticalBox(Empty);
	}
	const int32 First = FMath::Clamp(ItemSelection - VisibleRows / 2, 0, FMath::Max(0, List.Num() - VisibleRows));
	for (int32 Index = First; bShowList && Index < FMath::Min(List.Num(), First + VisibleRows); ++Index)
	{
		const FShopOffer& Offer = List[Index];
		const bool bSelected = bListActive && Index == ItemSelection;
		const FLinearColor Color = bSelected ? AccentColor : InkColor;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* NameText = MakeText(WidgetTree, 24, Color);
		NameText->SetText(FText::Format(LOCTEXT("ItemEntry", "{0} {1}"), FText::FromString(bSelected ? TEXT("▶") : TEXT("   ")), GetItemName(this, Offer.Item)));
		Row->AddChildToHorizontalBox(NameText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* Owned = MakeText(WidgetTree, 22, DimColor);
		Owned->SetText(FText::Format(LOCTEXT("Owned", "x{0}"), FText::AsNumber(State ? State->GetItemCount(Offer.Item) : 0)));
		USizeBox* OwnedSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		OwnedSize->SetWidthOverride(110.f);
		OwnedSize->SetContent(Owned);
		Row->AddChildToHorizontalBox(OwnedSize)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Price = MakeText(WidgetTree, 24, Color);
		Price->SetText(Money(Offer.Price));
		Price->SetJustification(ETextJustify::Right);
		USizeBox* PriceSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PriceSize->SetWidthOverride(140.f);
		PriceSize->SetContent(Price);
		Row->AddChildToHorizontalBox(PriceSize);
		ItemList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
	}

	// Quantité et total
	if (Mode == EMode::Quantity && List.IsValidIndex(ItemSelection))
	{
		QuantityText->SetText(FText::Format(LOCTEXT("Quantity", "◀ x{0} ▶     Total : {1}"),
			FText::AsNumber(Quantity), Money(Quantity * List[ItemSelection].Price)));
		QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		QuantityText->SetVisibility(ESlateVisibility::Hidden);
	}

	// Message : la vendeuse, sinon la description de l'objet choisi
	FText Displayed = Message;
	if (Displayed.IsEmpty() && bListActive && List.IsValidIndex(ItemSelection))
	{
		const FItemRow* Row = FindItem(this, List[ItemSelection].Item);
		Displayed = Row ? Row->Description : FText::GetEmpty();
	}
	MessageText->SetText(Displayed);

	OnShopRefreshed();
}

void UShopWidget::ShowResult(bool bSuccess, const FText& InMessage)
{
	Message = InMessage;
	if (Mode == EMode::Waiting || Mode == EMode::Quantity)
	{
		Mode = ListMode;
	}
	Quantity = 1;
	Refresh();
}

void UShopWidget::MoveSelection(int32 Delta)
{
	switch (Mode)
	{
	case EMode::Menu:
		MenuSelection = (MenuSelection + Delta + 3) % 3;
		Message = FText::GetEmpty();
		if (MenuSelection == 2)
		{
			Message = LOCTEXT("QuitHint", "Merci de votre visite !");
		}
		break;
	case EMode::Buy:
	case EMode::Sell:
		if (GetCurrentList().Num() > 0)
		{
			ItemSelection = (ItemSelection + Delta + GetCurrentList().Num()) % GetCurrentList().Num();
		}
		Message = FText::GetEmpty();
		break;
	case EMode::Quantity:
		ChangeQuantity(-Delta);
		return;
	default:
		return;
	}
	Refresh();
}

void UShopWidget::ChangeQuantity(int32 Delta)
{
	const int32 MaxQuantity = GetMaxQuantity();
	if (MaxQuantity <= 0)
	{
		return;
	}
	// Comme dans les jeux : on passe du maximum à 1 et inversement
	if (Quantity + Delta > MaxQuantity)
	{
		Quantity = Quantity == MaxQuantity ? 1 : MaxQuantity;
	}
	else if (Quantity + Delta < 1)
	{
		Quantity = Quantity == 1 ? MaxQuantity : 1;
	}
	else
	{
		Quantity += Delta;
	}
	Refresh();
}

void UShopWidget::Confirm()
{
	if (!Owner)
	{
		return;
	}
	switch (Mode)
	{
	case EMode::Menu:
		if (MenuSelection == 2)
		{
			Owner->CloseShop();
			return;
		}
		ListMode = MenuSelection == 0 ? EMode::Buy : EMode::Sell;
		Mode = ListMode;
		ItemSelection = 0;
		Message = FText::GetEmpty();
		break;

	case EMode::Buy:
	case EMode::Sell:
	{
		const TArray<FShopOffer>& List = GetCurrentList();
		if (!List.IsValidIndex(ItemSelection))
		{
			return;
		}
		if (GetMaxQuantity() <= 0)
		{
			const APokemonPlayerState* State = Owner->GetPokemonPlayerState();
			const bool bBagFull = State && State->GetItemCount(List[ItemSelection].Item) >= UShopRules::MaxItemCount;
			Message = UShopRules::GetErrorText(bBagFull ? EShopError::BagFull : EShopError::NotEnoughMoney);
			break;
		}
		Mode = EMode::Quantity;
		Quantity = 1;
		Message = ListMode == EMode::Buy
			? FText::Format(LOCTEXT("HowManyBuy", "{0} ? Combien en voulez-vous ?"), GetItemName(this, List[ItemSelection].Item))
			: FText::Format(LOCTEXT("HowManySell", "Combien de {0} voulez-vous vendre ?"), GetItemName(this, List[ItemSelection].Item));
		break;
	}

	case EMode::Quantity:
	{
		const TArray<FShopOffer>& List = GetCurrentList();
		if (!List.IsValidIndex(ItemSelection))
		{
			return;
		}
		Mode = EMode::Waiting;
		if (ListMode == EMode::Buy)
		{
			Owner->Buy(List[ItemSelection].Item, Quantity);
		}
		else
		{
			Owner->Sell(List[ItemSelection].Item, Quantity);
		}
		return;
	}

	default:
		return;
	}
	Refresh();
}

void UShopWidget::Back()
{
	switch (Mode)
	{
	case EMode::Menu:
		if (Owner)
		{
			Owner->CloseShop();
		}
		return;
	case EMode::Buy:
	case EMode::Sell:
		Mode = EMode::Menu;
		Message = LOCTEXT("Else", "Puis-je faire autre chose pour vous ?");
		break;
	case EMode::Quantity:
		Mode = ListMode;
		Message = FText::GetEmpty();
		break;
	default:
		return;
	}
	Refresh();
}

FReply UShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::E || Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (!InKeyEvent.IsRepeat())
		{
			Confirm();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (!InKeyEvent.IsRepeat())
		{
			Back();
		}
		return FReply::Handled();
	}

	// ZQSD (clavier AZERTY) comme WASD et les flèches
	if (Key == EKeys::Up || Key == EKeys::Z || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		MoveSelection(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		MoveSelection(1);
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Q || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		if (Mode == EMode::Quantity)
		{
			ChangeQuantity(-10);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		if (Mode == EMode::Quantity)
		{
			ChangeQuantity(10);
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Confirm();
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Back();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UShopWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	MoveSelection(InMouseEvent.GetWheelDelta() > 0.f ? -1 : 1);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
