#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PokemonFollowerAnimInstance.generated.h"

class UAnimSequence;

/**
 *  Animation du Pokémon qui suit (sans Animation Blueprint) : plusieurs animations mélangées par poids,
 *  avec des fondus quand on passe de l'une à l'autre (dodo -> debout -> marche, attente <-> marche lente...).
 *  APokemonFollower dit à chaque image ce qu'il veut (Want), l'instance fait les fondus.
 */
UCLASS(Transient, NotBlueprintable)
class UPokemonFollowerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** À appeler à chaque image : les animations voulues cette image-là (les autres s'effacent en fondu) */
	void Want(UAnimSequence* Sequence, float PlayRate, float Weight, bool bLoop);

	/** Durée des fondus (s) */
	void SetBlendTime(float Seconds) { BlendTime = FMath::Max(Seconds, 0.01f); }

	/** Rejoue cette animation depuis le début (joie) */
	void Restart(UAnimSequence* Sequence);

	/** Poids actuel (0 à 1, après fondu) et vitesse de lecture de cette animation */
	float GetWeight(const UAnimSequence* Sequence) const;
	float GetPlayRate(const UAnimSequence* Sequence) const;

	struct FLayer
	{
		UAnimSequence* Sequence = nullptr;
		float Time = 0.f;
		float PlayRate = 1.f;
		float Weight = 0.f;
		float TargetWeight = 0.f;
		bool bLoop = true;
		bool bWanted = false;
	};

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:

	TArray<FLayer> Layers;

	/** Garde les animations en mémoire tant qu'elles sont jouées */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Referenced;

	float BlendTime = 0.25f;
};
