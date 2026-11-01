#include "UI/GlassPanel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace PokemonGlass
{
	namespace
	{
		const TCHAR* MaterialPath = TEXT("/Game/UI/Materials/M_UI_Glass.M_UI_Glass");
		// Instance réglée à l'œil par L'amie, prise en priorité si elle existe
		const TCHAR* InstancePath = TEXT("/Game/UI/Materials/MI_UI_Glass.MI_UI_Glass");

		UMaterialInterface* FindMaterial()
		{
			if (UMaterialInterface* Instance = LoadObject<UMaterialInterface>(nullptr, InstancePath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Instance;
			}
			return LoadObject<UMaterialInterface>(nullptr, MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		/** Échelle d'affichage (DPI) : le matériau compte en pixels de la fenêtre, les maquettes en unités UMG */
		float GetDpiScale(UWidgetTree* Tree)
		{
			const float Scale = Tree ? UWidgetLayoutLibrary::GetViewportScale(Tree->GetOuter()) : 0.f;
			return Scale > 0.01f ? Scale : 1.f;
		}
	}

	UBackgroundBlur* Make(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding, float CornerRadius, FName Name)
	{
		UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), Name);
		Blur->SetBlurStrength(DefaultBlur);
		Blur->SetCornerRadius(FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius));
		Blur->SetPadding(FMargin(0.f));
		Blur->SetHorizontalAlignment(HAlign_Fill);
		Blur->SetVerticalAlignment(VAlign_Fill);

		UOverlay* Layers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Blur->SetContent(Layers);

		UImage* Glass = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		Glass->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UMaterialInterface* Material = FindMaterial())
		{
			UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Glass);
			Instance->SetScalarParameterValue(TEXT("CornerRadius"), CornerRadius);
			Instance->SetScalarParameterValue(TEXT("DpiScale"), GetDpiScale(Tree));
			Glass->SetBrushFromMaterial(Instance);
		}
		else
		{
			// Matériau pas encore fait (Tools/UI/make_glass_material.py) : verre simplifié
			Glass->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.16f), CornerRadius,
				FLinearColor(1.f, 1.f, 1.f, 0.3f), 1.f));
		}
		UOverlaySlot* GlassSlot = Layers->AddChildToOverlay(Glass);
		GlassSlot->SetHorizontalAlignment(HAlign_Fill);
		GlassSlot->SetVerticalAlignment(VAlign_Fill);

		if (Content)
		{
			UOverlaySlot* ContentSlot = Layers->AddChildToOverlay(Content);
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
			ContentSlot->SetPadding(Padding);
		}
		return Blur;
	}

	void AddTextShadow(UTextBlock* Text)
	{
		if (Text)
		{
			Text->SetShadowOffset(FVector2D(0.f, 2.f));
			Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		}
	}
}
