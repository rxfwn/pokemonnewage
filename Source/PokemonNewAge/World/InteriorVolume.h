#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteriorVolume.generated.h"

class UBoxComponent;

/**
 *  Volume posé dans un bâtiment : tant que le joueur est dedans, la caméra se rapproche
 *  pour ne pas traverser les murs ni le plafond. Aucun chargement, on reste dans le
 *  monde ouvert.
 */
UCLASS()
class AInteriorVolume : public AActor
{
	GENERATED_BODY()

	/** Volume de l'intérieur (régler sa taille avec Box Extent) */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Box;

protected:

	/** Distance maximale de la caméra à l'intérieur (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interior", meta = (ClampMin = 50, Units = "cm"))
	float InteriorCameraDistance = 250.f;

public:

	AInteriorVolume();

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};
