#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Character/TrainerAppearanceData.h"
#include "TrainerAnimInstance.generated.h"

/** Côté « fil d'animation » de UTrainerAnimInstance : calcule la pose normale, puis ajoute les déplacements d'os */
USTRUCT()
struct FTrainerAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FTrainerAnimInstanceProxy() = default;
	explicit FTrainerAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	/** Copie faite sur le fil principal à chaque image (PreUpdate) */
	TArray<FTrainerBoneOffset> Offsets;
};

/**
 *  Animation du dresseur avec les curseurs « par os » (déformation à la inZOI) : la pose de l'Animation Blueprint
 *  (ou la pose de référence s'il n'y a pas de graphe) reçoit les déplacements calculés par
 *  UTrainerAppearanceData::ComputeBoneOffsets. Les vêtements en Leader Pose suivent automatiquement.
 *  Pour garder ses animations : ouvrir l'Animation Blueprint retargeté > Paramètres de classe > Classe parente = TrainerAnimInstance.
 */
UCLASS(Blueprintable, Transient)
class UTrainerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** Remplace tous les déplacements d'os (appelé par TrainerAppearanceRender::Apply) */
	void SetBoneOffsets(const TArray<FTrainerBoneOffset>& InOffsets) { BoneOffsets = InOffsets; }

	const TArray<FTrainerBoneOffset>& GetBoneOffsets() const { return BoneOffsets; }

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FTrainerAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FTrainerAnimInstanceProxy*>(InProxy); }

private:

	TArray<FTrainerBoneOffset> BoneOffsets;
};
