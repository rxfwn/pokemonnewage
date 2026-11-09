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

const FTrainerPartDef* UTrainerAppearanceData::FindPart(FName Id) const
{
	return Id.IsNone() ? nullptr : Parts.FindByPredicate([Id](const FTrainerPartDef& P) { return P.Id == Id; });
}

const FTrainerPartSlotDef* UTrainerAppearanceData::FindSlot(FName Slot) const
{
	return PartSlots.FindByPredicate([Slot](const FTrainerPartSlotDef& S) { return S.Slot == Slot; });
}

TMap<FName, const FTrainerPartDef*> UTrainerAppearanceData::ResolveParts(const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance) const
{
	TMap<FName, const FTrainerPartDef*> Result;
	TSet<FName> Emptied; // « -Top » = le joueur a choisi « Aucun » pour le haut

	auto Use = [&](FName Id, bool bPlayerChoice)
	{
		const FString Text = Id.ToString();
		if (bPlayerChoice && Text.StartsWith(TEXT("-")))
		{
			const FName Slot(*Text.Mid(1));
			Emptied.Add(Slot);
			Result.Remove(Slot);
			return;
		}
		const FTrainerPartDef* Part = FindPart(Id);
		if (Part && Part->FitsBase(Base.Id) && (bPlayerChoice || !Emptied.Contains(Part->Slot)))
		{
			Result.Add(Part->Slot, Part);
		}
	};
	for (const FName Id : Appearance.Outfit)
	{
		Use(Id, true);
	}
	for (const FName Id : Base.DefaultParts)
	{
		const FTrainerPartDef* Part = FindPart(Id);
		if (Part && !Result.Contains(Part->Slot))
		{
			Use(Id, false);
		}
	}
	// Une robe ou une combinaison retire le haut et le bas (FTrainerPartSlotDef::Replaces)
	TArray<FName> ToRemove;
	for (const TPair<FName, const FTrainerPartDef*>& Pair : Result)
	{
		if (const FTrainerPartSlotDef* Slot = FindSlot(Pair.Key))
		{
			ToRemove.Append(Slot->Replaces);
		}
	}
	for (const FName Slot : ToRemove)
	{
		Result.Remove(Slot);
	}
	return Result;
}

TArray<FTrainerBoneOffset> UTrainerAppearanceData::ComputeBoneOffsets(const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance) const
{
	const float Muscle = Appearance.GetSlider(MuscleSliderId);
	const float Mass = Appearance.GetSlider(MassSliderId);

	TArray<FTrainerBoneOffset> Offsets;
	TMap<FName, int32> IndexByBone;
	for (const FTrainerSliderDef& Def : GetSliders(Base))
	{
		if (!Def.HasBones())
		{
			continue;
		}
		// Comme inZOI (Morphologie_Corps_Os : delta_gizmo, delta_muscle, delta_masse) : le curseur du groupe compte pour
		// GizmoCoef (0,4 le plus souvent), Muscles et Corpulence pour leurs coefficients. Sans GizmoCoef, une silhouette
		// à +1 appliquait le groupe en entier : fessiers x2,5 sur « Musclé ».
		const float Own = FMath::Clamp(Appearance.GetSlider(Def.Id), -1.f, 1.f) * Def.GizmoCoef;
		const float Global = GlobalSliderShare * (FMath::Clamp(Muscle, 0.f, 1.f) * Def.MuscleCoef + FMath::Clamp(Mass, -1.f, 1.f) * Def.MassCoef);
		const float Value = FMath::Clamp(Own + Global, -1.f, 1.f);
		if (FMath::IsNearlyZero(Value))
		{
			continue;
		}
		const float Weight = FMath::Abs(Value) * BoneDeformationScale;
		for (const FTrainerBoneOffset& Bone : Value > 0.f ? Def.BonesPositive : Def.BonesNegative)
		{
			int32& Index = IndexByBone.FindOrAdd(Bone.Bone, INDEX_NONE);
			if (Index == INDEX_NONE)
			{
				Index = Offsets.Num();
				FTrainerBoneOffset& New = Offsets.AddDefaulted_GetRef();
				New.Bone = Bone.Bone;
			}
			FTrainerBoneOffset& Sum = Offsets[Index];
			const FVector Location = bSwapBoneYZ ? FVector(Bone.Location.X, Bone.Location.Z, Bone.Location.Y) : Bone.Location;
			const FVector Scale = bSwapBoneYZ ? FVector(Bone.Scale.X, Bone.Scale.Z, Bone.Scale.Y) : Bone.Scale;
			Sum.Location += Location * Weight;
			Sum.Scale += Scale * Weight;
			Sum.Rotation = FQuat::Slerp(FQuat::Identity, Bone.Rotation.GetNormalized(), FMath::Min(Weight, 1.f)) * Sum.Rotation;
		}
	}
	for (FTrainerBoneOffset& Offset : Offsets)
	{
		Offset.Scale = Offset.Scale.BoundToCube(MaxBoneScaleOffset);
	}
	return Offsets;
}

FText FTrainerPartSlotDef::GetZoneLabel(int32 Zone) const
{
	if (ZoneLabels.IsValidIndex(Zone) && !ZoneLabels[Zone].IsEmpty())
	{
		return ZoneLabels[Zone];
	}
	return Zone == 0 ? NSLOCTEXT("TrainerAppearance", "ZoneMain", "Principale") : FText::Format(NSLOCTEXT("TrainerAppearance", "ZoneN", "Zone {0}"), Zone);
}
