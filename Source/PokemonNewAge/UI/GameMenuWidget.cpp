#include "UI/GameMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PokemonNewAgePlayerController.h"
#include "UI/Pokedex/PokedexScreenWidget.h"

#define LOCTEXT_NAMESPACE "GameMenu"

TSharedRef<SWidget> UGameMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Fond assombri sur tout l'écran, panneau à droite comme dans les jeux
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		RootOverlay = Root;

		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(FLinearColor(0.96f, 0.95f, 0.92f, 0.95f));
		Panel->SetPadding(FMargin(24.f, 20.f));
		UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
		PanelSlot->SetHorizontalAlignment(HAlign_Right);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
		PanelSlot->SetPadding(FMargin(0.f, 0.f, 60.f, 0.f));
		ListPanel = Panel;

		UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("List"));
		Panel->SetContent(List);

		AddEntry(List, LOCTEXT("Pokedex", "Pokédex"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnPokedexClicked);
		AddEntry(List, LOCTEXT("Pokemon", "Pokémon"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnPokemonClicked);
		AddEntry(List, LOCTEXT("Bag", "Sac"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnBagClicked);
		AddEntry(List, LOCTEXT("Trainer", "Dresseur"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTrainerClicked);
		AddEntry(List, LOCTEXT("Map", "Carte"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnMapClicked);
		AddEntry(List, LOCTEXT("Online", "En ligne"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnOnlineClicked);
		AddEntry(List, LOCTEXT("Save", "Sauvegarder"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnSaveClicked);
		AddEntry(List, LOCTEXT("Options", "Options"), true)->OnClicked.AddDynamic(this, &UGameMenuWidget::OnOptionsClicked);
		ResumeButton = AddEntry(List, LOCTEXT("Resume", "Reprendre"), true);
		ResumeButton->OnClicked.AddDynamic(this, &UGameMenuWidget::OnResumeClicked);
		UButton* QuitButton = AddEntry(List, LOCTEXT("Quit", "Quitter"), true);
		QuitButton->OnClicked.AddDynamic(this, &UGameMenuWidget::OnQuitClicked);
	}

	return Super::RebuildWidget();
}

UButton* UGameMenuWidget::AddEntry(UVerticalBox* List, const FText& Label, bool bAvailable)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(bAvailable ? Label : FText::Format(LOCTEXT("Soon", "{0}  (bientôt)"), Label));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.07f)));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 26;
	Text->SetFont(Font);
	Button->SetContent(Text);
	Button->SetIsEnabled(bAvailable);

	UVerticalBoxSlot* EntrySlot = List->AddChildToVerticalBox(Button);
	EntrySlot->SetPadding(FMargin(0.f, 4.f));
	EntrySlot->SetHorizontalAlignment(HAlign_Fill);
	return Button;
}

void UGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FocusDefaultEntry();
}

void UGameMenuWidget::FocusDefaultEntry()
{
	if (ResumeButton)
	{
		ResumeButton->SetKeyboardFocus();
	}
}

FReply UGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Tab || Key == EKeys::Escape || Key == EKeys::BackSpace
		|| Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGameMenuWidget::OnResumeClicked()
{
	CloseMenu();
}

void UGameMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UGameMenuWidget::OnPokedexClicked() { OpenScreen(EPokedexMenu::Pokedex); }
void UGameMenuWidget::OnPokemonClicked() { OpenScreen(EPokedexMenu::Pokemon); }
void UGameMenuWidget::OnBagClicked() { OpenScreen(EPokedexMenu::Bag); }
void UGameMenuWidget::OnTrainerClicked() { OpenScreen(EPokedexMenu::Trainer); }
void UGameMenuWidget::OnSaveClicked() { OpenScreen(EPokedexMenu::Save); }
void UGameMenuWidget::OnOptionsClicked() { OpenScreen(EPokedexMenu::Options); }
void UGameMenuWidget::OnMapClicked() { OpenScreen(EPokedexMenu::Map); }
void UGameMenuWidget::OnOnlineClicked() { OpenScreen(EPokedexMenu::Online); }

void UGameMenuWidget::OpenScreen(EPokedexMenu Menu)
{
	if (!Screen)
	{
		Screen = CreateWidget<UPokedexScreenWidget>(GetOwningPlayer(), UPokedexScreenWidget::StaticClass());
		Screen->bStandalone = true;
		Screen->OnRequestClose.AddUObject(this, &UGameMenuWidget::CloseScreen);
		UOverlaySlot* ScreenSlot = RootOverlay->AddChildToOverlay(Screen);
		ScreenSlot->SetHorizontalAlignment(HAlign_Center);
		ScreenSlot->SetVerticalAlignment(VAlign_Center);
	}
	Screen->SetVisibility(ESlateVisibility::Visible);
	ListPanel->SetVisibility(ESlateVisibility::Collapsed);
	Screen->OpenMenu(Menu);
	Screen->SetKeyboardFocus();
}

void UGameMenuWidget::CloseScreen()
{
	if (Screen)
	{
		Screen->SetVisibility(ESlateVisibility::Collapsed);
	}
	ListPanel->SetVisibility(ESlateVisibility::Visible);
	FocusDefaultEntry();
}

void UGameMenuWidget::CloseMenu()
{
	if (APokemonNewAgePlayerController* PlayerController = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseGameMenu();
	}
}

#undef LOCTEXT_NAMESPACE
