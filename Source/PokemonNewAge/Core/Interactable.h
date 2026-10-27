#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Tout ce avec quoi le joueur peut interagir avec la touche d'action (E) :
 *  portes, PNJ, panneaux, objets au sol...
 *  Utilisable en C++ comme en Blueprint (Class Settings > Interfaces > Interactable).
 */
class IInteractable
{
	GENERATED_BODY()

public:

	/** Appelé quand le joueur appuie sur la touche d'action à portée */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(APawn* InteractingPawn);
};
