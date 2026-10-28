#include "UI/Pokedex/PokedexScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/PokemonPlayerState.h"
#include "Core/PokemonSaveGame.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Landscape.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "PokemonNewAgePlayerController.h"
#include "UI/Pokedex/PokedexModel.h"

#define LOCTEXT_NAMESPACE "PokedexScreen"

namespace PokedexScreen
{
	// Couleurs de l'écran : noir bleuté quadrillé comme sur l'image de référence
	const FLinearColor Background(0.012f, 0.018f, 0.024f, 1.f);
	const FLinearColor Grid(0.05f, 0.075f, 0.09f, 1.f);
	const FLinearColor Text(0.86f, 0.93f, 0.96f, 1.f);
	const FLinearColor Dim(0.42f, 0.5f, 0.56f, 1.f);
	const FLinearColor Accent(0.3f, 0.8f, 1.f, 1.f);
	const FLinearColor Selected(0.06f, 0.32f, 0.5f, 0.9f);
	const FLinearColor ErrorColor(1.f, 0.32f, 0.3f, 1.f);
	const FLinearColor Good(0.35f, 0.92f, 0.45f, 1.f);
	const FLinearColor Gold(1.f, 0.78f, 0.25f, 1.f);

	/** Lignes visibles des longues listes */
	constexpr int32 ListWindow = 8;

	FLinearColor TypeColor(EPokemonType Type)
	{
		switch (Type)
		{
		case EPokemonType::Normal:   return FLinearColor(0.55f, 0.55f, 0.42f);
		case EPokemonType::Fire:     return FLinearColor(0.93f, 0.4f, 0.15f);
		case EPokemonType::Water:    return FLinearColor(0.3f, 0.5f, 0.95f);
		case EPokemonType::Electric: return FLinearColor(0.95f, 0.78f, 0.1f);
		case EPokemonType::Grass:    return FLinearColor(0.35f, 0.75f, 0.3f);
		case EPokemonType::Ice:      return FLinearColor(0.5f, 0.85f, 0.85f);
		case EPokemonType::Fighting: return FLinearColor(0.72f, 0.18f, 0.15f);
		case EPokemonType::Poison:   return FLinearColor(0.6f, 0.25f, 0.6f);
		case EPokemonType::Ground:   return FLinearColor(0.85f, 0.7f, 0.35f);
		case EPokemonType::Flying:   return FLinearColor(0.62f, 0.55f, 0.95f);
		case EPokemonType::Psychic:  return FLinearColor(0.97f, 0.33f, 0.5f);
		case EPokemonType::Bug:      return FLinearColor(0.62f, 0.72f, 0.12f);
		case EPokemonType::Rock:     return FLinearColor(0.72f, 0.62f, 0.25f);
		case EPokemonType::Ghost:    return FLinearColor(0.42f, 0.35f, 0.6f);
		case EPokemonType::Dragon:   return FLinearColor(0.42f, 0.22f, 0.98f);
		case EPokemonType::Dark:     return FLinearColor(0.42f, 0.35f, 0.28f);
		case EPokemonType::Steel:    return FLinearColor(0.72f, 0.72f, 0.82f);
		case EPokemonType::Fairy:    return FLinearColor(0.93f, 0.6f, 0.68f);
		default:                     return FLinearColor(0.3f, 0.3f, 0.3f);
		}
	}

	/** Badges de Kanto, dans l'ordre (identifiants de APokemonPlayerState::Badges) */
	struct FBadgeInfo { const TCHAR* Id; const TCHAR* Name; };
	const FBadgeInfo KantoBadges[] = {
		{ TEXT("BoulderBadge"), TEXT("Roche") },
		{ TEXT("CascadeBadge"), TEXT("Cascade") },
		{ TEXT("ThunderBadge"), TEXT("Foudre") },
		{ TEXT("RainbowBadge"), TEXT("Prisme") },
		{ TEXT("SoulBadge"), TEXT("Âme") },
		{ TEXT("MarshBadge"), TEXT("Marais") },
		{ TEXT("VolcanoBadge"), TEXT("Volcan") },
		{ TEXT("EarthBadge"), TEXT("Terre") },
	};

	FText FormatPlayTime(float Seconds)
	{
		const int32 Total = FMath::FloorToInt(Seconds / 60.f);
		return FText::FromString(FString::Printf(TEXT("%d h %02d"), Total / 60, Total % 60));
	}

	FText PocketName(int32 Pocket)
	{
		switch (Pocket)
		{
		case 0:  return LOCTEXT("PocketMedicine", "SOINS");
		case 1:  return LOCTEXT("PocketBalls", "CAPTURE");
		case 2:  return LOCTEXT("PocketOther", "BAIES ET MATÉRIAUX");
		default: return LOCTEXT("PocketKey", "OBJETS CLÉS");
		}
	}

	/** Options, dans l'ordre de la page */
	enum EOption : int32
	{
		OptTextSpeed, OptMasterVolume, OptMusicVolume, OptEffectsVolume, OptSensitivity, OptInvertY,
		OptShortcutBar, OptQuality, OptWindowMode, OptVSync, OptControls, OptDelete, OptionCount
	};

	/** Taille de l'image de la carte sur l'écran (pixels) */
	constexpr float MapSize = 470.f;
}

using namespace PokedexScreen;

// ---------------------------------------------------------------- Lignes cliquables

void UPokedexRowBinding::HandleClicked()
{
	if (UPokedexScreenWidget* Owner = Screen.Get())
	{
		Owner->OnRowClicked(Index);
	}
}

void UPokedexRowBinding::HandleHovered()
{
	if (UPokedexScreenWidget* Owner = Screen.Get())
	{
		Owner->OnRowHovered(Index);
	}
}

// ---------------------------------------------------------------- Arbre fixe

TSharedRef<SWidget> UPokedexScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(ScreenWidth);
		Root->SetHeightOverride(ScreenHeight);
		WidgetTree->RootWidget = Root;

		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Layers"));
		Root->SetContent(Layers);

		// Fond noir et quadrillage
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Back->SetBrushColor(Background);
		UOverlaySlot* BackSlot = Layers->AddChildToOverlay(Back);
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Fill);

		UCanvasPanel* GridPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Grid"));
		UOverlaySlot* GridSlot = Layers->AddChildToOverlay(GridPanel);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
		const float Step = 34.f;
		for (float X = Step; X < ScreenWidth; X += Step)
		{
			UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Line->SetBrushColor(Grid);
			UCanvasPanelSlot* LineSlot = GridPanel->AddChildToCanvas(Line);
			LineSlot->SetPosition(FVector2D(X, 0.f));
			LineSlot->SetSize(FVector2D(1.f, ScreenHeight));
		}
		for (float Y = Step; Y < ScreenHeight; Y += Step)
		{
			UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Line->SetBrushColor(Grid);
			UCanvasPanelSlot* LineSlot = GridPanel->AddChildToCanvas(Line);
			LineSlot->SetPosition(FVector2D(0.f, Y));
			LineSlot->SetSize(FVector2D(ScreenWidth, 1.f));
		}

		// Barre du haut, contenu, message, aide
		UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Main"));
		UOverlaySlot* MainSlot = Layers->AddChildToOverlay(Main);
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
		MainSlot->SetPadding(FMargin(22.f, 14.f, 22.f, 12.f));

		UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBar"));
		Main->AddChildToVerticalBox(TopBar);
		ClockText = MakeText(FText::GetEmpty(), 22, Text, true);
		TopBar->AddChildToHorizontalBox(ClockText);
		TitleText = MakeText(FText::GetEmpty(), 22, Accent, true);
		TitleText->SetJustification(ETextJustify::Center);
		UHorizontalBoxSlot* TitleSlot = TopBar->AddChildToHorizontalBox(TitleText);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TopBar->AddChildToHorizontalBox(MakeText(FText::FromString(TEXT("[|||]")), 18, Good, true));

		UBorder* Separator = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Separator->SetBrushColor(Dim);
		Separator->SetPadding(FMargin(0.f, 1.f));
		UVerticalBoxSlot* SeparatorSlot = Main->AddChildToVerticalBox(Separator);
		SeparatorSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));

		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
		UVerticalBoxSlot* ContentSlot = Main->AddChildToVerticalBox(Content);
		ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		StatusText = MakeText(FText::GetEmpty(), 19, Gold, true);
		StatusText->SetAutoWrapText(true);
		Main->AddChildToVerticalBox(StatusText);

		HintText = MakeText(FText::GetEmpty(), 16, Dim);
		UVerticalBoxSlot* HintSlot = Main->AddChildToVerticalBox(HintText);
		HintSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	return Super::RebuildWidget();
}

void UPokedexScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(bStandalone);
	BuildSpeciesList();
	ClockTimer = 0.f;
	if (Content && Content->GetChildrenCount() == 0)
	{
		Refresh();
	}
}

void UPokedexScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ClockTimer -= InDeltaTime;
	if (ClockTimer <= 0.f && ClockText)
	{
		ClockTimer = 1.f;
		ClockText->SetText(FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M"))));
	}

	if (StatusTimeLeft > 0.f)
	{
		StatusTimeLeft -= InDeltaTime;
		if (StatusTimeLeft <= 0.f && StatusText)
		{
			StatusText->SetText(FText::GetEmpty());
		}
	}

	// Les données arrivent du serveur un peu plus tard en multijoueur (soin, échange) : on relit régulièrement
	RefreshTimer -= InDeltaTime;
	if (RefreshTimer <= 0.f)
	{
		RefreshTimer = 0.5f;
		switch (Page)
		{
		case EPokedexPage::Home:
		case EPokedexPage::Party:
		case EPokedexPage::Summary:
		case EPokedexPage::Bag:
		case EPokedexPage::BagTarget:
		case EPokedexPage::Trainer:
		case EPokedexPage::Map:
		case EPokedexPage::Online:
			Refresh();
			break;
		default:
			break;
		}
	}
}

FReply UPokedexScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bStandalone && HandleKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UPokedexScreenWidget::HandleKey(const FKey& Key)
{
	if (IsCapturingKey())
	{
		CaptureKey(Key);
		return true;
	}
	if (Key == EKeys::Up || Key == EKeys::Z || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		Navigate(FIntPoint(0, -1));
	}
	else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		Navigate(FIntPoint(0, 1));
	}
	else if (Key == EKeys::Left || Key == EKeys::Q || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		Navigate(FIntPoint(-1, 0));
	}
	else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		Navigate(FIntPoint(1, 0));
	}
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		Confirm();
	}
	else if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (!Back() && bStandalone)
		{
			OnRequestClose.Broadcast();
		}
	}
	else
	{
		return false;
	}
	return true;
}

// ---------------------------------------------------------------- Navigation

void UPokedexScreenWidget::OpenMenu(EPokedexMenu Menu)
{
	CurrentMenu = Menu;
	switch (Menu)
	{
	case EPokedexMenu::Pokemon: MovingFrom = -1; SetPage(EPokedexPage::Party); break;
	case EPokedexMenu::Pokedex: BuildSpeciesList(); SetPage(EPokedexPage::Pokedex); break;
	case EPokedexMenu::Bag:     SetPage(EPokedexPage::Bag); break;
	case EPokedexMenu::Trainer: SetPage(EPokedexPage::Trainer); break;
	case EPokedexMenu::Map:     SetPage(EPokedexPage::Map); break;
	case EPokedexMenu::Save:    SetPage(EPokedexPage::Save); break;
	case EPokedexMenu::Online:  SetPage(EPokedexPage::Online); break;
	case EPokedexMenu::Options: SetPage(EPokedexPage::Options); break;
	case EPokedexMenu::Radio:
	case EPokedexMenu::Style:   SetPage(EPokedexPage::ComingSoon); break;
	}
}

FText UPokedexScreenWidget::GetMenuName(EPokedexMenu Menu)
{
	switch (Menu)
	{
	case EPokedexMenu::Pokemon: return LOCTEXT("MenuPokemon", "POKÉMON");
	case EPokedexMenu::Pokedex: return LOCTEXT("MenuPokedex", "POKÉDEX");
	case EPokedexMenu::Bag:     return LOCTEXT("MenuBag", "SAC");
	case EPokedexMenu::Radio:   return LOCTEXT("MenuRadio", "RADIO");
	case EPokedexMenu::Trainer: return LOCTEXT("MenuTrainer", "CARTE DRESSEUR");
	case EPokedexMenu::Map:     return LOCTEXT("MenuMap", "CARTE");
	case EPokedexMenu::Save:    return LOCTEXT("MenuSave", "SAUVEGARDER");
	case EPokedexMenu::Online:  return LOCTEXT("MenuOnline", "EN LIGNE");
	case EPokedexMenu::Options: return LOCTEXT("MenuOptions", "OPTIONS");
	case EPokedexMenu::Style:   return LOCTEXT("MenuStyle", "STYLE");
	}
	return FText::GetEmpty();
}

FText UPokedexScreenWidget::GetMenuDescription(EPokedexMenu Menu)
{
	switch (Menu)
	{
	case EPokedexMenu::Pokemon: return LOCTEXT("DescPokemon", "Ton équipe : stats, attaques, ordre de combat et objets tenus.");
	case EPokedexMenu::Pokedex: return LOCTEXT("DescPokedex", "Toutes les espèces vues ou capturées : description, types et où les trouver.");
	case EPokedexMenu::Bag:     return LOCTEXT("DescBag", "Soins, capture, baies et matériaux, objets clés.");
	case EPokedexMenu::Radio:   return LOCTEXT("DescRadio", "Musiques et émissions de Kanto. Bientôt !");
	case EPokedexMenu::Trainer: return LOCTEXT("DescTrainer", "Ton nom, ton argent, ton temps de jeu, tes badges et tes exploits.");
	case EPokedexMenu::Map:     return LOCTEXT("DescMap", "La carte de Kanto et l'endroit où tu te trouves.");
	case EPokedexMenu::Save:    return LOCTEXT("DescSave", "Enregistre ta progression maintenant.");
	case EPokedexMenu::Online:  return LOCTEXT("DescOnline", "Les joueurs connectés, les échanges et les combats entre amis.");
	case EPokedexMenu::Options: return LOCTEXT("DescOptions", "Texte, son, caméra, graphismes et commandes.");
	case EPokedexMenu::Style:   return LOCTEXT("DescStyle", "Personnalise ton dresseur. Bientôt !");
	}
	return FText::GetEmpty();
}

void UPokedexScreenWidget::ShowHome()
{
	SetPage(EPokedexPage::Home);
}

void UPokedexScreenWidget::SetPage(EPokedexPage NewPage, bool bResetSelection)
{
	Page = NewPage;
	if (bResetSelection)
	{
		Selection = 0;
		ScrollOffset = 0;
	}
	Refresh();
}

void UPokedexScreenWidget::Navigate(FIntPoint Direction)
{
	const int32 OldSelection = Selection;

	if (Direction.X != 0)
	{
		switch (Page)
		{
		case EPokedexPage::Summary:
			SummaryTab = (SummaryTab + Direction.X + 3) % 3;
			Feedback(EPokedexFeedback::Navigate);
			Refresh();
			return;
		case EPokedexPage::Bag:
			BagPocket = (BagPocket + Direction.X + 4) % 4;
			Selection = 0;
			ScrollOffset = 0;
			Feedback(EPokedexFeedback::Navigate);
			Refresh();
			return;
		case EPokedexPage::Options:
			ChangeOption(Selection, Direction.X);
			return;
		case EPokedexPage::Pokedex:
			// Gauche/droite : une page de liste d'un coup
			Selection = FMath::Clamp(Selection + Direction.X * ListWindow, 0, FMath::Max(0, RowCount - 1));
			break;
		default:
			return;
		}
	}
	else if (Direction.Y != 0)
	{
		switch (Page)
		{
		case EPokedexPage::Summary:
			if (const APokemonPlayerState* State = GetPlayerState(); State && State->Party.Num() > 1)
			{
				PartyIndex = (PartyIndex + Direction.Y + State->Party.Num()) % State->Party.Num();
				Feedback(EPokedexFeedback::Navigate);
				Refresh();
			}
			return;
		case EPokedexPage::PokedexEntry:
		{
			// Fiche précédente/suivante parmi les espèces vues
			const APokemonPlayerState* State = GetPlayerState();
			for (int32 Step = 1; State && Step < SpeciesList.Num(); ++Step)
			{
				const int32 Candidate = (DexIndex + Direction.Y * Step + SpeciesList.Num()) % SpeciesList.Num();
				if (State->PokedexSeen.Contains(SpeciesList[Candidate]))
				{
					DexIndex = Candidate;
					Feedback(EPokedexFeedback::Navigate);
					Refresh();
					break;
				}
			}
			return;
		}
		default:
			if (RowCount > 0)
			{
				Selection = (Selection + Direction.Y + RowCount) % RowCount;
			}
			break;
		}
	}

	if (Selection != OldSelection)
	{
		Feedback(EPokedexFeedback::Navigate);
		Refresh();
	}
}

void UPokedexScreenWidget::OnRowHovered(int32 Index)
{
	if (Index != Selection && Index >= 0 && Index < RowCount)
	{
		Selection = Index;
		Feedback(EPokedexFeedback::Navigate);
		// Seulement la surbrillance : reconstruire la page sous la souris ferait clignoter le survol
		for (int32 Row = 0; Row < RowBackgrounds.Num(); ++Row)
		{
			if (RowBackgrounds[Row] && RowBindings.IsValidIndex(Row))
			{
				RowBackgrounds[Row]->SetBrushColor(RowBindings[Row]->Index == Selection ? Selected : FLinearColor::Transparent);
			}
		}
	}
}

void UPokedexScreenWidget::OnRowClicked(int32 Index)
{
	if (Index >= 0 && Index < RowCount)
	{
		Selection = Index;
		Confirm();
	}
}

void UPokedexScreenWidget::Confirm()
{
	APokemonPlayerState* State = GetPlayerState();
	APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());

	switch (Page)
	{
	case EPokedexPage::Home:
		return;

	case EPokedexPage::Party:
		if (!State || !State->Party.IsValidIndex(Selection))
		{
			Feedback(EPokedexFeedback::Error);
			return;
		}
		if (MovingFrom >= 0)
		{
			// Deuxième choix de « Déplacer » : on échange
			if (MovingFrom != Selection && Controller)
			{
				Controller->ServerSwapPartyPokemon(MovingFrom, Selection);
				ShowStatus(LOCTEXT("Swapped", "Pokémon déplacés."));
			}
			MovingFrom = -1;
			Feedback(EPokedexFeedback::Confirm);
			Refresh();
			return;
		}
		PartyIndex = Selection;
		Feedback(EPokedexFeedback::Confirm);
		SetPage(EPokedexPage::PartyActions);
		return;

	case EPokedexPage::PartyActions:
		Feedback(EPokedexFeedback::Confirm);
		if (Selection == 0)
		{
			SummaryTab = 0;
			SetPage(EPokedexPage::Summary);
		}
		else if (Selection == 1)
		{
			MovingFrom = PartyIndex;
			Page = EPokedexPage::Party;
			Selection = PartyIndex;
			ShowStatus(LOCTEXT("MoveWhere", "Échanger avec quel Pokémon ?"));
			Refresh();
		}
		else if (Selection == 2 && State && State->Party.IsValidIndex(PartyIndex))
		{
			const FPokemonInstance& Pokemon = State->Party[PartyIndex];
			if (!Pokemon.HeldItem.IsNone())
			{
				// Il tient déjà un objet : on le reprend dans le sac
				const FItemRow* Held = GetDatabase() ? GetDatabase()->FindItem(Pokemon.HeldItem) : nullptr;
				if (Controller)
				{
					Controller->ServerTakeHeldItem(PartyIndex);
				}
				ShowStatus(FText::Format(LOCTEXT("TookItem", "{0} rangé dans le sac."), Held ? Held->DisplayName : FText::FromName(Pokemon.HeldItem)));
				Page = EPokedexPage::Party;
				Selection = PartyIndex;
				RefreshTimer = 0.1f;
				Refresh();
			}
			else
			{
				SetPage(EPokedexPage::GiveItem);
			}
		}
		else
		{
			Page = EPokedexPage::Party;
			Selection = PartyIndex;
			Refresh();
		}
		return;

	case EPokedexPage::GiveItem:
	{
		const TArray<FName> Items = GetHoldableItems();
		if (!Items.IsValidIndex(Selection) || !Controller || !State || !State->Party.IsValidIndex(PartyIndex))
		{
			Feedback(EPokedexFeedback::Error);
			return;
		}
		const FItemRow* Item = GetDatabase() ? GetDatabase()->FindItem(Items[Selection]) : nullptr;
		Controller->ServerGiveHeldItem(PartyIndex, Items[Selection]);
		Feedback(EPokedexFeedback::Confirm);
		ShowStatus(FText::Format(LOCTEXT("GaveItem", "{0} tient maintenant : {1}."),
			UPokemonLibrary::GetDisplayName(GetDatabase(), State->Party[PartyIndex]), Item ? Item->DisplayName : FText::FromName(Items[Selection])));
		Page = EPokedexPage::Party;
		Selection = PartyIndex;
		RefreshTimer = 0.1f;
		Refresh();
		return;
	}

	case EPokedexPage::Summary:
		Back();
		return;

	case EPokedexPage::Pokedex:
		if (State && SpeciesList.IsValidIndex(Selection) && State->PokedexSeen.Contains(SpeciesList[Selection]))
		{
			DexIndex = Selection;
			Feedback(EPokedexFeedback::Confirm);
			SetPage(EPokedexPage::PokedexEntry);
		}
		else
		{
			Feedback(EPokedexFeedback::Error);
			ShowStatus(LOCTEXT("NotSeen", "Ce Pokémon n'a pas encore été vu."), true);
		}
		return;

	case EPokedexPage::PokedexEntry:
		Back();
		return;

	case EPokedexPage::Bag:
	{
		const TArray<FName> Items = GetBagPocketItems(BagPocket);
		if (!Items.IsValidIndex(Selection))
		{
			Feedback(EPokedexFeedback::Error);
			return;
		}
		SelectedItem = Items[Selection];
		const UPokemonDatabase* Database = GetDatabase();
		const FItemRow* Item = Database ? Database->FindItem(SelectedItem) : nullptr;
		if (Item && Item->bUsableInField && Item->Category == EItemCategory::Medicine)
		{
			Feedback(EPokedexFeedback::Confirm);
			SetPage(EPokedexPage::BagTarget);
		}
		else if (SelectedItem == TEXT("Pokedex"))
		{
			Feedback(EPokedexFeedback::Confirm);
			ShowStatus(LOCTEXT("ItsYourDex", "C'est toi qui le tiens entre tes mains !"));
		}
		else
		{
			Feedback(EPokedexFeedback::Error);
			ShowStatus(LOCTEXT("NotNow", "Ce n'est pas le moment d'utiliser ça !"), true);
		}
		return;
	}

	case EPokedexPage::BagTarget:
		UseSelectedItemOn(Selection);
		return;

	case EPokedexPage::Trainer:
		return;

	case EPokedexPage::Save:
		if (Selection == 0)
		{
			DoSave();
		}
		else
		{
			Back();
		}
		return;

	case EPokedexPage::Options:
		if (Selection == OptDelete)
		{
			Feedback(EPokedexFeedback::Confirm);
			SetPage(EPokedexPage::ConfirmDelete);
		}
		else if (Selection == OptControls)
		{
			Feedback(EPokedexFeedback::Confirm);
			SetPage(EPokedexPage::Controls);
		}
		else
		{
			ChangeOption(Selection, 1);
		}
		return;

	case EPokedexPage::Online:
		Feedback(EPokedexFeedback::Error);
		ShowStatus(LOCTEXT("OnlineSoon", "Les échanges et les combats entre joueurs arrivent avec le jeu à 4 en ligne (SCRUM-69)."));
		return;

	case EPokedexPage::Map:
		Feedback(EPokedexFeedback::Navigate);
		return;

	case EPokedexPage::Controls:
	{
		APokemonNewAgePlayerController* Player = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
		const TArray<FPokemonBindableKey> Keys = Player ? Player->GetBindableKeys() : TArray<FPokemonBindableKey>();
		if (Keys.IsValidIndex(Selection))
		{
			CapturingKey = Keys[Selection].Id;
			Feedback(EPokedexFeedback::Confirm);
			ShowStatus(FText::Format(Keys[Selection].bGamepad
				? LOCTEXT("PressPad", "Appuie sur le bouton de la manette pour « {0} »   (Échap : annuler)")
				: LOCTEXT("PressKey", "Appuie sur la nouvelle touche pour « {0} »   (Échap : annuler)"), Keys[Selection].Name));
			StatusTimeLeft = 30.f;
			Refresh();
		}
		else if (Player && Selection == Keys.Num())
		{
			Player->ResetKeyBindings();
			Feedback(EPokedexFeedback::Saved);
			ShowStatus(LOCTEXT("KeysReset", "Touches d'origine remises."));
			Refresh();
		}
		return;
	}

	case EPokedexPage::ComingSoon:
		Back();
		return;

	case EPokedexPage::ConfirmDelete:
		if (Selection == 1)
		{
			const bool bDeleted = UPokemonSaveLibrary::DeleteSaveGame();
			Feedback(bDeleted ? EPokedexFeedback::Confirm : EPokedexFeedback::Error);
			ShowStatus(bDeleted ? LOCTEXT("Deleted", "Sauvegarde effacée. Le prochain lancement commencera une nouvelle partie.")
				: LOCTEXT("NothingToDelete", "Il n'y a pas de sauvegarde."), !bDeleted);
		}
		Page = EPokedexPage::Options;
		Selection = OptDelete;
		Refresh();
		return;
	}
}

bool UPokedexScreenWidget::Back()
{
	switch (Page)
	{
	case EPokedexPage::Home:
		return false;
	case EPokedexPage::Party:
		if (MovingFrom >= 0)
		{
			MovingFrom = -1;
			Feedback(EPokedexFeedback::Back);
			Refresh();
			return true;
		}
		break;
	case EPokedexPage::PartyActions:
	case EPokedexPage::Summary:
	case EPokedexPage::GiveItem:
		Feedback(EPokedexFeedback::Back);
		Page = EPokedexPage::Party;
		Selection = PartyIndex;
		Refresh();
		return true;
	case EPokedexPage::PokedexEntry:
		Feedback(EPokedexFeedback::Back);
		Page = EPokedexPage::Pokedex;
		Selection = DexIndex;
		Refresh();
		return true;
	case EPokedexPage::BagTarget:
	{
		Feedback(EPokedexFeedback::Back);
		Page = EPokedexPage::Bag;
		Selection = FMath::Max(0, GetBagPocketItems(BagPocket).IndexOfByKey(SelectedItem));
		Refresh();
		return true;
	}
	case EPokedexPage::ConfirmDelete:
	case EPokedexPage::Controls:
		Feedback(EPokedexFeedback::Back);
		Selection = Page == EPokedexPage::Controls ? int32(OptControls) : int32(OptDelete);
		Page = EPokedexPage::Options;
		Refresh();
		return true;
	default:
		break;
	}

	// Écran à plat : pas d'accueil, on rend la main à la liste du menu
	if (bStandalone)
	{
		return false;
	}
	Feedback(EPokedexFeedback::Back);
	SetPage(EPokedexPage::Home);
	return true;
}

// ---------------------------------------------------------------- Pages

void UPokedexScreenWidget::Refresh()
{
	if (!Content)
	{
		return;
	}
	const int32 SelectionBefore = Selection;
	Content->ClearChildren();
	RowTarget = Content;
	RowBindings.Reset();
	RowBackgrounds.Reset();
	RowCount = 0;

	switch (Page)
	{
	case EPokedexPage::Home:          BuildHome(); break;
	case EPokedexPage::Party:         BuildParty(); break;
	case EPokedexPage::PartyActions:  BuildPartyActions(); break;
	case EPokedexPage::Summary:       BuildSummary(); break;
	case EPokedexPage::Pokedex:       BuildPokedex(); break;
	case EPokedexPage::PokedexEntry:  BuildPokedexEntry(); break;
	case EPokedexPage::Bag:           BuildBag(); break;
	case EPokedexPage::BagTarget:     BuildBagTarget(); break;
	case EPokedexPage::Trainer:       BuildTrainer(); break;
	case EPokedexPage::Save:          BuildSave(); break;
	case EPokedexPage::Options:       BuildOptions(); break;
	case EPokedexPage::ConfirmDelete: BuildConfirmDelete(); break;
	case EPokedexPage::GiveItem:      BuildGiveItem(); break;
	case EPokedexPage::Map:           BuildMap(); break;
	case EPokedexPage::Online:        BuildOnline(); break;
	case EPokedexPage::Controls:      BuildControls(); break;
	case EPokedexPage::ComingSoon:    BuildComingSoon(); break;
	}

	// La liste a raccourci (objet épuisé...) : on recolle la sélection et on redessine la surbrillance
	if (RowCount > 0 && Selection >= RowCount)
	{
		Selection = RowCount - 1;
		if (SelectionBefore != Selection)
		{
			Refresh();
		}
	}
}

void UPokedexScreenWidget::BuildHome()
{
	TitleText->SetText(LOCTEXT("TitleHome", "POKÉDEX"));
	HintText->SetText(LOCTEXT("HintHome", "Choisis un des 10 boutons du volet.   Clic droit maintenu : tourner   Molette : zoom   Tab : fermer"));

	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	if (!State)
	{
		AddLine(LOCTEXT("NoState", "Pas de données de dresseur (GameMode du jeu ?)"), 20, ErrorColor);
		return;
	}

	AddLine(FText::Format(LOCTEXT("Hello", "Bonjour, {0} !"), FText::FromString(State->GetPlayerName())), 26, Text, true);
	if (!State->CurrentLocation.IsNone())
	{
		AddLine(FText::Format(LOCTEXT("Location", "Lieu : {0}"), FText::FromName(State->CurrentLocation)), 18, Dim);
	}
	AddSpacer(10.f);

	if (State->Party.Num() == 0)
	{
		AddLine(LOCTEXT("NoPokemon", "Tu n'as pas encore de Pokémon.\nVa voir le Professeur Chen !"), 20, Dim);
		return;
	}

	AddLine(LOCTEXT("TeamTitle", "ÉQUIPE"), 18, Accent, true);
	for (const FPokemonInstance& Pokemon : State->Party)
	{
		UHorizontalBox* Row = AddRow(-1, false);
		AddRowText(Row, UPokemonLibrary::GetDisplayName(Database, Pokemon), 20, Text, true, true);
		AddRowText(Row, FText::Format(LOCTEXT("LevelShort", "N.{0}  "), Pokemon.Level), 18, Dim);
		AddRowHPBar(Row, Pokemon.CurrentHP, Pokemon.GetMaxHP(), 170.f);
	}
	AddSpacer(8.f);
	AddLine(FText::Format(LOCTEXT("HomeDex", "Pokédex : {0} vus, {1} capturés     Badges : {2}"),
		State->PokedexSeen.Num(), State->PokedexCaught.Num(), State->Badges.Num()), 17, Dim);
}

void UPokedexScreenWidget::BuildParty()
{
	TitleText->SetText(LOCTEXT("TitleParty", "POKÉMON"));
	HintText->SetText(MovingFrom >= 0 ? LOCTEXT("HintMove", "A : échanger ici   B : annuler")
		: LOCTEXT("HintParty", "A : choisir   B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	if (!State || State->Party.Num() == 0)
	{
		AddLine(LOCTEXT("PartyEmpty", "Aucun Pokémon dans l'équipe."), 20, Dim);
		return;
	}

	for (int32 Index = 0; Index < State->Party.Num(); ++Index)
	{
		const FPokemonInstance& Pokemon = State->Party[Index];
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, FText::FromString(Index == MovingFrom ? TEXT("<> ") : FString::Printf(TEXT("%d  "), Index + 1)), 18, Index == MovingFrom ? Gold : Dim);
		AddRowText(Row, UPokemonLibrary::GetDisplayName(Database, Pokemon), 21, Pokemon.IsFainted() ? ErrorColor : Text, true, true);
		// Losange doré : il tient un objet
		AddRowText(Row, FText::FromString(Pokemon.HeldItem.IsNone() ? TEXT("    ") : TEXT(" <> ")), 16, Gold, false, true);
		AddRowText(Row, FText::Format(LOCTEXT("Level", "N.{0} "), Pokemon.Level), 18, Dim);
		AddRowHPBar(Row, Pokemon.CurrentHP, Pokemon.GetMaxHP(), 120.f);
		AddRowText(Row, FText::FromString(FString::Printf(TEXT(" %d/%d "), Pokemon.CurrentHP, Pokemon.GetMaxHP())), 16, Text);
		AddRowText(Row, Pokemon.IsFainted() ? LOCTEXT("KO", "K.O.") : GetStatusShort(Pokemon.Status), 15, ErrorColor, false, true);
	}
}

void UPokedexScreenWidget::BuildPartyActions()
{
	TitleText->SetText(LOCTEXT("TitleParty", "POKÉMON"));
	HintText->SetText(LOCTEXT("HintActions", "A : valider   B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	if (!State || !State->Party.IsValidIndex(PartyIndex))
	{
		Page = EPokedexPage::Party;
		BuildParty();
		return;
	}
	AddLine(FText::Format(LOCTEXT("WhatToDo", "Que faire avec {0} ?"), UPokemonLibrary::GetDisplayName(GetDatabase(), State->Party[PartyIndex])), 22, Text, true);
	AddSpacer(8.f);
	const FPokemonInstance& Pokemon = State->Party[PartyIndex];
	const FItemRow* Held = (GetDatabase() && !Pokemon.HeldItem.IsNone()) ? GetDatabase()->FindItem(Pokemon.HeldItem) : nullptr;
	const FText Actions[] = {
		LOCTEXT("ActionSummary", "Résumé"),
		LOCTEXT("ActionMove", "Déplacer"),
		Pokemon.HeldItem.IsNone() ? LOCTEXT("ActionGive", "Donner un objet")
			: FText::Format(LOCTEXT("ActionTake", "Reprendre l'objet ({0})"), Held ? Held->DisplayName : FText::FromName(Pokemon.HeldItem)),
		LOCTEXT("ActionCancel", "Annuler")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		AddRowText(AddRow(Index), Actions[Index], 22, Text, true);
	}
}

void UPokedexScreenWidget::BuildSummary()
{
	TitleText->SetText(LOCTEXT("TitleSummary", "RÉSUMÉ"));
	HintText->SetText(LOCTEXT("HintSummary", "Gauche/droite : page   Haut/bas : autre Pokémon   B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	if (!State || !State->Party.IsValidIndex(PartyIndex) || !Database)
	{
		AddLine(LOCTEXT("NoSummary", "Pokémon introuvable."), 20, ErrorColor);
		return;
	}
	const FPokemonInstance& Pokemon = State->Party[PartyIndex];
	const FSpeciesRow* Species = Database->FindSpecies(Pokemon.Species);

	// En-tête : nom, niveau, types
	UHorizontalBox* Header = AddRow(-1, false);
	AddRowText(Header, UPokemonLibrary::GetDisplayName(Database, Pokemon), 26, Text, false, true);
	AddRowText(Header, FText::Format(LOCTEXT("LevelLong", "  N.{0}  "), Pokemon.Level), 20, Dim);
	if (Species)
	{
		AddTypeBadges(Header, Species->Type1, Species->Type2);
	}
	UHorizontalBox* Header2 = AddRow(-1, false);
	AddRowHPBar(Header2, Pokemon.CurrentHP, Pokemon.GetMaxHP(), 200.f);
	AddRowText(Header2, FText::FromString(FString::Printf(TEXT("  PV %d/%d  "), Pokemon.CurrentHP, Pokemon.GetMaxHP())), 17, Text);
	AddRowText(Header2, Pokemon.IsFainted() ? LOCTEXT("KO", "K.O.") : GetStatusShort(Pokemon.Status), 17, ErrorColor, false, true);

	// Onglets
	UHorizontalBox* Tabs = AddRow(-1, false);
	const FText TabNames[] = { LOCTEXT("TabInfo", "INFOS"), LOCTEXT("TabStats", "STATS"), LOCTEXT("TabMoves", "ATTAQUES") };
	for (int32 Tab = 0; Tab < 3; ++Tab)
	{
		AddRowText(Tabs, FText::Format(LOCTEXT("TabFormat", "{0}    "), TabNames[Tab]), 18, Tab == SummaryTab ? Accent : Dim, false, true);
	}
	AddSpacer(4.f);

	if (SummaryTab == 0)
	{
		if (Species)
		{
			AddLabelValue(LOCTEXT("DexNumber", "N° Pokédex"), FText::FromString(FString::Printf(TEXT("%03d  %s"), Species->NationalDex, *Species->DisplayName.ToString())));
		}
		const FNatureRow* Nature = Database->FindNature(Pokemon.Nature);
		AddLabelValue(LOCTEXT("Nature", "Nature"), Nature ? Nature->DisplayName : FText::FromName(Pokemon.Nature));
		const FAbilityRow* Ability = Database->Abilities ? Database->Abilities->FindRow<FAbilityRow>(Pokemon.Ability, TEXT("Pokedex"), false) : nullptr;
		AddLabelValue(LOCTEXT("Ability", "Talent"), Ability ? Ability->DisplayName : FText::FromName(Pokemon.Ability));
		AddLabelValue(LOCTEXT("Gender", "Sexe"), (Species && Species->bGenderless) ? LOCTEXT("Genderless", "Inconnu")
			: (Pokemon.bIsFemale ? LOCTEXT("Female", "Femelle") : LOCTEXT("Male", "Mâle")));
		const FItemRow* Held = Pokemon.HeldItem.IsNone() ? nullptr : Database->FindItem(Pokemon.HeldItem);
		AddLabelValue(LOCTEXT("HeldItem", "Objet tenu"), Pokemon.HeldItem.IsNone() ? LOCTEXT("NoHeldItem", "aucun")
			: (Held ? Held->DisplayName : FText::FromName(Pokemon.HeldItem)));
		const FItemRow* Ball = Database->FindItem(Pokemon.Ball);
		AddLabelValue(LOCTEXT("Met", "Rencontré"), FText::Format(LOCTEXT("MetFormat", "{0}, N.{1}{2}"),
			Pokemon.MetLocation.IsNone() ? LOCTEXT("Unknown", "?") : FText::FromName(Pokemon.MetLocation), Pokemon.MetLevel,
			Ball ? FText::Format(LOCTEXT("BallFormat", " ({0})"), Ball->DisplayName) : FText::GetEmpty()));

		if (Species)
		{
			const int32 ThisLevel = Database->GetExperienceForLevel(Species->GrowthRate, Pokemon.Level);
			const int32 NextLevel = Database->GetExperienceForLevel(Species->GrowthRate, FMath::Min(Pokemon.Level + 1, 100));
			AddLabelValue(LOCTEXT("Exp", "Expérience"), FText::AsNumber(Pokemon.Experience));
			AddLabelValue(LOCTEXT("ExpNext", "Niveau suivant"), Pokemon.Level >= 100 ? LOCTEXT("MaxLevel", "niveau max")
				: FText::Format(LOCTEXT("ExpLeft", "encore {0} XP"), FMath::Max(0, NextLevel - Pokemon.Experience)));
			UProgressBar* ExpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
			ExpBar->SetPercent(NextLevel > ThisLevel ? float(Pokemon.Experience - ThisLevel) / float(NextLevel - ThisLevel) : 1.f);
			ExpBar->SetFillColorAndOpacity(Accent);
			USizeBox* ExpBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			ExpBox->SetHeightOverride(8.f);
			ExpBox->SetContent(ExpBar);
			UVerticalBoxSlot* ExpSlot = RowTarget->AddChildToVerticalBox(ExpBox);
			ExpSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}
	else if (SummaryTab == 1)
	{
		const FNatureRow* Nature = Database->FindNature(Pokemon.Nature);
		struct FStatLine { EPokemonStat Stat; FText Name; int32 Value; };
		const FStatLine Lines[] = {
			{ EPokemonStat::HP, LOCTEXT("StatHP", "PV"), Pokemon.GetMaxHP() },
			{ EPokemonStat::Attack, LOCTEXT("StatAtk", "Attaque"), Pokemon.Stats.Attack },
			{ EPokemonStat::Defense, LOCTEXT("StatDef", "Défense"), Pokemon.Stats.Defense },
			{ EPokemonStat::SpecialAttack, LOCTEXT("StatSpA", "Attaque Spé."), Pokemon.Stats.SpecialAttack },
			{ EPokemonStat::SpecialDefense, LOCTEXT("StatSpD", "Défense Spé."), Pokemon.Stats.SpecialDefense },
			{ EPokemonStat::Speed, LOCTEXT("StatSpe", "Vitesse"), Pokemon.Stats.Speed },
		};
		for (const FStatLine& Line : Lines)
		{
			const bool bNeutral = !Nature || Nature->IncreasedStat == Nature->DecreasedStat;
			const bool bUp = !bNeutral && Nature->IncreasedStat == Line.Stat;
			const bool bDown = !bNeutral && Nature->DecreasedStat == Line.Stat;
			UHorizontalBox* Row = AddRow(-1, false);
			AddRowText(Row, Line.Name, 20, bUp ? ErrorColor : (bDown ? Accent : Text), true);
			AddRowText(Row, FText::FromString(bUp ? TEXT("+ ") : (bDown ? TEXT("- ") : TEXT("  "))), 20, bUp ? ErrorColor : Accent, false, true);
			AddRowText(Row, FText::AsNumber(Line.Value), 20, Text, false, true);
		}
		AddSpacer(6.f);
		AddLine(LOCTEXT("NatureHint", "En rouge : stat augmentée par la nature, en bleu : diminuée."), 15, Dim);
	}
	else
	{
		for (int32 MoveIndex = 0; MoveIndex < 4; ++MoveIndex)
		{
			UHorizontalBox* Row = AddRow(-1, false);
			if (!Pokemon.Moves.IsValidIndex(MoveIndex) || Pokemon.Moves[MoveIndex].Move.IsNone())
			{
				AddRowText(Row, LOCTEXT("NoMove", "-"), 20, Dim, true);
				continue;
			}
			const FPokemonMoveSlot& MoveSlot = Pokemon.Moves[MoveIndex];
			const FMoveRow* Move = Database->FindMove(MoveSlot.Move);
			if (Move)
			{
				AddTypeBadges(Row, Move->Type, EPokemonType::None);
			}
			AddRowText(Row, Move ? Move->DisplayName : FText::FromName(MoveSlot.Move), 20, Text, true, true);
			AddRowText(Row, FText::FromString(FString::Printf(TEXT("PP %d/%d"), MoveSlot.CurrentPP, MoveSlot.MaxPP)), 17,
				MoveSlot.CurrentPP == 0 ? ErrorColor : Text);
			if (Move)
			{
				UHorizontalBox* Details = AddRow(-1, false);
				const FText Category = Move->Category == EMoveCategory::Physical ? LOCTEXT("Physical", "Physique")
					: (Move->Category == EMoveCategory::Special ? LOCTEXT("Special", "Spéciale") : LOCTEXT("Status", "Statut"));
				AddRowText(Details, FText::Format(LOCTEXT("MoveDetails", "      {0}   Puissance {1}   Précision {2}"), Category,
					Move->Power > 0 ? FText::AsNumber(Move->Power) : LOCTEXT("Dash", "-"),
					Move->Accuracy > 0 ? FText::AsNumber(Move->Accuracy) : LOCTEXT("Dash", "-")), 15, Dim, true);
			}
		}
	}
}

void UPokedexScreenWidget::BuildPokedex()
{
	TitleText->SetText(LOCTEXT("TitleDex", "POKÉDEX"));
	HintText->SetText(LOCTEXT("HintDex", "A : fiche   Gauche/droite : page suivante   B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	if (!State || SpeciesList.Num() == 0)
	{
		AddLine(LOCTEXT("NoDexData", "Données Pokémon introuvables (DA_PokemonDatabase ?)"), 20, ErrorColor);
		return;
	}

	UHorizontalBox* Counters = AddRow(-1, false);
	AddRowText(Counters, FText::Format(LOCTEXT("Seen", "Vus : {0}"), State->PokedexSeen.Num()), 19, Text, true, true);
	AddRowText(Counters, FText::Format(LOCTEXT("Caught", "Capturés : {0}"), State->PokedexCaught.Num()), 19, Good, true, true);
	AddSpacer(4.f);

	RowCount = SpeciesList.Num();
	Selection = FMath::Clamp(Selection, 0, RowCount - 1);
	if (Selection < ScrollOffset)
	{
		ScrollOffset = Selection;
	}
	else if (Selection >= ScrollOffset + ListWindow)
	{
		ScrollOffset = Selection - ListWindow + 1;
	}

	const UPokemonDatabase* Database = GetDatabase();
	for (int32 Index = ScrollOffset; Index < FMath::Min(RowCount, ScrollOffset + ListWindow); ++Index)
	{
		const FName Species = SpeciesList[Index];
		const FSpeciesRow* Row = Database ? Database->FindSpecies(Species) : nullptr;
		const bool bCaught = State->PokedexCaught.Contains(Species);
		const bool bSeen = bCaught || State->PokedexSeen.Contains(Species);

		UHorizontalBox* Line = AddRow(Index);
		// Pastille : rouge = capturé, grise = vu
		UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Dot->SetBrushColor(bCaught ? FLinearColor(0.9f, 0.15f, 0.12f) : (bSeen ? Dim : FLinearColor::Transparent));
		USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DotBox->SetWidthOverride(12.f);
		DotBox->SetHeightOverride(12.f);
		DotBox->SetContent(Dot);
		UHorizontalBoxSlot* DotSlot = Line->AddChildToHorizontalBox(DotBox);
		DotSlot->SetVerticalAlignment(VAlign_Center);
		DotSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));

		AddRowText(Line, FText::FromString(FString::Printf(TEXT("N°%03d   "), Row ? Row->NationalDex : Index + 1)), 19, Dim);
		AddRowText(Line, bSeen && Row ? Row->DisplayName : LOCTEXT("Unknown10", "----------"), 20, bSeen ? Text : Dim, true, true);
	}
}

void UPokedexScreenWidget::BuildPokedexEntry()
{
	TitleText->SetText(LOCTEXT("TitleEntry", "FICHE"));
	HintText->SetText(LOCTEXT("HintEntry", "Haut/bas : fiche suivante   B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	const FSpeciesRow* Species = (Database && SpeciesList.IsValidIndex(DexIndex)) ? Database->FindSpecies(SpeciesList[DexIndex]) : nullptr;
	if (!State || !Species)
	{
		AddLine(LOCTEXT("NoEntry", "Fiche introuvable."), 20, ErrorColor);
		return;
	}
	const bool bCaught = State->PokedexCaught.Contains(SpeciesList[DexIndex]);

	UHorizontalBox* Header = AddRow(-1, false);
	AddRowText(Header, FText::FromString(FString::Printf(TEXT("N°%03d  "), Species->NationalDex)), 22, Dim);
	AddRowText(Header, Species->DisplayName, 28, Text, true, true);
	UHorizontalBox* Types = AddRow(-1, false);
	AddRowText(Types, Species->Genus, 19, Accent, true);
	AddTypeBadges(Types, Species->Type1, Species->Type2);
	AddSpacer(8.f);

	if (bCaught)
	{
		AddLabelValue(LOCTEXT("Height", "Taille"), FText::FromString(FString::Printf(TEXT("%.1f m"), Species->HeightM)));
		AddLabelValue(LOCTEXT("Weight", "Poids"), FText::FromString(FString::Printf(TEXT("%.1f kg"), Species->WeightKg)));
		AddSpacer(8.f);
		AddLine(Species->PokedexEntry, 19, Text);
	}
	else
	{
		AddLabelValue(LOCTEXT("Height", "Taille"), LOCTEXT("UnknownValue", "???"));
		AddLabelValue(LOCTEXT("Weight", "Poids"), LOCTEXT("UnknownValue", "???"));
		AddSpacer(8.f);
		AddLine(LOCTEXT("CatchToKnow", "Capture ce Pokémon pour compléter sa fiche."), 19, Dim);
	}

	// Où le trouver (tables de rencontres)
	AddSpacer(8.f);
	const TArray<FText> Locations = GetSpeciesLocations(SpeciesList[DexIndex]);
	FString LocationList;
	for (const FText& Location : Locations)
	{
		LocationList += (LocationList.IsEmpty() ? TEXT("") : TEXT(", ")) + Location.ToString();
	}
	AddLabelValue(LOCTEXT("Habitat", "Où le trouver"), Locations.Num() > 0 ? FText::FromString(LocationList)
		: LOCTEXT("NoHabitat", "pas à l'état sauvage dans la V1"));
}

void UPokedexScreenWidget::BuildBag()
{
	TitleText->SetText(LOCTEXT("TitleBag", "SAC"));
	HintText->SetText(LOCTEXT("HintBag", "Gauche/droite : poche   A : utiliser   B : retour"));

	UHorizontalBox* Tabs = AddRow(-1, false);
	AddRowText(Tabs, FText::FromString(TEXT("<  ")), 20, Dim);
	AddRowText(Tabs, PocketName(BagPocket), 21, Accent, false, true);
	AddRowText(Tabs, FText::FromString(TEXT("  >")), 20, Dim);
	AddSpacer(4.f);

	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	const TArray<FName> Items = GetBagPocketItems(BagPocket);
	if (!State || Items.Num() == 0)
	{
		AddLine(LOCTEXT("PocketEmpty", "Cette poche est vide."), 20, Dim);
		return;
	}

	RowCount = Items.Num();
	Selection = FMath::Clamp(Selection, 0, RowCount - 1);
	const int32 Window = ListWindow - 2;
	if (Selection < ScrollOffset)
	{
		ScrollOffset = Selection;
	}
	else if (Selection >= ScrollOffset + Window)
	{
		ScrollOffset = Selection - Window + 1;
	}

	for (int32 Index = ScrollOffset; Index < FMath::Min(RowCount, ScrollOffset + Window); ++Index)
	{
		const FItemRow* Item = Database ? Database->FindItem(Items[Index]) : nullptr;
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, Item ? Item->DisplayName : FText::FromName(Items[Index]), 21, Text, true, true);
		if (!Item || Item->Category != EItemCategory::KeyItem)
		{
			AddRowText(Row, FText::FromString(FString::Printf(TEXT("x%d"), State->GetItemCount(Items[Index]))), 19, Text);
		}
	}

	if (const FItemRow* Item = Database ? Database->FindItem(Items[Selection]) : nullptr)
	{
		AddSpacer(6.f);
		AddLine(Item->Description, 17, Dim);
	}
}

void UPokedexScreenWidget::BuildBagTarget()
{
	const UPokemonDatabase* Database = GetDatabase();
	const FItemRow* Item = Database ? Database->FindItem(SelectedItem) : nullptr;
	const APokemonPlayerState* State = GetPlayerState();
	TitleText->SetText(LOCTEXT("TitleBag", "SAC"));
	HintText->SetText(LOCTEXT("HintTarget", "A : utiliser   B : retour"));

	if (!Item || !State || State->GetItemCount(SelectedItem) <= 0)
	{
		Page = EPokedexPage::Bag;
		Selection = 0;
		BuildBag();
		return;
	}

	AddLine(FText::Format(LOCTEXT("UseOn", "{0} (x{1}) : sur quel Pokémon ?"), Item->DisplayName, State->GetItemCount(SelectedItem)), 21, Text, true);
	AddSpacer(6.f);
	for (int32 Index = 0; Index < State->Party.Num(); ++Index)
	{
		const FPokemonInstance& Pokemon = State->Party[Index];
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, UPokemonLibrary::GetDisplayName(Database, Pokemon), 20, Pokemon.IsFainted() ? ErrorColor : Text, true, true);
		AddRowHPBar(Row, Pokemon.CurrentHP, Pokemon.GetMaxHP(), 120.f);
		AddRowText(Row, FText::FromString(FString::Printf(TEXT(" %d/%d "), Pokemon.CurrentHP, Pokemon.GetMaxHP())), 16, Text);
		AddRowText(Row, Pokemon.IsFainted() ? LOCTEXT("KO", "K.O.") : GetStatusShort(Pokemon.Status), 15, ErrorColor, false, true);
	}
}

void UPokedexScreenWidget::BuildTrainer()
{
	TitleText->SetText(LOCTEXT("TitleTrainer", "CARTE DRESSEUR"));
	HintText->SetText(LOCTEXT("HintTrainer", "B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	if (!State)
	{
		return;
	}
	AddLabelValue(LOCTEXT("Name", "Nom"), FText::FromString(State->GetPlayerName()));
	AddLabelValue(LOCTEXT("TrainerId", "N° ID"), FText::FromString(FString::Printf(TEXT("%05d"), State->TrainerNumber)));
	AddLabelValue(LOCTEXT("Money", "Argent"), FText::Format(LOCTEXT("MoneyFormat", "{0} P$"), FText::AsNumber(State->Money)));
	AddLabelValue(LOCTEXT("Dex", "Pokédex"), FText::Format(LOCTEXT("DexFormat", "{0} vus, {1} capturés"), State->PokedexSeen.Num(), State->PokedexCaught.Num()));
	AddLabelValue(LOCTEXT("PlayTime", "Temps de jeu"), FormatPlayTime(State->GetPlayTimeSeconds()));
	AddSpacer(10.f);
	AddLine(LOCTEXT("Badges", "BADGES"), 18, Accent, true);

	UHorizontalBox* BadgeRow = AddRow(-1, false);
	for (const FBadgeInfo& Badge : KantoBadges)
	{
		const bool bOwned = State->Badges.Contains(FName(Badge.Id));
		UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Cell->SetBrushColor(bOwned ? Gold : FLinearColor(0.08f, 0.1f, 0.12f));
		Cell->SetPadding(FMargin(2.f, 10.f));
		Cell->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Label = MakeText(FText::FromString(Badge.Name), 13, bOwned ? FLinearColor(0.1f, 0.06f, 0.f) : Dim, true);
		Cell->SetContent(Label);
		UHorizontalBoxSlot* CellSlot = BadgeRow->AddChildToHorizontalBox(Cell);
		CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CellSlot->SetPadding(FMargin(2.f));
	}

	// Exploits : calculés depuis la progression (pas encore de système de quêtes)
	AddSpacer(8.f);
	AddLine(LOCTEXT("Achievements", "EXPLOITS"), 18, Accent, true);
	struct FAchievement { FText Name; bool bDone; };
	const FAchievement Achievements[] = {
		{ LOCTEXT("AchFirstPokemon", "Recevoir son premier Pokémon"), State->Party.Num() + State->StoragePC.Num() > 0 },
		{ LOCTEXT("AchCatch10", "Capturer 10 Pokémon"), State->PokedexCaught.Num() >= 10 },
		{ LOCTEXT("AchSee50", "Voir 50 Pokémon"), State->PokedexSeen.Num() >= 50 },
		{ LOCTEXT("AchBoulder", "Battre Pierre, champion d'Argenta"), State->Badges.Contains(FName(TEXT("BoulderBadge"))) },
		{ LOCTEXT("AchAllBadges", "Obtenir les 8 badges de Kanto"), State->Badges.Num() >= 8 },
	};
	for (const FAchievement& Achievement : Achievements)
	{
		UHorizontalBox* Row = AddRow(-1, false);
		AddRowText(Row, FText::FromString(Achievement.bDone ? TEXT("[X]  ") : TEXT("[  ]  ")), 17, Achievement.bDone ? Good : Dim, false, true);
		AddRowText(Row, Achievement.Name, 17, Achievement.bDone ? Text : Dim, true);
	}
}

void UPokedexScreenWidget::BuildSave()
{
	TitleText->SetText(LOCTEXT("TitleSave", "SAUVEGARDER"));
	HintText->SetText(LOCTEXT("HintSave", "A : valider   B : retour"));

	if (const APokemonPlayerState* State = GetPlayerState())
	{
		AddLabelValue(LOCTEXT("Player", "Joueur"), FText::FromString(State->GetPlayerName()));
		AddLabelValue(LOCTEXT("Badges2", "Badges"), FText::AsNumber(State->Badges.Num()));
		AddLabelValue(LOCTEXT("Dex", "Pokédex"), FText::AsNumber(State->PokedexCaught.Num()));
		AddLabelValue(LOCTEXT("PlayTime", "Temps de jeu"), FormatPlayTime(State->GetPlayTimeSeconds()));
	}
	if (const UPokemonSaveGame* Existing = UPokemonSaveLibrary::LoadSaveGame())
	{
		AddLabelValue(LOCTEXT("LastSave", "Dernière"), FText::FromString(Existing->SavedAt.ToString(TEXT("%d/%m/%Y %H:%M"))));
	}
	AddSpacer(10.f);
	AddLine(LOCTEXT("SaveQuestion", "Sauvegarder la partie ?"), 22, Text, true);
	AddRowText(AddRow(0), LOCTEXT("Yes", "Oui"), 22, Text, true, true);
	AddRowText(AddRow(1), LOCTEXT("No", "Non"), 22, Text, true, true);
}

void UPokedexScreenWidget::BuildOptions()
{
	TitleText->SetText(LOCTEXT("TitleOptions", "OPTIONS"));
	HintText->SetText(LOCTEXT("HintOptions", "Gauche/droite : changer   B : retour"));

	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

	const FText Speeds[] = { LOCTEXT("Slow", "Lente"), LOCTEXT("Normal", "Normale"), LOCTEXT("Fast", "Rapide") };
	const FText Qualities[] = { LOCTEXT("Low", "Basse"), LOCTEXT("Medium", "Moyenne"), LOCTEXT("High", "Haute"), LOCTEXT("Epic", "Épique"), LOCTEXT("Cinematic", "Cinéma") };
	const int32 Quality = Settings ? Settings->GetOverallScalabilityLevel() : -1;
	const EWindowMode::Type WindowMode = Settings ? Settings->GetFullscreenMode() : EWindowMode::Windowed;

	auto Percent = [](float Value) { return FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Value * 100.f))); };
	const FText Names[OptionCount] = {
		LOCTEXT("OptTextSpeed", "Vitesse du texte"),
		LOCTEXT("OptVolume", "Volume général"),
		LOCTEXT("OptMusic", "Musique"),
		LOCTEXT("OptEffects", "Bruitages"),
		LOCTEXT("OptSensitivity", "Sensibilité caméra"),
		LOCTEXT("OptInvert", "Inverser haut/bas"),
		LOCTEXT("OptShortcutBar", "Icônes de menu à l'écran"),
		LOCTEXT("OptQuality", "Qualité graphique"),
		LOCTEXT("OptWindow", "Affichage"),
		LOCTEXT("OptVSync", "Synchro verticale"),
		LOCTEXT("OptControls", "Commandes"),
		LOCTEXT("OptDelete", "Effacer la sauvegarde"),
	};
	const FText Values[OptionCount] = {
		Speeds[FMath::Clamp(Options->TextSpeed, 0, 2)],
		Percent(Options->MasterVolume),
		Percent(Options->MusicVolume),
		Percent(Options->EffectsVolume),
		FText::FromString(FString::Printf(TEXT("x %.1f"), Options->CameraSensitivity)),
		Options->bInvertCameraY ? LOCTEXT("Yes", "Oui") : LOCTEXT("No", "Non"),
		Options->bShowShortcutBar ? LOCTEXT("Yes", "Oui") : LOCTEXT("No", "Non"),
		Quality >= 0 && Quality <= 4 ? Qualities[Quality] : LOCTEXT("Custom", "Perso"),
		WindowMode == EWindowMode::Fullscreen ? LOCTEXT("Fullscreen", "Plein écran")
			: (WindowMode == EWindowMode::WindowedFullscreen ? LOCTEXT("Borderless", "Plein écran fenêtré") : LOCTEXT("Windowed", "Fenêtré")),
		Settings && Settings->IsVSyncEnabled() ? LOCTEXT("Yes", "Oui") : LOCTEXT("No", "Non"),
		FText::GetEmpty(),
		FText::GetEmpty(),
	};
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, Names[Index], 19, Index == OptDelete ? ErrorColor : Text, true);
		if (!Values[Index].IsEmpty())
		{
			AddRowText(Row, FText::Format(LOCTEXT("OptionValue", "<  {0}  >"), Values[Index]), 18, Accent, false, true);
		}
		else if (Index == OptControls)
		{
			AddRowText(Row, LOCTEXT("OptionOpen", "changer  >"), 18, Accent, false, true);
		}
	}
}

void UPokedexScreenWidget::BuildConfirmDelete()
{
	TitleText->SetText(LOCTEXT("TitleOptions", "OPTIONS"));
	HintText->SetText(LOCTEXT("HintActions", "A : valider   B : retour"));
	AddLine(LOCTEXT("DeleteQuestion", "Effacer la sauvegarde de ce PC ?\nLa partie en cours continue, mais le prochain lancement repartira de zéro."), 20, Text, true);
	AddSpacer(8.f);
	AddRowText(AddRow(0), LOCTEXT("DeleteNo", "Non"), 22, Text, true, true);
	AddRowText(AddRow(1), LOCTEXT("DeleteYes", "Oui, effacer"), 22, ErrorColor, true, true);
}

void UPokedexScreenWidget::BuildGiveItem()
{
	TitleText->SetText(LOCTEXT("TitleParty", "POKÉMON"));
	HintText->SetText(LOCTEXT("HintGive", "A : donner   B : retour"));

	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	if (!State || !State->Party.IsValidIndex(PartyIndex))
	{
		Page = EPokedexPage::Party;
		BuildParty();
		return;
	}
	AddLine(FText::Format(LOCTEXT("GiveWhat", "Que doit tenir {0} ?"), UPokemonLibrary::GetDisplayName(Database, State->Party[PartyIndex])), 22, Text, true);
	AddSpacer(6.f);

	const TArray<FName> Items = GetHoldableItems();
	if (Items.Num() == 0)
	{
		AddLine(LOCTEXT("NothingToHold", "Aucun objet du sac ne peut être tenu."), 20, Dim);
		return;
	}

	RowCount = Items.Num();
	Selection = FMath::Clamp(Selection, 0, RowCount - 1);
	const int32 Window = ListWindow - 2;
	if (Selection < ScrollOffset)
	{
		ScrollOffset = Selection;
	}
	else if (Selection >= ScrollOffset + Window)
	{
		ScrollOffset = Selection - Window + 1;
	}
	for (int32 Index = ScrollOffset; Index < FMath::Min(RowCount, ScrollOffset + Window); ++Index)
	{
		const FItemRow* Item = Database ? Database->FindItem(Items[Index]) : nullptr;
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, Item ? Item->DisplayName : FText::FromName(Items[Index]), 21, Text, true, true);
		AddRowText(Row, FText::FromString(FString::Printf(TEXT("x%d"), State->GetItemCount(Items[Index]))), 19, Text);
	}
	if (const FItemRow* Item = Database ? Database->FindItem(Items[Selection]) : nullptr)
	{
		AddSpacer(6.f);
		AddLine(Item->Description, 17, Dim);
	}
}

void UPokedexScreenWidget::BuildMap()
{
	TitleText->SetText(LOCTEXT("TitleMap", "CARTE DE KANTO"));
	HintText->SetText(LOCTEXT("HintMap", "Haut/bas : choisir une ville   B : retour"));

	const UPokedexModel* Model = GetModel();
	if (!Model || !Model->MapTexture)
	{
		AddLine(LOCTEXT("NoMap", "Carte introuvable : lancer Tools/Pokedex/import_pokedex_unreal.py dans Unreal."), 20, ErrorColor);
		return;
	}

	auto MakeDot = [this](const FLinearColor& Color, float Size, const FLinearColor& Outline)
	{
		UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Dot->SetBrush(FSlateRoundedBoxBrush(Color, Size * 0.5f, Outline, 2.f));
		return Dot;
	};

	UHorizontalBox* Layout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Content->AddChildToVerticalBox(Layout);

	// Carte à gauche, avec les villes et le joueur par-dessus
	USizeBox* MapBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MapBox->SetWidthOverride(MapSize);
	MapBox->SetHeightOverride(MapSize);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	MapBox->SetContent(Canvas);
	Layout->AddChildToHorizontalBox(MapBox)->SetVerticalAlignment(VAlign_Top);

	UImage* MapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	MapImage->SetBrushFromTexture(Model->MapTexture);
	UCanvasPanelSlot* ImageSlot = Canvas->AddChildToCanvas(MapImage);
	ImageSlot->SetPosition(FVector2D::ZeroVector);
	ImageSlot->SetSize(FVector2D(MapSize, MapSize));

	const TArray<int32> Towns = GetMapTowns();
	if (Towns.Num() > 0)
	{
		Selection = FMath::Clamp(Selection, 0, Towns.Num() - 1);
	}
	auto AddMarker = [Canvas](UWidget* Marker, const FVector2D& UV, float Size)
	{
		UCanvasPanelSlot* MarkerSlot = Canvas->AddChildToCanvas(Marker);
		MarkerSlot->SetSize(FVector2D(Size, Size));
		MarkerSlot->SetPosition(UV * MapSize - FVector2D(Size * 0.5f, Size * 0.5f));
	};
	for (int32 Index = 0; Index < Towns.Num(); ++Index)
	{
		const FPokedexMapPlace& Place = Model->MapPlaces[Towns[Index]];
		const bool bSelected = Index == Selection;
		AddMarker(MakeDot(bSelected ? Gold : Accent, bSelected ? 16.f : 9.f, Background), Place.UV, bSelected ? 16.f : 9.f);
	}
	if (Towns.IsValidIndex(Selection))
	{
		// Nom de la ville choisie à côté de son point
		const FPokedexMapPlace& Place = Model->MapPlaces[Towns[Selection]];
		UBorder* Label = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Label->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
		Label->SetPadding(FMargin(6.f, 1.f));
		Label->SetContent(MakeText(Place.Name, 15, Gold, true));
		UCanvasPanelSlot* LabelSlot = Canvas->AddChildToCanvas(Label);
		LabelSlot->SetAutoSize(true);
		const bool bLeft = Place.UV.X > 0.7f;
		LabelSlot->SetAlignment(FVector2D(bLeft ? 1.f : 0.f, 0.5f));
		LabelSlot->SetPosition(Place.UV * MapSize + FVector2D(bLeft ? -14.f : 14.f, 0.f));
	}

	FVector2D PlayerUV;
	const bool bHasPlayer = GetPlayerMapPosition(PlayerUV);
	if (bHasPlayer)
	{
		AddMarker(MakeDot(FLinearColor(0.95f, 0.12f, 0.1f), 18.f, FLinearColor::White), PlayerUV, 18.f);
	}

	// Colonne de droite : où on est, puis la liste des villes
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* ColumnSlot = Layout->AddChildToHorizontalBox(Column);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	RowTarget = Column;

	const int32 Here = bHasPlayer ? FindPlaceAt(PlayerUV) : -1;
	AddLine(LOCTEXT("YouAreHere", "TU ES ICI"), 16, Dim, true);
	AddLine(Here >= 0 ? Model->MapPlaces[Here].Name : (bHasPlayer ? LOCTEXT("Wild", "En pleine nature") : LOCTEXT("NoLandscape", "Hors de Kanto")),
		21, Text, true);
	AddSpacer(8.f);
	AddLine(LOCTEXT("TownsTitle", "VILLES"), 16, Dim, true);

	const int32 Window = ListWindow - 1;
	if (Selection < ScrollOffset)
	{
		ScrollOffset = Selection;
	}
	else if (Selection >= ScrollOffset + Window)
	{
		ScrollOffset = Selection - Window + 1;
	}
	for (int32 Index = ScrollOffset; Index < FMath::Min(Towns.Num(), ScrollOffset + Window); ++Index)
	{
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, Model->MapPlaces[Towns[Index]].Name, 18, Towns[Index] == Here ? Gold : Text, true, Index == Selection);
	}
	RowCount = Towns.Num();
}

void UPokedexScreenWidget::BuildOnline()
{
	TitleText->SetText(LOCTEXT("TitleOnline", "EN LIGNE"));
	HintText->SetText(LOCTEXT("HintOnline", "A : choisir   B : retour"));

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	const FText Mode = NetMode == NM_Client ? LOCTEXT("ModeClient", "Connecté à la partie d'un ami")
		: (NetMode == NM_ListenServer ? LOCTEXT("ModeHost", "Tu héberges la partie")
		: (NetMode == NM_DedicatedServer ? LOCTEXT("ModeServer", "Serveur") : LOCTEXT("ModeSolo", "Partie solo")));
	AddLabelValue(LOCTEXT("OnlineMode", "Mode"), Mode);
	AddSpacer(6.f);

	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const APlayerState* Mine = GetPlayerState();
	AddLine(FText::Format(LOCTEXT("PlayersTitle", "JOUEURS ({0})"), GameState ? GameState->PlayerArray.Num() : 1), 18, Accent, true);
	if (GameState)
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (!Player)
			{
				continue;
			}
			UHorizontalBox* Row = AddRow(-1, false);
			AddRowText(Row, FText::FromString(Player->GetPlayerName()), 19, Player == Mine ? Gold : Text, true, true);
			if (const APokemonPlayerState* Trainer = Cast<APokemonPlayerState>(Player))
			{
				AddRowText(Row, FText::Format(LOCTEXT("PlayerSummary", "{0} badge(s)   {1} capturés  "), Trainer->Badges.Num(), Trainer->PokedexCaught.Num()), 16, Dim);
			}
			if (Player == Mine)
			{
				AddRowText(Row, LOCTEXT("ItsYou", "(toi)"), 16, Gold);
			}
		}
	}
	AddSpacer(10.f);

	const FText Actions[] = { LOCTEXT("Trade", "Échanger des Pokémon"), LOCTEXT("Duel", "Combattre un joueur") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, Actions[Index], 20, Text, true, true);
		AddRowText(Row, LOCTEXT("Soon", "bientôt"), 16, Dim);
	}
}

void UPokedexScreenWidget::BuildControls()
{
	TitleText->SetText(LOCTEXT("TitleControls", "COMMANDES"));
	HintText->SetText(IsCapturingKey() ? LOCTEXT("HintCapture", "Appuie sur une touche   Échap : annuler")
		: LOCTEXT("HintControls", "A : changer la touche   B : retour"));

	const APokemonNewAgePlayerController* Player = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	const TArray<FPokemonBindableKey> Keys = Player ? Player->GetBindableKeys() : TArray<FPokemonBindableKey>();
	if (Keys.Num() == 0)
	{
		AddLine(LOCTEXT("NoKeys", "Aucune touche à changer (le personnage n'est pas encore là ?)"), 19, Dim);
		return;
	}

	// Une ligne par commande, puis « Touches d'origine » ; la liste défile
	RowCount = Keys.Num() + 1;
	Selection = FMath::Clamp(Selection, 0, RowCount - 1);
	const int32 Window = ListWindow + 3;
	if (Selection < ScrollOffset)
	{
		ScrollOffset = Selection;
	}
	else if (Selection >= ScrollOffset + Window)
	{
		ScrollOffset = Selection - Window + 1;
	}

	for (int32 Index = ScrollOffset; Index < FMath::Min(RowCount, ScrollOffset + Window); ++Index)
	{
		if (Index == Keys.Num())
		{
			AddRowText(AddRow(Index), LOCTEXT("ResetKeys", "Remettre les touches d'origine"), 18, ErrorColor, true, true);
			continue;
		}
		const FPokemonBindableKey& Entry = Keys[Index];
		if (Index == 0 || Entry.bGamepad != Keys[Index - 1].bGamepad)
		{
			AddLine(Entry.bGamepad ? LOCTEXT("Gamepad", "MANETTE") : LOCTEXT("Keyboard", "CLAVIER"), 15, Accent, true);
		}
		UHorizontalBox* Row = AddRow(Index);
		AddRowText(Row, Entry.Name, 18, Text, true, true);
		const bool bWaiting = CapturingKey == Entry.Id;
		const bool bChanged = Entry.Key != Entry.DefaultKey;
		AddRowText(Row, bWaiting ? LOCTEXT("Waiting", "...") : Entry.Key.GetDisplayName(), 18, bWaiting ? Gold : (bChanged ? Good : Accent), false, true);
	}
}

void UPokedexScreenWidget::CaptureKey(const FKey& Key)
{
	APokemonNewAgePlayerController* Player = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	const TArray<FPokemonBindableKey> Keys = Player ? Player->GetBindableKeys() : TArray<FPokemonBindableKey>();
	const FPokemonBindableKey* Entry = Keys.FindByPredicate([this](const FPokemonBindableKey& Candidate) { return Candidate.Id == CapturingKey; });

	if (!Entry || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Left)
	{
		CapturingKey = NAME_None;
		Feedback(EPokedexFeedback::Back);
		ShowStatus(LOCTEXT("CaptureCancelled", "Touche inchangée."));
		Refresh();
		return;
	}
	// Une touche du bon appareil, et pas un stick ni la souris
	const bool bRightDevice = Key.IsGamepadKey() == Entry->bGamepad;
	if (!bRightDevice || Key.IsAnalog() || Key.IsMouseButton() || Key.ToString().Contains(TEXT("Stick_")))
	{
		Feedback(EPokedexFeedback::Error);
		ShowStatus(Entry->bGamepad ? LOCTEXT("NeedPad", "Il faut un bouton de la manette (Échap : annuler).")
			: LOCTEXT("NeedKey", "Il faut une touche du clavier (Échap : annuler)."), true);
		StatusTimeLeft = 30.f;
		return;
	}

	CapturingKey = NAME_None;
	Player->SetKeyBinding(Entry->Id, Key);
	Feedback(EPokedexFeedback::Saved);
	ShowStatus(FText::Format(LOCTEXT("KeyChanged", "« {0} » : {1}"), Entry->Name, Key.GetDisplayName()));
	Refresh();
}

void UPokedexScreenWidget::BuildComingSoon()
{
	TitleText->SetText(GetMenuName(CurrentMenu));
	HintText->SetText(LOCTEXT("HintSoon", "B : retour"));
	AddSpacer(30.f);
	AddLine(GetMenuDescription(CurrentMenu), 22, Text, true);
	AddSpacer(10.f);
	AddLine(LOCTEXT("ComingSoon", "Ce menu n'est pas encore fait. Il arrivera dans une prochaine version."), 19, Dim);
}

// ---------------------------------------------------------------- Actions

void UPokedexScreenWidget::UseSelectedItemOn(int32 Index)
{
	APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	const FItemRow* Item = Database ? Database->FindItem(SelectedItem) : nullptr;
	APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	if (!State || !Item || !Controller || !State->Party.IsValidIndex(Index))
	{
		Feedback(EPokedexFeedback::Error);
		return;
	}

	const FPokemonInstance& Pokemon = State->Party[Index];
	if (!APokemonPlayerState::CanUseItemOnPokemon(*Item, Pokemon))
	{
		Feedback(EPokedexFeedback::Error);
		ShowStatus(LOCTEXT("NoEffect", "Ça n'aura aucun effet."), true);
		return;
	}

	const FText Name = UPokemonLibrary::GetDisplayName(Database, Pokemon);
	const FText Message = Pokemon.IsFainted() ? FText::Format(LOCTEXT("Revived", "{0} est ranimé !"), Name)
		: ((Item->HealHP > 0 || Item->bHealFull) && Pokemon.CurrentHP < Pokemon.GetMaxHP()
			? FText::Format(LOCTEXT("Healed", "{0} récupère des PV !"), Name)
			: FText::Format(LOCTEXT("Cured", "{0} va mieux !"), Name));
	Controller->ServerUseFieldItem(SelectedItem, Index);
	Feedback(EPokedexFeedback::Confirm);
	ShowStatus(Message);
	RefreshTimer = 0.1f;
}

void UPokedexScreenWidget::DoSave()
{
	if (UPokemonSaveLibrary::SaveGame(GetOwningPlayer()))
	{
		Feedback(EPokedexFeedback::Saved);
		ShowStatus(FText::Format(LOCTEXT("SavedOk", "{0} a sauvegardé la partie."),
			GetPlayerState() ? FText::FromString(GetPlayerState()->GetPlayerName()) : FText::GetEmpty()));
		Refresh();
	}
	else
	{
		Feedback(EPokedexFeedback::Error);
		ShowStatus(LOCTEXT("SaveFailed", "La sauvegarde a échoué."), true);
	}
}

void UPokedexScreenWidget::ChangeOption(int32 Option, int32 Delta)
{
	UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	bool bChanged = true;

	auto Step = [Delta](float Value) { return FMath::Clamp(FMath::RoundToFloat(Value * 10.f + Delta) / 10.f, 0.f, 1.f); };
	switch (Option)
	{
	case OptTextSpeed:     Options->TextSpeed = (Options->TextSpeed + Delta + 3) % 3; break;
	case OptMasterVolume:  Options->MasterVolume = Step(Options->MasterVolume); break;
	case OptMusicVolume:   Options->MusicVolume = Step(Options->MusicVolume); break;
	case OptEffectsVolume: Options->EffectsVolume = Step(Options->EffectsVolume); break;
	case OptSensitivity:   Options->CameraSensitivity = FMath::Clamp(FMath::RoundToFloat(Options->CameraSensitivity * 10.f + Delta) / 10.f, 0.5f, 2.f); break;
	case OptInvertY:       Options->bInvertCameraY = !Options->bInvertCameraY; break;
	case OptShortcutBar:   Options->bShowShortcutBar = !Options->bShowShortcutBar; break;
	case OptQuality:
		if (Settings)
		{
			Settings->SetOverallScalabilityLevel(FMath::Clamp(FMath::Max(Settings->GetOverallScalabilityLevel(), 0) + Delta, 0, 4));
		}
		break;
	case OptWindowMode:
		if (Settings)
		{
			const int32 Mode = (int32(Settings->GetFullscreenMode()) + Delta + 3) % 3;
			Settings->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(Mode));
		}
		break;
	case OptVSync:
		if (Settings)
		{
			Settings->SetVSyncEnabled(!Settings->IsVSyncEnabled());
		}
		break;
	default:
		bChanged = false;
		break;
	}

	if (!bChanged)
	{
		return;
	}
	if (Option <= OptShortcutBar)
	{
		UPokemonSaveLibrary::SaveOptions(Options);
		UPokemonSaveLibrary::ApplyOptions(this, Options);
		if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
		{
			Controller->ApplyGameOptions();
		}
	}
	else if (Settings)
	{
		Settings->ApplySettings(false);
	}
	Feedback(EPokedexFeedback::Navigate);
	Refresh();
}

void UPokedexScreenWidget::ShowStatus(const FText& Message, bool bError)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(bError ? ErrorColor : Gold));
		StatusTimeLeft = 3.5f;
	}
}

void UPokedexScreenWidget::Feedback(EPokedexFeedback Type)
{
	OnFeedback.Broadcast(Type);
}

// ---------------------------------------------------------------- Briques

UTextBlock* UPokedexScreenWidget::MakeText(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold)
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

UHorizontalBox* UPokedexScreenWidget::AddRow(int32 Index, bool bSelectable)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UBorder* Highlight = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Highlight->SetPadding(FMargin(8.f, 3.f));
	Highlight->SetContent(Row);

	if (!bSelectable || Index < 0)
	{
		Highlight->SetBrushColor(FLinearColor::Transparent);
		UVerticalBoxSlot* RowSlot = RowTarget->AddChildToVerticalBox(Highlight);
		RowSlot->SetPadding(FMargin(0.f, 1.f));
		return Row;
	}

	RowCount = FMath::Max(RowCount, Index + 1);
	Highlight->SetBrushColor(Index == Selection ? Selected : FLinearColor::Transparent);

	// Bouton invisible : clic et survol à la souris (à plat, ou via le Widget Interaction sur l'écran 3D)
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	Style.Normal = FSlateNoResource();
	Style.Hovered = FSlateNoResource();
	Style.Pressed = FSlateNoResource();
	Style.Disabled = FSlateNoResource();
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f);
	Button->SetStyle(Style);
	Button->SetContent(Highlight);

	UPokedexRowBinding* Binding = NewObject<UPokedexRowBinding>(this);
	Binding->Index = Index;
	Binding->Screen = this;
	Button->OnClicked.AddDynamic(Binding, &UPokedexRowBinding::HandleClicked);
	Button->OnHovered.AddDynamic(Binding, &UPokedexRowBinding::HandleHovered);
	RowBindings.Add(Binding);
	RowBackgrounds.Add(Highlight);

	UVerticalBoxSlot* RowSlot = RowTarget->AddChildToVerticalBox(Button);
	RowSlot->SetPadding(FMargin(0.f, 1.f));
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
	return Row;
}

void UPokedexScreenWidget::AddRowText(UHorizontalBox* Row, const FText& InText, int32 Size, const FLinearColor& Color, bool bFill, bool bBold)
{
	UTextBlock* Block = MakeText(InText, Size, Color, bBold);
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Block);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	if (bFill)
	{
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UPokedexScreenWidget::AddRowHPBar(UHorizontalBox* Row, int32 HP, int32 MaxHP, float Width)
{
	const float Ratio = MaxHP > 0 ? FMath::Clamp(float(HP) / float(MaxHP), 0.f, 1.f) : 0.f;
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	Bar->SetPercent(Ratio);
	Bar->SetFillColorAndOpacity(Ratio > 0.5f ? Good : (Ratio > 0.2f ? Gold : ErrorColor));
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Width);
	Box->SetHeightOverride(10.f);
	Box->SetContent(Bar);
	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(Box);
	BarSlot->SetVerticalAlignment(VAlign_Center);
}

void UPokedexScreenWidget::AddLine(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Block = MakeText(InText, Size, Color, bBold);
	Block->SetAutoWrapText(true);
	UVerticalBoxSlot* LineSlot = RowTarget->AddChildToVerticalBox(Block);
	LineSlot->SetPadding(FMargin(8.f, 2.f));
}

void UPokedexScreenWidget::AddLabelValue(const FText& Label, const FText& Value)
{
	UHorizontalBox* Row = AddRow(-1, false);
	UTextBlock* LabelText = MakeText(Label, 18, Dim, true);
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(190.f);
	LabelBox->SetContent(LabelText);
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
	AddRowText(Row, Value, 19, Text, true);
}

void UPokedexScreenWidget::AddSpacer(float Height)
{
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Spacer->SetSize(FVector2D(1.f, Height));
	RowTarget->AddChildToVerticalBox(Spacer);
}

void UPokedexScreenWidget::AddTypeBadges(UHorizontalBox* Row, EPokemonType Type1, EPokemonType Type2)
{
	for (const EPokemonType Type : { Type1, Type2 })
	{
		if (Type == EPokemonType::None)
		{
			continue;
		}
		UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Badge->SetBrushColor(TypeColor(Type));
		Badge->SetPadding(FMargin(8.f, 1.f));
		Badge->SetContent(MakeText(GetTypeName(Type).ToUpper(), 14, FLinearColor::White, true));
		UHorizontalBoxSlot* BadgeSlot = Row->AddChildToHorizontalBox(Badge);
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
		BadgeSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
}

// ---------------------------------------------------------------- Données

APokemonPlayerState* UPokedexScreenWidget::GetPlayerState() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller ? Controller->GetPlayerState<APokemonPlayerState>() : nullptr;
}

const UPokemonDatabase* UPokedexScreenWidget::GetDatabase() const
{
	return UPokemonDataSubsystem::GetDatabase(this);
}

FText UPokedexScreenWidget::GetTypeName(EPokemonType Type) const
{
	const UPokemonDatabase* Database = GetDatabase();
	const FName RowName(*StaticEnum<EPokemonType>()->GetNameStringByValue(int64(Type)));
	if (Database && Database->TypeChart)
	{
		if (const FTypeChartRow* Row = Database->TypeChart->FindRow<FTypeChartRow>(RowName, TEXT("Pokedex"), false))
		{
			return Row->DisplayName;
		}
	}
	return FText::FromName(RowName);
}

FText UPokedexScreenWidget::GetStatusShort(EMajorStatus Status) const
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

TArray<FName> UPokedexScreenWidget::GetBagPocketItems(int32 Pocket) const
{
	TArray<FName> Items;
	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	if (!State)
	{
		return Items;
	}
	for (const FItemStack& Stack : State->Inventory)
	{
		if (Stack.Count <= 0)
		{
			continue;
		}
		const FItemRow* Item = Database ? Database->FindItem(Stack.Item) : nullptr;
		int32 ItemPocket = 2;
		if (Item)
		{
			switch (Item->Category)
			{
			case EItemCategory::Medicine: ItemPocket = 0; break;
			case EItemCategory::PokeBall: ItemPocket = 1; break;
			case EItemCategory::KeyItem:  ItemPocket = 3; break;
			default:                      ItemPocket = 2; break;
			}
		}
		if (ItemPocket == Pocket)
		{
			Items.Add(Stack.Item);
		}
	}
	return Items;
}

void UPokedexScreenWidget::BuildSpeciesList()
{
	SpeciesList.Reset();
	const UPokemonDatabase* Database = GetDatabase();
	if (!Database || !Database->Species)
	{
		return;
	}
	TArray<TPair<int32, FName>> Sorted;
	for (const TPair<FName, uint8*>& Pair : Database->Species->GetRowMap())
	{
		const FSpeciesRow* Row = reinterpret_cast<const FSpeciesRow*>(Pair.Value);
		Sorted.Emplace(Row->NationalDex, Pair.Key);
	}
	Sorted.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B) { return A.Key < B.Key; });
	for (const TPair<int32, FName>& Pair : Sorted)
	{
		SpeciesList.Add(Pair.Value);
	}
}

void UPokedexScreenWidget::GetInfo(FText& OutTitle, FText& OutText) const
{
	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();

	switch (Page)
	{
	case EPokedexPage::Party:
	case EPokedexPage::PartyActions:
	case EPokedexPage::Summary:
	case EPokedexPage::BagTarget:
	{
		const int32 Index = (Page == EPokedexPage::Party || Page == EPokedexPage::BagTarget) ? Selection : PartyIndex;
		if (State && State->Party.IsValidIndex(Index))
		{
			const FPokemonInstance& Pokemon = State->Party[Index];
			const FItemRow* Held = (Database && !Pokemon.HeldItem.IsNone()) ? Database->FindItem(Pokemon.HeldItem) : nullptr;
			OutTitle = UPokemonLibrary::GetDisplayName(Database, Pokemon);
			OutText = FText::Format(LOCTEXT("InfoPokemon", "N.{0}   PV {1}/{2}\nObjet : {3}"), Pokemon.Level, Pokemon.CurrentHP, Pokemon.GetMaxHP(),
				Pokemon.HeldItem.IsNone() ? LOCTEXT("NoHeldItem", "aucun") : (Held ? Held->DisplayName : FText::FromName(Pokemon.HeldItem)));
			return;
		}
		break;
	}
	case EPokedexPage::Pokedex:
	case EPokedexPage::PokedexEntry:
	{
		const int32 Index = Page == EPokedexPage::Pokedex ? Selection : DexIndex;
		const FSpeciesRow* Species = (Database && SpeciesList.IsValidIndex(Index)) ? Database->FindSpecies(SpeciesList[Index]) : nullptr;
		if (State && Species)
		{
			if (!State->PokedexSeen.Contains(SpeciesList[Index]) && !State->PokedexCaught.Contains(SpeciesList[Index]))
			{
				OutTitle = FText::FromString(FString::Printf(TEXT("N°%03d  ???"), Species->NationalDex));
				OutText = LOCTEXT("InfoUnseen", "Pas encore rencontré.");
				return;
			}
			FString Types = GetTypeName(Species->Type1).ToString();
			if (Species->Type2 != EPokemonType::None)
			{
				Types += TEXT(" / ") + GetTypeName(Species->Type2).ToString();
			}
			OutTitle = FText::FromString(FString::Printf(TEXT("N°%03d  %s"), Species->NationalDex, *Species->DisplayName.ToString()));
			OutText = FText::Format(LOCTEXT("InfoSpecies", "{0}\n{1}"), Species->Genus, FText::FromString(Types));
			return;
		}
		break;
	}
	case EPokedexPage::Bag:
	case EPokedexPage::GiveItem:
	{
		const TArray<FName> Items = Page == EPokedexPage::Bag ? GetBagPocketItems(BagPocket) : GetHoldableItems();
		const FItemRow* Item = (Database && Items.IsValidIndex(Selection)) ? Database->FindItem(Items[Selection]) : nullptr;
		if (Item)
		{
			OutTitle = Item->DisplayName;
			OutText = Item->Description;
			return;
		}
		break;
	}
	case EPokedexPage::Map:
	{
		const UPokedexModel* Model = GetModel();
		const TArray<int32> Towns = GetMapTowns();
		if (Model && Towns.IsValidIndex(Selection))
		{
			FVector2D PlayerUV;
			const bool bHere = GetPlayerMapPosition(PlayerUV) && FindPlaceAt(PlayerUV) == Towns[Selection];
			OutTitle = Model->MapPlaces[Towns[Selection]].Name;
			OutText = bHere ? LOCTEXT("InfoHere", "Tu es ici.") : LOCTEXT("InfoTown", "Ville de Kanto.");
			return;
		}
		break;
	}
	default:
		break;
	}

	if (Page == EPokedexPage::Home)
	{
		OutTitle = LOCTEXT("TitleHome", "POKÉDEX");
		OutText = LOCTEXT("InfoHome", "Choisis un menu sur le volet.");
		return;
	}
	OutTitle = GetMenuName(CurrentMenu);
	OutText = GetMenuDescription(CurrentMenu);
}

FName UPokedexScreenWidget::GetFeaturedSpecies() const
{
	const APokemonPlayerState* State = GetPlayerState();
	if (!State)
	{
		return NAME_None;
	}
	auto IfSeen = [State](FName Species)
	{
		return State->PokedexSeen.Contains(Species) || State->PokedexCaught.Contains(Species) ? Species : NAME_None;
	};
	switch (Page)
	{
	case EPokedexPage::PokedexEntry:
		return SpeciesList.IsValidIndex(DexIndex) ? IfSeen(SpeciesList[DexIndex]) : NAME_None;
	case EPokedexPage::Pokedex:
		return SpeciesList.IsValidIndex(Selection) ? IfSeen(SpeciesList[Selection]) : NAME_None;
	case EPokedexPage::Party:
		return State->Party.IsValidIndex(Selection) ? State->Party[Selection].Species : NAME_None;
	case EPokedexPage::PartyActions:
	case EPokedexPage::Summary:
	case EPokedexPage::GiveItem:
		return State->Party.IsValidIndex(PartyIndex) ? State->Party[PartyIndex].Species : NAME_None;
	default:
		return NAME_None;
	}
}

const UPokedexModel* UPokedexScreenWidget::GetModel() const
{
	if (!bModelLoaded)
	{
		bModelLoaded = true;
		CachedModel = LoadObject<UPokedexModel>(nullptr, UPokedexModel::AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	return CachedModel;
}

TArray<FName> UPokedexScreenWidget::GetHoldableItems() const
{
	TArray<FName> Items;
	const APokemonPlayerState* State = GetPlayerState();
	const UPokemonDatabase* Database = GetDatabase();
	if (!State)
	{
		return Items;
	}
	for (const FItemStack& Stack : State->Inventory)
	{
		const FItemRow* Item = Database ? Database->FindItem(Stack.Item) : nullptr;
		if (Stack.Count > 0 && Item && Item->Category != EItemCategory::KeyItem)
		{
			Items.Add(Stack.Item);
		}
	}
	return Items;
}

TArray<FText> UPokedexScreenWidget::GetSpeciesLocations(FName Species) const
{
	TArray<FText> Locations;
	const UPokemonDatabase* Database = GetDatabase();
	if (!Database || !Database->Encounters)
	{
		return Locations;
	}
	TSet<FString> Seen;
	for (const TPair<FName, uint8*>& Pair : Database->Encounters->GetRowMap())
	{
		const FEncounterTableRow* Table = reinterpret_cast<const FEncounterTableRow*>(Pair.Value);
		const bool bLivesHere = Table->Slots.ContainsByPredicate([Species](const FEncounterSlot& EncounterSlot) { return EncounterSlot.Species == Species; });
		if (!bLivesHere)
		{
			continue;
		}
		const FText Name = Table->LocationDisplayName.IsEmpty() ? FText::FromName(Table->Location) : Table->LocationDisplayName;
		bool bAlreadyIn = false;
		Seen.Add(Name.ToString(), &bAlreadyIn);
		if (!bAlreadyIn)
		{
			Locations.Add(Name);
		}
	}
	return Locations;
}

bool UPokedexScreenWidget::GetPlayerMapPosition(FVector2D& OutUV) const
{
	const APlayerController* Controller = GetOwningPlayer();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return false;
	}
	const UPokedexModel* Model = GetModel();
	const float Size = float(FMath::Max(2, Model ? Model->MapHeightmapSize : 4033) - 1);
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		// Repère du Landscape : un point de heightmap par unité, X = colonne = nord, Y = ligne = est
		const FVector Local = It->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
		OutUV = FVector2D(Local.Y / Size, 1.f - Local.X / Size);
		return OutUV.X >= 0.f && OutUV.X <= 1.f && OutUV.Y >= 0.f && OutUV.Y <= 1.f;
	}
	return false;
}

int32 UPokedexScreenWidget::FindPlaceAt(const FVector2D& UV) const
{
	const UPokedexModel* Model = GetModel();
	if (!Model)
	{
		return -1;
	}
	int32 Best = -1;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Model->MapPlaces.Num(); ++Index)
	{
		const FPokedexMapPlace& Place = Model->MapPlaces[Index];
		const float Distance = float(FVector2D::Distance(UV, Place.UV));
		if (Distance > FMath::Max(Place.Radius, 0.002f))
		{
			continue;
		}
		// Les villes passent avant les routes qui y arrivent
		const float Score = Distance / FMath::Max(Place.Radius, 0.002f) + (Place.Kind == EPokedexPlaceKind::Town ? 0.f : 10.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Index;
		}
	}
	return Best;
}

TArray<int32> UPokedexScreenWidget::GetMapTowns() const
{
	TArray<int32> Towns;
	if (const UPokedexModel* Model = GetModel())
	{
		for (int32 Index = 0; Index < Model->MapPlaces.Num(); ++Index)
		{
			if (Model->MapPlaces[Index].Kind == EPokedexPlaceKind::Town)
			{
				Towns.Add(Index);
			}
		}
	}
	return Towns;
}

#undef LOCTEXT_NAMESPACE
