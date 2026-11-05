#include "Follower/PokemonFollower.h"
#include "Follower/PokemonFollowerComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace PokemonFollower
{
	/** Modèles importés par Tools/Pokemon/import_pokemon_models.py (docs/MODELES_POKEMON.md) */
	const TCHAR* ModelFolder = TEXT("/Game/Pokemon/Models");
	/** Les modèles regardent +Y : un quart de tour pour regarder +X comme l'acteur */
	const FRotator ModelRotation(0.f, -90.f, 0.f);
	/** Taille de la sphère qui remplace un modèle manquant */
	constexpr float PlaceholderDiameter = 50.f;
	/** Rayon approximatif du joueur, pour ne pas lui marcher dans les talons */
	constexpr float LeaderRadius = 40.f;
	/** Le joueur est arrêté en dessous de cette vitesse (cm/s) */
	constexpr float LeaderStillSpeed = 20.f;
	/** Un point du chemin du joueur tous les... (cm) */
	constexpr float TrailSpacing = 20.f;
	constexpr int32 MaxTrailPoints = 150;
	/** Le joueur a sauté plus loin que ça d'un coup (porte, point de téléportation) : on oublie son chemin */
	constexpr float TrailJump = 500.f;
	/** Capsule de collision : bornes (cm). Le modèle peut dépasser (Onix) : il ne touche rien */
	constexpr float MinRadius = 20.f;
	constexpr float MaxRadius = 60.f;
	constexpr float MinHalfHeight = 30.f;
	constexpr float MaxHalfHeight = 110.f;

	UObject* LoadModelAsset(const FString& Species, const FString& Name)
	{
		return StaticLoadObject(UObject::StaticClass(), nullptr,
			*FString::Printf(TEXT("%s/%s/%s.%s"), ModelFolder, *Species, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

APokemonFollower::APokemonFollower()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	// Pas de contrôleur : le serveur le déplace lui-même (bRunPhysicsWithNoController)
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Ne bloque ni les joueurs (ni les autres Pokémon qui suivent), ni la caméra, ni les traces de visibilité.
	// Garde les murs et le sol. La touche d'action le trouve quand même (recherche par type d'objet).
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(25.f, PokemonFollower::MinHalfHeight);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	USkeletalMeshComponent* Skeletal = GetMesh();
	Skeletal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skeletal->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -PokemonFollower::MinHalfHeight), PokemonFollower::ModelRotation);
	Skeletal->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Skeletal->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	StaticModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticModel"));
	StaticModel->SetupAttachment(Capsule);
	StaticModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticModel->SetRelativeRotation(PokemonFollower::ModelRotation);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bRunPhysicsWithNoController = true;
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = false;
	// Démarrages et arrêts progressifs, comme un animal (pas une glissade instantanée)
	Move->MaxAcceleration = 900.f;
	Move->BrakingDecelerationWalking = 900.f;
	Move->GroundFriction = 6.f;
	// Sans freinage en vol, il glisserait sans fin
	Move->BrakingDecelerationFlying = 700.f;
	Move->NavAgentProps.bCanFly = true;
}

void APokemonFollower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APokemonFollower, Species);
	DOREPLIFETIME(APokemonFollower, FollowMode);
	DOREPLIFETIME(APokemonFollower, Leader);
	DOREPLIFETIME(APokemonFollower, Pose);
}

void APokemonFollower::BeginPlay()
{
	Super::BeginPlay();
	LoadModel();
}

void APokemonFollower::OnRep_Species()
{
	LoadModel();
}

// ---------------------------------------------------------------- Mise en place (serveur)

void APokemonFollower::Setup(APawn* InLeader, FName InSpecies, EFollowMode InMode)
{
	Leader = InLeader;
	Species = InSpecies;
	FollowMode = InMode;
	LoadModel();
	ApplyMovementMode();
	TeleportBehindLeader();
}

void APokemonFollower::SetLeader(APawn* InLeader)
{
	if (Leader != InLeader)
	{
		Leader = InLeader;
		TeleportBehindLeader();
	}
}

void APokemonFollower::ApplyMovementMode()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const EMovementMode Mode = IsFlyer() ? MOVE_Flying : MOVE_Walking;
	Move->DefaultLandMovementMode = Mode;
	Move->SetMovementMode(Mode);
}

void APokemonFollower::LoadModel()
{
	using namespace PokemonFollower;
	if (Species.IsNone() || Species == LoadedSpecies)
	{
		return;
	}
	LoadedSpecies = Species;
	const FString Id = Species.ToString();

	IdleAnim = WalkAnim = RunAnim = HappyAnim = SleepAnim = PlayingAnim = nullptr;
	CurrentAnim = EAnimState::None;

	USkeletalMeshComponent* Skeletal = GetMesh();
	FBoxSphereBounds Bounds(FVector(0.f, 0.f, PlaceholderDiameter * 0.5f), FVector(PlaceholderDiameter * 0.5f), PlaceholderDiameter * 0.5f);
	bool bPlaceholder = false;

	if (USkeletalMesh* SkeletalAsset = Cast<USkeletalMesh>(LoadModelAsset(Id, TEXT("SK_") + Id)))
	{
		Skeletal->SetSkeletalMesh(SkeletalAsset);
		Skeletal->SetVisibility(true);
		StaticModel->SetStaticMesh(nullptr);
		Bounds = SkeletalAsset->GetBounds();
		IdleAnim = Cast<UAnimSequence>(LoadModelAsset(Id, FString::Printf(TEXT("A_%s_Idle"), *Id)));
		WalkAnim = Cast<UAnimSequence>(LoadModelAsset(Id, FString::Printf(TEXT("A_%s_Walk"), *Id)));
		RunAnim = Cast<UAnimSequence>(LoadModelAsset(Id, FString::Printf(TEXT("A_%s_Run"), *Id)));
		HappyAnim = Cast<UAnimSequence>(LoadModelAsset(Id, FString::Printf(TEXT("A_%s_Happy"), *Id)));
		SleepAnim = Cast<UAnimSequence>(LoadModelAsset(Id, FString::Printf(TEXT("A_%s_Sleep"), *Id)));
	}
	else
	{
		Skeletal->SetSkeletalMesh(nullptr);
		Skeletal->SetVisibility(false);
		UStaticMesh* StaticAsset = Cast<UStaticMesh>(LoadModelAsset(Id, TEXT("SM_") + Id));
		if (StaticAsset)
		{
			Bounds = StaticAsset->GetBounds();
		}
		else
		{
			// Pas encore de modèle pour cette espèce : une sphère à sa place
			StaticAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			bPlaceholder = true;
		}
		StaticModel->SetStaticMesh(StaticAsset);
	}

	// Taille : celle du modèle (taille réelle du Pokédex), réduite si MaxDisplayHeight le demande
	const FVector3f Extent(Bounds.BoxExtent);
	const float Height = FMath::Max(Extent.Z * 2.f, 1.f);
	const float Scale = (MaxDisplayHeight > 0.f && Height > MaxDisplayHeight) ? MaxDisplayHeight / Height : 1.f;
	const float Radius = FMath::Clamp(FMath::Min(Extent.X, Extent.Y) * Scale * 0.8f, MinRadius, MaxRadius);
	const float HalfHeight = FMath::Clamp(Extent.Z * Scale, FMath::Max(Radius, MinHalfHeight), MaxHalfHeight);
	BodyRadius = FMath::Min(FMath::Max(Extent.X, Extent.Y) * Scale, 300.f);
	GetCapsuleComponent()->SetCapsuleSize(Radius, HalfHeight);

	// Démarche : un petit Pokémon fait de petits pas, il doit trottiner pour suivre un humain
	const float HeightMeters = Height * Scale / 100.f;
	WalkGaitSpeed = FMath::Clamp(HeightMeters * WalkSpeedPerMeter, static_cast<float>(WalkSpeedRange.X), static_cast<float>(WalkSpeedRange.Y));
	RunGaitSpeed = FMath::Clamp(HeightMeters * RunSpeedPerMeter, static_cast<float>(RunSpeedRange.X), static_cast<float>(RunSpeedRange.Y));
	if (const FVector2D* Override = SpeedOverrides.Find(Species))
	{
		WalkGaitSpeed = FMath::Max(static_cast<float>(Override->X), 30.f);
		RunGaitSpeed = FMath::Max(static_cast<float>(Override->Y), WalkGaitSpeed);
	}
	RunGaitSpeed = FMath::Max(RunGaitSpeed, WalkGaitSpeed * 1.5f);

	// Pieds du modèle (Z = 0 dans le mesh) en bas de la capsule
	const FVector MeshOffset(0.f, 0.f, -HalfHeight);
	Skeletal->SetRelativeScale3D(FVector(Scale));
	Skeletal->SetRelativeLocationAndRotation(MeshOffset, ModelRotation);
	if (bPlaceholder)
	{
		StaticModel->SetRelativeScale3D(FVector(PlaceholderDiameter / 100.f));
		StaticModel->SetRelativeLocation(MeshOffset + FVector(0.f, 0.f, PlaceholderDiameter * 0.5f));
	}
	else
	{
		StaticModel->SetRelativeScale3D(FVector(Scale));
		StaticModel->SetRelativeLocation(MeshOffset);
	}
	// Le lissage réseau des autres joueurs part de cette position du mesh
	CacheInitialMeshOffset(MeshOffset, ModelRotation);
}

// ---------------------------------------------------------------- Boucle

void APokemonFollower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		UpdateBehavior(DeltaSeconds);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateAnimation();
	}
	HappyTimeLeft = FMath::Max(HappyTimeLeft - DeltaSeconds, 0.f);
}

// ---------------------------------------------------------------- Outils (serveur)

FVector APokemonFollower::GetLeaderFeet() const
{
	if (!Leader)
	{
		return GetActorLocation();
	}
	float Radius = 0.f, HalfHeight = 0.f;
	Leader->GetSimpleCollisionCylinder(Radius, HalfHeight);
	return Leader->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
}

float APokemonFollower::GetLeaderYaw() const
{
	return Leader ? Leader->GetActorRotation().Yaw : GetActorRotation().Yaw;
}

void APokemonFollower::RecordTrail()
{
	const FVector Feet = GetLeaderFeet();
	if (Trail.Num() > 0 && FVector::DistSquared(Trail.Last(), Feet) > FMath::Square(PokemonFollower::TrailJump))
	{
		// Le joueur a été déplacé d'un coup : son ancien chemin ne mène plus à lui
		Trail.Reset();
	}
	if (Trail.Num() == 0 || FVector::DistSquared(Trail.Last(), Feet) > FMath::Square(PokemonFollower::TrailSpacing))
	{
		Trail.Add(Feet);
		if (Trail.Num() > PokemonFollower::MaxTrailPoints)
		{
			Trail.RemoveAt(0, Trail.Num() - PokemonFollower::MaxTrailPoints);
		}
	}
}

FVector APokemonFollower::GetTrailPoint(float DistanceBack) const
{
	FVector Previous = GetLeaderFeet();
	float Remaining = DistanceBack;
	for (int32 Index = Trail.Num() - 1; Index >= 0; --Index)
	{
		const float Segment = FVector::Dist(Previous, Trail[Index]);
		if (Segment >= Remaining && Segment > KINDA_SMALL_NUMBER)
		{
			return FMath::Lerp(Previous, Trail[Index], Remaining / Segment);
		}
		Remaining -= Segment;
		Previous = Trail[Index];
	}
	// Chemin trop court (début, téléportation) : tout droit derrière le joueur
	const FVector Back = Leader ? -Leader->GetActorForwardVector().GetSafeNormal2D() : FVector::ZeroVector;
	return Previous + Back * Remaining;
}

bool APokemonFollower::FindGround(const FVector& Around, float& OutGroundZ) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PokemonFollowerGround), false, this);
	Params.AddIgnoredActor(Leader.Get());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Around + FVector(0.f, 0.f, 400.f), Around - FVector(0.f, 0.f, 2000.f), Objects, Params))
	{
		OutGroundZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

bool APokemonFollower::IsReachable(const FVector& Target) const
{
	// Ligne droite libre (pas de mur ni de rocher entre lui et le point), à mi-hauteur de son corps
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PokemonFollowerPath), false, this);
	Params.AddIgnoredActor(Leader.Get());
	FVector To = Target;
	if (!IsFlyer())
	{
		float GroundZ = Target.Z;
		FindGround(Target, GroundZ);
		To.Z = GroundZ + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return !GetWorld()->LineTraceTestByObjectType(GetActorLocation(), To, Objects, Params);
}

FVector APokemonFollower::ToFlightHeight(const FVector& Spot) const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Bob = BobAmplitude * FMath::Sin(BobTime * BobFrequency * UE_TWO_PI);
	FVector Result = Spot;
	float GroundZ = Spot.Z;
	const bool bGround = FindGround(Spot, GroundZ);
	if (FollowMode == EFollowMode::Float)
	{
		Result.Z = GroundZ + FloatHeight + HalfHeight + Bob;
	}
	else
	{
		Result.Z = GetLeaderFeet().Z + FMath::Max(FlyHeight, HalfHeight + 20.f) + Bob;
		if (bGround)
		{
			Result.Z = FMath::Max<double>(Result.Z, GroundZ + HalfHeight + 30.f);
		}
	}
	return Result;
}

bool APokemonFollower::TurnTowardsYaw(float Yaw, float DeltaSeconds)
{
	const float Current = GetActorRotation().Yaw;
	const float NewYaw = FMath::FixedTurn(Current, Yaw, TurnRate * DeltaSeconds);
	SetActorRotation(FRotator(0.f, NewYaw, 0.f));
	return FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, Yaw)) < 5.f;
}

bool APokemonFollower::MoveTowards(const FVector& Target, float Speed, float AcceptRadius, float DeltaSeconds)
{
	FVector ToTarget = Target - GetActorLocation();
	if (!IsFlyer())
	{
		ToTarget.Z = 0.f;
	}
	const float Distance = ToTarget.Size();
	if (Distance < AcceptRadius)
	{
		StuckTimer = 0.f;
		return true;
	}

	// Il tourne d'abord, puis avance droit devant lui : des courbes, pas de glissade de côté
	const FVector Direction2D = ToTarget.GetSafeNormal2D();
	if (!Direction2D.IsNearlyZero())
	{
		TurnTowardsYaw(Direction2D.Rotation().Yaw, DeltaSeconds);
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float Facing = Direction2D.IsNearlyZero() ? 1.f : FVector::DotProduct(Forward, Direction2D);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = Speed;
	Move->MaxFlySpeed = Speed;
	float Push = 0.f;
	if (IsFlyer())
	{
		// En l'air il peut se décaler un peu de côté, et monter ou descendre
		Push = FMath::Clamp((Facing + 0.5f) / 1.5f, 0.2f, 1.f);
		AddMovementInput(ToTarget / Distance, Push);
	}
	else
	{
		Push = FMath::Clamp((Facing - 0.3f) / 0.7f, 0.f, 1.f);
		if (Push > 0.f)
		{
			AddMovementInput(Forward, Push);
		}
	}

	// Il veut avancer mais ne bouge pas : coincé
	if (Push > 0.5f && GetVelocity().Size() < 20.f)
	{
		StuckTimer += DeltaSeconds;
	}
	else
	{
		StuckTimer = 0.f;
	}
	return false;
}

void APokemonFollower::StopMoving()
{
	// Pas d'entrée : le mouvement freine tout seul (BrakingDeceleration)
	StuckTimer = 0.f;
}

void APokemonFollower::PickFollowSpot()
{
	if (FollowMode == EFollowMode::Fly)
	{
		// Un volant reste plutôt sur un côté du joueur
		const float Side = FMath::RandBool() ? 1.f : -1.f;
		SpotAngle = Side * FMath::FRandRange(55.f, 120.f);
	}
	else
	{
		SpotAngle = FMath::FRandRange(-SideAngle, SideAngle);
	}
	SpotDistance = FMath::FRandRange(MinDistance, FMath::Max(MinDistance, ComfortDistance * 0.85f));
	ReplanTimeLeft = FMath::FRandRange(ReplanInterval.X, ReplanInterval.Y);
}

// ---------------------------------------------------------------- Comportement (serveur)

void APokemonFollower::UpdateBehavior(float DeltaSeconds)
{
	if (!Leader)
	{
		return;
	}
	RecordTrail();
	BobTime += DeltaSeconds;

	const FVector Location = GetActorLocation();
	const FVector LeaderLocation = Leader->GetActorLocation();
	if (FVector::DistSquared(Location, LeaderLocation) > FMath::Square(TeleportDistance)
		|| (!IsFlyer() && FMath::Abs(Location.Z - LeaderLocation.Z) > 800.f))
	{
		TeleportBehindLeader();
		return;
	}

	// Pendant sa joie (caresse), il reste sur place, tourné vers le joueur
	if (HappyTimeLeft > 0.f)
	{
		StopMoving();
		TurnTowardsYaw((LeaderLocation - Location).GetSafeNormal2D().Rotation().Yaw, DeltaSeconds);
		return;
	}

	const float LeaderSpeed = Leader->GetVelocity().Size2D();
	LeaderIdleTime = LeaderSpeed > PokemonFollower::LeaderStillSpeed ? 0.f : LeaderIdleTime + DeltaSeconds;
	const float Distance = FVector::Dist2D(Location, LeaderLocation);

	if (LeaderIdleTime <= 0.f || Distance > MaxDistance + BodyRadius)
	{
		// Le joueur repart (ou il est trop loin) : il se réveille et le suit
		Pose = EFollowerPose::None;
		if (Behavior != EBehavior::Follow)
		{
			Behavior = EBehavior::Follow;
			PickFollowSpot();
		}
		UpdateFollow(DeltaSeconds, Distance);
	}
	else if (LeaderIdleTime < IdleDelay && Behavior == EBehavior::Follow)
	{
		// Le joueur vient de s'arrêter : il finit d'arriver à sa place
		UpdateFollow(DeltaSeconds, Distance);
	}
	else
	{
		UpdateIdle(DeltaSeconds);
	}
}

void APokemonFollower::UpdateFollow(float DeltaSeconds, float DistanceToLeader)
{
	ReplanTimeLeft -= DeltaSeconds;
	if (ReplanTimeLeft <= 0.f)
	{
		PickFollowSpot();
	}

	const float LeaderSpeed = Leader->GetVelocity().Size2D();
	const bool bLeaderMoving = LeaderSpeed > PokemonFollower::LeaderStillSpeed;
	const float Reach = BodyRadius + PokemonFollower::LeaderRadius;

	// Il ne repart que quand le joueur sort de sa zone de confort (ou s'il est trop collé à lui)
	if (!bCatchingUp && (DistanceToLeader > ComfortDistance + Reach || DistanceToLeader < MinDistance * 0.6f + Reach))
	{
		bCatchingUp = true;
	}
	if (!bCatchingUp)
	{
		StopMoving();
		// Il regarde le joueur, sans tourner la tête au moindre pas
		const float ToLeaderYaw = (Leader->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, ToLeaderYaw)) > 45.f)
		{
			TurnTowardsYaw(ToLeaderYaw, DeltaSeconds);
		}
		return;
	}

	// Sa place autour du joueur
	const FRotator SpotRotation(0.f, GetLeaderYaw() + 180.f + SpotAngle, 0.f);
	FVector Spot = GetLeaderFeet() + SpotRotation.Vector() * (SpotDistance + Reach);
	if (!IsReachable(IsFlyer() ? ToFlightHeight(Spot) : Spot))
	{
		// Mur ou rocher sur la route : il passe par où le joueur est passé
		Spot = GetTrailPoint(SpotDistance + Reach);
	}
	if (IsFlyer())
	{
		Spot = ToFlightHeight(Spot);
	}

	// Vitesse : celle du joueur, plus son retard ; il court au-delà de MaxDistance
	FVector ToSpot = Spot - GetActorLocation();
	if (!IsFlyer())
	{
		ToSpot.Z = 0.f;
	}
	const float SpotDistanceNow = ToSpot.Size();
	if (DistanceToLeader > MaxDistance + Reach)
	{
		bRunning = true;
	}
	else if (DistanceToLeader < ComfortDistance + Reach)
	{
		bRunning = false;
	}
	const float MinSpeed = WalkGaitSpeed * static_cast<float>(PlayRateRange.X);
	const float MaxSpeed = RunGaitSpeed * static_cast<float>(PlayRateRange.Y);
	float Speed = FMath::Clamp(LeaderSpeed + SpotDistanceNow * 1.5f, MinSpeed, MaxSpeed);
	if (bRunning)
	{
		Speed = FMath::Clamp(FMath::Max(Speed, RunGaitSpeed), RunGaitSpeed, MaxSpeed);
	}

	const bool bArrived = MoveTowards(Spot, Speed, 35.f, DeltaSeconds);
	if (bArrived && !bLeaderMoving)
	{
		// Arrivé et le joueur est arrêté : il se pose
		bCatchingUp = false;
		bRunning = false;
	}

	if (StuckTimer > StuckTime)
	{
		StuckTimer = 0.f;
		if (DistanceToLeader > ComfortDistance + Reach)
		{
			TeleportBehindLeader();
		}
		else
		{
			// Assez près quand même : il renonce à cette place
			bCatchingUp = false;
			PickFollowSpot();
		}
	}
}

void APokemonFollower::UpdateIdle(float DeltaSeconds)
{
	switch (Behavior)
	{
	case EBehavior::Follow:
		// Il vient de s'arrêter : une petite pause avant de faire sa vie
		bCatchingUp = false;
		bRunning = false;
		Behavior = EBehavior::Pause;
		ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y) * 0.5f;
		break;

	case EBehavior::Pause:
		StopMoving();
		if (FollowMode == EFollowMode::Walk && SleepAnim && SleepAfter > 0.f && LeaderIdleTime > SleepAfter)
		{
			Pose = EFollowerPose::Sleep;
		}
		if (Pose == EFollowerPose::Sleep)
		{
			return;
		}
		ActionTimeLeft -= DeltaSeconds;
		if (ActionTimeLeft <= 0.f)
		{
			ChooseIdleAction();
		}
		break;

	case EBehavior::LookAround:
		StopMoving();
		if (TurnTowardsYaw(LookYaw, DeltaSeconds * 0.5f))
		{
			ActionTimeLeft -= DeltaSeconds;
			if (ActionTimeLeft <= 0.f)
			{
				Behavior = EBehavior::Pause;
				ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
			}
		}
		break;

	case EBehavior::Wander:
	{
		const FVector Target = IsFlyer() ? ToFlightHeight(WanderTarget) + FVector(0.f, 0.f, WanderHeightOffset) : WanderTarget;
		const float Speed = bRunning ? RunGaitSpeed : WalkGaitSpeed;
		if (MoveTowards(Target, Speed, 35.f, DeltaSeconds) || StuckTimer > StuckTime)
		{
			StuckTimer = 0.f;
			bRunning = false;
			Behavior = EBehavior::Pause;
			ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
		}
		break;
	}
	}
}

void APokemonFollower::ChooseIdleAction()
{
	const FVector Feet = GetLeaderFeet();
	const FVector Location = GetActorLocation();
	const float Distance = FVector::Dist2D(Location, Feet);
	const float Roll = FMath::FRand();
	const float Reach = BodyRadius + PokemonFollower::LeaderRadius;

	// Une balade : vers un point au hasard autour du joueur (ou retour près de lui s'il s'est éloigné)
	const bool bGoBack = Distance > ComfortDistance + Reach && Roll < 0.6f;
	if (bGoBack || Roll < 0.45f)
	{
		for (int32 Attempt = 0; Attempt < 6; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.f, 360.f);
			const float Radius = bGoBack ? FMath::FRandRange(MinDistance, ComfortDistance) : FMath::FRandRange(MinDistance, WanderRadius);
			FVector Candidate = Feet + FRotator(0.f, Angle, 0.f).Vector() * (Radius + Reach);
			float GroundZ = Candidate.Z;
			if (!FindGround(Candidate, GroundZ))
			{
				continue;
			}
			// Pas en bas d'une falaise ni en haut d'un talus
			if (!IsFlyer() && FMath::Abs(GroundZ - Feet.Z) > 120.f)
			{
				continue;
			}
			Candidate.Z = GroundZ;
			if (!IsReachable(IsFlyer() ? ToFlightHeight(Candidate) : Candidate))
			{
				continue;
			}
			WanderTarget = Candidate;
			// Un volant monte ou descend un peu en se baladant
			WanderHeightOffset = IsFlyer() ? FMath::FRandRange(-40.f, 60.f) : 0.f;
			bRunning = !bGoBack && FMath::FRand() < 0.15f;
			Behavior = EBehavior::Wander;
			return;
		}
	}

	if (Roll < 0.85f)
	{
		// Regarde autour de lui, ou se tourne vers le joueur
		const float ToLeaderYaw = (Feet - Location).GetSafeNormal2D().Rotation().Yaw;
		LookYaw = Roll < 0.7f
			? GetActorRotation().Yaw + (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(50.f, 150.f)
			: ToLeaderYaw;
		ActionTimeLeft = FMath::FRandRange(1.f, 3.f);
		Behavior = EBehavior::LookAround;
		return;
	}

	// Un moment de joie
	if (HappyAnim)
	{
		MulticastPlayHappy();
	}
	Behavior = EBehavior::Pause;
	ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
}

void APokemonFollower::TeleportBehindLeader()
{
	if (!Leader || !HasAuthority())
	{
		return;
	}
	const FVector Feet = GetLeaderFeet();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FRotator Rotation(0.f, GetLeaderYaw(), 0.f);

	Trail.Reset();
	Trail.Add(Feet);
	StuckTimer = 0.f;
	bCatchingUp = false;
	bRunning = false;
	Behavior = EBehavior::Follow;
	Pose = EFollowerPose::None;
	PickFollowSpot();

	FVector Spot = Feet - Rotation.Vector() * (SpotDistance + BodyRadius + PokemonFollower::LeaderRadius);
	if (IsFlyer())
	{
		Spot = ToFlightHeight(Spot);
	}
	else
	{
		float GroundZ = Feet.Z;
		FindGround(Spot, GroundZ);
		Spot.Z = GroundZ + HalfHeight + 2.f;
	}

	if (!TeleportTo(Spot, Rotation, false, false))
	{
		// Pas de place derrière (mur) : aux pieds du joueur, qu'il traverse sans le gêner
		TeleportTo(FVector(Feet.X, Feet.Y, Feet.Z + HalfHeight + 2.f), Rotation, false, true);
	}
	GetCharacterMovement()->StopMovementImmediately();
}

// ---------------------------------------------------------------- Caresse

void APokemonFollower::Interact_Implementation(APawn* InteractingPawn)
{
	if (!InteractingPawn)
	{
		return;
	}
	if (HasAuthority())
	{
		Pet(InteractingPawn);
	}
	else if (const APlayerController* Player = Cast<APlayerController>(InteractingPawn->GetController()))
	{
		if (UPokemonFollowerComponent* Component = Player->FindComponentByClass<UPokemonFollowerComponent>())
		{
			Component->PetFollower(this);
		}
	}
}

void APokemonFollower::Pet(APawn* ByPawn)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ByPawn)
	{
		const FVector Direction = (ByPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!Direction.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
		}
	}
	// Réveillé par la caresse
	Pose = EFollowerPose::None;
	LeaderIdleTime = 0.f;
	Behavior = EBehavior::Pause;
	ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
	GetCharacterMovement()->StopMovementImmediately();
	MulticastPlayHappy();
}

void APokemonFollower::MulticastPlayHappy_Implementation()
{
	HappyTimeLeft = HappyAnim ? HappyAnim->GetPlayLength() : 0.8f;
	CurrentAnim = EAnimState::None;
	if (!HappyAnim && HasAuthority() && FollowMode == EFollowMode::Walk)
	{
		// Pas d'animation de joie : un petit saut
		LaunchCharacter(FVector(0.f, 0.f, 320.f), false, true);
	}
}

// ---------------------------------------------------------------- Animation (tous les joueurs)

void APokemonFollower::UpdateAnimation()
{
	if (Pose == EFollowerPose::Sleep && SleepAnim)
	{
		PlayState(EAnimState::Sleep, 1.f);
		return;
	}
	if (HappyTimeLeft > 0.f && HappyAnim)
	{
		PlayState(EAnimState::Happy, 1.f);
		return;
	}

	const FVector Velocity = GetVelocity();
	const float Speed = IsFlyer() ? Velocity.Size() : Velocity.Size2D();
	const float MinRate = IsFlyer() ? FMath::Max(static_cast<float>(PlayRateRange.X), 0.8f) : static_cast<float>(PlayRateRange.X);
	const float MaxRate = IsFlyer() ? FMath::Min(static_cast<float>(PlayRateRange.Y), 1.4f) : static_cast<float>(PlayRateRange.Y);

	// Démarche choisie d'après la vitesse réelle, et jouée au rythme du sol (les pattes ne glissent pas)
	const float RunThreshold = FMath::Sqrt(WalkGaitSpeed * RunGaitSpeed);
	if (Speed < 15.f)
	{
		PlayState(EAnimState::Idle, 1.f);
	}
	else if (Speed > RunThreshold && RunAnim)
	{
		PlayState(EAnimState::Run, FMath::Clamp(Speed / RunGaitSpeed, MinRate, MaxRate));
	}
	else
	{
		PlayState(EAnimState::Walk, FMath::Clamp(Speed / WalkGaitSpeed, MinRate, MaxRate));
	}
}

void APokemonFollower::PlayState(EAnimState State, float PlayRate)
{
	USkeletalMeshComponent* Skeletal = GetMesh();
	if (!Skeletal->GetSkeletalMeshAsset())
	{
		return;
	}

	if (State != CurrentAnim)
	{
		CurrentAnim = State;
		UAnimSequence* Anim = nullptr;
		switch (State)
		{
		case EAnimState::Happy: Anim = HappyAnim; break;
		case EAnimState::Sleep: Anim = SleepAnim; break;
		case EAnimState::Run: Anim = RunAnim ? RunAnim : WalkAnim; break;
		case EAnimState::Walk: Anim = WalkAnim; break;
		default: break;
		}
		if (!Anim)
		{
			Anim = IdleAnim;
		}
		if (Anim && (Anim != PlayingAnim || State == EAnimState::Happy))
		{
			Skeletal->PlayAnimation(Anim, State != EAnimState::Happy);
			PlayingAnim = Anim;
		}
	}
	Skeletal->SetPlayRate(PlayingAnim == IdleAnim || PlayingAnim == SleepAnim ? 1.f : PlayRate);
}
