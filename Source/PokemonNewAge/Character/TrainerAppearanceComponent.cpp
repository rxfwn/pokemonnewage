#include "Character/TrainerAppearanceComponent.h"
#include "Character/TrainerAppearanceData.h"
#include "Core/PokemonPlayerState.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* DefaultAppearanceDataPath = TEXT("/Game/Characters/Player/DA_TrainerAppearance.DA_TrainerAppearance");
}

UTrainerAppearanceComponent::UTrainerAppearanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UTrainerAppearanceData* UTrainerAppearanceComponent::GetAppearanceData()
{
	if (!AppearanceData)
	{
		AppearanceData = LoadObject<UTrainerAppearanceData>(nullptr, DefaultAppearanceDataPath, nullptr, LOAD_NoWarn);
	}
	return AppearanceData;
}

void UTrainerAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	TryApplyFromPlayerState();
}

void UTrainerAppearanceComponent::TryApplyFromPlayerState()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr)
	{
		ApplyAppearance(State->Appearance);
		return;
	}
	// PNJ ou PlayerState pas encore arrivé : 10 essais sur 5 s (le PlayerState rappelle de toute façon à chaque changement)
	if (++RetryCount <= 10 && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(RetryTimer, this, &UTrainerAppearanceComponent::TryApplyFromPlayerState, 0.5f, false);
	}
}

FTrainerAppearance UTrainerAppearanceComponent::GetCurrentAppearance() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr;
	return State ? State->Appearance : FTrainerAppearance();
}

FTrainerAppearance UTrainerAppearanceComponent::Sanitize(const FTrainerAppearance& Appearance)
{
	FTrainerAppearance Clean = Appearance;
	const UTrainerAppearanceData* Data = GetAppearanceData();
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

void UTrainerAppearanceComponent::RequestAppearance(const FTrainerAppearance& Appearance)
{
	ApplyAppearance(Appearance); // aperçu immédiat chez soi
	ServerSetAppearance(Appearance); // en solo ou sur l'hôte, l'appel est direct
}

void UTrainerAppearanceComponent::ServerSetAppearance_Implementation(const FTrainerAppearance& Appearance)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr)
	{
		State->SetAppearance(Sanitize(Appearance));
	}
}

void UTrainerAppearanceComponent::ApplyAppearance(const FTrainerAppearance& InAppearance)
{
	const FTrainerAppearance Appearance = Sanitize(InAppearance);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const UTrainerAppearanceData* Data = GetAppearanceData();
	const FTrainerBaseDef* Base = Data ? Data->FindBase(Appearance.Base) : nullptr;
	if (!Mesh || !Base)
	{
		return; // pas encore de catalogue : on laisse le Dresseur provisoire
	}

	if (AppliedBase != Base->Id)
	{
		if (USkeletalMesh* NewMesh = Base->Mesh.LoadSynchronous())
		{
			Mesh->SetSkeletalMesh(NewMesh);
			if (Base->AnimClass)
			{
				Mesh->SetAnimInstanceClass(Base->AnimClass);
			}
			Mesh->EmptyOverrideMaterials(); // les MID de l'ancienne base ne vont pas sur la nouvelle
			AppliedBase = Base->Id;
		}
	}

	// Le corps et tout ce qui le suit (vêtements, cheveux en Leader Pose) reçoivent les mêmes morphs et couleurs
	TArray<USkeletalMeshComponent*> Targets = { Mesh };
	TArray<USkeletalMeshComponent*> Children;
	Character->GetComponents<USkeletalMeshComponent>(Children);
	for (USkeletalMeshComponent* Child : Children)
	{
		if (Child != Mesh && Child->LeaderPoseComponent.Get() == Mesh)
		{
			Targets.Add(Child);
		}
	}
	for (USkeletalMeshComponent* Target : Targets)
	{
		ApplyMorphs(*Base, Appearance, Target);
		ApplyColor(Target, Data->SkinMaterialSlots, Data->SkinColorParameter, Appearance.SkinColor);
		ApplyColor(Target, Data->EyeMaterialSlots, Data->EyeColorParameter, Appearance.EyeColor);
	}
}

void UTrainerAppearanceComponent::ApplyMorphs(const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, USkeletalMeshComponent* Mesh)
{
	const UTrainerAppearanceData* Data = GetAppearanceData();
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

void UTrainerAppearanceComponent::ApplyColor(USkeletalMeshComponent* Mesh, const TArray<FName>& Slots, FName Parameter, const FLinearColor& Color)
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
