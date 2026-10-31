#include "UI/Pokedex/PokedexWidgets.h"
#include "UI/GameFonts.h"
#include "UI/Pokedex/PokedexDevice.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateRoundedBoxBrush.h"

// ---------------------------------------------------------------- Calque d'entrée

TSharedRef<SWidget> UPokedexInputWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Bordure transparente mais « visible » : elle attrape la souris sur tout l'écran
		UBorder* Catcher = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Catcher"));
		Catcher->SetBrushColor(FLinearColor::Transparent);
		Catcher->SetVisibility(ESlateVisibility::Visible);
		WidgetTree->RootWidget = Catcher;
	}
	SetIsFocusable(true);
	return Super::RebuildWidget();
}

FVector2D UPokedexInputWidget::ToViewportPixels(const FPointerEvent& InMouseEvent)
{
	FVector2D PixelPosition, ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, InMouseEvent.GetScreenSpacePosition(), PixelPosition, ViewportPosition);
	return PixelPosition;
}

FReply UPokedexInputWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (APokedexDevice* Target = Device.Get())
	{
		Target->HandlePointerDown(InMouseEvent.GetEffectingButton(), ToViewportPixels(InMouseEvent));
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPokedexInputWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (APokedexDevice* Target = Device.Get())
	{
		Target->HandlePointerUp(InMouseEvent.GetEffectingButton(), ToViewportPixels(InMouseEvent));
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UPokedexInputWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (APokedexDevice* Target = Device.Get())
	{
		const FVector2D Delta = InMouseEvent.GetCursorDelta();
		Target->HandlePointerMove(ToViewportPixels(InMouseEvent), Delta);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPokedexInputWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (APokedexDevice* Target = Device.Get())
	{
		Target->HandleWheel(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

FReply UPokedexInputWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (APokedexDevice* Target = Device.Get(); Target && Target->HandleKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UPokedexInputWidget::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (APokedexDevice* Target = Device.Get())
	{
		Target->HandleAnalog(InAnalogEvent.GetKey(), InAnalogEvent.GetAnalogValue());
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

// ---------------------------------------------------------------- Écrans du volet

namespace PokedexPanels
{
	const FLinearColor PanelBackground(0.012f, 0.03f, 0.042f, 1.f);
	const FLinearColor PanelText(0.86f, 0.95f, 0.98f, 1.f);
	const FLinearColor PanelAccent(0.35f, 0.85f, 1.f, 1.f);
	const FLinearColor PanelDim(0.45f, 0.58f, 0.64f, 1.f);

	UTextBlock* MakePanelText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color, bool bBold)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetFont(PokemonFonts::Get(Size, bBold));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}
}

TSharedRef<SWidget> UPokedexIconWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(PixelSize.X);
		Root->SetHeightOverride(PixelSize.Y);
		WidgetTree->RootWidget = Root;

		Halo = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Halo"));
		Halo->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, PixelSize.Y * 0.2f));
		Halo->SetBrushColor(FLinearColor::Transparent);
		Halo->SetHorizontalAlignment(HAlign_Center);
		Halo->SetVerticalAlignment(VAlign_Center);
		Root->SetContent(Halo);

		const float Side = FMath::Min(PixelSize.X, PixelSize.Y) * 0.92f;
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconBox"));
		IconBox->SetWidthOverride(Side);
		IconBox->SetHeightOverride(Side);
		Halo->SetContent(IconBox);

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
		IconBox->SetContent(IconImage);
		IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SetIcon(PendingIcon);
	}
	return Super::RebuildWidget();
}

void UPokedexIconWidget::SetIcon(UTexture2D* Icon)
{
	PendingIcon = Icon;
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(Icon, false);
		IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UPokedexIconWidget::SetHighlighted(bool bHighlighted, bool bFocused)
{
	if (Halo)
	{
		Halo->SetBrushColor(bHighlighted ? FLinearColor(1.f, 1.f, 1.f, bFocused ? 0.45f : 0.25f) : FLinearColor::Transparent);
	}
	if (IconImage)
	{
		IconImage->SetRenderScale(FVector2D(bHighlighted ? 1.1f : 1.f));
	}
}

TSharedRef<SWidget> UPokedexInfoWidget::RebuildWidget()
{
	using namespace PokedexPanels;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(PixelSize.X);
		Root->SetHeightOverride(PixelSize.Y);
		WidgetTree->RootWidget = Root;

		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Back"));
		Back->SetBrushColor(PanelBackground);
		Back->SetPadding(FMargin(26.f, 18.f));
		Root->SetContent(Back);

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
		Back->SetContent(Lines);
		TitleText = MakePanelText(WidgetTree, 34, PanelAccent, true);
		Lines->AddChildToVerticalBox(TitleText);
		BodyText = MakePanelText(WidgetTree, 24, PanelText, false);
		BodyText->SetAutoWrapText(true);
		UVerticalBoxSlot* BodySlot = Lines->AddChildToVerticalBox(BodyText);
		BodySlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		SetInfo(PendingTitle, PendingText);
	}
	return Super::RebuildWidget();
}

void UPokedexInfoWidget::SetInfo(const FText& Title, const FText& Text)
{
	PendingTitle = Title;
	PendingText = Text;
	if (TitleText && BodyText)
	{
		TitleText->SetText(Title);
		BodyText->SetText(Text);
	}
}

TSharedRef<SWidget> UPokedexMiniWidget::RebuildWidget()
{
	using namespace PokedexPanels;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(PixelSize.X);
		Root->SetHeightOverride(PixelSize.Y);
		WidgetTree->RootWidget = Root;

		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Back"));
		Back->SetBrushColor(PanelBackground);
		Back->SetPadding(FMargin(18.f, 8.f));
		Back->SetHorizontalAlignment(HAlign_Center);
		Back->SetVerticalAlignment(VAlign_Center);
		Root->SetContent(Back);

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
		Back->SetContent(Lines);
		ValueText = MakePanelText(WidgetTree, FMath::RoundToInt(PixelSize.Y * 0.32f), PanelText, true);
		ValueText->SetJustification(ETextJustify::Center);
		Lines->AddChildToVerticalBox(ValueText)->SetHorizontalAlignment(HAlign_Center);
		LabelText = MakePanelText(WidgetTree, FMath::RoundToInt(PixelSize.Y * 0.15f), PanelDim, true);
		LabelText->SetJustification(ETextJustify::Center);
		Lines->AddChildToVerticalBox(LabelText)->SetHorizontalAlignment(HAlign_Center);
		SetValues(PendingValue, PendingLabel);
	}
	return Super::RebuildWidget();
}

void UPokedexMiniWidget::SetValues(const FText& Value, const FText& Label)
{
	PendingValue = Value;
	PendingLabel = Label;
	if (ValueText && LabelText)
	{
		ValueText->SetText(Value);
		LabelText->SetText(Label);
	}
}
