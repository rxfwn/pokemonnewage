#include "UI/ShortcutBarWidget.h"
#include "Battle/BattleComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/PokemonSaveGame.h"
#include "PokemonNewAgePlayerController.h"
#include "UI/GameMenuWidget.h"
#include "UI/Pokedex/PokedexScreenWidget.h"

#define LOCTEXT_NAMESPACE "ShortcutBar"

namespace ShortcutBar
{
	constexpr float IconSize = 44.f;
	constexpr float RefreshInterval = 0.25f;
}

using namespace ShortcutBar;

TSharedRef<SWidget> UShortcutBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		// Affichage seulement : ne prend jamais les clics de la souris
		SetVisibility(ESlateVisibility::HitTestInvisible);

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		Panel->SetPadding(FMargin(8.f, 6.f));
		UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
		PanelSlot->SetHorizontalAlignment(HAlign_Right);
		PanelSlot->SetVerticalAlignment(VAlign_Bottom);
		PanelSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 24.f));

		UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Bar"));
		Panel->SetContent(Bar);

		for (const EPokedexMenu Menu : UGameMenuWidget::GetTabs())
		{
			// Icône, et la touche dans le coin en bas à droite
			UOverlay* Item = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(IconSize);
			Box->SetHeightOverride(IconSize);
			Item->AddChildToOverlay(Box);
			if (UTexture2D* Icon = UGameMenuWidget::GetMenuIcon(Menu))
			{
				UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Image->SetBrushFromTexture(Icon, false);
				Box->SetContent(Image);
			}
			else
			{
				UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				Name->SetText(UPokedexScreenWidget::GetMenuName(Menu));
				FSlateFontInfo Font = Name->GetFont();
				Font.Size = 9;
				Name->SetFont(Font);
				Name->SetAutoWrapText(true);
				Box->SetContent(Name);
			}

			UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			KeyText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.35f)));
			KeyText->SetShadowOffset(FVector2D(1.f, 1.f));
			KeyText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
			FSlateFontInfo KeyFont = KeyText->GetFont();
			KeyFont.Size = 11;
			KeyText->SetFont(KeyFont);
			UOverlaySlot* KeySlot = Item->AddChildToOverlay(KeyText);
			KeySlot->SetHorizontalAlignment(HAlign_Right);
			KeySlot->SetVerticalAlignment(VAlign_Bottom);
			KeyTexts.Add(KeyText);

			UHorizontalBoxSlot* ItemSlot = Bar->AddChildToHorizontalBox(Item);
			ItemSlot->SetPadding(FMargin(3.f, 0.f));
		}
	}

	return Super::RebuildWidget();
}

void UShortcutBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshTimer -= InDeltaTime;
	if (RefreshTimer <= 0.f)
	{
		RefreshTimer = RefreshInterval;
		Refresh();
	}
}

void UShortcutBarWidget::Refresh()
{
	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	// Menu, boutique, PC... montrent la souris : la barre se retire
	const bool bVisible = Controller && (!Options || Options->bShowShortcutBar) && !Controller->IsGameMenuOpen()
		&& !Controller->bShowMouseCursor && !(Controller->BattleComponent && Controller->BattleComponent->IsInBattle());
	if (Panel)
	{
		Panel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bVisible)
	{
		return;
	}

	const TArray<EPokedexMenu> Menus = UGameMenuWidget::GetTabs();
	for (int32 Index = 0; Index < KeyTexts.Num() && Index < Menus.Num(); ++Index)
	{
		const FKey Key = Controller->GetShortcutKey(Menus[Index]);
		KeyTexts[Index]->SetText(Key.IsValid() ? Key.GetDisplayName(false) : FText::GetEmpty());
	}
}

#undef LOCTEXT_NAMESPACE
