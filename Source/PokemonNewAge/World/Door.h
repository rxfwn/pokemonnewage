#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Door.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 *  Porte de bâtiment du monde ouvert : elle tourne sur ses gonds, sans chargement.
 *  Elle s'ouvre quand le joueur approche (ou avec la touche d'action) et se referme
 *  quand il s'éloigne. Elle s'ouvre toujours du côté opposé au joueur.
 *  Repère : l'axe X de l'acteur est la normale de la porte, Y sa largeur, le pivot est
 *  au milieu du bas de l'encadrement.
 */
UCLASS()
class ADoor : public AActor, public IInteractable
{
	GENERATED_BODY()

	/** Gond : tourne autour de Z, porte le battant */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Hinge;

	/** Battant (un cube mis à l'échelle tant qu'il n'y a pas de vrai modèle) */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Panel;

	/** Zone de détection du joueur, des deux côtés de la porte */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Trigger;

protected:

	/** Largeur du passage (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 50, Units = "cm"))
	float DoorWidth = 140.f;

	/** Hauteur du passage (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 100, Units = "cm"))
	float DoorHeight = 240.f;

	/** Épaisseur du battant (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 1, Units = "cm"))
	float DoorThickness = 8.f;

	/** Angle d'ouverture (degrés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 10, ClampMax = 170, Units = "deg"))
	float OpenAngle = 100.f;

	/** Vitesse de rotation du battant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 0.1))
	float OpenSpeed = 6.f;

	/** S'ouvre toute seule quand le joueur approche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door")
	bool bOpenOnApproach = true;

	/** Temps avant de se refermer une fois le joueur parti (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 0, Units = "s"))
	float CloseDelay = 1.5f;

	/** Porte fermée à clé (bâtiment pas encore accessible) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door")
	bool bLocked = false;

	/** Distance de détection devant et derrière la porte (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door", meta = (ClampMin = 50, Units = "cm"))
	float DetectionDepth = 220.f;

public:

	ADoor();

	/** Ouvre la porte du côté opposé à ce pawn */
	UFUNCTION(BlueprintCallable, Category="Door")
	void Open(APawn* FromPawn);

	/** Referme la porte */
	UFUNCTION(BlueprintCallable, Category="Door")
	void Close();

	/** Vrai si la porte est ouverte ou en train de s'ouvrir */
	UFUNCTION(BlueprintPure, Category="Door")
	bool IsOpen() const { return !FMath::IsNearlyZero(TargetAngle); }

	/** Touche d'action : ouvre ou ferme */
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:

	/** Met les composants à la taille réglée dans le panneau Détails */
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

private:

	/** Vrai si un pawn joueur est dans la zone de détection */
	bool IsPlayerNearby() const;

	/** Angle actuel et angle visé du battant (degrés) */
	float CurrentAngle = 0.f;
	float TargetAngle = 0.f;

	FTimerHandle CloseTimer;
};
