#include "UI/Pokedex/PokedexWidgets.h"
#include "UI/Pokedex/PokedexDevice.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

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

// ---------------------------------------------------------------- Étiquette d'un bouton

TSharedRef<SWidget> UPokedexLabelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(Width);
		Root->SetHeightOverride(Height);
		WidgetTree->RootWidget = Root;

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FLinearColor::Transparent);
		Frame->SetPadding(FMargin(24.f, 40.f, 24.f, 20.f));
		Frame->SetHorizontalAlignment(HAlign_Right);
		Frame->SetVerticalAlignment(VAlign_Center);
		Root->SetContent(Frame);

		LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = 52;
		Font.TypefaceFontName = TEXT("Bold");
		LabelText->SetFont(Font);
		LabelText->SetJustification(ETextJustify::Right);
		LabelText->SetShadowOffset(FVector2D(2.f, 2.f));
		LabelText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Frame->SetContent(LabelText);
		SetLabel(PendingText, BaseColor);
	}
	return Super::RebuildWidget();
}

void UPokedexLabelWidget::SetLabel(const FText& Text, const FLinearColor& Color)
{
	PendingText = Text;
	BaseColor = Color;
	if (LabelText)
	{
		LabelText->SetText(Text);
		// Les noms longs (Sauvegarder, nom du joueur) rétrécissent pour tenir sur le bouton
		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = Text.ToString().Len() > 9 ? 36 : 52;
		LabelText->SetFont(Font);
		SetHighlighted(false);
	}
}

void UPokedexLabelWidget::SetHighlighted(bool bHighlighted)
{
	if (LabelText)
	{
		LabelText->SetColorAndOpacity(FSlateColor(bHighlighted ? FLinearColor::LerpUsingHSV(BaseColor, FLinearColor::White, 0.6f) : BaseColor));
	}
}
