#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/TrainerAppearance.h"
#include "CharacterStudio.generated.h"

class UCameraComponent;
class URectLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTrainerAppearanceData;

/** Où regarde la caméra du studio */
UENUM()
enum class EStudioFocus : uint8
{
	Body,
	Face
};

/**
 *  Studio photo de l'éditeur de personnage (comme inZOI) : une pièce gris clair fermée (le soleil et le ciel
 *  du monde n'y entrent pas), trois éclairages au choix, la caméra qui passe du corps entier au visage,
 *  et un aperçu du personnage qu'on fait tourner à la souris.
 *  Créé seulement sur le PC du joueur qui édite (jamais répliqué), loin au-dessus de lui ; voir UTrainerAppearanceComponent.
 */
UCLASS(NotPlaceable)
class ACharacterStudio : public AActor
{
	GENERATED_BODY()

public:

	ACharacterStudio();

	void Init(UTrainerAppearanceData* InData, const FTrainerAppearance& Appearance);

	/** Habille l'aperçu */
	void ShowAppearance(const FTrainerAppearance& Appearance);

	void SetFocus(EStudioFocus NewFocus);
	EStudioFocus GetFocus() const { return Focus; }

	/** Fait tourner le personnage (degrés) */
	void Turn(float DeltaYaw);

	/** Molette : plus = plus près */
	void Zoom(float Amount);

	/** Éclairage suivant (Studio, Doux, Soirée) ; renvoie son nom */
	FText NextLighting();
	FText GetLightingName() const;

	/** Animation suivante (l'attente, puis les poses de la base) ; renvoie son nom. Sert au banc d'essai */
	FText NextAnimation();

	/** Enchaînement automatique attente -> pose -> attente (vrai par défaut ; le banc d'essai le coupe) */
	void SetAutoPoses(bool bEnable);

	USkeletalMeshComponent* GetPreview() const { return Preview; }

	/** Position d'un os (ou socket) de l'aperçu, plus un décalage dans le repère du personnage. Faux si l'os n'existe pas */
	bool GetBoneLocation(FName Bone, const FVector& Offset, FVector& OutLocation) const;

	virtual void Tick(float DeltaSeconds) override;

private:

	UStaticMeshComponent* MakeWall(FName Name, const FVector& Location, const FVector& SizeCm);
	URectLightComponent* MakeLight(FName Name, const FVector& Location, float Width);
	void ApplyLighting();

	/** Ce que vise la caméra (centre de la tête en gros plan, milieu du corps sinon) */
	FVector GetFocusPoint() const;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<USceneComponent> Turntable;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<USkeletalMeshComponent> Preview;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<URectLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<URectLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<URectLightComponent> RimLight;

	UPROPERTY(VisibleAnywhere, Category="Studio")
	TObjectPtr<URectLightComponent> TopLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;

	UPROPERTY(Transient)
	TObjectPtr<UTrainerAppearanceData> Data;

	/** Remet la pose choisie après un changement d'apparence (Apply remet l'attente de la base) */
	void ApplyStudioAnimation();

	/** Enchaînement automatique des poses (vue du corps seulement) */
	void TickAutoPoses(float DeltaSeconds);

	bool bAutoPoses = true;
	/** Temps passé dans l'attente, ou temps restant de la pose en cours */
	float AutoTimer = 0.f;
	/** Prochaine pose jouée (index dans FTrainerBaseDef::StudioAnimations) */
	int32 NextAutoPose = 0;

	FName AppliedBase;
	/** 0 = attente de la base, N = FTrainerBaseDef::StudioAnimations[N - 1] */
	int32 AnimationIndex = 0;
	EStudioFocus Focus = EStudioFocus::Body;
	float ZoomFactor = 1.f;
	float TargetYaw = 0.f;
	float CurrentYaw = 0.f;
	int32 LightingIndex = 0;
	bool bSnapCamera = true;
};
