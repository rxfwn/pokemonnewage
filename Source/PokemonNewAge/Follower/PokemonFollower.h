#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/Interactable.h"
#include "Pokemon/PokemonTypes.h"
#include "PokemonFollower.generated.h"

class UAnimSequence;
class UStaticMeshComponent;

/**
 *  Le Pokémon de tête de l'équipe, qui suit son dresseur hors combat (SCRUM-78, docs/POKEMON_QUI_SUIT.md).
 *  - Walk : marche dans les pas du joueur (il suit le chemin du joueur, pas la ligne droite).
 *  - Fly : vole à côté du joueur, à hauteur d'épaule, en ondulant un peu.
 *  - Float : flotte juste au-dessus du sol, dans les pas du joueur (Fantominus, Smogo, Magnéti...).
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

	// ------------------------------------------------ Réglages (un Blueprint enfant peut les changer)

	/** Distance derrière le joueur, mesurée sur son chemin (en plus de la taille des deux) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=50, Units="cm"))
	float FollowDistance = 110.f;

	/** Vitesse de marche (le joueur marche à 250 cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=50, Units="cm/s"))
	float WalkSpeed = 270.f;

	/** Vitesse maximale pour rattraper le joueur (il court à 500 cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=100, Units="cm/s"))
	float RunSpeed = 620.f;

	/** Au-dessus de cette vitesse, animation de course (si le modèle en a une) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=50, Units="cm/s"))
	float RunAnimationSpeed = 380.f;

	/** Pokémon qui vole : hauteur du milieu de son corps au-dessus des pieds du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float FlyHeight = 150.f;

	/** Pokémon qui vole : écart sur le côté droit du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float FlySideOffset = 120.f;

	/** Pokémon qui vole : un peu en arrière du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float FlyBackOffset = 50.f;

	/** Pokémon qui flotte : hauteur au-dessus du sol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float FloatHeight = 40.f;

	/** Ondulation en vol (amplitude, en cm) et sa fréquence (par seconde) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol", meta=(Units="cm"))
	float BobAmplitude = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower|Vol")
	float BobFrequency = 0.7f;

	/** Plus loin que ça du joueur : il réapparaît derrière lui */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower", meta=(ClampMin=500, Units="cm"))
	float TeleportDistance = 2500.f;

	/** Bloqué (loin de sa place et presque immobile) pendant ce temps : il réapparaît derrière le joueur */
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

	/** Espèce (ligne de DT_Species) : le modèle se charge chez chaque joueur */
	UPROPERTY(ReplicatedUsing=OnRep_Species)
	FName Species;

	UPROPERTY(ReplicatedUsing=OnRep_Species)
	EFollowMode FollowMode = EFollowMode::Walk;

	/** Le pion du joueur suivi */
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Leader;

	/** Modèle sans squelette (SM_<Espèce>), ou sphère tant qu'il n'y a pas de modèle */
	UPROPERTY(VisibleAnywhere, Category="Follower")
	TObjectPtr<UStaticMeshComponent> StaticModel;

private:

	void LoadModel();
	void ApplyMovementMode();

	/** Serveur : chemin du joueur, direction, vitesse, réapparition */
	void UpdateFollow(float DeltaSeconds);
	void RecordTrail();
	/** Point à DistanceBack derrière le joueur, mesuré le long de son chemin */
	FVector GetTrailPoint(float DistanceBack) const;
	FVector GetDesiredLocation() const;
	FVector GetLeaderFeet() const;
	bool FindGround(const FVector& Around, float& OutGroundZ) const;
	void FaceTowards(const FVector& Target, float DeltaSeconds, float Speed);
	bool IsFlyer() const { return FollowMode == EFollowMode::Fly || FollowMode == EFollowMode::Float; }

	/** Tous les joueurs : animation d'après la vitesse */
	void UpdateAnimation(float DeltaSeconds);

	enum class EAnimState : uint8 { None, Idle, Walk, Run, Happy };
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
	TObjectPtr<UAnimSequence> PlayingAnim;

	EAnimState CurrentAnim = EAnimState::None;
	float HappyTimeLeft = 0.f;

	/** Serveur : positions des pieds du joueur, de la plus ancienne à la plus récente */
	TArray<FVector> Trail;
	float StuckTimer = 0.f;
	float BobTime = 0.f;
	/** Rayon du modèle, pour ne pas coller le joueur */
	float BodyRadius = 30.f;
	FName LoadedSpecies;
};
