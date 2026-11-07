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

namespace
{
	/** La taille de la tête doit agrandir tout le visage : ses enfants héritent de son échelle */
	const FName HeadBoneName(TEXT("head"));
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
	// Rapport d'échelle ajouté à chaque os : ses enfants ne doivent pas en hériter (comme inZOI) ;
	// sinon grossir le cou grossirait aussi la tête, et les deux os du cou s'additionneraient
	TMap<int32, FVector> ScaleRatio;
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
		if (!Offset.Scale.IsNearlyZero())
		{
			const FVector OldScale = Transform.GetScale3D();
			const FVector NewScale = (OldScale + Offset.Scale).ComponentMax(FVector(0.05));
			Transform.SetScale3D(NewScale);
			if (Offset.Bone != HeadBoneName)
			{
				ScaleRatio.Add(PoseIndex.GetInt(), NewScale / OldScale.ComponentMax(FVector(0.001)));
			}
		}
	}
	if (!ScaleRatio.IsEmpty())
	{
		for (int32 Raw = 1; Raw < Output.Pose.GetNumBones(); ++Raw)
		{
			const FCompactPoseBoneIndex Index(Raw);
			if (const FVector* Ratio = ScaleRatio.Find(Bones.GetParentBoneIndex(Index).GetInt()))
			{
				FTransform& Child = Output.Pose[Index];
				Child.SetScale3D(Child.GetScale3D() / *Ratio);
			}
		}
	}
	return true;
}
