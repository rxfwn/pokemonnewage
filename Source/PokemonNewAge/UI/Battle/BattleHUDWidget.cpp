#include "UI/Battle/BattleHUDWidget.h"
#include "Battle/BattleComponent.h"
#include "Battle/BattleText.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/PokemonSaveGame.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "PokemonNewAgePlayerController.h"

#define LOCTEXT_NAMESPACE "BattleHUD"

namespace BattleHUD
{
	const FLinearColor Panel(0.02f, 0.025f, 0.035f, 0.82f);
	const FLinearColor Text(0.95f, 0.96f, 0.97f, 1.f);
	const FLinearColor Dim(0.6f, 0.65f, 0.7f, 1.f);
	const FLinearColor Good(0.3f, 0.85f, 0.4f, 1.f);
	const FLinearColor Warn(0.98f, 0.75f, 0.2f, 1.f);
	const FLinearColor Bad(0.95f, 0.25f, 0.2f, 1.f);
	const FLinearColor Exp(0.25f, 0.6f, 1.f, 1.f);

	const FLinearColor ActionColors[] = {
		FLinearColor(0.85f, 0.25f, 0.2f),   // Attaque
		FLinearColor(0.95f, 0.65f, 0.15f),  // Sac
		FLinearColor(0.3f, 0.75f, 0.35f),   // Pokémon
		FLinearColor(0.3f, 0.5f, 0.9f),     // Fuite
	};

	FLinearColor TypeColor(EPokemonType Type)
	{
		switch (Type)
		{
		case EPokemonType::Fire:     return FLinearColor(0.93f, 0.4f, 0.15f);
		case EPokemonType::Water:    return FLinearColor(0.3f, 0.5f, 0.95f);
		case EPokemonType::Electric: return FLinearColor(0.9f, 0.72f, 0.1f);
		case EPokemonType::Grass:    return FLinearColor(0.35f, 0.7f, 0.3f);
		case EPokemonType::Ice:      return FLinearColor(0.45f, 0.8f, 0.82f);
		case EPokemonType::Fighting: return FLinearColor(0.72f, 0.18f, 0.15f);
		case EPokemonType::Poison:   return FLinearColor(0.6f, 0.25f, 0.6f);
		case EPokemonType::Ground:   return FLinearColor(0.8f, 0.65f, 0.3f);
		case EPokemonType::Flying:   return FLinearColor(0.58f, 0.52f, 0.9f);
		case EPokemonType::Psychic:  return FLinearColor(0.95f, 0.33f, 0.5f);
		case EPokemonType::Bug:      return FLinearColor(0.58f, 0.68f, 0.12f);
		case EPokemonType::Rock:     return FLinearColor(0.68f, 0.58f, 0.25f);
		case EPokemonType::Ghost:    return FLinearColor(0.42f, 0.35f, 0.6f);
		case EPokemonType::Dragon:   return FLinearColor(0.42f, 0.22f, 0.95f);
		case EPokemonType::Dark:     return FLinearColor(0.42f, 0.35f, 0.28f);
		case EPokemonType::Steel:    return FLinearColor(0.65f, 0.65f, 0.75f);
		case EPokemonType::Fairy:    return FLinearColor(0.9f, 0.55f, 0.65f);
		default:                     return FLinearColor(0.5f, 0.5f, 0.42f);
		}
	}

	FLinearColor Darken(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, 1.f);
	}

	FLinearColor HPColor(float Ratio)
	{
		return Ratio > 0.5f ? Good : (Ratio > 0.2f ? Warn : Bad);
	}

	FText StatusText(EMajorStatus Status)
	{
		switch (Status)
		{
		case EMajorStatus::Paralysis:     return LOCTEXT("PAR", "PAR");
		case EMajorStatus::Sleep:         return LOCTEXT("SLP", "SOM");
		case EMajorStatus::Freeze:        return LOCTEXT("FRZ", "GEL");
		case EMajorStatus::Burn:          return LOCTEXT("BRN", "BRU");
		case EMajorStatus::Poison:
		case EMajorStatus::BadlyPoisoned: return LOCTEXT("PSN", "PSN");
		default:                          return FText::GetEmpty();
		}
	}

	/** Événements qui changent les PV du Pokémon concerné (HPAfter est alors valable) */
	bool ChangesHP(EBattleEventType Type)
	{
		switch (Type)
		{
		case EBattleEventType::Damage:
		case EBattleEventType::Heal:
		case EBattleEventType::Recoil:
		case EBattleEventType::StatusDamage:
		case EBattleEventType::HurtByConfusion:
		case EBattleEventType::LeechSeedDrain:
		case EBattleEventType::TrapDamage:
			return true;
		default:
			return false;
		}
	}
}

using namespace BattleHUD;

void UBattleHUDButtonBinding::HandleClicked()
{
	if (UBattleHUDWidget* Owner = HUD.Get())
	{
		Owner->OnButton(Action, Index);
	}
}

// ---------------------------------------------------------------- Construction

TSharedRef<SWidget> UBattleHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Cadre de l'adversaire en haut à gauche, celui du joueur à droite au-dessus du texte
		BuildInfoBox(Boxes[1], false);
		UOverlaySlot* EnemySlot = Root->AddChildToOverlay(Boxes[1].Frame);
		EnemySlot->SetHorizontalAlignment(HAlign_Left);
		EnemySlot->SetVerticalAlignment(VAlign_Top);
		EnemySlot->SetPadding(FMargin(60.f, 50.f, 0.f, 0.f));

		BuildInfoBox(Boxes[0], true);
		UOverlaySlot* PlayerSlot = Root->AddChildToOverlay(Boxes[0].Frame);
		PlayerSlot->SetHorizontalAlignment(HAlign_Right);
		PlayerSlot->SetVerticalAlignment(VAlign_Bottom);
		PlayerSlot->SetPadding(FMargin(0.f, 0.f, 60.f, 250.f));

		// Boîte de texte en bas
		UBorder* MessageFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MessageFrame"));
		MessageFrame->SetBrush(FSlateRoundedBoxBrush(Panel, 14.f));
		MessageFrame->SetPadding(FMargin(32.f, 22.f));
		UOverlaySlot* MessageSlot = Root->AddChildToOverlay(MessageFrame);
		MessageSlot->SetHorizontalAlignment(HAlign_Fill);
		MessageSlot->SetVerticalAlignment(VAlign_Bottom);
		MessageSlot->SetPadding(FMargin(60.f, 0.f, 60.f, 40.f));

		USizeBox* MessageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MessageSize->SetMinDesiredHeight(150.f);
		MessageFrame->SetContent(MessageSize);
		UHorizontalBox* MessageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		MessageSize->SetContent(MessageRow);
		MessageText = MakeText(FText::GetEmpty(), 30, Text);
		MessageText->SetAutoWrapText(true);
		UHorizontalBoxSlot* TextSlot = MessageRow->AddChildToHorizontalBox(MessageText);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MessageArrow = MakeText(FText::FromString(TEXT("v")), 26, Warn, true);
		UHorizontalBoxSlot* ArrowSlot = MessageRow->AddChildToHorizontalBox(MessageArrow);
		ArrowSlot->SetVerticalAlignment(VAlign_Bottom);

		// Menu des choix, à droite au-dessus de la boîte de texte
		MenuFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuFrame"));
		MenuFrame->SetBrush(FSlateRoundedBoxBrush(Panel, 14.f));
		MenuFrame->SetPadding(FMargin(16.f));
		UOverlaySlot* MenuSlot = Root->AddChildToOverlay(MenuFrame);
		MenuSlot->SetHorizontalAlignment(HAlign_Right);
		MenuSlot->SetVerticalAlignment(VAlign_Bottom);
		MenuSlot->SetPadding(FMargin(0.f, 0.f, 80.f, 210.f));
		MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
		MenuFrame->SetContent(MenuBox);
		MenuFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetIsFocusable(true);
	return Super::RebuildWidget();
}

void UBattleHUDWidget::BuildInfoBox(FInfoBox& Box, bool bPlayer)
{
	Box.Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box.Frame->SetBrush(FSlateRoundedBoxBrush(Panel, 12.f));
	Box.Frame->SetPadding(FMargin(22.f, 14.f));
	Box.Frame->SetVisibility(ESlateVisibility::Hidden);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(440.f);
	Box.Frame->SetContent(Size);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Size->SetContent(Column);

	UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Top);
	Box.Name = MakeText(FText::GetEmpty(), 28, Text, true);
	Top->AddChildToHorizontalBox(Box.Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Box.Status = MakeText(FText::GetEmpty(), 20, Bad, true);
	UHorizontalBoxSlot* StatusSlot = Top->AddChildToHorizontalBox(Box.Status);
	StatusSlot->SetVerticalAlignment(VAlign_Center);
	StatusSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	Box.Level = MakeText(FText::GetEmpty(), 24, Text, true);
	Top->AddChildToHorizontalBox(Box.Level)->SetVerticalAlignment(VAlign_Center);

	UHorizontalBox* HPRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* HPRowSlot = Column->AddChildToVerticalBox(HPRow);
	HPRowSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	HPRow->AddChildToHorizontalBox(MakeText(LOCTEXT("HP", "PV  "), 18, Warn, true))->SetVerticalAlignment(VAlign_Center);
	Box.HPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarSize->SetHeightOverride(14.f);
	BarSize->SetContent(Box.HPBar);
	UHorizontalBoxSlot* BarSlot = HPRow->AddChildToHorizontalBox(BarSize);
	BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BarSlot->SetVerticalAlignment(VAlign_Center);

	if (bPlayer)
	{
		Box.HPText = MakeText(FText::GetEmpty(), 22, Text, true);
		Box.HPText->SetJustification(ETextJustify::Right);
		Column->AddChildToVerticalBox(Box.HPText)->SetHorizontalAlignment(HAlign_Right);

		Box.ExpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Box.ExpBar->SetFillColorAndOpacity(Exp);
		USizeBox* ExpSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ExpSize->SetHeightOverride(6.f);
		ExpSize->SetContent(Box.ExpBar);
		Column->AddChildToVerticalBox(ExpSize)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}
}

UTextBlock* UBattleHUDWidget::MakeText(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(InText);
	Block->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	Block->SetFont(Font);
	return Block;
}

const UPokemonDatabase* UBattleHUDWidget::GetDatabase() const
{
	return UPokemonDataSubsystem::GetDatabase(this);
}

// ---------------------------------------------------------------- Combat

void UBattleHUDWidget::Begin(UBattleComponent* InBattle, const FBattleSnapshot& InSnapshot)
{
	Battle = InBattle;
	Snapshot = InSnapshot;
	PreviousSnapshot = InSnapshot;

	// Vitesse du texte (Options)
	const int32 Speed = UPokemonSaveLibrary::GetOptions()->TextSpeed;
	CharsPerSecond = Speed == 0 ? 30.f : (Speed == 2 ? 140.f : 60.f);
	HoldTime = Speed == 0 ? 1.3f : (Speed == 2 ? 0.45f : 0.85f);

	Battle->OnBattleEvents.AddDynamic(this, &UBattleHUDWidget::HandleEvents);
	Battle->OnBattleEnded.AddDynamic(this, &UBattleHUDWidget::HandleEnded);
	Battle->OnActionRefused.AddDynamic(this, &UBattleHUDWidget::HandleRefused);
	Battle->OnMovesToLearn.AddDynamic(this, &UBattleHUDWidget::HandleMovesToLearn);
}

void UBattleHUDWidget::HandleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& InSnapshot)
{
	if (Queue.Num() == 0 && !bMessageActive)
	{
		PreviousSnapshot = Snapshot;
	}
	Snapshot = InSnapshot;
	Queue.Append(Events);
	bWaitingForServer = false;
	ClearMenu();
	if (!bMessageActive)
	{
		StartNextEvent();
	}
}

void UBattleHUDWidget::HandleEnded(const FBattleResult& InResult)
{
	bEnded = true;
	Result = InResult;
	EvolutionStep = 0;
	if (Queue.Num() == 0 && !bMessageActive)
	{
		ShowMenuForPhase();
	}
}

void UBattleHUDWidget::HandleRefused(EBattleActionError Error)
{
	bWaitingForServer = false;
	ShowMessage(UBattleText::GetActionErrorText(Error));
}

void UBattleHUDWidget::HandleMovesToLearn(int32 PartyIndex, const TArray<FName>& Moves)
{
	for (const FName Move : Moves)
	{
		MovesToLearn.Emplace(PartyIndex, Move);
	}
	bWaitingForServer = false;
	if (!bMessageActive && Queue.Num() == 0)
	{
		ShowMenuForPhase();
	}
}

void UBattleHUDWidget::StartNextEvent()
{
	while (Queue.Num() > 0)
	{
		const FBattleEvent Event = Queue[0];
		Queue.RemoveAt(0);
		ApplyEventVisuals(Event);
		const FText EventText = UBattleText::GetEventText(this, Snapshot, Event);
		if (!EventText.IsEmpty())
		{
			ShowMessage(EventText);
			return;
		}
	}

	// Tous les messages sont passés : l'état final du serveur fait foi
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Snapshot.Sides.IsValidIndex(Side) && ShownIndex[Side] >= 0)
		{
			ShownIndex[Side] = Snapshot.Sides[Side].ActiveIndex;
			RefreshInfoBox(Side, false);
		}
	}
	ShowMenuForPhase();
}

void UBattleHUDWidget::ApplyEventVisuals(const FBattleEvent& Event)
{
	const int32 Side = Event.Side;
	if (Side < 0 || Side > 1)
	{
		return;
	}
	switch (Event.Type)
	{
	case EBattleEventType::SentOut:
	{
		// PV au moment où il sort : ceux d'avant ce lot d'événements
		ShownIndex[Side] = Event.PartyIndex;
		const FPokemonInstance* Before = PreviousSnapshot.Sides.IsValidIndex(Side) && PreviousSnapshot.Sides[Side].Party.IsValidIndex(Event.PartyIndex)
			? &PreviousSnapshot.Sides[Side].Party[Event.PartyIndex] : nullptr;
		ShownLevel[Side] = 0;
		RefreshInfoBox(Side, true);
		if (Before && Before->Species == Event.Species)
		{
			DisplayedHP[Side] = TargetHP[Side] = Before->CurrentHP;
			ShownLevel[Side] = Before->Level;
			Boxes[Side].Level->SetText(FText::Format(LOCTEXT("Level", "N.{0}"), Before->Level));
		}
		Boxes[Side].Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		break;
	}
	case EBattleEventType::Withdrew:
	case EBattleEventType::ForcedOut:
		Boxes[Side].Frame->SetVisibility(ESlateVisibility::Hidden);
		break;
	case EBattleEventType::Fainted:
		TargetHP[Side] = 0.f;
		break;
	case EBattleEventType::LevelUp:
		if (Event.PartyIndex == ShownIndex[Side])
		{
			ShownLevel[Side] = Event.Amount;
			Boxes[Side].Level->SetText(FText::Format(LOCTEXT("Level", "N.{0}"), Event.Amount));
		}
		break;
	case EBattleEventType::Caught:
		Boxes[1].Frame->SetVisibility(ESlateVisibility::Hidden);
		break;
	default:
		if (ChangesHP(Event.Type) && Event.PartyIndex == ShownIndex[Side])
		{
			TargetHP[Side] = Event.HPAfter;
		}
		break;
	}
}

const FPokemonInstance* UBattleHUDWidget::GetDisplayedPokemon(int32 Side) const
{
	if (!Snapshot.Sides.IsValidIndex(Side) || !Snapshot.Sides[Side].Party.IsValidIndex(ShownIndex[Side]))
	{
		return nullptr;
	}
	return &Snapshot.Sides[Side].Party[ShownIndex[Side]];
}

void UBattleHUDWidget::RefreshInfoBox(int32 Side, bool bInstantHP)
{
	const FPokemonInstance* Pokemon = GetDisplayedPokemon(Side);
	if (!Pokemon)
	{
		return;
	}
	FInfoBox& Box = Boxes[Side];
	const UPokemonDatabase* Database = GetDatabase();
	Box.Name->SetText(UPokemonLibrary::GetDisplayName(Database, *Pokemon));
	const int32 Level = ShownLevel[Side] > 0 && bInstantHP ? ShownLevel[Side] : Pokemon->Level;
	ShownLevel[Side] = Level;
	Box.Level->SetText(FText::Format(LOCTEXT("Level", "N.{0}"), Level));
	Box.Status->SetText(Pokemon->IsFainted() ? LOCTEXT("KO", "K.O.") : StatusText(Pokemon->Status));
	MaxHP[Side] = FMath::Max(1, Pokemon->GetMaxHP());
	TargetHP[Side] = Pokemon->CurrentHP;
	if (bInstantHP)
	{
		DisplayedHP[Side] = TargetHP[Side];
	}

	if (Box.ExpBar && Database)
	{
		if (const FSpeciesRow* Species = Database->FindSpecies(Pokemon->Species))
		{
			const int32 ThisLevel = Database->GetExperienceForLevel(Species->GrowthRate, Pokemon->Level);
			const int32 NextLevel = Database->GetExperienceForLevel(Species->GrowthRate, FMath::Min(Pokemon->Level + 1, 100));
			Box.ExpBar->SetPercent(NextLevel > ThisLevel ? FMath::Clamp(float(Pokemon->Experience - ThisLevel) / float(NextLevel - ThisLevel), 0.f, 1.f) : 1.f);
		}
	}
}

// ---------------------------------------------------------------- Boîte de texte

void UBattleHUDWidget::ShowMessage(const FText& InText)
{
	FullMessage = InText.ToString();
	MessageChars = 0.f;
	MessageHold = 0.f;
	bMessageActive = true;
	MessageText->SetText(FText::GetEmpty());
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
}

bool UBattleHUDWidget::IsMessageBusy() const
{
	return bMessageActive;
}

void UBattleHUDWidget::AdvanceMessage()
{
	if (!bMessageActive)
	{
		return;
	}
	// Premier appui : tout le texte d'un coup ; deuxième : message suivant
	if (MessageChars < FullMessage.Len())
	{
		MessageChars = FullMessage.Len();
		return;
	}
	bMessageActive = false;
	StartNextEvent();
}

void UBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Machine à écrire, puis le message suivant tout seul après un temps (ou au clic)
	if (bMessageActive)
	{
		const int32 Before = FMath::FloorToInt(MessageChars);
		MessageChars = FMath::Min(MessageChars + CharsPerSecond * InDeltaTime, float(FullMessage.Len()));
		const int32 After = FMath::FloorToInt(MessageChars);
		if (After != Before || After == 0)
		{
			MessageText->SetText(FText::FromString(FullMessage.Left(After)));
		}
		const bool bBarsMoving = FMath::Abs(DisplayedHP[0] - TargetHP[0]) > 0.5f || FMath::Abs(DisplayedHP[1] - TargetHP[1]) > 0.5f;
		if (After >= FullMessage.Len() && !bBarsMoving)
		{
			MessageArrow->SetVisibility(Queue.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			MessageHold += InDeltaTime;
			if (MessageHold >= HoldTime)
			{
				AdvanceMessage();
			}
		}
	}

	// Barres de PV : une barre pleine se vide en une seconde environ
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Speed = FMath::Max(MaxHP[Side] * 0.9f, 20.f);
		DisplayedHP[Side] = FMath::FInterpConstantTo(DisplayedHP[Side], TargetHP[Side], InDeltaTime, Speed);
		const float Ratio = FMath::Clamp(DisplayedHP[Side] / MaxHP[Side], 0.f, 1.f);
		if (Boxes[Side].HPBar)
		{
			Boxes[Side].HPBar->SetPercent(Ratio);
			Boxes[Side].HPBar->SetFillColorAndOpacity(HPColor(Ratio));
		}
		if (Boxes[Side].HPText)
		{
			Boxes[Side].HPText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(DisplayedHP[Side]), FMath::RoundToInt(MaxHP[Side]))));
		}
	}

	if (CloseTimer > 0.f)
	{
		CloseTimer -= InDeltaTime;
		if (CloseTimer <= 0.f)
		{
			FinishBattle();
		}
	}
}

// ---------------------------------------------------------------- Choix du joueur

void UBattleHUDWidget::ShowMenuForPhase()
{
	if (bWaitingForServer)
	{
		return;
	}

	// Fin du combat : évolutions puis attaques à apprendre, puis on ferme
	if (bEnded)
	{
		if (Result.Evolutions.IsValidIndex(EvolutionStep))
		{
			ShowEvolution();
			return;
		}
		if (MovesToLearn.Num() > 0)
		{
			ShowForgetMove(MovesToLearn[0].Key, MovesToLearn[0].Value, true);
			return;
		}
		ClearMenu();
		// Un peu d'attente : après une évolution, le serveur peut encore proposer une attaque
		if (CloseTimer < 0.f)
		{
			CloseTimer = 1.f;
		}
		return;
	}

	const int32 Local = Snapshot.LocalSide;
	switch (Snapshot.Phase)
	{
	case EBattlePhase::WaitingForActions:
		ShowMainMenu();
		break;
	case EBattlePhase::WaitingForReplacement:
		if (Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].GetActive() && Snapshot.Sides[Local].GetActive()->IsFainted())
		{
			ShowParty(EPartyMode::Replacement);
		}
		break;
	case EBattlePhase::WaitingForMoveLearn:
		ShowForgetMove(Snapshot.PendingLearnPartyIndex, Snapshot.PendingLearnMove, false);
		break;
	default:
		break;
	}
}

void UBattleHUDWidget::ClearMenu()
{
	Menu = EMenu::None;
	MenuBox->ClearChildren();
	Bindings.Reset();
	FirstButton = nullptr;
	MenuFrame->SetVisibility(ESlateVisibility::Collapsed);
}

UButton* UBattleHUDWidget::AddMenuButton(UPanelWidget* Panel, const FText& InText, EBattleHUDAction Action, int32 Index,
	const FLinearColor& Color, bool bEnabled, const FText& SubText)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	Style.Normal = FSlateRoundedBoxBrush(Darken(Color, 0.55f), 10.f);
	Style.Hovered = FSlateRoundedBoxBrush(Darken(Color, 1.f), 10.f, FLinearColor::White, 2.f);
	Style.Pressed = FSlateRoundedBoxBrush(Darken(Color, 0.4f), 10.f);
	Style.Disabled = FSlateRoundedBoxBrush(FLinearColor(0.15f, 0.15f, 0.17f), 10.f);
	Style.NormalPadding = FMargin(18.f, 10.f);
	Style.PressedPadding = FMargin(18.f, 12.f, 18.f, 8.f);
	Button->SetStyle(Style);
	Button->SetIsEnabled(bEnabled);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Content->AddChildToVerticalBox(MakeText(InText, 24, bEnabled ? Text : Dim, true));
	if (!SubText.IsEmpty())
	{
		Content->AddChildToVerticalBox(MakeText(SubText, 16, Dim));
	}
	Button->SetContent(Content);

	UBattleHUDButtonBinding* Binding = NewObject<UBattleHUDButtonBinding>(this);
	Binding->Action = Action;
	Binding->Index = Index;
	Binding->HUD = this;
	Button->OnClicked.AddDynamic(Binding, &UBattleHUDButtonBinding::HandleClicked);
	Bindings.Add(Binding);

	if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Panel))
	{
		UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Button, Index / 2, Index % 2);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else if (UVerticalBox* Box = Cast<UVerticalBox>(Panel))
	{
		Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 3.f));
	}
	if (!FirstButton && bEnabled)
	{
		FirstButton = Button;
	}
	return Button;
}

void UBattleHUDWidget::ShowMainMenu()
{
	ClearMenu();
	Menu = EMenu::Main;
	const FPokemonInstance* Active = GetDisplayedPokemon(Snapshot.LocalSide);
	MessageText->SetText(Active ? FText::Format(LOCTEXT("WhatWillDo", "Que doit faire {0} ?"), UPokemonLibrary::GetDisplayName(GetDatabase(), *Active)) : FText::GetEmpty());
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	Grid->SetMinDesiredSlotWidth(200.f);
	MenuBox->AddChildToVerticalBox(Grid);
	AddMenuButton(Grid, LOCTEXT("Fight", "ATTAQUE"), EBattleHUDAction::Fight, 0, ActionColors[0]);
	AddMenuButton(Grid, LOCTEXT("Bag", "SAC"), EBattleHUDAction::Bag, 1, ActionColors[1]);
	AddMenuButton(Grid, LOCTEXT("Pokemon", "POKÉMON"), EBattleHUDAction::Pokemon, 2, ActionColors[2]);
	AddMenuButton(Grid, LOCTEXT("Run", "FUITE"), EBattleHUDAction::Run, 3, ActionColors[3], Snapshot.BattleType == EBattleType::Wild);

	MenuFrame->SetVisibility(ESlateVisibility::Visible);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

void UBattleHUDWidget::ShowMoves()
{
	ClearMenu();
	Menu = EMenu::Moves;
	const FPokemonInstance* Active = GetDisplayedPokemon(Snapshot.LocalSide);
	const UPokemonDatabase* Database = GetDatabase();
	if (!Active)
	{
		ShowMainMenu();
		return;
	}

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	Grid->SetMinDesiredSlotWidth(260.f);
	MenuBox->AddChildToVerticalBox(Grid);
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		if (!Active->Moves.IsValidIndex(Slot) || Active->Moves[Slot].Move.IsNone())
		{
			AddMenuButton(Grid, FText::FromString(TEXT("-")), EBattleHUDAction::Move, Slot, FLinearColor(0.2f, 0.2f, 0.2f), false);
			continue;
		}
		const FPokemonMoveSlot& MoveSlot = Active->Moves[Slot];
		const FMoveRow* Move = Database ? Database->FindMove(MoveSlot.Move) : nullptr;
		AddMenuButton(Grid, Move ? Move->DisplayName : FText::FromName(MoveSlot.Move), EBattleHUDAction::Move, Slot,
			Move ? TypeColor(Move->Type) : FLinearColor::Gray, true,
			FText::FromString(FString::Printf(TEXT("PP %d/%d"), MoveSlot.CurrentPP, MoveSlot.MaxPP)));
	}
	AddMenuButton(MenuBox, LOCTEXT("Back", "Retour"), EBattleHUDAction::Back, 0, FLinearColor(0.3f, 0.3f, 0.35f));
	MenuFrame->SetVisibility(ESlateVisibility::Visible);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

void UBattleHUDWidget::ShowBag()
{
	ClearMenu();
	Menu = EMenu::Bag;
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	int32 Count = 0;
	if (Snapshot.Sides.IsValidIndex(Local) && Database)
	{
		for (int32 Index = 0; Index < Snapshot.Sides[Local].Inventory.Num(); ++Index)
		{
			const FItemStack& Stack = Snapshot.Sides[Local].Inventory[Index];
			const FItemRow* Item = Database->FindItem(Stack.Item);
			if (!Item || !Item->bUsableInBattle || Stack.Count <= 0)
			{
				continue;
			}
			AddMenuButton(MenuBox, FText::Format(LOCTEXT("ItemCount", "{0}  x{1}"), Item->DisplayName, Stack.Count), EBattleHUDAction::Item, Index,
				Item->IsPokeBall() ? FLinearColor(0.75f, 0.2f, 0.2f) : FLinearColor(0.25f, 0.55f, 0.4f), true, Item->Description);
			++Count;
		}
	}
	if (Count == 0)
	{
		MenuBox->AddChildToVerticalBox(MakeText(LOCTEXT("NoItems", "Aucun objet utilisable."), 20, Dim));
	}
	AddMenuButton(MenuBox, LOCTEXT("Back", "Retour"), EBattleHUDAction::Back, 0, FLinearColor(0.3f, 0.3f, 0.35f));
	MenuFrame->SetVisibility(ESlateVisibility::Visible);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

void UBattleHUDWidget::ShowParty(EPartyMode Mode)
{
	ClearMenu();
	Menu = EMenu::Party;
	PartyMode = Mode;
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	if (!Snapshot.Sides.IsValidIndex(Local))
	{
		return;
	}
	const FBattleSide& Side = Snapshot.Sides[Local];

	MessageText->SetText(Mode == EPartyMode::Replacement ? LOCTEXT("SendWho", "Quel Pokémon envoyer ?")
		: (Mode == EPartyMode::ItemTarget ? LOCTEXT("UseOnWho", "Sur quel Pokémon ?") : LOCTEXT("SwitchWho", "Quel Pokémon envoyer au combat ?")));
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);

	for (int32 Index = 0; Index < Side.Party.Num(); ++Index)
	{
		const FPokemonInstance& Pokemon = Side.Party[Index];
		const bool bActive = Index == Side.ActiveIndex;
		const bool bEnabled = Mode == EPartyMode::ItemTarget || (!Pokemon.IsFainted() && !bActive);
		const float Ratio = Pokemon.GetMaxHP() > 0 ? float(Pokemon.CurrentHP) / float(Pokemon.GetMaxHP()) : 0.f;
		AddMenuButton(MenuBox, FText::Format(LOCTEXT("PartyLine", "{0}   N.{1}"), UPokemonLibrary::GetDisplayName(Database, Pokemon), Pokemon.Level),
			EBattleHUDAction::PartyPick, Index, Pokemon.IsFainted() ? FLinearColor(0.35f, 0.1f, 0.1f) : Darken(HPColor(Ratio), 0.7f), bEnabled,
			FText::FromString(FString::Printf(TEXT("PV %d/%d %s%s"), Pokemon.CurrentHP, Pokemon.GetMaxHP(),
				*StatusText(Pokemon.Status).ToString(), bActive ? TEXT("  (au combat)") : TEXT(""))));
	}
	if (Mode == EPartyMode::Replacement)
	{
		if (Snapshot.BattleType == EBattleType::Wild)
		{
			AddMenuButton(MenuBox, LOCTEXT("RunInstead", "Fuir"), EBattleHUDAction::Run, 0, ActionColors[3]);
		}
	}
	else
	{
		AddMenuButton(MenuBox, LOCTEXT("Back", "Retour"), EBattleHUDAction::Back, 0, FLinearColor(0.3f, 0.3f, 0.35f));
	}
	MenuFrame->SetVisibility(ESlateVisibility::Visible);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

void UBattleHUDWidget::ShowForgetMove(int32 PartyIndex, FName NewMove, bool bAfterEvolution)
{
	ClearMenu();
	CloseTimer = -1.f;
	Menu = EMenu::ForgetMove;
	PartyMode = bAfterEvolution ? EPartyMode::AfterEvolutionLearn : EPartyMode::Switch;
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	const FPokemonInstance* Pokemon = Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].Party.IsValidIndex(PartyIndex)
		? &Snapshot.Sides[Local].Party[PartyIndex] : nullptr;
	const FMoveRow* Move = Database ? Database->FindMove(NewMove) : nullptr;
	const FText MoveName = Move ? Move->DisplayName : FText::FromName(NewMove);

	MessageText->SetText(FText::Format(LOCTEXT("ForgetWhich", "{0} veut apprendre {1}, mais connaît déjà 4 capacités. Laquelle oublier ?"),
		Pokemon ? UPokemonLibrary::GetDisplayName(Database, *Pokemon) : FText::GetEmpty(), MoveName));
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);

	for (int32 Slot = 0; Pokemon && Slot < Pokemon->Moves.Num(); ++Slot)
	{
		const FMoveRow* Known = Database ? Database->FindMove(Pokemon->Moves[Slot].Move) : nullptr;
		AddMenuButton(MenuBox, Known ? Known->DisplayName : FText::FromName(Pokemon->Moves[Slot].Move), EBattleHUDAction::ForgetMove, Slot,
			Known ? TypeColor(Known->Type) : FLinearColor::Gray);
	}
	AddMenuButton(MenuBox, FText::Format(LOCTEXT("DontLearn", "Ne pas apprendre {0}"), MoveName), EBattleHUDAction::KeepMoves, 0, FLinearColor(0.3f, 0.3f, 0.35f));
	MenuFrame->SetVisibility(ESlateVisibility::Visible);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

void UBattleHUDWidget::ShowEvolution()
{
	ClearMenu();
	CloseTimer = -1.f;
	Menu = EMenu::Evolution;
	const FPendingEvolution& Evolution = Result.Evolutions[EvolutionStep];
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	const FPokemonInstance* Pokemon = Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].Party.IsValidIndex(Evolution.PartyIndex)
		? &Snapshot.Sides[Local].Party[Evolution.PartyIndex] : nullptr;
	const FSpeciesRow* NewSpecies = Database ? Database->FindSpecies(Evolution.NewSpecies) : nullptr;

	MessageText->SetText(FText::Format(LOCTEXT("Evolving", "Quoi ? {0} évolue en {1} !"),
		Pokemon ? UPokemonLibrary::GetDisplayName(Database, *Pokemon) : FText::GetEmpty(),
		NewSpecies ? NewSpecies->DisplayName : FText::FromName(Evolution.NewSpecies)));
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
	AddMenuButton(MenuBox, LOCTEXT("Evolve", "Laisser évoluer"), EBattleHUDAction::EvolveYes, 0, FLinearColor(0.3f, 0.6f, 0.95f));
	AddMenuButton(MenuBox, LOCTEXT("Stop", "Arrêter l'évolution (B)"), EBattleHUDAction::EvolveNo, 1, FLinearColor(0.3f, 0.3f, 0.35f));
	MenuFrame->SetVisibility(ESlateVisibility::Visible);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

void UBattleHUDWidget::OnButton(EBattleHUDAction Action, int32 Index)
{
	if (!Battle)
	{
		return;
	}
	const int32 Local = Snapshot.LocalSide;

	switch (Action)
	{
	case EBattleHUDAction::Fight:
		ShowMoves();
		break;
	case EBattleHUDAction::Bag:
		ShowBag();
		break;
	case EBattleHUDAction::Pokemon:
		ShowParty(EPartyMode::Switch);
		break;
	case EBattleHUDAction::Run:
		if (Menu == EMenu::Party && PartyMode == EPartyMode::Replacement)
		{
			ClearMenu();
			bWaitingForServer = true;
			Battle->RunInsteadOfReplacement();
		}
		else
		{
			Submit(FBattleAction::MakeRun());
		}
		break;
	case EBattleHUDAction::Move:
		Submit(FBattleAction::MakeMove(Index));
		break;
	case EBattleHUDAction::Item:
	{
		const UPokemonDatabase* Database = GetDatabase();
		const FItemStack* Stack = Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].Inventory.IsValidIndex(Index)
			? &Snapshot.Sides[Local].Inventory[Index] : nullptr;
		const FItemRow* Item = Stack && Database ? Database->FindItem(Stack->Item) : nullptr;
		if (!Item)
		{
			break;
		}
		if (Item->IsPokeBall())
		{
			Submit(FBattleAction::MakeItem(Stack->Item));
		}
		else
		{
			PendingItem = Stack->Item;
			ShowParty(EPartyMode::ItemTarget);
		}
		break;
	}
	case EBattleHUDAction::PartyPick:
		if (PartyMode == EPartyMode::Replacement)
		{
			ClearMenu();
			bWaitingForServer = true;
			Battle->ChooseReplacement(Index);
		}
		else if (PartyMode == EPartyMode::ItemTarget)
		{
			Submit(FBattleAction::MakeItem(PendingItem, Index));
		}
		else
		{
			Submit(FBattleAction::MakeSwitch(Index));
		}
		break;
	case EBattleHUDAction::ForgetMove:
	case EBattleHUDAction::KeepMoves:
	{
		const int32 ForgetSlot = Action == EBattleHUDAction::ForgetMove ? Index : -1;
		ClearMenu();
		if (PartyMode == EPartyMode::AfterEvolutionLearn && MovesToLearn.Num() > 0)
		{
			const TPair<int32, FName> Pending = MovesToLearn[0];
			MovesToLearn.RemoveAt(0);
			if (ForgetSlot >= 0)
			{
				Battle->LearnPendingMove(Pending.Key, Pending.Value, ForgetSlot);
			}
			ShowMenuForPhase();
		}
		else
		{
			bWaitingForServer = true;
			Battle->ResolveMoveLearn(ForgetSlot);
		}
		break;
	}
	case EBattleHUDAction::EvolveYes:
	case EBattleHUDAction::EvolveNo:
		if (Result.Evolutions.IsValidIndex(EvolutionStep))
		{
			Battle->ConfirmEvolution(Result.Evolutions[EvolutionStep].PartyIndex, Action == EBattleHUDAction::EvolveYes);
			++EvolutionStep;
			ClearMenu();
			if (Action == EBattleHUDAction::EvolveYes)
			{
				// Le serveur dira s'il y a des attaques à apprendre (HandleMovesToLearn)
				ShowMessage(LOCTEXT("Evolved", "Félicitations ! Ton Pokémon a évolué !"));
			}
			else
			{
				ShowMessage(LOCTEXT("Stopped", "Hein ? L'évolution s'est arrêtée."));
			}
		}
		break;
	case EBattleHUDAction::Back:
		if (Menu == EMenu::Party && PartyMode == EPartyMode::ItemTarget)
		{
			ShowBag();
		}
		else
		{
			ShowMainMenu();
		}
		break;
	}
}

void UBattleHUDWidget::Submit(const FBattleAction& Action)
{
	ClearMenu();
	bWaitingForServer = true;
	MessageText->SetText(LOCTEXT("Waiting", "..."));
	Battle->SubmitAction(Action);
}

FReply UBattleHUDWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bConfirm = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom;
	const bool bBack = Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;

	if (bMessageActive && (bConfirm || bBack))
	{
		AdvanceMessage();
		return FReply::Handled();
	}
	if (bBack)
	{
		if (Menu == EMenu::Moves || Menu == EMenu::Bag || (Menu == EMenu::Party && PartyMode != EPartyMode::Replacement))
		{
			OnButton(EBattleHUDAction::Back, 0);
			return FReply::Handled();
		}
		if (Menu == EMenu::Evolution)
		{
			OnButton(EBattleHUDAction::EvolveNo, 0);
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UBattleHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Clic n'importe où : message suivant
	if (bMessageActive)
	{
		AdvanceMessage();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBattleHUDWidget::FinishBattle()
{
	if (Battle)
	{
		Battle->OnBattleEvents.RemoveDynamic(this, &UBattleHUDWidget::HandleEvents);
		Battle->OnBattleEnded.RemoveDynamic(this, &UBattleHUDWidget::HandleEnded);
		Battle->OnActionRefused.RemoveDynamic(this, &UBattleHUDWidget::HandleRefused);
		Battle->OnMovesToLearn.RemoveDynamic(this, &UBattleHUDWidget::HandleMovesToLearn);
	}
	RemoveFromParent();
	if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		Controller->OnBattleHUDClosed();
	}
}

#undef LOCTEXT_NAMESPACE
