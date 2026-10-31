#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PokeBallThrow.generated.h"

class UPointLightComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Lancer de Poké Ball en 3D (SCRUM-40) : la Ball part de la main du joueur en cloche, s'ouvre dans un éclair
 *  au-dessus du Pokémon, tombe au sol, secoue 0 à 3 fois, puis clic (capturé) ou s'ouvre (le Pokémon se libère).
 *  Purement visuel et local : le résultat vient du serveur (UBattleSession), UCaptureComponent le fait jouer.
 *  Modèle : la Ball lancée en deux moitiés (SCRUM-75, PokeBallModel.h), dont le couvercle s'ouvre vraiment.
 *  Sinon l'ancien SM_PokeBall teinté, et sans rien d'importé une sphère de couleur.
 */
UCLASS()
class APokeBallThrow : public AActor
{
	GENERATED_BODY()

	/** Moitié basse de la Ball (ou Ball entière avec l'ancien modèle) */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* Mesh;

	/** Charnière du couvercle, à l'arrière de la Ball */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	USceneComponent* Hinge;

	/** Moitié haute (couvercle avec le bouton) */
	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* TopMesh;

	UPROPERTY(VisibleAnywhere, Category="Components", meta=(AllowPrivateAccess="true"))
	UPointLightComponent* Flash;

public:

	APokeBallThrow();

	/** Lance la Ball. Shakes : secousses à montrer (0 à 3). IgnoredActor : le joueur (pour trouver le sol) */
	void Play(const FVector& From, const FVector& Target, FName Ball, int32 Shakes, bool bCaught, AActor* IgnoredActor);

	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Ancien modèle d'une seule pièce, teinté selon la Ball : utilisé seulement si /Game/Capture/Balls est vide */
	UPROPERTY(EditAnywhere, Category="Ball")
	TSoftObjectPtr<UStaticMesh> BallMesh;

	/** Ouverture du couvercle quand la Ball aspire le Pokémon, et quand il se libère */
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="Degrees"))
	float OpenAngle = 70.f;

	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="Degrees"))
	float BrokeFreeAngle = 110.f;

	/** Lueur des deux moitiés (paramètre Eclat de M_Ball) au plus fort de l'éclair */
	UPROPERTY(EditAnywhere, Category="Ball")
	float GlowStrength = 6.f;

	/** Ancien modèle : paramètre de couleur de la moitié haute dans le matériau (case 0) */
	UPROPERTY(EditAnywhere, Category="Ball")
	FName ColorParameter = TEXT("CouleurHaut");

	/** Ancien modèle : couleur de la moitié haute selon la Ball (ligne de DT_Items) */
	UPROPERTY(EditAnywhere, Category="Ball")
	TMap<FName, FLinearColor> BallColors;

	/** Rotation ajoutée pour que le bouton regarde le joueur, si le modèle n'est pas tourné vers +X */
	UPROPERTY(EditAnywhere, Category="Ball", meta=(Units="Degrees"))
	float ButtonYawOffset = 0.f;

	/** Diamètre de la Ball à l'écran (une vraie Poké Ball fait environ 8 cm, un peu plus pour la voir de loin) */
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin=2, Units="cm"))
	float BallDiameter = 12.f;

	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="s"))
	float ThrowDuration = 0.7f;

	/** Hauteur de la cloche au-dessus de la ligne droite */
	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="cm"))
	float ArcHeight = 160.f;

	/** Hauteur où la Ball s'ouvre au-dessus du sol */
	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="cm"))
	float OpenHeight = 110.f;

	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="s"))
	float OpenDuration = 0.45f;

	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="s"))
	float DropDuration = 0.3f;

	/** Une secousse puis une pause */
	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="s"))
	float ShakeDuration = 0.9f;

	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="Degrees"))
	float ShakeAngle = 25.f;

	/** Temps pendant lequel la Ball reste visible après le clic ou l'ouverture */
	UPROPERTY(EditAnywhere, Category="Ball|Timing", meta=(Units="s"))
	float ResultDuration = 1.4f;

	/** Sons (par défaut ceux de Content/Capture/Sons, créés par Tools/Capture/import_pokeball_unreal.py) */
	UPROPERTY(EditAnywhere, Category="Ball|Sound")
	TSoftObjectPtr<USoundBase> ThrowSound;

	UPROPERTY(EditAnywhere, Category="Ball|Sound")
	TSoftObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, Category="Ball|Sound")
	TSoftObjectPtr<USoundBase> ShakeSound;

	UPROPERTY(EditAnywhere, Category="Ball|Sound")
	TSoftObjectPtr<USoundBase> CaughtSound;

	UPROPERTY(EditAnywhere, Category="Ball|Sound")
	TSoftObjectPtr<USoundBase> BrokeFreeSound;

	/** Pour ajouter des effets en Blueprint (particules, étoiles...) */
	UFUNCTION(BlueprintImplementableEvent, Category="Ball")
	void OnBallOpened();

	UFUNCTION(BlueprintImplementableEvent, Category="Ball")
	void OnBallShake(int32 ShakeNumber);

	UFUNCTION(BlueprintImplementableEvent, Category="Ball")
	void OnBallResult(bool bCaught);

private:

	enum class EStage : uint8 { Idle, Throw, Open, Drop, Shake, Result };

	void EnterStage(EStage NewStage);
	void PlaySound(const TSoftObjectPtr<USoundBase>& Sound) const;
	void SetupMesh(FName Ball);
	void SetLid(float Degrees, float Glow, const FLinearColor& GlowColor);
	/** Le Pokémon se libère : la Ball en deux moitiés s'ouvre un peu plus longtemps */
	float BrokeFreeDuration() const { return bTwoParts ? 0.6f : 0.3f; }

	EStage Stage = EStage::Idle;
	float StageTime = 0.f;
	int32 ShakesToShow = 0;
	int32 ShakesDone = 0;
	bool bCaughtResult = false;

	FVector StartPoint = FVector::ZeroVector;
	FVector AirPoint = FVector::ZeroVector;
	FVector GroundPoint = FVector::ZeroVector;
	float BaseScale = 1.f;
	/** Vrai avec les modèles en deux moitiés (le couvercle s'ouvre) */
	bool bTwoParts = false;
	FRotator RestRotation = FRotator::ZeroRotator;
};
