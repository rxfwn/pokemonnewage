#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TeleportPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;
class UTextRenderComponent;

/**
 *  Point de téléportation de test : on en pose un où l'on veut (entrée d'une ville, sommet d'un belvédère...),
 *  on lui donne un nom (Point Name), puis en jeu, dans la console (touche ²) :
 *    Teleports            liste les points, numérotés
 *    Teleport Argenta     va au point « Argenta » (le début du nom suffit, majuscules sans importance)
 *    Teleport 3           va au 3e point de la liste
 *  Le joueur arrive sur le point, regardant dans le sens de la flèche.
 *
 *  Toujours chargé (pas « Is Spatially Loaded ») : on peut s'y rendre depuis l'autre bout de Kanto.
 *  Le nom s'affiche au-dessus du point dans l'éditeur seulement.
 */
UCLASS()
class ATeleportPoint : public AActor
{
	GENERATED_BODY()

public:

	ATeleportPoint();

	/** Nom utilisé par la commande Teleport (Argenta, Route3, MontSelenite...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Teleport")
	FName PointName = TEXT("Point");

	/** Tous les points de la map, triés par nom */
	static TArray<ATeleportPoint*> GetAll(const UObject* WorldContext);

	/** Le point dont le nom est Query (ou commence par Query), ou le N-ième de la liste si Query est un nombre */
	static ATeleportPoint* Find(const UObject* WorldContext, const FString& Query);

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

protected:

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> Label;
#endif

	/** Sens dans lequel le joueur regarde en arrivant */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UArrowComponent> Arrow;
};
