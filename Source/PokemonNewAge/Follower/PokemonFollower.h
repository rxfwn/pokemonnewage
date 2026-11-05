#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/Interactable.h"
#include "Pokemon/PokemonTypes.h"
#include "PokemonFollower.generated.h"

class UAnimSequence;
class USkeletalMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

/** Pose longue jouée chez tous les joueurs (le reste se déduit de la vitesse) */
UENUM()
enum class EFollowerPose : uint8
{
	None,
	Sleep
};

/**
 *  Le Pokémon de tête de l'équipe, qui accompagne son dresseur hors combat (SCRUM-78, docs/POKEMON_QUI_SUIT.md).
 *  Il a son propre petit comportement au lieu de marcher dans les pas du joueur :
 *  - le joueur bouge : il choisit une place autour de lui (derrière, un peu sur le côté) entre MinDistance
 *    et ComfortDistance, ne repart que quand le joueur s'éloigne, et change de place de temps en temps ;
 *    il marche ou court selon le retard (vitesse de démarche calculée d'après sa taille), tourne avant d'avancer ;
 *  - le joueur s'arrête : il se balade autour de lui, s'arrête, regarde autour, revient le voir,
 *    et finit par s'endormir s'il a une animation de sommeil.
 *  Fly : mêmes règles, en l'air à côté du joueur. Float : au ras du sol.
 *  Créé et déplacé par le serveur (UPokemonFollowerComponent), vu par tous les joueurs du monde.
 *  Il ne bloque ni les joueurs ni la caméra. Coincé ou trop loin : il réapparaît derrière le joueur.
 *  Touche d'action (E) devant lui : il se tourne vers le joueur et montre sa joie.
 */
UCLASS()
class APokemonFollower : public ACharacter, public IInteractable
{
	GENERATED_BODY()

public:

	APokemonFollower();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** IInteractable : caresse (côté joueur, envoyée au serveur par UPokemonFollowerComponent) */
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	/** Serveur : qui suivre, quelle espèce et comment. À appeler juste après l'apparition */
	void Setup(APawn* InLeader, FName InSpecies, EFollowMode InMode);

	/** Serveur : nouveau pion à suivre (réapparition du joueur) */
	void SetLeader(APawn* InLeader);

	APawn* GetLeader() const { return Leader; }
	FName GetSpecies() const { return Species; }
	EFollowMode GetFollowMode() const { return FollowMode; }

	/** Serveur : se replace juste derrière le joueur, posé au sol (ou en l'air s'il vole) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Follower")
	void TeleportBehindLeader();

	/** Serveur : se tourne vers ce pion et montre sa joie, chez tous les joueurs */
	void Pet(APawn* ByPawn);

	/**
	 *  Serveur : coup de sifflet. Il accourt au pied du joueur (à sa gauche) et y reste ;
	 *  trop loin ou bloqué, il disparaît et réapparaît à côté du joueur. Un 2e coup de sifflet le laisse libre.
	 */
	void Whistle();

	/** Serveur : il est au pied (sifflé) */
	bool IsAtHeel() const { return bHeel; }

	/** Serveur : disparaît dans des étoiles et réapparaît à côté du joueur */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Follower")
	void StartTeleport();

	// ------------------------------------------------ Distances

	/** Il ne s'approche pas plus près du joueur que ça */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Distance", meta=(ClampMin=50, Units="cm"))
	float MinDistance = 120.f;

	/** Tant qu'il est plus près que ça, il reste où il est (il ne repart pas au moindre pas du joueur) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Distance", meta=(ClampMin=100, Units="cm"))
	float ComfortDistance = 260.f;

	/** Au-delà, il court pour rattraper le joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Distance", meta=(ClampMin=200, Units="cm"))
	float MaxDistance = 500.f;

	/** Plus loin que ça du joueur : il réapparaît derrière lui */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Distance", meta=(ClampMin=500, Units="cm"))
	float TeleportDistance = 2500.f;

	/** Côté où il se place : jusqu'à cet angle de part et d'autre de « juste derrière le joueur » */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Distance", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float SideAngle = 75.f;

	/** Il change de place autour du joueur toutes les... (au hasard entre les deux) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Distance", meta=(Units="s"))
	FVector2D ReplanInterval = FVector2D(4.f, 9.f);

	// ------------------------------------------------ Démarche

	/**
	 *  Vitesse de l'animation de marche jouée à vitesse normale, par mètre de taille du Pokémon.
	 *  Pikachu (40 cm) marche à 100 cm/s, Bulbizarre (70 cm) à 175 cm/s. Les pattes vont au rythme du sol.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche", meta=(ClampMin=50, Units="cm/s"))
	float WalkSpeedPerMeter = 250.f;

	/** Pareil pour la course : Pikachu court à 320 cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche", meta=(ClampMin=100, Units="cm/s"))
	float RunSpeedPerMeter = 800.f;

	/** Bornes de ces vitesses (minimum, maximum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche")
	FVector2D WalkSpeedRange = FVector2D(70.f, 260.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche")
	FVector2D RunSpeedRange = FVector2D(240.f, 650.f);

	/** Vitesses propres à une espèce (X = marche, Y = course, en cm/s), si le calcul par la taille ne va pas */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche")
	TMap<FName, FVector2D> SpeedOverrides;

	/** Vitesse de rotation (il tourne avant d'avancer, au lieu de glisser de côté) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche", meta=(ClampMin=30, Units="deg/s"))
	float TurnRate = 300.f;

	/** L'animation peut accélérer ou ralentir jusque-là pour coller à la vitesse réelle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Demarche")
	FVector2D PlayRateRange = FVector2D(0.6f, 1.8f);

	// ------------------------------------------------ Quand le joueur ne bouge pas

	/** Le joueur est immobile depuis ce temps : il commence à faire sa vie */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Balade", meta=(ClampMin=0, Units="s"))
	float IdleDelay = 1.5f;

	/** Il se balade jusqu'à cette distance du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Balade", meta=(ClampMin=100, Units="cm"))
	float WanderRadius = 400.f;

	/** Pause entre deux envies (au hasard entre les deux) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Balade", meta=(Units="s"))
	FVector2D PauseDuration = FVector2D(2.f, 6.f);

	/** Le joueur est immobile depuis ce temps : il s'endort (s'il a une animation Sleep). 0 = jamais */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Balade", meta=(ClampMin=0, Units="s"))
	float SleepAfter = 20.f;

	// ------------------------------------------------ Vol

	/** Pokémon qui vole : hauteur du milieu de son corps au-dessus des pieds du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float FlyHeight = 150.f;

	/** Pokémon qui flotte : hauteur au-dessus du sol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float FloatHeight = 40.f;

	/** Ondulation en vol (amplitude, en cm) et sa fréquence (par seconde) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float BobAmplitude = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol")
	float BobFrequency = 0.7f;

	// ------------------------------------------------ Sifflet

	/** Au pied : distance du joueur (à sa gauche, un peu en arrière) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Sifflet", meta=(ClampMin=30, Units="cm"))
	float HeelDistance = 70.f;

	/** Au pied : angle depuis « juste derrière le joueur » (90 = à sa gauche, -90 = à sa droite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Sifflet", meta=(Units="deg"))
	float HeelAngle = 70.f;

	// ------------------------------------------------ Effets

	/** Durée de la disparition dans les étoiles avant de réapparaître */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Effets", meta=(ClampMin=0.05, Units="s"))
	float VanishTime = 0.35f;

	/** Durée de l'apparition (il grandit d'un coup, avec un petit rebond) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Effets", meta=(ClampMin=0.05, Units="s"))
	float AppearTime = 0.45f;

	/** Sommeil : une respiration (la bulle gonfle puis dégonfle) toutes les... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Effets", meta=(ClampMin=0.5, Units="s"))
	float BreathPeriod = 3.f;

	/** Sommeil : rayon maximal de la bulle, en part de la taille du Pokémon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Effets")
	float BubbleSizeRatio = 0.09f;

	/** Décalage de la planche d'expressions des yeux pour « yeux fermés » (modèles Let's Go : 3e ligne) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Effets")
	FVector2D ClosedEyeOffset = FVector2D(0.f, 0.5f);

	/** Clignement des yeux éveillé : toutes les... (au hasard entre les deux, 0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Effets", meta=(Units="s"))
	FVector2D BlinkInterval = FVector2D(2.5f, 6.f);

	// ------------------------------------------------ Divers

	/** Bloqué (il veut avancer mais ne bouge presque pas) pendant ce temps : il abandonne, ou réapparaît */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=0.5, Units="s"))
	float StuckTime = 2.f;

	/** Taille maximale affichée (0 = taille réelle du Pokédex, Onix fait alors 8,8 m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=0, Units="cm"))
	float MaxDisplayHeight = 0.f;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Species();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHappy();

	/** Tous les joueurs : il rapetisse dans des étoiles */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastVanish(FVector_NetQuantize At);

	/** Tous les joueurs : il réapparaît dans des étoiles */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAppear(FVector_NetQuantize At);

	/** Espèce (ligne de DT_Species) : le modèle se charge chez chaque joueur */
	UPROPERTY(ReplicatedUsing=OnRep_Species)
	FName Species;

	UPROPERTY(ReplicatedUsing=OnRep_Species)
	EFollowMode FollowMode = EFollowMode::Walk;

	/** Le pion du joueur suivi */
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Leader;

	/** Endormi... (joué chez tous les joueurs) */
	UPROPERTY(Replicated)
	EFollowerPose Pose = EFollowerPose::None;

	/** Modèle sans squelette (SM_<Espèce>), ou sphère tant qu'il n'y a pas de modèle */
	UPROPERTY(VisibleAnywhere, Category="Follower")
	TObjectPtr<UStaticMeshComponent> StaticModel;

	/** Sommeil : bulle au bout du nez qui gonfle et dégonfle */
	UPROPERTY(VisibleAnywhere, Category="Follower")
	TObjectPtr<UStaticMeshComponent> SleepBubble;

	/** Sommeil : les « Z » qui montent au-dessus de sa tête */
	UPROPERTY(VisibleAnywhere, Category="Follower")
	TArray<TObjectPtr<UTextRenderComponent>> SleepLetters;

	/** Étoiles de l'apparition et de la disparition */
	UPROPERTY(VisibleAnywhere, Category="Follower")
	TArray<TObjectPtr<UStaticMeshComponent>> Sparkles;

private:

	void LoadModel();
	void ApplyMovementMode();

	// Serveur : comportement
	enum class EBehavior : uint8 { Follow, Wander, Pause, LookAround };
	void UpdateBehavior(float DeltaSeconds);
	void UpdateFollow(float DeltaSeconds, float DistanceToLeader);
	void UpdateIdle(float DeltaSeconds);
	void ChooseIdleAction();
	void PickFollowSpot();
	/** Avance vers Target à la vitesse donnée en tournant d'abord. Vrai une fois arrivé */
	bool MoveTowards(const FVector& Target, float Speed, float AcceptRadius, float DeltaSeconds);
	void StopMoving();
	bool TurnTowardsYaw(float Yaw, float DeltaSeconds);
	/** Hauteur voulue du centre de la capsule au-dessus de ce point (vol, flotte) */
	FVector ToFlightHeight(const FVector& Spot) const;
	bool IsReachable(const FVector& Target) const;
	bool FindGround(const FVector& Around, float& OutGroundZ) const;
	void RecordTrail();
	FVector GetTrailPoint(float DistanceBack) const;
	FVector GetLeaderFeet() const;
	float GetLeaderYaw() const;
	bool IsFlyer() const { return FollowMode == EFollowMode::Fly || FollowMode == EFollowMode::Float; }

	// Tous les joueurs : animation d'après la vitesse, effets
	void UpdateAnimation();
	void UpdateEffects(float DeltaSeconds);
	/** Pose les pieds sur le sol à chaque image (d'après les os des pieds de l'animation) */
	void UpdateGrounding(float DeltaSeconds);
	void FindFootBones(const USkeletalMesh* Asset);
	/** Serveur : disparition, avec la raison dans le Journal de sortie (Output Log) */
	void TeleportBecause(const TCHAR* Reason);
	void UpdateSleepEffects(float DeltaSeconds, bool bAsleep);
	void SetEyesClosed(bool bClosed);
	void FindEyeMaterials();
	void BurstSparkles(const FVector& Center, int32 Count);
	void UpdateSparkles(float DeltaSeconds);
	void ApplyModelScale(float Factor);
	FVector GetBodyPoint(FName Socket, float HeightRatio) const;
	enum class EAnimState : uint8 { None, Idle, Walk, Run, Happy, Sleep };
	void PlayState(EAnimState State, float PlayRate);

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> HappyAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> SleepAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PlayingAnim;

	EAnimState CurrentAnim = EAnimState::None;
	float HappyTimeLeft = 0.f;

	// Tous les joueurs : effets
	/** Échelle du modèle (taille réelle ou réduite) et sa hauteur affichée */
	float ModelScale = 1.f;
	float ModelHeight = 50.f;
	/** Temps écoulé depuis le début de la disparition / de l'apparition (-1 = rien en cours) */
	float VanishClock = -1.f;
	float AppearClock = -1.f;
	float SleepClock = 0.f;
	float BlinkTimeLeft = 3.f;
	float BlinkCloseLeft = 0.f;
	bool bEyesClosed = false;
	bool bEyeMaterialsSearched = false;
	/** Emplacements des yeux et leur matériau « yeux fermés » (<matériau>_Closed, Tools/Pokemon/setup_follower_assets.py) */
	TArray<int32> ClosedEyeSlots;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> ClosedEyeMaterials;
	struct FSparkle
	{
		FVector Origin = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f;
		float Size = 1.f;
		float Spin = 0.f;
	};
	TArray<FSparkle> SparkleStates;
	int32 NextSparkle = 0;
	float AppliedScaleFactor = -1.f;
	/** Échelle de StaticModel quand le facteur vaut 1 (sphère de remplacement comprise) */
	float StaticBaseScale = 1.f;
	/** Os des pieds et leur hauteur au-dessus du bas du modèle en pose de repos (échelle 1) */
	TArray<FName> FootBones;
	TArray<float> FootRestHeights;
	/** Hauteur du mesh dans la capsule (pieds du modèle au bas de la capsule), et correction de l'animation */
	float BaseMeshZ = 0.f;
	float GroundOffset = 0.f;

	/** Vitesse de marche et de course de cette espèce (animation à vitesse normale) */
	float WalkGaitSpeed = 150.f;
	float RunGaitSpeed = 400.f;

	// Serveur
	EBehavior Behavior = EBehavior::Follow;
	/** Sifflé : reste au pied */
	bool bHeel = false;
	/** Sifflé : il saute de joie en arrivant */
	bool bHeelGreeting = false;
	/** Disparition en cours (secondes restantes avant de réapparaître) */
	float TeleportTimeLeft = -1.f;
	/** Place choisie autour du joueur : angle depuis « juste derrière » et distance */
	float SpotAngle = 0.f;
	float SpotDistance = 180.f;
	float ReplanTimeLeft = 0.f;
	/** Il est en route vers sa place (il ne s'arrête qu'en y arrivant) */
	bool bCatchingUp = false;
	bool bRunning = false;
	float LeaderIdleTime = 0.f;
	float ActionTimeLeft = 0.f;
	FVector WanderTarget = FVector::ZeroVector;
	float WanderHeightOffset = 0.f;
	float LookYaw = 0.f;
	/** Positions des pieds du joueur, pour contourner un mur par son chemin */
	TArray<FVector> Trail;
	float StuckTimer = 0.f;
	/** Bloqué une fois : il passe par le chemin du joueur pendant ce temps avant d'abandonner */
	float ForceTrailTimeLeft = 0.f;
	float BobTime = 0.f;
	/** Rayon du modèle, pour ne pas coller le joueur */
	float BodyRadius = 30.f;
	FName LoadedSpecies;
};
