#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "EvolutionBackdrop.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Fond de la scène d'évolution (demandé par L'amie le 2026-11-03) : un ciel bleu étoilé et animé derrière le
 * Pokémon (étoiles qui scintillent, bokeh qui monte, traînées de lumière) et un sol lumineux sous ses pieds,
 * à la place du terrain sombre.
 *
 * - Ciel : grand panneau accroché à la caméra, toujours derrière le Pokémon et assez grand pour remplir l'image.
 * - Sol : disque sous les pieds, juste au-dessus du terrain, assez grand pour aller jusqu'au ciel.
 * Les matériaux viennent de Tools/Progression/import_evolution_backdrop.py (M_EvolutionSky, M_EvolutionFloor).
 * S'ils manquent, rien n'est affiché (fond d'avant, le terrain).
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class POKEMONNEWAGE_API UEvolutionBackdropComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UEvolutionBackdropComponent();

	/** Ciel derrière le Pokémon (non éclairé, animé) */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond")
	TSoftObjectPtr<UMaterialInterface> SkyMaterial;

	/** Sol lumineux sous le Pokémon */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond")
	TSoftObjectPtr<UMaterialInterface> FloorMaterial;

	/** Le ciel est à (distance caméra-Pokémon + ce recul) de la caméra, en cm, au minimum */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond", meta=(ClampMin="100"))
	float SkyDistanceBehind = 600.f;

	/** Marge du ciel autour de l'image (1 = bord à bord) : couvre les écrans plus hauts que 16:9 */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond", meta=(ClampMin="1"))
	float SkyMargin = 1.6f;

	/** Hauteur du sol au-dessus des pieds (cm) : cache l'herbe et les petites bosses du terrain */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond")
	float FloorLift = 3.f;

protected:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UStaticMeshComponent* MakePlane(const TCHAR* Name, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutDynamic);
	void UpdateLayout();
	float SceneRadius() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Sky;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyDynamic;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FloorDynamic;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;
};
