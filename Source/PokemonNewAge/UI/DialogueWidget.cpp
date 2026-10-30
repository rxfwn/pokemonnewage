#include "UI/DialogueWidget.h"
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
#include "Dialogue/DialogueComponent.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "DialogueWidget"

namespace
{
	const FLinearColor BoxColor(0.97f, 0.96f, 0.93f, 0.96f);
	const FLinearColor InkColor(0.08f, 0.08f, 0.10f);
	const FLinearColor NamePlateColor(0.13f, 0.25f, 0.45f, 0.97f);
	const FLinearColor AccentColor(0.85f, 0.18f, 0.16f);

	UTextBlock* MakeText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color, FName Name = NAME_None)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}
}

void UDialogueWidget::Init(UDialogueComponent* InOwner, float InLettersPerSecond, USoundBase* InTypingSound)
{
	Owner = InOwner;
	LettersPerSecond = InLettersPerSecond;
	TypingSound = InTypingSound;
	SetIsFocusable(true);
}

TSharedRef<SWidget> UDialogueWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Colonne en bas de l'écran : réponses (à droite), puis la boîte de texte
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
		ColumnSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnSlot->SetVerticalAlignment(VAlign_Bottom);
		ColumnSlot->SetPadding(FMargin(40.f, 0.f, 40.f, 40.f));

		// Réponses Oui / Non
		ChoicesPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChoicesPanel"));
		ChoicesPanel->SetBrushColor(BoxColor);
		ChoicesPanel->SetPadding(FMargin(28.f, 14.f));
		ChoicesPanel->SetVisibility(ESlateVisibility::Collapsed);
		ChoicesList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoicesList"));
		ChoicesPanel->SetContent(ChoicesList);
		UVerticalBoxSlot* ChoicesSlot = Column->AddChildToVerticalBox(ChoicesPanel);
		ChoicesSlot->SetHorizontalAlignment(HAlign_Right);
		ChoicesSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

		// Cartouche du nom, collé au coin haut gauche de la boîte
		NamePlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NamePlate"));
		NamePlate->SetBrushColor(NamePlateColor);
		NamePlate->SetPadding(FMargin(22.f, 6.f));
		NameText = MakeText(WidgetTree, 22, FLinearColor::White, TEXT("NameText"));
		NamePlate->SetContent(NameText);
		UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NamePlate);
		NameSlot->SetHorizontalAlignment(HAlign_Left);
		NameSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));

		// Boîte de texte : largeur fixe pour que le texte ne saute pas d'une ligne à l'autre
		USizeBox* BoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BoxSize"));
		BoxSize->SetWidthOverride(1300.f);
		BoxSize->SetMinDesiredHeight(170.f);
		Column->AddChildToVerticalBox(BoxSize);

		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
		Box->SetBrushColor(BoxColor);
		Box->SetPadding(FMargin(36.f, 26.f, 36.f, 18.f));
		BoxSize->SetContent(Box);

		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Inside"));
		Box->SetContent(Inside);

		BodyText = MakeText(WidgetTree, 28, InkColor, TEXT("BodyText"));
		BodyText->SetAutoWrapText(true);
		UVerticalBoxSlot* BodySlot = Inside->AddChildToVerticalBox(BodyText);
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ContinueArrow = MakeText(WidgetTree, 22, AccentColor, TEXT("ContinueArrow"));
		ContinueArrow->SetText(FText::FromString(TEXT("▼")));
		UVerticalBoxSlot* ArrowSlot = Inside->AddChildToVerticalBox(ContinueArrow);
		ArrowSlot->SetHorizontalAlignment(HAlign_Right);
	}

	return Super::RebuildWidget();
}

void UDialogueWidget::ShowLine(const FText& SpeakerName, const FText& Text, const TArray<FText>& Choices, bool bHasVoice)
{
	FullText = Text.ToString();
	CurrentChoices = Choices;
	SelectedChoice = 0;
	VisibleLetters = LettersPerSecond > 0.f ? 0 : FullText.Len();
	LetterProgress = 0.f;
	BlinkTime = 0.f;
	bPlayTypingSound = !bHasVoice && TypingSound != nullptr;

	NameText->SetText(SpeakerName);
	NamePlate->SetVisibility(SpeakerName.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);

	RefreshText();
	RefreshChoices();
	OnLineStarted(SpeakerName, Text);
}

void UDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsTyping())
	{
		LetterProgress += InDeltaTime * LettersPerSecond;
		const int32 NewLetters = FMath::Min(FullText.Len(), FMath::FloorToInt32(LetterProgress));
		if (NewLetters > VisibleLetters)
		{
			// Petit son une lettre sur deux, pas sur les espaces
			if (bPlayTypingSound && NewLetters % 2 == 0 && !FChar::IsWhitespace(FullText[NewLetters - 1]))
			{
				UGameplayStatics::PlaySound2D(this, TypingSound);
			}
			VisibleLetters = NewLetters;
			RefreshText();
			if (!IsTyping())
			{
				RefreshChoices();
			}
		}
	}
	else if (CurrentChoices.Num() == 0)
	{
		// La flèche clignote quand on peut continuer
		BlinkTime += InDeltaTime;
		ContinueArrow->SetRenderOpacity(FMath::Fmod(BlinkTime, 1.f) < 0.6f ? 1.f : 0.f);
	}
}

void UDialogueWidget::RefreshText()
{
	BodyText->SetText(FText::FromString(FullText.Left(VisibleLetters)));
	ContinueArrow->SetRenderOpacity(0.f);
}

void UDialogueWidget::RefreshChoices()
{
	ChoicesList->ClearChildren();
	const bool bShow = CurrentChoices.Num() > 0 && !IsTyping();
	ChoicesPanel->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShow)
	{
		return;
	}

	for (int32 Index = 0; Index < CurrentChoices.Num(); ++Index)
	{
		const bool bSelected = Index == SelectedChoice;
		UTextBlock* Entry = MakeText(WidgetTree, 26, bSelected ? AccentColor : InkColor);
		Entry->SetText(FText::Format(LOCTEXT("Choice", "{0} {1}"),
			FText::FromString(bSelected ? TEXT("▶") : TEXT("   ")), CurrentChoices[Index]));
		UVerticalBoxSlot* EntrySlot = ChoicesList->AddChildToVerticalBox(Entry);
		EntrySlot->SetPadding(FMargin(0.f, 3.f));
	}
}

void UDialogueWidget::MoveSelection(int32 Delta)
{
	if (CurrentChoices.Num() == 0 || IsTyping())
	{
		return;
	}
	SelectedChoice = (SelectedChoice + Delta + CurrentChoices.Num()) % CurrentChoices.Num();
	RefreshChoices();
}

void UDialogueWidget::Confirm()
{
	if (IsTyping())
	{
		VisibleLetters = FullText.Len();
		RefreshText();
		RefreshChoices();
		return;
	}
	if (!Owner)
	{
		return;
	}
	if (CurrentChoices.Num() > 0)
	{
		Owner->SelectChoice(SelectedChoice);
	}
	else
	{
		Owner->AdvanceLine();
	}
}

FReply UDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::E || Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom
		|| Key == EKeys::Gamepad_FaceButton_Left)
	{
		if (!InKeyEvent.IsRepeat())
		{
			Confirm();
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

	// Retour : choisit la dernière réponse (« Non »), sinon fait comme « valider »
	if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (CurrentChoices.Num() > 0 && !IsTyping())
		{
			SelectedChoice = CurrentChoices.Num() - 1;
			RefreshChoices();
		}
		if (!InKeyEvent.IsRepeat())
		{
			Confirm();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#undef LOCTEXT_NAMESPACE
