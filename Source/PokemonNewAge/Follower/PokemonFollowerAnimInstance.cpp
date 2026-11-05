#include "Follower/PokemonFollowerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

namespace PokemonFollowerAnim
{
	/** Calcule la pose : chaque animation à son temps, mélangées selon leurs poids */
	struct FProxy : public FAnimInstanceProxy
	{
		explicit FProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

		TArray<UPokemonFollowerAnimInstance::FLayer> Layers;

		virtual bool Evaluate(FPoseContext& Output) override
		{
			float Total = 0.f;
			for (const UPokemonFollowerAnimInstance::FLayer& Layer : Layers)
			{
				Total += Layer.Sequence ? Layer.Weight : 0.f;
			}
			if (Total <= KINDA_SMALL_NUMBER)
			{
				Output.ResetToRefPose();
				return true;
			}

			// Mélange de proche en proche : pose = mélange(pose jusqu'ici, animation suivante)
			bool bFirst = true;
			float Accumulated = 0.f;
			for (const UPokemonFollowerAnimInstance::FLayer& Layer : Layers)
			{
				if (!Layer.Sequence || Layer.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				FPoseContext Sampled(this);
				FAnimationPoseData SampledData(Sampled);
				Layer.Sequence->GetAnimationPose(SampledData, FAnimExtractContext(static_cast<double>(Layer.Time), false, FDeltaTimeRecord(), Layer.bLoop));
				if (bFirst)
				{
					Output = Sampled;
					bFirst = false;
					Accumulated = Layer.Weight;
					continue;
				}
				Accumulated += Layer.Weight;
				FPoseContext Blended(this);
				FAnimationPoseData SoFarData(Output);
				FAnimationPoseData BlendedData(Blended);
				FAnimationRuntime::BlendTwoPosesTogether(SoFarData, SampledData, 1.f - Layer.Weight / Accumulated, BlendedData);
				Output = Blended;
			}
			return true;
		}
	};
}

FAnimInstanceProxy* UPokemonFollowerAnimInstance::CreateAnimInstanceProxy()
{
	return new PokemonFollowerAnim::FProxy(this);
}

void UPokemonFollowerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<PokemonFollowerAnim::FProxy*>(InProxy);
}

void UPokemonFollowerAnimInstance::Want(UAnimSequence* Sequence, float PlayRate, float Weight, bool bLoop)
{
	if (!Sequence)
	{
		return;
	}
	FLayer* Found = Layers.FindByPredicate([Sequence](const FLayer& Layer) { return Layer.Sequence == Sequence; });
	if (!Found)
	{
		Found = &Layers.AddDefaulted_GetRef();
		Found->Sequence = Sequence;
		Referenced.AddUnique(Sequence);
	}
	Found->PlayRate = PlayRate;
	Found->bLoop = bLoop;
	Found->TargetWeight = FMath::Clamp(Weight, 0.f, 1.f);
	Found->bWanted = true;
}

void UPokemonFollowerAnimInstance::Restart(UAnimSequence* Sequence)
{
	for (FLayer& Layer : Layers)
	{
		if (Layer.Sequence == Sequence)
		{
			Layer.Time = 0.f;
		}
	}
}

float UPokemonFollowerAnimInstance::GetWeight(const UAnimSequence* Sequence) const
{
	const FLayer* Found = Layers.FindByPredicate([Sequence](const FLayer& Layer) { return Layer.Sequence == Sequence; });
	return Found ? Found->Weight : 0.f;
}

float UPokemonFollowerAnimInstance::GetPlayRate(const UAnimSequence* Sequence) const
{
	const FLayer* Found = Layers.FindByPredicate([Sequence](const FLayer& Layer) { return Layer.Sequence == Sequence; });
	return Found ? Found->PlayRate : 0.f;
}

void UPokemonFollowerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	for (int32 Index = Layers.Num() - 1; Index >= 0; --Index)
	{
		FLayer& Layer = Layers[Index];
		if (!Layer.bWanted)
		{
			Layer.TargetWeight = 0.f;
		}
		Layer.bWanted = false;
		Layer.Weight = FMath::FInterpConstantTo(Layer.Weight, Layer.TargetWeight, DeltaSeconds, 1.f / BlendTime);
		const float Length = Layer.Sequence ? Layer.Sequence->GetPlayLength() : 0.f;
		if (Length > 0.f)
		{
			Layer.Time += DeltaSeconds * Layer.PlayRate;
			Layer.Time = Layer.bLoop ? FMath::Fmod(Layer.Time, Length) : FMath::Min(Layer.Time, Length);
		}
		if (Layer.Weight <= 0.f && Layer.TargetWeight <= 0.f)
		{
			// Effacée : la prochaine fois elle repart du début
			Referenced.Remove(Layer.Sequence);
			Layers.RemoveAt(Index);
		}
	}

	// La pose est calculée plus tard (parfois sur un autre fil) : on lui donne une copie
	GetProxyOnGameThread<PokemonFollowerAnim::FProxy>().Layers = Layers;
}
