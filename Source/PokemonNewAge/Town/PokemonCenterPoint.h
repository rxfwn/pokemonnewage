#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PokemonCenterPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;
class USoundBase;

/**
 *  Point de retour d'un Centre Pokémon (SCRUM-53). À poser devant le comptoir de Joëlle, la flèche vers la sortie :
 *  quand Joëlle soigne l'équipe, ce Centre devient le « dernier Centre visité », et si toute l'équipe est K.O.
 *  le joueur réapparaît ici.
 *
 *  Le plus proche du joueur (dans Heal Radius) est choisi au moment du soin : un point par Centre suffit.
 *  En poser aussi un dans la chambre ou le salon de la maison du joueur, avec Is Home coché :
 *  c'est là qu'on revient avant d'avoir visité un Centre (maman soigne aussi l'équipe).
 *
 *  Toujours chargé (pas « Is Spatially Loaded ») : le joueur peut être renvoyé ici depuis l'autre bout de Kanto.
 */
UCLASS()
class APokemonCenterPoint : public AActor
{
	GENERATED_BODY()

public:

	APokemonCenterPoint();

	/** Identifiant unique, gardé dans la sauvegarde (ViridianPokemonCenter, PewterPokemonCenter, PlayerHouse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokemon Center")
	FName PointId;

	/** Nom du lieu, affiché et noté comme lieu actuel du joueur (ViridianCity...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokemon Center")
	FName Location;

	/** Maison du joueur : point de retour tant qu'aucun Centre n'a soigné l'équipe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokemon Center")
	bool bIsHome = false;

	/** Le soin compte pour ce point si le joueur est à moins de cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokemon Center", meta=(ClampMin=100, Units="cm"))
	float HealRadius = 1500.f;

	/** Petite musique du soin (le jingle des Centres), jouée chez le joueur soigné. Facultatif */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokemon Center")
	TObjectPtr<USoundBase> HealSound;

	/** Le point le plus proche de Location dans HealRadius, nullptr sinon */
	static APokemonCenterPoint* FindNearest(const UObject* WorldContext, const FVector& Location);

	/** Le point PointId, nullptr s'il n'existe pas */
	static APokemonCenterPoint* FindById(const UObject* WorldContext, FName PointId);

	/** La maison du joueur (bIsHome), nullptr si aucune */
	static APokemonCenterPoint* FindHome(const UObject* WorldContext);

protected:

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	/** Sens dans lequel le joueur regarde en réapparaissant */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UArrowComponent> Arrow;
};
