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
 * - Sol : disque à la hauteur exacte des pieds et de la Ball, assez grand pour aller jusqu'au ciel. La scène
 *   entière est soulevée de GroundClearance au-dessus du terrain (AEvolutionScene::PlaceInFrontOf) : l'herbe et
 *   les petites bosses restent sous le sol, et rien ne s'enfonce dedans.
 * - Devant le Pokémon : deux panneaux transparents accrochés à la caméra (additifs). Le plus proche porte de gros
 *   bokeh flous (le flou de profondeur les adoucit encore), l'autre des éclats et des poussières de lumière juste
 *   devant le Pokémon. Le centre de l'image reste presque dégagé pour voir la transformation en points.
 * Les matériaux viennent de Tools/Progression/import_evolution_backdrop.py (M_EvolutionSky, M_EvolutionFloor,
 * M_EvolutionFrontNear, M_EvolutionFrontMid). Les panneaux de devant sont facultatifs.
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

	/** Effets flous tout près de la caméra (gros bokeh) */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond")
	TSoftObjectPtr<UMaterialInterface> FrontNearMaterial;

	/** Effets juste devant le Pokémon (éclats, poussières de lumière) */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond")
	TSoftObjectPtr<UMaterialInterface> FrontMidMaterial;

	/** Hauteur de la scène au-dessus du terrain (cm). Haut (30 m) : aucune colline ni arbre du décor ne passe
	 *  devant le ciel (une pente de terrain coupait l'image en biais). Le sol lumineux fait le vrai sol. */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond", meta=(ClampMin="0"))
	float GroundClearance = 3000.f;

	/** Vignette de la caméra quand le fond est là (la scène en met 1,1 : bords noirs) */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond", meta=(ClampMin="0"))
	float BackdropVignette = 0.15f;

	/** Vitesse à laquelle les effets de devant s'écartent du centre ou y reviennent (par seconde) */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond", meta=(ClampMin="0.1"))
	float FrontClearingSpeed = 1.5f;

	/** true pendant la transformation : les effets de devant laissent le centre dégagé ; false : ils passent devant */
	void SetFrontClearing(bool bClear) { bClearFront = bClear; }

	/** Panneau des gros bokeh, en part de la distance caméra-Pokémon */
	UPROPERTY(EditAnywhere, Category="Evolution|Fond", meta=(ClampMin="0.1", ClampMax="0.9"))
	float FrontNearDistance = 0.35f;

	/** Hauteur dont soulever la scène au-dessus du terrain (0 si le fond n'est pas là) */
	float GetGroundClearance() const { return Sky ? GroundClearance : 0.f; }

protected:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UStaticMeshComponent* MakePlane(const TCHAR* Name, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutDynamic);
	UStaticMeshComponent* MakeCameraPanel(const TCHAR* Name, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutDynamic);
	void PlaceCameraPanel(UStaticMeshComponent* Panel, float Distance) const;
	void UpdateLayout();
	float SceneRadius() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Sky;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FrontNear;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FrontMid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrontNearDynamic;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrontMidDynamic;

	bool bClearFront = false;
	float FrontClearing = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyDynamic;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FloorDynamic;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;
};
