#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DialogueTriggerVolume.generated.h"

class UBoxComponent;

/**
 *  Volume au sol qui lance une scène (conversation Trigger = Script) quand le joueur y entre :
 *  Chen qui arrête le joueur à la sortie de Bourg Palette, Régis devant le labo, le vieil homme de Jadielle...
 *  La scène ne se lance que si ses conditions (drapeaux) sont remplies pour CE joueur ;
 *  ses drapeaux de fin l'empêchent ensuite de se rejouer.
 */
UCLASS()
class ADialogueTriggerVolume : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Box;

public:

	ADialogueTriggerVolume();

	/** Conversation à jouer (ligne de DT_Dialogues) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(GetOptions="GetConversationOptions"))
	FName ConversationId;

	/** Liste déroulante de « Conversation Id » : les scènes (Trigger = Script) de DT_Dialogues */
	UFUNCTION()
	TArray<FString> GetConversationOptions() const;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
