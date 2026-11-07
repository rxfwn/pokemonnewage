#include "Character/TrainerAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"

void FTrainerAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UTrainerAnimInstance* Trainer = Cast<UTrainerAnimInstance>(InAnimInstance))
	{
		Offsets = Trainer->GetBoneOffsets();
		if (Idle != Trainer->GetIdleAnimation())
		{
			Idle = Trainer->GetIdleAnimation();
			IdleTime = 0.f;
		}
	}
}

void FTrainerAnimInstanceProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	if (Idle)
	{
		const float Length = Idle->GetPlayLength();
		IdleTime = Length > 0.f ? FMath::Fmod(IdleTime + DeltaSeconds, Length) : 0.f;
	}
}

bool FTrainerAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// 1) La pose normale : le graphe de l'Animation Blueprint enfant, sinon l'animation d'attente, sinon la pose de référence
	if (FAnimNode_Base* Root = GetRootNode())
	{
		EvaluateAnimationNode_WithRoot(Output, Root);
	}
	else if (Idle)
	{
		FAnimationPoseData PoseData(Output);
		Idle->GetAnimationPose(PoseData, FAnimExtractContext(double(IdleTime), false, FDeltaTimeRecord(), true));
	}
	else
	{
		Output.ResetToRefPose();
	}

	// 2) Les curseurs : on ajoute les déplacements dans l'espace local de chaque os
	if (Offsets.IsEmpty())
	{
		return true;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	for (const FTrainerBoneOffset& Offset : Offsets)
	{
		const int32 MeshIndex = RefSkeleton.FindBoneIndex(Offset.Bone);
		if (MeshIndex == INDEX_NONE)
		{
			continue; // os absent de ce mesh (autre squelette) : ignoré
		}
		const FCompactPoseBoneIndex PoseIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (!PoseIndex.IsValid())
		{
			continue; // os coupé par le LOD courant
		}
		FTransform& Transform = Output.Pose[PoseIndex];
		Transform.AddToTranslation(Offset.Location);
		Transform.SetRotation((Offset.Rotation * Transform.GetRotation()).GetNormalized());
		Transform.SetScale3D(Transform.GetScale3D() + Offset.Scale);
	}
	return true;
}
