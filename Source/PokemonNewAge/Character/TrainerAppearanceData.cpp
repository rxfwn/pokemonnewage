#include "Character/TrainerAppearanceData.h"

UTrainerAppearanceData::UTrainerAppearanceData()
{
	// Teintes multipliées à la peau de Character Creator : du très clair au très foncé (à régler à l'œil)
	SkinPalette = {
		FLinearColor(1.00f, 1.00f, 1.00f), FLinearColor(1.00f, 0.93f, 0.88f), FLinearColor(0.98f, 0.86f, 0.76f),
		FLinearColor(0.93f, 0.78f, 0.64f), FLinearColor(0.86f, 0.68f, 0.52f), FLinearColor(0.76f, 0.57f, 0.42f),
		FLinearColor(0.64f, 0.46f, 0.33f), FLinearColor(0.52f, 0.36f, 0.25f), FLinearColor(0.42f, 0.28f, 0.19f),
		FLinearColor(0.33f, 0.21f, 0.14f), FLinearColor(0.25f, 0.16f, 0.11f), FLinearColor(0.18f, 0.12f, 0.08f) };
	EyePalette = {
		FLinearColor(0.25f, 0.15f, 0.08f), FLinearColor(0.12f, 0.07f, 0.04f), FLinearColor(0.45f, 0.30f, 0.12f),
		FLinearColor(0.40f, 0.38f, 0.15f), FLinearColor(0.20f, 0.40f, 0.20f), FLinearColor(0.15f, 0.35f, 0.40f),
		FLinearColor(0.20f, 0.35f, 0.65f), FLinearColor(0.45f, 0.55f, 0.70f), FLinearColor(0.45f, 0.45f, 0.48f),
		FLinearColor(0.55f, 0.20f, 0.20f), FLinearColor(0.45f, 0.25f, 0.55f), FLinearColor(0.85f, 0.65f, 0.15f) };
}

const FTrainerBaseDef* UTrainerAppearanceData::FindBase(FName Id) const
{
	if (Bases.IsEmpty())
	{
		return nullptr;
	}
	const FTrainerBaseDef* Found = Bases.FindByPredicate([Id](const FTrainerBaseDef& B) { return B.Id == Id; });
	return Found ? Found : &Bases[0];
}

const FTrainerSliderDef* UTrainerAppearanceData::FindSlider(const FTrainerBaseDef& Base, FName Slider) const
{
	auto Match = [Slider](const FTrainerSliderDef& S) { return S.Id == Slider; };
	if (const FTrainerSliderDef* Found = Base.Sliders.FindByPredicate(Match))
	{
		return Found;
	}
	return CommonSliders.FindByPredicate(Match);
}

TArray<FTrainerSliderDef> UTrainerAppearanceData::GetSliders(const FTrainerBaseDef& Base) const
{
	TArray<FTrainerSliderDef> All = CommonSliders;
	All.Append(Base.Sliders);
	return All;
}
