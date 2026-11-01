#include "UI/GlassPanel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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

		UImage* BuildImage(UWidgetTree* Tree, const FGlassLook& Look, FName Name)
		{
			UImage* Glass = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
			Glass->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UMaterialInterface* Material = FindMaterial())
			{
				UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Glass);
				Instance->SetScalarParameterValue(TEXT("CornerRadius"), Look.CornerRadius);
				Instance->SetScalarParameterValue(TEXT("DpiScale"), GetDpiScale(Tree));
				const FGlassLook Default;
				auto SetIfChanged = [Instance](const TCHAR* Parameter, float Value, float DefaultValue)
				{
					if (!FMath::IsNearlyEqual(Value, DefaultValue))
					{
						Instance->SetScalarParameterValue(Parameter, Value);
					}
				};
				SetIfChanged(TEXT("GradientAngle"), Look.GradientAngle, Default.GradientAngle);
				SetIfChanged(TEXT("GradientStart"), Look.GradientStart, Default.GradientStart);
				SetIfChanged(TEXT("GradientEnd"), Look.GradientEnd, Default.GradientEnd);
				SetIfChanged(TEXT("FillOpacityStart"), Look.FillOpacityStart, Default.FillOpacityStart);
				SetIfChanged(TEXT("FillOpacityEnd"), Look.FillOpacityEnd, Default.FillOpacityEnd);
				SetIfChanged(TEXT("StrokeOpacityStart"), Look.StrokeOpacityStart, Default.StrokeOpacityStart);
				SetIfChanged(TEXT("StrokeOpacityEnd"), Look.StrokeOpacityEnd, Default.StrokeOpacityEnd);
				SetIfChanged(TEXT("LayerOpacity"), Look.LayerOpacity, Default.LayerOpacity);
				SetIfChanged(TEXT("LightIntensity"), Look.LightIntensity, Default.LightIntensity);
				Glass->SetBrushFromMaterial(Instance);
			}
			else
			{
				// Matériau pas encore fait (Tools/UI/make_glass_material.py) : verre simplifié
				const float Fill = 0.5f * (Look.FillOpacityStart + Look.FillOpacityEnd) * Look.LayerOpacity;
				Glass->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, Fill), Look.CornerRadius,
					FLinearColor(1.f, 1.f, 1.f, Look.StrokeOpacityEnd + 0.1f), Look.StrokeOpacityEnd > 0.f ? 1.f : 0.f));
			}
			return Glass;
		}
	}

	UBackgroundBlur* Make(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding, const FGlassLook& Look, FName Name)
	{
		UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), Name);
		Blur->SetBlurStrength(Look.Blur);
		Blur->SetCornerRadius(FVector4(Look.CornerRadius, Look.CornerRadius, Look.CornerRadius, Look.CornerRadius));
		Blur->SetPadding(FMargin(0.f));
		Blur->SetHorizontalAlignment(HAlign_Fill);
		Blur->SetVerticalAlignment(VAlign_Fill);

		UOverlay* Layers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Blur->SetContent(Layers);

		UOverlaySlot* GlassSlot = Layers->AddChildToOverlay(BuildImage(Tree, Look, NAME_None));
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

	UImage* MakeImage(UWidgetTree* Tree, const FGlassLook& Look, FName Name)
	{
		return BuildImage(Tree, Look, Name);
	}

	void AddTextShadow(UTextBlock* Text)
	{
		if (Text)
		{
			Text->SetShadowOffset(FVector2D(3.f, 4.f));
			Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.25f));
		}
	}
}
