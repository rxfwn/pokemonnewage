#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Character/TrainerAppearanceData.h"
#include "TrainerAnimInstance.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/** Côté « fil d'animation » de UTrainerAnimInstance : calcule la pose normale, puis ajoute les déplacements d'os */
USTRUCT()
struct FTrainerAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FTrainerAnimInstanceProxy() = default;
	explicit FTrainerAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	/** Copie faite sur le fil principal à chaque image (PreUpdate) */
	TArray<FTrainerBoneOffset> Offsets;

	/** Pose copiée du corps (mode « suit le corps » des têtes) : index d'os de ce mesh -> transformation locale */
	TArray<TPair<int32, FTransform>> CopiedBones;
	TSet<FName> CopiedNames;

	/** Correspondance os de ce mesh -> os du corps, refaite quand l'un des deux meshes change */
	TArray<int32> SourceMap;
	TWeakObjectPtr<const USkeletalMesh> MappedOwnMesh;
	TWeakObjectPtr<const USkeletalMesh> MappedSourceMesh;

	/** Os de ce mesh sans enfant (os d'aide : fessiers, pectoraux...), pour HelperScaleLimit */
	TArray<bool> LeafBones;

	/** Copie de UTrainerAnimInstance::HelperScaleLimit */
	float HelperScaleLimit = 0.f;

	/** Copie de UTrainerAnimInstance::IdleHeadMotion */
	float IdleHeadMotion = 1.f;

	/** Animation jouée en boucle quand il n'y a pas de graphe (studio de l'éditeur) */
	UAnimSequenceBase* Idle = nullptr; // gardé en vie par UTrainerAnimInstance::IdleAnimation
	float IdleTime = 0.f;
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

	/** Animation d'attente jouée en boucle s'il n'y a pas d'Animation Blueprint enfant (studio) ; nul = pose de référence */
	void SetIdleAnimation(UAnimSequenceBase* InIdle) { IdleAnimation = InIdle; }

	UAnimSequenceBase* GetIdleAnimation() const { return IdleAnimation; }

	/**
	 *  Mode « suit le corps » (têtes inZOI) : les os communs prennent la pose du corps, les autres (os du visage,
	 *  absents du corps) gardent leur pose de référence et reçoivent les curseurs du visage. Remplace la Leader Pose,
	 *  qui ne laisse pas bouger les os propres à la tête.
	 */
	void SetPoseSource(USkeletalMeshComponent* InSource) { PoseSource = InSource; }

	USkeletalMeshComponent* GetPoseSource() const { return PoseSource.Get(); }

	/**
	 *  Vêtements en mode « suit le corps » : écart d'échelle maximal des os d'aide copiés (os sans enfant, 0 = aucune
	 *  limite). Les silhouettes d'inZOI grossissent ces os jusqu'à x2,5 (fessiers de « Musclé ») : la peau, collée à
	 *  l'os, suit bien, mais le bas d'un t-shirt, loin de l'os, était projeté loin derrière (dos qui gonfle).
	 */
	float HelperScaleLimit = 0.f;

	/** Part du mouvement du cou et de la tête gardée dans l'attente (1 = tout ; le studio la réduit : visage stable) */
	float IdleHeadMotion = 1.f;

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FTrainerAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FTrainerAnimInstanceProxy*>(InProxy); }

private:

	TArray<FTrainerBoneOffset> BoneOffsets;

	TWeakObjectPtr<USkeletalMeshComponent> PoseSource;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> IdleAnimation;
};
