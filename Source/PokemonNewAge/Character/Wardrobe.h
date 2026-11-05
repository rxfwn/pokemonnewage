#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Wardrobe.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

/**
 *  Armoire de la chambre : touche d'action (E) devant elle = éditeur de personnage sur l'onglet Tenue.
 *  À poser dans la chambre de L_Kanto ; mettre son propre modèle dans Mesh (Détails).
 */
UCLASS()
class AWardrobe : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	AWardrobe();

	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wardrobe")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Zone que la touche d'action détecte (le modèle peut ne pas avoir de collision) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wardrobe")
	TObjectPtr<UBoxComponent> InteractArea;
};
