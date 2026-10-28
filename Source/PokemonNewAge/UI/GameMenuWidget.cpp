#include "UI/GameMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PokemonNewAgePlayerController.h"
#include "UI/Pokedex/PokedexModel.h"
#include "UI/Pokedex/PokedexScreenWidget.h"

#define LOCTEXT_NAMESPACE "GameMenu"

namespace GameMenu
{
	const FLinearColor TabColor(0.08f, 0.09f, 0.11f, 0.92f);
	const FLinearColor TabSelectedColor(0.86f, 0.16f, 0.18f, 1.f);
	const FLinearColor TextColor(0.95f, 0.95f, 0.96f);
	const FLinearColor KeyColor(1.f, 0.82f, 0.35f);

	/** Noms des icônes, dans l'ordre de EPokedexMenu (comme ICONS dans import_pokedex_unreal.py) */
	const TCHAR* IconNames[] = { TEXT("Pokemon"), TEXT("Pokedex"), TEXT("Bag"), TEXT("Radio"), TEXT("Trainer"),
		TEXT("Map"), TEXT("Save"), TEXT("Online"), TEXT("Options"), TEXT("Style") };

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Value, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(Value);
		Text->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}
}

using namespace GameMenu;

TArray<EPokedexMenu> UGameMenuWidget::GetTabs()
{
	return { EPokedexMenu::Pokemon, EPokedexMenu::Pokedex, EPokedexMenu::Bag, EPokedexMenu::Trainer,
		EPokedexMenu::Map, EPokedexMenu::Save, EPokedexMenu::Online, EPokedexMenu::Options };
}

UTexture2D* UGameMenuWidget::GetMenuIcon(EPokedexMenu Menu)
{
	const int32 Index = int32(Menu);
	if (const UPokedexModel* Model = LoadObject<UPokedexModel>(nullptr, UPokedexModel::AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		Model && Model->MenuIcons.IsValidIndex(Index) && Model->MenuIcons[Index])
	{
		return Model->MenuIcons[Index];
	}
	if (Index < 0 || Index >= UE_ARRAY_COUNT(IconNames))
	{
		return nullptr;
	}
	const FString Path = FString::Printf(TEXT("/Game/UI/Pokedex/Icons/T_PokedexIcon_%s.T_PokedexIcon_%s"), IconNames[Index], IconNames[Index]);
	return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

TSharedRef<SWidget> UGameMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Décor flouté et assombri derrière le menu
		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur"));
		Blur->SetBlurStrength(8.f);
		UOverlaySlot* BlurSlot = Root->AddChildToOverlay(Blur);
		BlurSlot->SetHorizontalAlignment(HAlign_Fill);
		BlurSlot->SetVerticalAlignment(VAlign_Fill);

		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Main"));
		UOverlaySlot* MainSlot = Root->AddChildToOverlay(Main);
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
		MainSlot->SetPadding(FMargin(40.f, 28.f));

		// Onglets à icônes
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
		UVerticalBoxSlot* TabsSlot = Main->AddChildToVerticalBox(Tabs);
		TabsSlot->SetHorizontalAlignment(HAlign_Center);
		TabsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		const TArray<EPokedexMenu> Menus = GetTabs();
		for (int32 Index = 0; Index < Menus.Num() && Index < 8; ++Index)
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Button->SetContent(Content);

			if (UTexture2D* Icon = GetMenuIcon(Menus[Index]))
			{
				USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				IconBox->SetWidthOverride(56.f);
				IconBox->SetHeightOverride(56.f);
				UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Image->SetBrushFromTexture(Icon, false);
				IconBox->SetContent(Image);
				UVerticalBoxSlot* IconSlot = Content->AddChildToVerticalBox(IconBox);
				IconSlot->SetHorizontalAlignment(HAlign_Center);
			}
			Content->AddChildToVerticalBox(MakeText(WidgetTree, UPokedexScreenWidget::GetMenuName(Menus[Index]), 16, TextColor));
			UTextBlock* KeyText = MakeText(WidgetTree, FText::GetEmpty(), 12, KeyColor);
			Content->AddChildToVerticalBox(KeyText);

			// AddDynamic a besoin du nom de la fonction écrit en toutes lettres
			switch (Index)
			{
			case 0: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab0Clicked); break;
			case 1: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab1Clicked); break;
			case 2: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab2Clicked); break;
			case 3: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab3Clicked); break;
			case 4: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab4Clicked); break;
			case 5: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab5Clicked); break;
			case 6: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab6Clicked); break;
			default: Button->OnClicked.AddDynamic(this, &UGameMenuWidget::OnTab7Clicked); break;
			}
			UHorizontalBoxSlot* ButtonSlot = Tabs->AddChildToHorizontalBox(Button);
			ButtonSlot->SetPadding(FMargin(4.f, 0.f));
			TabButtons.Add(Button);
			TabKeys.Add(KeyText);
		}

		// Page de l'onglet, agrandie pour remplir l'écran
		UScaleBox* PageScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("PageScale"));
		PageScale->SetStretch(EStretch::ScaleToFit);
		UVerticalBoxSlot* PageSlot = Main->AddChildToVerticalBox(PageScale);
		PageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PageSlot->SetHorizontalAlignment(HAlign_Center);
		PageSlot->SetVerticalAlignment(VAlign_Center);

		Screen = WidgetTree->ConstructWidget<UPokedexScreenWidget>(UPokedexScreenWidget::StaticClass(), TEXT("Page"));
		Screen->bStandalone = true;
		Screen->OnRequestClose.AddUObject(this, &UGameMenuWidget::CloseMenu);
		PageScale->SetContent(Screen);

		// Aide et boutons du bas
		UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Bottom"));
		UVerticalBoxSlot* BottomSlot = Main->AddChildToVerticalBox(Bottom);
		BottomSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		UTextBlock* Help = MakeText(WidgetTree, LOCTEXT("Help", "LB / RB ou Page préc. / Page suiv. : onglet      Échap : retour      Tab : fermer"), 14, TextColor);
		Help->SetJustification(ETextJustify::Left);
		UHorizontalBoxSlot* HelpSlot = Bottom->AddChildToHorizontalBox(Help);
		HelpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HelpSlot->SetVerticalAlignment(VAlign_Center);
		AddButton(Bottom, LOCTEXT("Resume", "Reprendre"))->OnClicked.AddDynamic(this, &UGameMenuWidget::OnResumeClicked);
		AddButton(Bottom, LOCTEXT("Quit", "Quitter le jeu"))->OnClicked.AddDynamic(this, &UGameMenuWidget::OnQuitClicked);
	}

	return Super::RebuildWidget();
}

UButton* UGameMenuWidget::AddButton(UHorizontalBox* Bar, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(TabColor);
	UTextBlock* Text = MakeText(WidgetTree, Label, 16, TextColor);
	Button->SetContent(Text);
	UHorizontalBoxSlot* ButtonSlot = Bar->AddChildToHorizontalBox(Button);
	ButtonSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	return Button;
}

void UGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshTabs();
}

void UGameMenuWidget::OpenTab(EPokedexMenu Menu)
{
	// Le Pokédex reste en 3D (demandé par L'amie) : le menu classique lui laisse la place
	if (Menu == EPokedexMenu::Pokedex)
	{
		if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()); Controller && Controller->SwitchToPokedex3D())
		{
			return;
		}
	}
	const int32 Index = GetTabs().IndexOfByKey(Menu);
	CurrentTab = Index != INDEX_NONE ? Index : 0;
	if (Screen)
	{
		Screen->OpenMenu(GetTabs()[CurrentTab]);
	}
	RefreshTabs();
	FocusDefaultEntry();
}

void UGameMenuWidget::FocusDefaultEntry()
{
	if (Screen)
	{
		Screen->SetKeyboardFocus();
	}
}

void UGameMenuWidget::CycleTab(int32 Delta)
{
	const int32 Count = GetTabs().Num();
	OpenTab(GetTabs()[(CurrentTab + Delta + Count) % Count]);
}

void UGameMenuWidget::RefreshTabs()
{
	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	const TArray<EPokedexMenu> Menus = GetTabs();
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		TabButtons[Index]->SetBackgroundColor(Index == CurrentTab ? TabSelectedColor : TabColor);
		const FKey Key = Controller ? Controller->GetShortcutKey(Menus[Index]) : FKey();
		TabKeys[Index]->SetText(Key.IsValid() ? FText::Format(LOCTEXT("TabKey", "[{0}]"), Key.GetDisplayName(false)) : FText::GetEmpty());
	}
}

FReply UGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	EPokedexMenu Shortcut;

	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::PageUp)
	{
		CycleTab(-1);
	}
	else if (Key == EKeys::Gamepad_RightShoulder || Key == EKeys::PageDown)
	{
		CycleTab(1);
	}
	else if (Controller && Controller->FindShortcut(Key, Shortcut))
	{
		// Le raccourci de l'onglet ouvert ferme le menu, comme dans les jeux PC
		if (GetTabs()[CurrentTab] == Shortcut)
		{
			CloseMenu();
		}
		else
		{
			OpenTab(Shortcut);
		}
	}
	else if ((Controller && Controller->IsMenuKey(Key)) || Key == EKeys::Escape)
	{
		CloseMenu();
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

void UGameMenuWidget::OnResumeClicked()
{
	CloseMenu();
}

void UGameMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UGameMenuWidget::OnTab0Clicked() { OpenTab(GetTabs()[0]); }
void UGameMenuWidget::OnTab1Clicked() { OpenTab(GetTabs()[1]); }
void UGameMenuWidget::OnTab2Clicked() { OpenTab(GetTabs()[2]); }
void UGameMenuWidget::OnTab3Clicked() { OpenTab(GetTabs()[3]); }
void UGameMenuWidget::OnTab4Clicked() { OpenTab(GetTabs()[4]); }
void UGameMenuWidget::OnTab5Clicked() { OpenTab(GetTabs()[5]); }
void UGameMenuWidget::OnTab6Clicked() { OpenTab(GetTabs()[6]); }
void UGameMenuWidget::OnTab7Clicked() { OpenTab(GetTabs()[7]); }

void UGameMenuWidget::CloseMenu()
{
	if (APokemonNewAgePlayerController* PlayerController = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseGameMenu();
	}
}

#undef LOCTEXT_NAMESPACE
