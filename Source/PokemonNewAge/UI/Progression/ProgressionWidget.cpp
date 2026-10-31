#include "UI/Progression/ProgressionWidget.h"
#include "Progression/ProgressionComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"

#define LOCTEXT_NAMESPACE "Progression"

namespace ProgressionUI
{
	// Mêmes couleurs que l'écran de combat
	const FLinearColor PanelColor(0.02f, 0.025f, 0.035f, 0.82f);
	const FLinearColor Text(0.95f, 0.96f, 0.97f, 1.f);
	const FLinearColor Dim(0.6f, 0.65f, 0.7f, 1.f);
	const FLinearColor Good(0.3f, 0.85f, 0.4f, 1.f);
	const FLinearColor Warn(0.98f, 0.75f, 0.2f, 1.f);

	FLinearColor Darken(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, 1.f);
	}

	FText MoveName(const UPokemonDatabase* Database, FName Move)
	{
		const FMoveRow* Row = Database ? Database->FindMove(Move) : nullptr;
		return Row ? Row->DisplayName : FText::FromName(Move);
	}
}

using namespace ProgressionUI;

void UProgressionButtonBinding::HandleClicked()
{
	if (UProgressionWidget* Widget = Owner.Get())
	{
		Widget->ChooseForgetSlot(Slot);
	}
}

// ---------------------------------------------------------------- Construction

TSharedRef<SWidget> UProgressionWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Flash blanc de l'évolution, sous le texte
		FlashBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Flash"));
		FlashBorder->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
		FlashBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* FlashSlot = Root->AddChildToOverlay(FlashBorder);
		FlashSlot->SetHorizontalAlignment(HAlign_Fill);
		FlashSlot->SetVerticalAlignment(VAlign_Fill);

		// Boîte de texte en bas
		MessageFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MessageFrame"));
		MessageFrame->SetBrush(FSlateRoundedBoxBrush(PanelColor, 14.f));
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
		MessageRow->AddChildToHorizontalBox(MessageText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MessageArrow = MakeText(FText::FromString(TEXT("v")), 26, Warn, true);
		MessageRow->AddChildToHorizontalBox(MessageArrow)->SetVerticalAlignment(VAlign_Bottom);
		MessageArrow->SetVisibility(ESlateVisibility::Hidden);

		// Panneau des stats, à droite au-dessus du texte
		StatsFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatsFrame"));
		StatsFrame->SetBrush(FSlateRoundedBoxBrush(PanelColor, 14.f));
		StatsFrame->SetPadding(FMargin(24.f, 16.f));
		UOverlaySlot* StatsSlot = Root->AddChildToOverlay(StatsFrame);
		StatsSlot->SetHorizontalAlignment(HAlign_Right);
		StatsSlot->SetVerticalAlignment(VAlign_Bottom);
		StatsSlot->SetPadding(FMargin(0.f, 0.f, 80.f, 220.f));
		USizeBox* StatsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StatsSize->SetWidthOverride(360.f);
		StatsFrame->SetContent(StatsSize);
		StatsGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		StatsGrid->SetSlotPadding(FMargin(6.f, 4.f));
		StatsSize->SetContent(StatsGrid);
		StatsFrame->SetVisibility(ESlateVisibility::Collapsed);

		// Choix de l'attaque à oublier
		MenuFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuFrame"));
		MenuFrame->SetBrush(FSlateRoundedBoxBrush(PanelColor, 14.f));
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

UTextBlock* UProgressionWidget::MakeText(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold)
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

const UPokemonDatabase* UProgressionWidget::GetDatabase() const
{
	return UPokemonDataSubsystem::GetDatabase(this);
}

// ---------------------------------------------------------------- Boîte de texte

void UProgressionWidget::ShowMessage(const FText& InText, bool bInWaitForInput, TFunction<void()> OnDone)
{
	FullMessage = InText.ToString();
	MessageChars = 0.f;
	MessageHold = 0.f;
	bMessageActive = true;
	bWaitForInput = bInWaitForInput;
	MessageDone = MoveTemp(OnDone);
	MessageFrame->SetVisibility(ESlateVisibility::Visible);
	MessageText->SetText(FText::GetEmpty());
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
}

void UProgressionWidget::SetCaption(const FText& InText)
{
	bMessageActive = false;
	MessageDone = nullptr;
	MessageFrame->SetVisibility(ESlateVisibility::Visible);
	MessageText->SetText(InText);
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
}

void UProgressionWidget::SetFlash(float Alpha)
{
	if (FlashBorder)
	{
		FlashBorder->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f)));
	}
}

void UProgressionWidget::FinishMessage()
{
	bMessageActive = false;
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
	TFunction<void()> Done = MoveTemp(MessageDone);
	MessageDone = nullptr;
	if (Done)
	{
		Done();
	}
}

void UProgressionWidget::Confirm()
{
	if (bMessageActive)
	{
		// Premier appui : tout le texte d'un coup ; deuxième : la suite
		if (MessageChars < FullMessage.Len())
		{
			MessageChars = FullMessage.Len();
			MessageText->SetText(FText::FromString(FullMessage));
			return;
		}
		FinishMessage();
		return;
	}
	if (LevelUpStep == 1)
	{
		LevelUpStep = 2;
		ShowStatsPanel(true);
	}
	else if (LevelUpStep == 2)
	{
		LevelUpStep = 0;
		StatsFrame->SetVisibility(ESlateVisibility::Collapsed);
		TFunction<void()> Done = MoveTemp(LevelUpDone);
		LevelUpDone = nullptr;
		if (Done)
		{
			Done();
		}
	}
}

void UProgressionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bMessageActive)
	{
		return;
	}
	const int32 Before = FMath::FloorToInt(MessageChars);
	MessageChars = FMath::Min(MessageChars + CharsPerSecond * InDeltaTime, float(FullMessage.Len()));
	const int32 After = FMath::FloorToInt(MessageChars);
	if (After != Before || After == 0)
	{
		MessageText->SetText(FText::FromString(FullMessage.Left(After)));
	}
	if (After >= FullMessage.Len())
	{
		if (bWaitForInput)
		{
			MessageArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			MessageHold += InDeltaTime;
			if (MessageHold >= AutoHoldTime)
			{
				FinishMessage();
			}
		}
	}
}

// ---------------------------------------------------------------- Montée de niveau

void UProgressionWidget::ShowLevelUp(const FText& PokemonName, const FLevelUpInfo& Info, TFunction<void()> OnDone)
{
	LevelUp = Info;
	LevelUpDone = MoveTemp(OnDone);
	UGameplayStatics::PlaySound2D(this, UProgressionComponent::LoadSound(TEXT("SW_LevelUp")));
	ShowMessage(FText::Format(LOCTEXT("LevelUp", "{0} monte au niveau {1} !"), PokemonName, Info.Level), true, [this]()
	{
		LevelUpStep = 1;
		ShowStatsPanel(false);
	});
}

void UProgressionWidget::ShowStatsPanel(bool bTotals)
{
	StatsGrid->ClearChildren();
	const FText Names[] = {
		LOCTEXT("StatHP", "PV max."), LOCTEXT("StatAtk", "Attaque"), LOCTEXT("StatDef", "Défense"),
		LOCTEXT("StatSpA", "Atq. Spé."), LOCTEXT("StatSpD", "Déf. Spé."), LOCTEXT("StatSpe", "Vitesse")
	};
	const FPokemonBaseStats& A = LevelUp.StatsBefore;
	const FPokemonBaseStats& B = LevelUp.StatsAfter;
	const int32 Before[] = { A.HP, A.Attack, A.Defense, A.SpecialAttack, A.SpecialDefense, A.Speed };
	const int32 After[] = { B.HP, B.Attack, B.Defense, B.SpecialAttack, B.SpecialDefense, B.Speed };
	for (int32 Row = 0; Row < 6; ++Row)
	{
		StatsGrid->AddChildToUniformGrid(MakeText(Names[Row], 22, Text), Row, 0);
		const int32 Gain = After[Row] - Before[Row];
		const FText Value = bTotals ? FText::AsNumber(After[Row]) : FText::FromString(FString::Printf(TEXT("+%d"), Gain));
		UTextBlock* ValueText = MakeText(Value, 22, bTotals || Gain == 0 ? Text : Good, true);
		ValueText->SetJustification(ETextJustify::Right);
		UUniformGridSlot* ValueSlot = StatsGrid->AddChildToUniformGrid(ValueText, Row, 1);
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
	}
	StatsFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	MessageArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------- Choix de l'attaque à oublier

void UProgressionWidget::ShowForgetMove(const FPokemonInstance& Pokemon, FName NewMove, TFunction<void(int32)> OnChosen)
{
	LearningPokemon = Pokemon;
	LearningMove = NewMove;
	ForgetChosen = MoveTemp(OnChosen);
	const UPokemonDatabase* Database = GetDatabase();
	const FText Name = UPokemonLibrary::GetDisplayName(Database, Pokemon);
	const FText Move = MoveName(Database, NewMove);

	ShowMessage(FText::Format(LOCTEXT("WantsToLearn", "{0} veut apprendre {1}."), Name, Move), true, [this, Name, Move, Database]()
	{
		ShowMessage(FText::Format(LOCTEXT("AlreadyFour", "Mais {0} connaît déjà quatre capacités. Quelle capacité doit-il oublier pour apprendre {1} ?"), Name, Move), true, [this, Move, Database]()
		{
			ClearMenu();
			bChoosing = true;
			for (int32 MoveSlot = 0; MoveSlot < LearningPokemon.Moves.Num(); ++MoveSlot)
			{
				const FPokemonMoveSlot& Known = LearningPokemon.Moves[MoveSlot];
				const FMoveRow* Row = Database ? Database->FindMove(Known.Move) : nullptr;
				UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
				const FLinearColor Color(0.25f, 0.45f, 0.75f);
				FButtonStyle Style = Button->GetStyle();
				Style.Normal = FSlateRoundedBoxBrush(Darken(Color, 0.55f), 10.f);
				Style.Hovered = FSlateRoundedBoxBrush(Darken(Color, 1.f), 10.f, FLinearColor::White, 2.f);
				Style.Pressed = FSlateRoundedBoxBrush(Darken(Color, 0.4f), 10.f);
				Style.NormalPadding = FMargin(18.f, 10.f);
				Style.PressedPadding = FMargin(18.f, 12.f, 18.f, 8.f);
				Button->SetStyle(Style);
				UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				Content->AddChildToVerticalBox(MakeText(Row ? Row->DisplayName : FText::FromName(Known.Move), 24, Text, true));
				Content->AddChildToVerticalBox(MakeText(FText::Format(LOCTEXT("PP", "PP {0}/{1}"), Known.CurrentPP, Known.MaxPP), 16, Dim));
				Button->SetContent(Content);
				UProgressionButtonBinding* Binding = NewObject<UProgressionButtonBinding>(this);
				Binding->Slot = MoveSlot;
				Binding->Owner = this;
				Button->OnClicked.AddDynamic(Binding, &UProgressionButtonBinding::HandleClicked);
				Bindings.Add(Binding);
				MenuBox->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 3.f));
				if (!FirstButton)
				{
					FirstButton = Button;
				}
			}

			UButton* Skip = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			FButtonStyle SkipStyle = Skip->GetStyle();
			SkipStyle.Normal = FSlateRoundedBoxBrush(FLinearColor(0.17f, 0.17f, 0.2f), 10.f);
			SkipStyle.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.3f, 0.3f, 0.35f), 10.f, FLinearColor::White, 2.f);
			SkipStyle.Pressed = SkipStyle.Normal;
			SkipStyle.NormalPadding = FMargin(18.f, 10.f);
			Skip->SetStyle(SkipStyle);
			Skip->SetContent(MakeText(FText::Format(LOCTEXT("DontLearn", "Ne pas apprendre {0} (B)"), Move), 22, Text, true));
			UProgressionButtonBinding* SkipBinding = NewObject<UProgressionButtonBinding>(this);
			SkipBinding->Slot = -1;
			SkipBinding->Owner = this;
			Skip->OnClicked.AddDynamic(SkipBinding, &UProgressionButtonBinding::HandleClicked);
			Bindings.Add(SkipBinding);
			MenuBox->AddChildToVerticalBox(Skip)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

			MenuFrame->SetVisibility(ESlateVisibility::Visible);
			if (FirstButton)
			{
				FirstButton->SetKeyboardFocus();
			}
		});
	});
}

void UProgressionWidget::ClearMenu()
{
	MenuBox->ClearChildren();
	Bindings.Reset();
	FirstButton = nullptr;
	MenuFrame->SetVisibility(ESlateVisibility::Collapsed);
}

void UProgressionWidget::ChooseForgetSlot(int32 MoveSlot)
{
	if (!bChoosing)
	{
		return;
	}
	bChoosing = false;
	ClearMenu();
	SetKeyboardFocus();

	const UPokemonDatabase* Database = GetDatabase();
	const FText Name = UPokemonLibrary::GetDisplayName(Database, LearningPokemon);
	const FText Move = MoveName(Database, LearningMove);
	TFunction<void(int32)> Chosen = MoveTemp(ForgetChosen);
	ForgetChosen = nullptr;
	auto Done = [Chosen, MoveSlot]()
	{
		if (Chosen)
		{
			Chosen(MoveSlot);
		}
	};

	if (!LearningPokemon.Moves.IsValidIndex(MoveSlot))
	{
		ShowMessage(FText::Format(LOCTEXT("NotLearned", "{0} n'a pas appris {1}."), Name, Move), true, Done);
		return;
	}
	const FText Forgotten = MoveName(Database, LearningPokemon.Moves[MoveSlot].Move);
	ShowMessage(FText::Format(LOCTEXT("Forgot", "1, 2 et… Tadaa ! {0} a oublié {1}…"), Name, Forgotten), true, [this, Name, Move, Done]()
	{
		UGameplayStatics::PlaySound2D(this, UProgressionComponent::LoadSound(TEXT("SW_MoveLearned")));
		ShowMessage(FText::Format(LOCTEXT("Learned", "Et il a appris {1} !"), Name, Move), true, Done);
	});
}

// ---------------------------------------------------------------- Entrées

FReply UProgressionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bConfirm = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom;
	const bool bBack = Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;

	if (bChoosing)
	{
		if (bBack)
		{
			ChooseForgetSlot(-1);
			return FReply::Handled();
		}
		// Flèches et A : navigation et clic des boutons par UMG
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (bBack && OnBack && OnBack())
	{
		return FReply::Handled();
	}
	if (bConfirm || bBack)
	{
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UProgressionWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bChoosing)
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OnBack && OnBack())
		{
			return FReply::Handled();
		}
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#undef LOCTEXT_NAMESPACE
