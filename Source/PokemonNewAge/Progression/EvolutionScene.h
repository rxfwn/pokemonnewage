#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvolutionScene.generated.h"

class UAudioComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UPointLightComponent;
class UProgressionWidget;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 *  Séquence d'évolution en 3D (SCRUM-47, docs/PROGRESSION.md). Jouée côté client seulement, devant le joueur :
 *  la caméra se pose face au Pokémon, le décor s'assombrit et se floute, le Pokémon devient une silhouette
 *  de lumière, alterne de plus en plus vite avec la silhouette de sa nouvelle forme, puis un flash révèle
 *  la nouvelle espèce. B (ou Échap) avant le flash annule l'évolution.
 *
 *  Modèles : /Game/Pokemon/Models/<Espèce>/SK_<Espèce> (ou SM_), comme le Pokédex ; une sphère s'ils manquent.
 *  Lueur : /Game/Progression/M_EvolutionGlow en matériau de surcouche (Overlay Material).
 *  Un Blueprint enfant peut régler les durées, la lumière et la caméra dans le panneau Détails.
 */
UCLASS()
class AEvolutionScene : public AActor
{
	GENERATED_BODY()

public:

	AEvolutionScene();

	/** Lance la séquence devant le joueur. OnFinished(vrai si évolué) à la toute fin */
	void Play(APlayerController* Controller, UProgressionWidget* InWidget, const FText& InPokemonName, FName FromSpecies, FName ToSpecies,
		TFunction<void(bool)> OnFinished);

	/** B : arrête l'évolution si le flash n'a pas encore eu lieu. Vrai si la demande est prise */
	bool RequestCancel();

	virtual void Tick(float DeltaTime) override;

	// ------------------------------------------------ Réglages

	/** Entrée en scène, avant que le Pokémon ne brille (s) */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float IntroTime = 2.f;

	/** Montée de la lueur (s) */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float GlowTime = 1.5f;

	/** Alternance des deux silhouettes, de plus en plus rapide (s) */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float MorphTime = 7.f;

	/** Nombre d'allers-retours au début et à la fin de l'alternance, par seconde */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float MorphStartRate = 0.6f;

	UPROPERTY(EditAnywhere, Category="Evolution")
	float MorphEndRate = 6.f;

	/** Disparition de la lueur après le flash (s) */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float RevealTime = 1.6f;

	/** Distance de la scène devant le joueur (cm) */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float DistanceFromPlayer = 350.f;

	/** Couleur de la lueur */
	UPROPERTY(EditAnywhere, Category="Evolution")
	FLinearColor GlowColor = FLinearColor(0.75f, 0.9f, 1.f);

	/** Hauteur de la sphère qui remplace un modèle absent (cm) */
	UPROPERTY(EditAnywhere, Category="Evolution")
	float PlaceholderHeight = 80.f;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<UCameraComponent> Camera;

	/** Lumière principale sur le Pokémon */
	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<USpotLightComponent> KeyLight;

	/** Lumière qui pulse avec la lueur et éclate au flash */
	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<USkeletalMeshComponent> OldSkeletal;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<UStaticMeshComponent> OldStatic;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<USkeletalMeshComponent> NewSkeletal;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<UStaticMeshComponent> NewStatic;

	UPROPERTY(VisibleAnywhere, Category="Evolution")
	TObjectPtr<UAudioComponent> Music;

private:

	enum class EPhase : uint8 { Intro, Glow, Morph, Flash, Reveal, Done, Cancelled };

	/** Charge le modèle d'une espèce dans l'un des deux composants, pieds au sol et face à la caméra. Hauteur (cm) */
	float LoadModel(FName Species, USkeletalMeshComponent* Skeletal, UStaticMeshComponent* Static, UMeshComponent*& OutMesh);
	void SetGlow(UMaterialInstanceDynamic* Material, float Amount);
	void SetModelScale(UMeshComponent* Mesh, float Scale);
	void PlaceInFrontOf(APlayerController* Controller);
	void EnterPhase(EPhase NewPhase);
	void Finish(bool bEvolved);
	void PlaySound(const TCHAR* Name);

	UPROPERTY(Transient) TObjectPtr<UProgressionWidget> Widget;
	UPROPERTY(Transient) TObjectPtr<UMeshComponent> OldMesh;
	UPROPERTY(Transient) TObjectPtr<UMeshComponent> NewMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> OldGlow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> NewGlow;

	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	bool bPawnWasHidden = false;
	TFunction<void(bool)> FinishedCallback;

	FText PokemonName;
	FText NewSpeciesName;
	EPhase Phase = EPhase::Intro;
	float PhaseTime = 0.f;
	float MorphPhase = 0.f;
	FVector OldBaseScale = FVector::OneVector;
	FVector NewBaseScale = FVector::OneVector;
	float BaseLightIntensity = 0.f;
	bool bCancelRequested = false;
};
