#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoryGate.generated.h"

class UBoxComponent;
class UArrowComponent;
class APawn;

/**
 *  Barrière d'histoire (multijoueur, 2026-11-02) : une zone de passage (sortie de ville, entrée de route) que chaque joueur
 *  ne franchit que s'il a les drapeaux ou badges demandés. Dans un même monde, celui qui a débloqué la ville passe,
 *  celui qui ne l'a pas encore est repoussé du côté d'où il vient, au même endroit et au même moment.
 *  Le serveur décide pour chaque joueur avec SON PlayerState ; aucun mur n'est partagé. Voir docs/MULTIJOUEUR.md.
 *
 *  Placement : la flèche bleue pointe vers le côté fermé (la ville à débloquer). Étirer la boîte sur toute la largeur du passage.
 */
UCLASS()
class AStoryGate : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Box;

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* Arrow;

public:

	AStoryGate();

	/** Drapeaux d'histoire à avoir tous (GotPokedex, Beaten_Brock...), voir docs/DIALOGUES.md */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Gate")
	TArray<FName> RequiredFlags;

	/** Badges à avoir tous (BoulderBadge...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Gate")
	TArray<FName> RequiredBadges;

	/** Scène jouée au joueur repoussé (ligne de DT_Dialogues, Trigger = Script) ; vide = Message ci-dessous */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Gate")
	FName ConversationId;

	/** Message si pas de scène */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Gate")
	FText Message;

	/** Distance à laquelle le joueur est remis devant la boîte, en cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Gate", meta=(ClampMin="50"))
	float PushBackDistance = 150.f;

	/** Vrai si ce joueur peut passer */
	UFUNCTION(BlueprintPure, Category="Story Gate")
	bool CanPass(const APlayerController* Player) const;

protected:

	virtual void BeginPlay() override;

	/** Serveur : repousse les joueurs qui ne peuvent pas passer et sont dans la boîte */
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:

	/** Remet le personnage du côté ouvert ; vrai s'il a été repoussé */
	bool PushBack(APawn* Pawn);

	/** Message une fois par passage, pas à chaque poussée */
	TMap<TWeakObjectPtr<APawn>, float> LastMessageTime;
};
