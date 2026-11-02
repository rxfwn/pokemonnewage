#include "UI/SoftShadowTextBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> USoftShadowTextBlock::RebuildWidget()
{
	// Le texte principal garde l'ombre nette de UTextBlock à zéro (transparente par défaut) : seules les copies font l'ombre
	TSharedRef<SWidget> Main = Super::RebuildWidget();
	ShadowOverlay = SNew(SOverlay);

	// 3 x 3 copies espacées de Blur / 2 autour du décalage, poids d'une cloche (sigma = Blur / 2) ;
	// les opacités sont réglées pour que leur empilement donne SoftShadowOpacity au cœur
	const float Step = SoftShadowBlur * 0.5f;
	const float Sigma = FMath::Max(SoftShadowBlur * 0.5f, 0.01f);
	float Weights[9];
	float Total = 0.f;
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float X = float(Index % 3 - 1) * Step;
		const float Y = float(Index / 3 - 1) * Step;
		Weights[Index] = FMath::Exp(-(X * X + Y * Y) / (2.f * Sigma * Sigma));
		Total += Weights[Index];
	}
	const float CoreOpacity = -FMath::Loge(1.f - FMath::Clamp(SoftShadowOpacity, 0.f, 0.95f));
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const FVector2D Offset = SoftShadowOffset + FVector2D(float(Index % 3 - 1), float(Index / 3 - 1)) * Step;
		ShadowOverlay->AddSlot()
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(this, &USoftShadowTextBlock::GetShadowText)))
			.Font(TAttribute<FSlateFontInfo>::Create(TAttribute<FSlateFontInfo>::FGetter::CreateUObject(this, &USoftShadowTextBlock::GetShadowFont)))
			.Justification(TAttribute<ETextJustify::Type>::Create(TAttribute<ETextJustify::Type>::FGetter::CreateUObject(this, &USoftShadowTextBlock::GetShadowJustification)))
			.AutoWrapText(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(this, &USoftShadowTextBlock::GetShadowAutoWrap)))
			.WrapTextAt(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(this, &USoftShadowTextBlock::GetShadowWrapAt)))
			.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, CoreOpacity * Weights[Index] / Total))
			.RenderTransform(FSlateRenderTransform(FVector2f(Offset)))
			.Visibility(EVisibility::HitTestInvisible)
		];
	}
	ShadowOverlay->AddSlot()
	[
		Main
	];
	return ShadowOverlay.ToSharedRef();
}

void USoftShadowTextBlock::OnJustificationChanged(ETextJustify::Type InJustification)
{
	ShadowJustification = InJustification;
	Super::OnJustificationChanged(InJustification);
}

void USoftShadowTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ShadowOverlay.Reset();
}
