#include "Character/TrainerAppearanceRender.h"
#include "Character/TrainerAppearanceData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const TCHAR* DefaultAppearanceDataPath = TEXT("/Game/Characters/Player/DA_TrainerAppearance.DA_TrainerAppearance");

	void ApplyMorphs(const UTrainerAppearanceData* Data, const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, USkeletalMeshComponent* Mesh)
	{
		for (const FTrainerSliderDef& Def : Data->GetSliders(Base))
		{
			const float Value = Appearance.GetSlider(Def.Id);
			// Un morph absent du mesh est ignoré par Unreal : pas d'erreur si un vêtement n'a pas ce morph
			if (!Def.MorphPositive.IsNone())
			{
				Mesh->SetMorphTarget(Def.MorphPositive, FMath::Max(0.f, Value));
			}
			if (!Def.MorphNegative.IsNone())
			{
				Mesh->SetMorphTarget(Def.MorphNegative, FMath::Max(0.f, -Value));
			}
		}
	}

	void ApplyColor(USkeletalMeshComponent* Mesh, const TArray<FName>& Slots, FName Parameter, const FLinearColor& Color)
	{
		if (Parameter.IsNone())
		{
			return;
		}
		for (const FName Slot : Slots)
		{
			const int32 Index = Mesh->GetMaterialIndex(Slot);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Index));
			if (!MID)
			{
				MID = Mesh->CreateDynamicMaterialInstance(Index);
			}
			if (MID)
			{
				MID->SetVectorParameterValue(Parameter, Color);
			}
		}
	}
}

UTrainerAppearanceData* TrainerAppearanceRender::LoadDefaultData()
{
	return LoadObject<UTrainerAppearanceData>(nullptr, DefaultAppearanceDataPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FTrainerAppearance TrainerAppearanceRender::Sanitize(const UTrainerAppearanceData* Data, const FTrainerAppearance& Appearance)
{
	FTrainerAppearance Clean = Appearance;
	const FTrainerBaseDef* Base = Data ? Data->FindBase(Appearance.Base) : nullptr;
	if (!Base)
	{
		return Clean; // pas de catalogue : on garde tel quel (rien ne s'affichera de toute façon)
	}
	Clean.Base = Base->Id;
	Clean.Sliders.Reset();
	for (const FTrainerSliderValue& Value : Appearance.Sliders)
	{
		if (const FTrainerSliderDef* Def = Data->FindSlider(*Base, Value.Slider))
		{
			const float Min = Def->MorphNegative.IsNone() ? 0.f : -Def->Limit;
			Clean.SetSlider(Value.Slider, FMath::Clamp(Value.Value, Min, Def->Limit));
		}
	}
	auto ClampColor = [](const FLinearColor& C) { return FLinearColor(FMath::Clamp(C.R, 0.f, 1.f), FMath::Clamp(C.G, 0.f, 1.f), FMath::Clamp(C.B, 0.f, 1.f), 1.f); };
	Clean.SkinColor = ClampColor(Appearance.SkinColor);
	Clean.EyeColor = ClampColor(Appearance.EyeColor);
	// Vêtements : il faudra vérifier que le joueur les possède (boutiques), pas encore de catalogue
	return Clean;
}

bool TrainerAppearanceRender::Apply(const UTrainerAppearanceData* Data, AActor* Owner, USkeletalMeshComponent* Mesh, const FTrainerAppearance& InAppearance, FName& InOutAppliedBase)
{
	const FTrainerBaseDef* Base = Data ? Data->FindBase(InAppearance.Base) : nullptr;
	if (!Mesh || !Base)
	{
		return false;
	}
	const FTrainerAppearance Appearance = Sanitize(Data, InAppearance);

	if (InOutAppliedBase != Base->Id)
	{
		USkeletalMesh* NewMesh = Base->Mesh.LoadSynchronous();
		if (!NewMesh)
		{
			return false;
		}
		Mesh->SetSkeletalMesh(NewMesh);
		if (Base->AnimClass)
		{
			Mesh->SetAnimInstanceClass(Base->AnimClass);
		}
		Mesh->EmptyOverrideMaterials(); // les MID de l'ancienne base ne vont pas sur la nouvelle
		InOutAppliedBase = Base->Id;
	}

	// Le corps et tout ce qui le suit (vêtements, cheveux en Leader Pose) reçoivent les mêmes morphs et couleurs
	TArray<USkeletalMeshComponent*> Targets = { Mesh };
	if (Owner)
	{
		TArray<USkeletalMeshComponent*> Children;
		Owner->GetComponents<USkeletalMeshComponent>(Children);
		for (USkeletalMeshComponent* Child : Children)
		{
			if (Child != Mesh && Child->LeaderPoseComponent.Get() == Mesh)
			{
				Targets.Add(Child);
			}
		}
	}
	for (USkeletalMeshComponent* Target : Targets)
	{
		ApplyMorphs(Data, *Base, Appearance, Target);
		ApplyColor(Target, Data->SkinMaterialSlots, Data->SkinColorParameter, Appearance.SkinColor);
		ApplyColor(Target, Data->EyeMaterialSlots, Data->EyeColorParameter, Appearance.EyeColor);
	}
	return true;
}
