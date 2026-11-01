#include "UI/StorageWidget.h"
#include "UI/GameFonts.h"
#include "UI/GlassPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Capture/CaptureComponent.h"
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
#include "GameFramework/PlayerController.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Town/PokemonCenterComponent.h"

#define LOCTEXT_NAMESPACE "StorageWidget"

namespace
{
	// Mêmes couleurs que la boîte de dialogue
	// Boîtes en verre (maquettes Figma de L'amie, 2026-11-01) : texte blanc avec ombre
	const FLinearColor InkColor = PokemonGlass::TextColor;
	const FLinearColor DimColor(0.78f, 0.79f, 0.82f);
	const FLinearColor TitleColor(0.13f, 0.25f, 0.45f, 0.97f);
	const FLinearColor AccentColor(1.f, 0.62f, 0.12f);

	/** Lignes visibles de la boîte (elle défile autour de la sélection) */
	constexpr int32 VisibleBoxRows = 10;

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

	/** « Bulbizarre  N.12  PV 30/34 » */
	FText DescribePokemon(const UPokemonDatabase* Database, const FPokemonInstance& Pokemon)
	{
		const FText Hp = Pokemon.IsFainted() ? LOCTEXT("KO", "K.O.")
			: FText::Format(LOCTEXT("Hp", "PV {0}/{1}"), FText::AsNumber(Pokemon.CurrentHP), FText::AsNumber(Pokemon.GetMaxHP()));
		return FText::Format(LOCTEXT("PokemonLine", "{0}   N.{1}   {2}"), UPokemonLibrary::GetDisplayName(Database, Pokemon),
			FText::AsNumber(Pokemon.Level), Hp);
	}

	FText GetActionName(int32 Action)
	{
		switch (Action)
		{
		case 0: return LOCTEXT("Deposit", "Déposer");
		case 1: return LOCTEXT("Swap", "Échanger");
		case 2: return LOCTEXT("Withdraw", "Retirer");
		case 3: return LOCTEXT("Release", "Relâcher");
		case 4: return LOCTEXT("ConfirmRelease", "Oui, relâcher");
		default: return LOCTEXT("Cancel", "Annuler");
		}
	}
}

void UStorageWidget::Init(UPokemonCenterComponent* InOwner)
{
	Owner = InOwner;
	Message = LOCTEXT("Welcome", "Système de stockage de Pokémon de Léo. Que voulez-vous faire ?");
	SetIsFocusable(true);
	if (UCaptureComponent* Capture = GetCapture())
	{
		Capture->OnStorageRefused.AddUniqueDynamic(this, &UStorageWidget::HandleStorageRefused);
	}
}

void UStorageWidget::NativeDestruct()
{
	if (UCaptureComponent* Capture = GetCapture())
	{
		Capture->OnStorageRefused.RemoveDynamic(this, &UStorageWidget::HandleStorageRefused);
	}
	Super::NativeDestruct();
}

UCaptureComponent* UStorageWidget::GetCapture() const
{
	const AActor* Controller = Owner ? Owner->GetOwner() : nullptr;
	return Controller ? Controller->FindComponentByClass<UCaptureComponent>() : nullptr;
}

const TArray<FPokemonInstance>* UStorageWidget::GetParty() const
{
	const APlayerController* Controller = Owner ? Cast<APlayerController>(Owner->GetOwner()) : nullptr;
	const APokemonPlayerState* State = Controller ? Controller->GetPlayerState<APokemonPlayerState>() : nullptr;
	return State ? &State->Party : nullptr;
}

const TArray<FPokemonInstance>* UStorageWidget::GetStorage() const
{
	const APlayerController* Controller = Owner ? Cast<APlayerController>(Owner->GetOwner()) : nullptr;
	const APokemonPlayerState* State = Controller ? Controller->GetPlayerState<APokemonPlayerState>() : nullptr;
	return State ? &State->StoragePC : nullptr;
}

TSharedRef<SWidget> UStorageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
		Width->SetWidthOverride(1300.f);
		UOverlaySlot* WidthSlot = Root->AddChildToOverlay(Width);
		WidthSlot->SetHorizontalAlignment(HAlign_Center);
		WidthSlot->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Inside"));
		Width->SetContent(Inside);

		UBorder* Header = MakeBox(WidgetTree, TitleColor, FMargin(28.f, 12.f));
		Inside->AddChildToVerticalBox(Header);
		UTextBlock* Title = MakeText(WidgetTree, 26, FLinearColor::White);
		Title->SetText(LOCTEXT("Title", "PC de Léo"));
		Header->SetContent(Title);

		UHorizontalBox* Middle = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Middle"));
		Inside->AddChildToVerticalBox(Middle)->SetPadding(FMargin(0.f, 10.f));

		// Équipe
		USizeBox* PartySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PartySize->SetWidthOverride(520.f);
		PartySize->SetMinDesiredHeight(480.f);
		Middle->AddChildToHorizontalBox(PartySize)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		UWidget* PartyBoxFrame = nullptr;
		UBorder* PartyBox = MakeGlassBox(WidgetTree, FMargin(24.f, 16.f), PartyBoxFrame);
		PartySize->SetContent(PartyBoxFrame);
		UVerticalBox* PartyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PartyBox->SetContent(PartyColumn);
		UTextBlock* PartyTitle = MakeText(WidgetTree, 22, DimColor);
		PartyTitle->SetText(LOCTEXT("PartyTitle", "Équipe"));
		PartyColumn->AddChildToVerticalBox(PartyTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		PartyList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PartyList"));
		PartyColumn->AddChildToVerticalBox(PartyList);
		ActionList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ActionList"));
		UVerticalBoxSlot* ActionSlot = PartyColumn->AddChildToVerticalBox(ActionList);
		ActionSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));

		// Boîte
		USizeBox* BoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BoxSize->SetMinDesiredHeight(480.f);
		Middle->AddChildToHorizontalBox(BoxSize)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UWidget* BoxPanelFrame = nullptr;
		UBorder* BoxPanel = MakeGlassBox(WidgetTree, FMargin(24.f, 16.f), BoxPanelFrame);
		BoxSize->SetContent(BoxPanelFrame);
		UVerticalBox* BoxColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		BoxPanel->SetContent(BoxColumn);
		BoxTitle = MakeText(WidgetTree, 22, DimColor, TEXT("BoxTitle"));
		BoxColumn->AddChildToVerticalBox(BoxTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		BoxList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BoxList"));
		BoxColumn->AddChildToVerticalBox(BoxList);

		// Message
		USizeBox* MessageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MessageSize->SetMinDesiredHeight(110.f);
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

int32 UStorageWidget::ComputeSignature() const
{
	uint32 Signature = 0;
	for (const TArray<FPokemonInstance>* List : { GetParty(), GetStorage() })
	{
		Signature = HashCombine(Signature, ::GetTypeHash(List ? List->Num() : -1));
		if (List)
		{
			for (const FPokemonInstance& Pokemon : *List)
			{
				Signature = HashCombine(Signature, HashCombine(GetTypeHash(Pokemon.Species), HashCombine(::GetTypeHash(Pokemon.CurrentHP),
					HashCombine(::GetTypeHash(Pokemon.Level), GetTypeHash(Pokemon.Nickname)))));
			}
		}
	}
	return static_cast<int32>(Signature);
}

void UStorageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Équipe et PC répliqués : on redessine quand la réponse du serveur arrive
	if (ComputeSignature() != LastSignature)
	{
		Refresh();
	}
}

TArray<UStorageWidget::EAction> UStorageWidget::GetActions() const
{
	if (Column == EColumn::Party)
	{
		const TArray<FPokemonInstance>* Party = GetParty();
		if (!Party || !Party->IsValidIndex(PartySelection))
		{
			return {};
		}
		return { EAction::Deposit, EAction::Swap, EAction::Cancel };
	}
	const TArray<FPokemonInstance>* Storage = GetStorage();
	if (!Storage || !Storage->IsValidIndex(Box * UPokemonStorage::BoxSize + BoxSelection))
	{
		return {};
	}
	return { EAction::Withdraw, EAction::Release, EAction::Cancel };
}

void UStorageWidget::Refresh()
{
	if (!PartyList)
	{
		return;
	}
	LastSignature = ComputeSignature();
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const TArray<FPokemonInstance>* Party = GetParty();
	const TArray<FPokemonInstance>* Storage = GetStorage();
	const int32 PartyCount = Party ? Party->Num() : 0;
	PartySelection = FMath::Clamp(PartySelection, 0, FMath::Max(0, PartyCount - 1));

	// Équipe : 6 places
	PartyList->ClearChildren();
	for (int32 Index = 0; Index < UPokemonStorage::MaxPartySize; ++Index)
	{
		const bool bSelected = Column == EColumn::Party && Index == PartySelection;
		const bool bSwapping = Index == SwapPartyIndex;
		UTextBlock* Entry = MakeText(WidgetTree, 22, bSelected ? AccentColor : (Index < PartyCount ? InkColor : DimColor));
		const FText Line = Index < PartyCount ? DescribePokemon(Database, (*Party)[Index]) : LOCTEXT("EmptyParty", "-");
		Entry->SetText(FText::Format(LOCTEXT("Entry", "{0} {1}"), FText::FromString(bSelected ? TEXT("▶") : (bSwapping ? TEXT("⇄") : TEXT("   "))), Line));
		PartyList->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 5.f));
	}

	// Boîte : 30 places, la liste défile
	const int32 StorageCount = Storage ? Storage->Num() : 0;
	const int32 InBox = FMath::Clamp(StorageCount - Box * UPokemonStorage::BoxSize, 0, UPokemonStorage::BoxSize);
	BoxTitle->SetText(FText::Format(LOCTEXT("BoxTitle", "◀ Boîte {0} / {1} ▶      {2} / {3} Pokémon"),
		FText::AsNumber(Box + 1), FText::AsNumber(UPokemonStorage::BoxCount), FText::AsNumber(InBox), FText::AsNumber(UPokemonStorage::BoxSize)));
	BoxList->ClearChildren();
	const int32 First = FMath::Clamp(BoxSelection - VisibleBoxRows / 2, 0, UPokemonStorage::BoxSize - VisibleBoxRows);
	for (int32 SlotIndex = First; SlotIndex < First + VisibleBoxRows; ++SlotIndex)
	{
		const int32 Index = Box * UPokemonStorage::BoxSize + SlotIndex;
		const bool bSelected = Column == EColumn::Box && SlotIndex == BoxSelection;
		const bool bFilled = Storage && Storage->IsValidIndex(Index);
		UTextBlock* Entry = MakeText(WidgetTree, 22, bSelected ? AccentColor : (bFilled ? InkColor : DimColor));
		const FText Line = bFilled ? DescribePokemon(Database, (*Storage)[Index]) : LOCTEXT("EmptySlot", "-");
		Entry->SetText(FText::Format(LOCTEXT("BoxEntry", "{0} {1}. {2}"), FText::FromString(bSelected ? TEXT("▶") : TEXT("   ")),
			FText::AsNumber(SlotIndex + 1), Line));
		BoxList->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 3.f));
	}

	// Actions
	ActionList->ClearChildren();
	if (bActionsOpen)
	{
		ActionSelection = FMath::Clamp(ActionSelection, 0, FMath::Max(0, Actions.Num() - 1));
		for (int32 Index = 0; Index < Actions.Num(); ++Index)
		{
			const bool bSelected = Index == ActionSelection;
			UTextBlock* Entry = MakeText(WidgetTree, 24, bSelected ? AccentColor : InkColor);
			Entry->SetText(FText::Format(LOCTEXT("Action", "{0} {1}"), FText::FromString(bSelected ? TEXT("▶") : TEXT("   ")),
				GetActionName(static_cast<int32>(Actions[Index]))));
			ActionList->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 3.f));
		}
	}

	MessageText->SetText(Message);
	OnStorageRefreshed();
}

void UStorageWidget::MoveSelection(int32 Delta)
{
	if (bActionsOpen)
	{
		if (Actions.Num() > 0)
		{
			ActionSelection = (ActionSelection + Delta + Actions.Num()) % Actions.Num();
		}
	}
	else if (Column == EColumn::Party)
	{
		const int32 Count = GetParty() ? GetParty()->Num() : 0;
		if (Count > 0)
		{
			PartySelection = (PartySelection + Delta + Count) % Count;
		}
	}
	else
	{
		BoxSelection = (BoxSelection + Delta + UPokemonStorage::BoxSize) % UPokemonStorage::BoxSize;
	}
	Refresh();
}

void UStorageWidget::ChangeColumn()
{
	if (bActionsOpen || SwapPartyIndex != INDEX_NONE)
	{
		return;
	}
	Column = Column == EColumn::Party ? EColumn::Box : EColumn::Party;
	Refresh();
}

void UStorageWidget::ChangeBox(int32 Delta)
{
	if (bActionsOpen)
	{
		return;
	}
	Box = (Box + Delta + UPokemonStorage::BoxCount) % UPokemonStorage::BoxCount;
	Refresh();
}

void UStorageWidget::Confirm()
{
	if (bActionsOpen)
	{
		if (Actions.IsValidIndex(ActionSelection))
		{
			RunAction(Actions[ActionSelection]);
		}
		return;
	}

	// Échange : on vient de choisir le Pokémon du PC
	if (SwapPartyIndex != INDEX_NONE && Column == EColumn::Box)
	{
		const int32 Index = Box * UPokemonStorage::BoxSize + BoxSelection;
		if (UCaptureComponent* Capture = GetCapture(); Capture && GetStorage() && GetStorage()->IsValidIndex(Index))
		{
			Capture->SwapWithStorage(SwapPartyIndex, Index);
			Message = LOCTEXT("Swapped", "Les deux Pokémon ont changé de place.");
		}
		SwapPartyIndex = INDEX_NONE;
		Column = EColumn::Party;
		Refresh();
		return;
	}

	Actions = GetActions();
	if (Actions.Num() > 0)
	{
		bActionsOpen = true;
		ActionSelection = 0;
		Message = LOCTEXT("WhatToDo", "Que faire de ce Pokémon ?");
		Refresh();
	}
}

void UStorageWidget::RunAction(EAction Action)
{
	UCaptureComponent* Capture = GetCapture();
	const int32 StorageIndex = Box * UPokemonStorage::BoxSize + BoxSelection;
	bActionsOpen = false;
	switch (Action)
	{
	case EAction::Deposit:
		if (Capture)
		{
			Capture->DepositPokemon(PartySelection);
			Message = LOCTEXT("Deposited", "Le Pokémon a été déposé dans le PC.");
		}
		break;
	case EAction::Swap:
		SwapPartyIndex = PartySelection;
		Column = EColumn::Box;
		Message = LOCTEXT("ChooseSwap", "Avec quel Pokémon du PC l'échanger ?");
		break;
	case EAction::Withdraw:
		if (Capture)
		{
			Capture->WithdrawPokemon(StorageIndex);
			Message = LOCTEXT("Withdrawn", "Le Pokémon a rejoint l'équipe.");
		}
		break;
	case EAction::Release:
		// Définitif : on redemande
		bActionsOpen = true;
		Actions = { EAction::ConfirmRelease, EAction::Cancel };
		ActionSelection = 1;
		Message = LOCTEXT("ReallyRelease", "Relâcher ce Pokémon ? Vous ne le reverrez plus.");
		break;
	case EAction::ConfirmRelease:
		if (Capture)
		{
			Capture->ReleasePokemon(StorageIndex);
			Message = LOCTEXT("Released", "Le Pokémon a été relâché. Bye bye !");
		}
		break;
	case EAction::Cancel:
		Message = FText::GetEmpty();
		break;
	}
	Refresh();
}

void UStorageWidget::HandleStorageRefused(EPokemonStorageError Error)
{
	Message = UPokemonStorage::GetErrorText(Error);
	Refresh();
}

void UStorageWidget::Back()
{
	if (bActionsOpen)
	{
		bActionsOpen = false;
		Message = FText::GetEmpty();
		Refresh();
		return;
	}
	if (SwapPartyIndex != INDEX_NONE)
	{
		SwapPartyIndex = INDEX_NONE;
		Column = EColumn::Party;
		Message = FText::GetEmpty();
		Refresh();
		return;
	}
	if (Owner)
	{
		Owner->ClosePC();
	}
}

FReply UStorageWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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
	if (Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Q || Key == EKeys::A || Key == EKeys::D
		|| Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right
		|| Key == EKeys::Gamepad_LeftStick_Left || Key == EKeys::Gamepad_LeftStick_Right)
	{
		ChangeColumn();
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_LeftShoulder)
	{
		ChangeBox(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::PageDown || Key == EKeys::Tab || Key == EKeys::Gamepad_RightShoulder)
	{
		ChangeBox(1);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UStorageWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UStorageWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	MoveSelection(InMouseEvent.GetWheelDelta() > 0.f ? -1 : 1);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
