#include "Character/TrainerAppearanceRender.h"
#include "Character/TrainerAppearanceData.h"
#include "Character/TrainerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* DefaultAppearanceDataPath = TEXT("/Game/Characters/Player/DA_TrainerAppearance.DA_TrainerAppearance");

	/** Catalogue créé par Tools/InZOI/import_inzoi.py dans le dossier local (hors Git) : prioritaire s'il existe */
	const TCHAR* ImportedAppearanceDataPath = TEXT("/Game/ImportedAssets/InZOI/DA_TrainerAppearance_InZOI.DA_TrainerAppearance_InZOI");

	/** Étiquette des Skeletal Mesh de pièces créés par Apply (un par emplacement) */
	const FName PartTag(TEXT("TrainerPart"));

	/** Met à jour les pièces (tête, vêtements...) : un Skeletal Mesh par emplacement, en Leader Pose sur le corps */
	void ApplyParts(const UTrainerAppearanceData* Data, const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, AActor* Owner, USkeletalMeshComponent* Mesh)
	{
		if (!Owner || (Data->Parts.IsEmpty() && Data->PartSlots.IsEmpty()))
		{
			return;
		}
		const TMap<FName, const FTrainerPartDef*> Wanted = Data->ResolveParts(Base, Appearance);

		TMap<FName, USkeletalMeshComponent*> Existing;
		TArray<USkeletalMeshComponent*> Components;
		Owner->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Component->ComponentHasTag(PartTag) && Component->ComponentTags.Num() > 1)
			{
				Existing.Add(Component->ComponentTags[1], Component);
			}
		}

		// Emplacements vidés : on retire le mesh
		for (const TPair<FName, USkeletalMeshComponent*>& Pair : Existing)
		{
			if (!Wanted.Contains(Pair.Key))
			{
				Pair.Value->SetSkeletalMesh(nullptr);
				Pair.Value->SetVisibility(false);
			}
		}

		for (const TPair<FName, const FTrainerPartDef*>& Pair : Wanted)
		{
			USkeletalMesh* PartMesh = Pair.Value->Mesh.LoadSynchronous();
			if (!PartMesh)
			{
				continue;
			}
			USkeletalMeshComponent* Component = Existing.FindRef(Pair.Key);
			if (!Component)
			{
				const FName Name = MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), *FString::Printf(TEXT("Part_%s"), *Pair.Key.ToString()));
				Component = NewObject<USkeletalMeshComponent>(Owner, Name, RF_Transient);
				Component->ComponentTags = { PartTag, Pair.Key };
				Component->SetupAttachment(Mesh);
				Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Component->bUseBoundsFromLeaderPoseComponent = true;
				Component->SetCastShadow(Mesh->CastShadow);
				Component->RegisterComponent();
				Owner->AddInstanceComponent(Component);
			}
			if (Component->GetSkeletalMeshAsset() != PartMesh)
			{
				Component->SetSkeletalMesh(PartMesh);
				Component->EmptyOverrideMaterials();
			}
			for (int32 Index = 0; Index < Pair.Value->Materials.Num(); ++Index)
			{
				if (UMaterialInterface* Material = Pair.Value->Materials[Index].LoadSynchronous())
				{
					Component->SetMaterial(Index, Material);
				}
			}
			Component->SetVisibility(true);
			Component->SetLeaderPoseComponent(Mesh);
		}
	}

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
	if (UTrainerAppearanceData* Imported = LoadObject<UTrainerAppearanceData>(nullptr, ImportedAppearanceDataPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		return Imported;
	}
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
			const float Min = Def->HasNegative() ? -Def->Limit : 0.f;
			Clean.SetSlider(Value.Slider, FMath::Clamp(Value.Value, Min, Def->Limit));
		}
	}
	auto ClampColor = [](const FLinearColor& C) { return FLinearColor(FMath::Clamp(C.R, 0.f, 1.f), FMath::Clamp(C.G, 0.f, 1.f), FMath::Clamp(C.B, 0.f, 1.f), 1.f); };
	Clean.SkinColor = ClampColor(Appearance.SkinColor);
	Clean.EyeColor = ClampColor(Appearance.EyeColor);
	// Pièces : on garde celles du catalogue qui vont à cette base, une par emplacement (la dernière gagne), et les « -Emplacement » (Aucun)
	// Plus tard : vérifier que le joueur possède le vêtement (boutiques)
	Clean.Outfit.Reset();
	TMap<FName, FName> BySlot;
	for (const FName Id : Appearance.Outfit)
	{
		const FString Text = Id.ToString();
		if (Text.StartsWith(TEXT("-")))
		{
			const FName Slot(*Text.Mid(1));
			if (const FTrainerPartSlotDef* SlotDef = Data->FindSlot(Slot); SlotDef && !SlotDef->bRequired)
			{
				BySlot.Add(Slot, Id);
			}
		}
		else if (const FTrainerPartDef* Part = Data->FindPart(Id); Part && Part->FitsBase(Base->Id))
		{
			BySlot.Add(Part->Slot, Id);
		}
	}
	BySlot.GenerateValueArray(Clean.Outfit);
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
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->SetAnimInstanceClass(Base->AnimClass);
		}
		else if (!Mesh->GetAnimInstance())
		{
			// Aperçu sans animation (studio) : pose de référence + curseurs par os
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->SetAnimInstanceClass(UTrainerAnimInstance::StaticClass());
		}
		Mesh->EmptyOverrideMaterials(); // les MID de l'ancienne base ne vont pas sur la nouvelle
		InOutAppliedBase = Base->Id;
	}

	// Pièces (tête, vêtements) avant les couleurs : la peau de la tête doit aussi être teintée
	ApplyParts(Data, *Base, Appearance, Owner, Mesh);

	// Curseurs « par os » : seulement si l'animation sait les appliquer (TrainerAnimInstance ou un enfant)
	if (UTrainerAnimInstance* Anim = Cast<UTrainerAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->SetBoneOffsets(Data->ComputeBoneOffsets(*Base, Appearance));
		Anim->SetIdleAnimation(Base->IdleAnimation.LoadSynchronous());
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
