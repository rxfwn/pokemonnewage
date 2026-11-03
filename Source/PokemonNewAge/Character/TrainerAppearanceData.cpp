#include "Character/TrainerAppearanceData.h"

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
