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
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxFlySpeed = WalkSpeed;
	Move->MaxAcceleration = 1600.f;
	Move->BrakingDecelerationWalking = 1400.f;
	// Sans freinage en vol, il glisserait sans fin
	Move->BrakingDecelerationFlying = 1400.f;
	Move->NavAgentProps.bCanFly = true;
}

void APokemonFollower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APokemonFollower, Species);
	DOREPLIFETIME(APokemonFollower, FollowMode);
	DOREPLIFETIME(APokemonFollower, Leader);
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

	IdleAnim = WalkAnim = RunAnim = HappyAnim = PlayingAnim = nullptr;
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

// ---------------------------------------------------------------- Déplacement (serveur)

void APokemonFollower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		UpdateFollow(DeltaSeconds);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateAnimation(DeltaSeconds);
	}
	HappyTimeLeft = FMath::Max(HappyTimeLeft - DeltaSeconds, 0.f);
}

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

FVector APokemonFollower::GetDesiredLocation() const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Bob = BobAmplitude * FMath::Sin(BobTime * BobFrequency * UE_TWO_PI);

	if (FollowMode == EFollowMode::Fly)
	{
		// À côté du joueur, un peu en arrière, à hauteur d'épaule
		const FVector Feet = GetLeaderFeet();
		const FRotator Yaw(0.f, Leader ? Leader->GetActorRotation().Yaw : GetActorRotation().Yaw, 0.f);
		const FVector Forward = Yaw.Vector();
		const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
		FVector Spot = Feet - Forward * (FlyBackOffset + BodyRadius * 0.5f) + Right * (FlySideOffset + BodyRadius);
		Spot.Z = Feet.Z + FMath::Max(FlyHeight, HalfHeight + 20.f) + Bob;
		float GroundZ = 0.f;
		if (FindGround(Spot, GroundZ))
		{
			Spot.Z = FMath::Max<double>(Spot.Z, GroundZ + HalfHeight + 30.f);
		}
		return Spot;
	}

	// Dans les pas du joueur
	FVector Spot = GetTrailPoint(FollowDistance + BodyRadius + PokemonFollower::LeaderRadius);
	if (FollowMode == EFollowMode::Float)
	{
		float GroundZ = Spot.Z;
		FindGround(Spot, GroundZ);
		Spot.Z = GroundZ + FloatHeight + HalfHeight + Bob;
	}
	return Spot;
}

void APokemonFollower::FaceTowards(const FVector& Target, float DeltaSeconds, float Speed)
{
	const FVector Direction = (Target - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}
	const FRotator Wanted(0.f, Direction.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Wanted, DeltaSeconds, Speed));
}

void APokemonFollower::UpdateFollow(float DeltaSeconds)
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

	// Pendant sa joie (caresse), il reste sur place
	if (HappyTimeLeft > 0.f)
	{
		FaceTowards(LeaderLocation, DeltaSeconds, 6.f);
		return;
	}

	FVector ToTarget = GetDesiredLocation() - Location;
	if (!IsFlyer())
	{
		ToTarget.Z = 0.f;
	}
	const float Distance = ToTarget.Size();
	const float LeaderSpeed = Leader->GetVelocity().Size2D();
	const float ArriveDistance = IsFlyer() ? 15.f : 25.f;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Distance > ArriveDistance)
	{
		// Va à la vitesse du joueur, plus vite s'il est en retard
		const float Speed = FMath::Clamp(LeaderSpeed + (Distance - ArriveDistance) * 2.f, 80.f, RunSpeed);
		Move->MaxWalkSpeed = Speed;
		Move->MaxFlySpeed = Speed;
		AddMovementInput(ToTarget / Distance, 1.f);

		const FVector Velocity = GetVelocity();
		FaceTowards(Location + (Velocity.SizeSquared2D() > FMath::Square(30.f) ? Velocity : ToTarget), DeltaSeconds, 10.f);
	}
	else if (LeaderSpeed < 10.f)
	{
		// Arrivé et le joueur s'est arrêté : il se tourne vers lui
		FaceTowards(LeaderLocation, DeltaSeconds, 3.f);
	}

	// Coincé (mur, falaise, eau) : il réapparaît derrière le joueur
	if (Distance > 300.f && GetVelocity().Size() < 40.f)
	{
		StuckTimer += DeltaSeconds;
		if (StuckTimer > StuckTime)
		{
			TeleportBehindLeader();
		}
	}
	else
	{
		StuckTimer = 0.f;
	}
}

void APokemonFollower::TeleportBehindLeader()
{
	if (!Leader || !HasAuthority())
	{
		return;
	}
	const FVector Feet = GetLeaderFeet();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FRotator Rotation(0.f, Leader->GetActorRotation().Yaw, 0.f);

	Trail.Reset();
	Trail.Add(Feet);
	StuckTimer = 0.f;

	FVector Spot;
	if (FollowMode == EFollowMode::Fly)
	{
		Spot = GetDesiredLocation();
	}
	else
	{
		Spot = Feet - Rotation.Vector() * (FollowDistance + BodyRadius + PokemonFollower::LeaderRadius);
		float GroundZ = Feet.Z;
		FindGround(Spot, GroundZ);
		Spot.Z = GroundZ + HalfHeight + (FollowMode == EFollowMode::Float ? FloatHeight : 2.f);
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

void APokemonFollower::UpdateAnimation(float DeltaSeconds)
{
	if (HappyTimeLeft > 0.f && HappyAnim)
	{
		PlayState(EAnimState::Happy, 1.f);
		return;
	}

	const FVector Velocity = GetVelocity();
	const float Speed = IsFlyer() ? Velocity.Size() : Velocity.Size2D();
	if (Speed < 20.f)
	{
		PlayState(EAnimState::Idle, 1.f);
	}
	else if (Speed > RunAnimationSpeed && RunAnim)
	{
		PlayState(EAnimState::Run, FMath::Clamp(Speed / RunSpeed * 1.2f, 0.7f, 1.5f));
	}
	else
	{
		PlayState(EAnimState::Walk, FMath::Clamp(Speed / WalkSpeed, 0.5f, 1.6f));
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
	Skeletal->SetPlayRate(PlayingAnim == IdleAnim ? 1.f : PlayRate);
}
