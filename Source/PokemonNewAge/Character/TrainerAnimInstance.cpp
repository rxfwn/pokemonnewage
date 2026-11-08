#include "Character/TrainerAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	/** Durée du fondu entre deux animations du studio (secondes) */
	constexpr float IdleBlendTime = 0.35f;
}

void FTrainerAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UTrainerAnimInstance* Trainer = Cast<UTrainerAnimInstance>(InAnimInstance))
	{
		Offsets = Trainer->GetBoneOffsets();
		HelperScaleLimit = Trainer->HelperScaleLimit;
		IdleHeadMotion = Trainer->IdleHeadMotion;
		if (Idle != Trainer->GetIdleAnimation())
		{
			// Fondu depuis l'animation d'avant (attente -> pose -> attente) au lieu d'un saut de pose
			PreviousIdle = Idle;
			PreviousTime = IdleTime;
			BlendRemaining = PreviousIdle ? IdleBlendTime : 0.f;
			Idle = Trainer->GetIdleAnimation();
			IdleTime = 0.f;
		}

		// Mode « suit le corps » : copie (fil principal) de la pose locale du corps pour les os qu'on a en commun
		CopiedBones.Reset();
		CopiedNames.Reset();
		USkeletalMeshComponent* Source = Trainer->GetPoseSource();
		const USkeletalMeshComponent* Own = Trainer->GetSkelMeshComponent();
		const USkeletalMesh* SourceMesh = Source ? Cast<USkeletalMesh>(Source->GetSkeletalMeshAsset()) : nullptr;
		const USkeletalMesh* OwnMesh = Own ? Cast<USkeletalMesh>(Own->GetSkeletalMeshAsset()) : nullptr;
		if (SourceMesh && OwnMesh)
		{
			if (MappedOwnMesh.Get() != OwnMesh || MappedSourceMesh.Get() != SourceMesh)
			{
				const FReferenceSkeleton& OwnRef = OwnMesh->GetRefSkeleton();
				const FReferenceSkeleton& SourceRef = SourceMesh->GetRefSkeleton();
				SourceMap.Init(INDEX_NONE, OwnRef.GetNum());
				for (int32 Index = 0; Index < OwnRef.GetNum(); ++Index)
				{
					SourceMap[Index] = SourceRef.FindBoneIndex(OwnRef.GetBoneName(Index));
				}
				LeafBones.Init(true, OwnRef.GetNum());
				for (int32 Index = 0; Index < OwnRef.GetNum(); ++Index)
				{
					const int32 Parent = OwnRef.GetParentIndex(Index);
					if (Parent != INDEX_NONE)
					{
						LeafBones[Parent] = false;
					}
				}
				MappedOwnMesh = OwnMesh;
				MappedSourceMesh = SourceMesh;
			}
			const TArray<FTransform> SourcePose = Source->GetBoneSpaceTransforms();
			const FReferenceSkeleton& OwnRef = OwnMesh->GetRefSkeleton();
			for (int32 Index = 0; Index < SourceMap.Num(); ++Index)
			{
				if (SourceMap[Index] != INDEX_NONE && SourcePose.IsValidIndex(SourceMap[Index]))
				{
					CopiedBones.Emplace(Index, SourcePose[SourceMap[Index]]);
					CopiedNames.Add(OwnRef.GetBoneName(Index));
				}
			}
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
	if (PreviousIdle && BlendRemaining > 0.f)
	{
		const float Length = PreviousIdle->GetPlayLength();
		PreviousTime = Length > 0.f ? FMath::Fmod(PreviousTime + DeltaSeconds, Length) : 0.f;
		BlendRemaining = FMath::Max(0.f, BlendRemaining - DeltaSeconds);
	}
}

namespace
{
	/** La taille de la tête doit agrandir tout le visage : ses enfants héritent de son échelle */
	const FName HeadBoneName(TEXT("head"));
	const FName PelvisBoneName(TEXT("pelvis"));
	/** Os du cou et de la tête adoucis dans l'attente du studio (IdleHeadMotion) */
	const FName HeadMotionBones[] = { FName(TEXT("neck_01")), FName(TEXT("neck_02")), FName(TEXT("head")) };
	/** Colonne : les attentes « Crowd » penchent tout le haut du corps ; on en garde un peu plus que pour la tête */
	const FName SpineMotionBones[] = { FName(TEXT("spine_02")), FName(TEXT("spine_03")), FName(TEXT("spine_04")), FName(TEXT("spine_05")) };
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
		if (PreviousIdle && BlendRemaining > 0.f)
		{
			FCompactPose PreviousPose;
			PreviousPose.SetBoneContainer(&Output.Pose.GetBoneContainer());
			FBlendedCurve PreviousCurve;
			UE::Anim::FStackAttributeContainer PreviousAttributes;
			FAnimationPoseData PreviousData(PreviousPose, PreviousCurve, PreviousAttributes);
			PreviousIdle->GetAnimationPose(PreviousData, FAnimExtractContext(double(PreviousTime), false, FDeltaTimeRecord(), true));
			const float PreviousWeight = BlendRemaining / IdleBlendTime;
			for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
			{
				FTransform Blended;
				Blended.Blend(Output.Pose[BoneIndex], PreviousPose[BoneIndex], PreviousWeight);
				Output.Pose[BoneIndex] = Blended;
			}
		}
		// Sur place : la racine reste à sa pose de référence (certaines attentes d'inZOI la déplacent)
		const FCompactPoseBoneIndex RootIndex(0);
		if (Output.Pose.IsValidIndex(RootIndex))
		{
			Output.Pose[RootIndex] = Output.Pose.GetRefPose(RootIndex);
		}
		// Le bassin aussi : les attentes « Crowd » d'inZOI le décalent de plusieurs dizaines de cm (le personnage sortait
		// de l'écran). On garde sa hauteur et son balancement, mais il revient au-dessus de ses pieds (seul le déplacement vertical est gardé).
		const int32 PelvisMesh = Output.Pose.GetBoneContainer().GetPoseBoneIndexForBoneName(PelvisBoneName);
		if (PelvisMesh != INDEX_NONE)
		{
			const FCompactPoseBoneIndex PelvisIndex = Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(PelvisMesh));
			if (PelvisIndex.IsValid())
			{
				// Verticale exprimée dans le repère du parent (la racine des glb d'inZOI peut être tournée)
				const FCompactPoseBoneIndex Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(PelvisIndex);
				const FQuat ParentRotation = Parent.IsValid() ? Output.Pose[Parent].GetRotation() : FQuat::Identity;
				const FVector Up = ParentRotation.UnrotateVector(FVector::UpVector);
				const FVector Ref = Output.Pose.GetRefPose(PelvisIndex).GetTranslation();
				const FVector Current = Output.Pose[PelvisIndex].GetTranslation();
				Output.Pose[PelvisIndex].SetTranslation(Ref + Up * FVector::DotProduct(Current - Ref, Up));
			}
		}
		// Cou et tête : les attentes « Crowd » regardent en l'air et penchent la tête ; en gros plan le visage bougeait
		// sans cesse. On n'en garde qu'une partie (rotation seulement, vers la pose de référence).
		if (IdleHeadMotion < 1.f)
		{
			TArray<TPair<FName, float>> Damped;
			for (const FName& BoneName : HeadMotionBones)
			{
				Damped.Emplace(BoneName, IdleHeadMotion);
			}
			for (const FName& BoneName : SpineMotionBones)
			{
				Damped.Emplace(BoneName, FMath::Lerp(IdleHeadMotion, 1.f, 0.3f));
			}
			for (const TPair<FName, float>& Pair : Damped)
			{
				const FName BoneName = Pair.Key;
				const int32 MeshIndex = Output.Pose.GetBoneContainer().GetPoseBoneIndexForBoneName(BoneName);
				if (MeshIndex == INDEX_NONE)
				{
					continue;
				}
				const FCompactPoseBoneIndex BoneIndex = Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
				if (BoneIndex.IsValid())
				{
					const FQuat Ref = Output.Pose.GetRefPose(BoneIndex).GetRotation();
					Output.Pose[BoneIndex].SetRotation(FQuat::Slerp(Ref, Output.Pose[BoneIndex].GetRotation(), FMath::Max(0.f, Pair.Value)).GetNormalized());
				}
			}
		}
	}
	else
	{
		Output.ResetToRefPose();
	}

	// 1 bis) Mode « suit le corps » : les os communs prennent la pose du corps (déjà déformée par ses curseurs)
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	for (const TPair<int32, FTransform>& Copied : CopiedBones)
	{
		const FCompactPoseBoneIndex PoseIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Copied.Key));
		if (PoseIndex.IsValid())
		{
			Output.Pose[PoseIndex] = Copied.Value;
			if (HelperScaleLimit > 0.f && LeafBones.IsValidIndex(Copied.Key) && LeafBones[Copied.Key])
			{
				const FVector Scale = Copied.Value.GetScale3D();
				const FVector Limited(FMath::Clamp(Scale.X, 1.f - HelperScaleLimit, 1.f + HelperScaleLimit),
					FMath::Clamp(Scale.Y, 1.f - HelperScaleLimit, 1.f + HelperScaleLimit),
					FMath::Clamp(Scale.Z, 1.f - HelperScaleLimit, 1.f + HelperScaleLimit));
				Output.Pose[PoseIndex].SetScale3D(Limited);
			}
		}
	}

	// 2) Les curseurs : on ajoute les déplacements dans l'espace local de chaque os
	if (Offsets.IsEmpty())
	{
		return true;
	}
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	// Rapport d'échelle ajouté à chaque os : ses enfants ne doivent pas en hériter (comme inZOI) ;
	// sinon grossir le cou grossirait aussi la tête, et les deux os du cou s'additionneraient
	TMap<int32, FVector> ScaleRatio;
	for (const FTrainerBoneOffset& Offset : Offsets)
	{
		if (CopiedNames.Contains(Offset.Bone))
		{
			continue; // os copié du corps : il a déjà reçu ce déplacement
		}
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
				// L'échelle du parent agrandit aussi la position locale de l'enfant : sans ça, élargir les épaules ou
				// le torse décollait le bras et ouvrait le ventre (silhouettes « Rond »). On l'annule pour les deux.
				FTransform& Child = Output.Pose[Index];
				Child.SetScale3D(Child.GetScale3D() / *Ratio);
				Child.SetTranslation(Child.GetTranslation() / *Ratio);
			}
		}
	}
	return true;
}
