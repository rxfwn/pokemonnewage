#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvolutionScene.generated.h"

class UAudioComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
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
 *  Entrée (maquette de L'amie, docs/PROGRESSION.md) : la Ball de sa capture (PokeBallModel.h) tremble sur l'herbe,
 *  s'ouvre en laissant passer la lumière, un rayon monte avec des étincelles, puis une explosion blanche laisse
 *  le Pokémon déjà en silhouette de lumière et la transformation commence. Rayon, étincelles et onde de choc :
 *  formes de base du moteur avec /Game/Progression/M_EvolutionLight. Sans Ball importée, le Pokémon est là dès
 *  le début comme avant.
 *  Un Blueprint enfant peut régler les durées, la lumière et la caméra dans le panneau Détails.
 */
UCLASS()
class AEvolutionScene : public AActor
{
	GENERATED_BODY()

public:

	AEvolutionScene();

	/** Lance la séquence devant le joueur. Ball : celle où le Pokémon a été capturé (None = Poké Ball).
	 *  OnFinished(vrai si évolué) à la toute fin */
	void Play(APlayerController* Controller, UProgressionWidget* InWidget, const FText& InPokemonName, FName FromSpecies, FName ToSpecies,
		FName Ball, TFunction<void(bool)> OnFinished);

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

	/** Taille de la Poké Ball d'où sort le Pokémon (cm ; plus grande qu'en vrai pour bien la voir) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BallDiameter = 24.f;

	/** La Ball tremble sur l'herbe (s), nombre d'oscillations et inclinaison (degrés) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float WobbleTime = 2.f;

	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	int32 WobbleCount = 3;

	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float WobbleAngle = 22.f;

	/** Ouverture : le couvercle s'entrouvre, la lumière sort (s) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BallOpenTime = 1.f;

	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BallOpenAngle = 45.f;

	/** Rayon de lumière qui monte, avec les étincelles (s) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BeamTime = 1.2f;

	/** Explosion blanche qui laisse le Pokémon en silhouette ; la caméra recule pendant ce temps (s) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BurstTime = 1.f;

	/** Couleur de la lumière sortie de la Ball (rayon, étincelles) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	FLinearColor BallLightColor = FLinearColor(0.7f, 0.85f, 1.f);

	/** Distance de la caméra à la Ball au début, en diamètres de Ball */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BallShotDistance = 4.2f;

	/** Ouverture de l'objectif pendant le gros plan : assez fermée pour que toute la Ball soit nette
	 *  (le flou d'arrière-plan de la caméra revient quand elle recule vers le Pokémon) */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BallShotFstop = 6.f;

	/** Hauteur de la Ball à l'écran pendant le gros plan : 0 = au centre, positif = plus haut (en diamètres),
	 *  pour qu'elle reste au-dessus de la boîte de texte */
	UPROPERTY(EditAnywhere, Category="Evolution|Ball")
	float BallShotRaise = 0.15f;

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

	/** Poké Ball d'où sort le Pokémon : moitié basse > charnière > couvercle */
	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<UStaticMeshComponent> BallBottom;

	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<USceneComponent> BallHinge;

	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<UStaticMeshComponent> BallTop;

	/** Boule de lumière dans la Ball, qui grossit et éclate */
	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<UStaticMeshComponent> LightCore;

	/** Rayon de lumière vertical */
	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<UStaticMeshComponent> Beam;

	/** Onde de choc de l'explosion (sphère plus claire sur les bords) */
	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<UStaticMeshComponent> Shockwave;

	/** Étincelles (traits de lumière étirés dans leur direction) */
	UPROPERTY(VisibleAnywhere, Category="Evolution|Ball")
	TObjectPtr<UInstancedStaticMeshComponent> Sparks;

private:

	enum class EPhase : uint8 { Intro, BallWobble, BallOpen, BallBeam, BallBurst, Glow, Morph, Flash, Reveal, Done, Cancelled };

	struct FSpark
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f;
		float Length = 0.f;
	};

	/** Charge le modèle d'une espèce dans l'un des deux composants, pieds au sol et face à la caméra. Hauteur (cm) */
	float LoadModel(FName Species, USkeletalMeshComponent* Skeletal, UStaticMeshComponent* Static, UMeshComponent*& OutMesh);
	void SetGlow(UMaterialInstanceDynamic* Material, float Amount);
	void SetModelScale(UMeshComponent* Mesh, float Scale);
	void PlaceInFrontOf(APlayerController* Controller);
	void EnterPhase(EPhase NewPhase);
	void Finish(bool bEvolved);
	void PlaySound(const TCHAR* Name);
	/** Pose la Ball sur l'herbe, à la place du Pokémon ; faux si aucune Ball n'est importée */
	bool SetupBall(FName Ball);
	/** Les quatre temps de l'entrée par la Ball (maquette) */
	void TickBallPhase(float DeltaTime);
	/** Cache la Ball et ses effets (fin de l'entrée ou annulation) */
	void HideBall();
	void SetupEffects();
	void SetEffectGlow(UMaterialInstanceDynamic* Material, float Amount, const FLinearColor& Color);
	/** Lance des étincelles depuis la Ball : Up 0 = dans toutes les directions, 1 = vers le haut */
	void SpawnSparks(int32 Count, float Speed, float Up);
	void TickSparks(float DeltaTime);
	/** Caméra : 0 = gros plan sur la Ball, 1 = cadrage du Pokémon */
	void UpdateCamera(float Blend);
	bool IsBallPhase() const { return Phase >= EPhase::BallWobble && Phase <= EPhase::BallBurst; }

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
	bool bBallIntro = false;
	bool bPokemonOut = false;
	float BallBaseScale = 1.f;
	FVector BallRest = FVector::ZeroVector;
	FVector PokemonShotLocation = FVector::ZeroVector;
	FVector PokemonShotTarget = FVector::ZeroVector;
	FVector BallShotLocation = FVector::ZeroVector;
	float CameraBlend = 1.f;
	float PokemonFstop = 1.2f;
	float PokemonHeight = 0.f;
	float SparkTimer = 0.f;
	int32 LastWobble = -1;
	TArray<FSpark> SparkList;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShockLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SparkLight;
};
