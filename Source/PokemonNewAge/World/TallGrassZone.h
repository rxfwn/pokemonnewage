#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallGrassZone.generated.h"

class APawn;
class UBoxComponent;

/** Un joueur qui marche dans les herbes (serveur). */
USTRUCT()
struct FGrassWalker
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<APawn> Pawn;

	FVector LastLocation = FVector::ZeroVector;
	float Distance = 0.f;
	int32 SafeSteps = 0;
	bool bWasInBattle = false;
};

/**
 *  Hautes herbes (SCRUM-39) : à chaque pas d'un joueur dans la boîte, une chance de rencontrer un Pokémon
 *  sauvage tiré dans DT_Encounters (EncounterTable). Invisible en jeu, on la pose sur l'herbe qu'on voit.
 *  Le script de blockout en place une sur chaque carré de hautes herbes de Kanto, avec la table de la route.
 *  Tout se passe sur le serveur ; le combat démarre comme avec la commande CombatRencontre.
 */
UCLASS()
class ATallGrassZone : public AActor
{
	GENERATED_BODY()

	/** Étendue des herbes (régler sa taille avec Box Extent) */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	UBoxComponent* Box;

public:

	ATallGrassZone();

	/** Ligne de DT_Encounters (Route1_Walk, ViridianForest_Walk...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName EncounterTable = TEXT("Route1_Walk");

	/** Taux du lieu comme en 3G : 21 sur les routes (environ 12 % par pas), 14 dans la Forêt de Jade, 0 = jamais */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0, ClampMax=180))
	int32 EncounterRate = 21;

	/** Longueur d'un « pas » : une chance de rencontre chaque fois que le joueur a parcouru cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=20, Units="cm"))
	float StepLength = 150.f;

	/** Pas sans rencontre après un combat, pour pouvoir sortir des herbes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0))
	int32 SafeStepsAfterBattle = 4;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

private:

	/** Vrai si le pas déclenche un combat */
	bool TakeStep(FGrassWalker& Walker, APlayerController* Player);

	UPROPERTY(Transient)
	TArray<FGrassWalker> Walkers;

	FRandomStream Stream;
};
