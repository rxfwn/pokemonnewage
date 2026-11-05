#include "Follower/PokemonFollower.h"
#include "Follower/PokemonFollowerComponent.h"
#include "Follower/PokemonFollowerAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogPokemonFollower, Log, All);

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
	/** Bloqué : il passe par le chemin du joueur pendant ce temps avant de réapparaître (s) */
	constexpr float TrailRetryTime = 4.f;
	/** Capsule de collision : bornes (cm). Le modèle peut dépasser (Onix) : il ne touche rien */
	constexpr float MinRadius = 20.f;
	constexpr float MaxRadius = 60.f;
	constexpr float MinHalfHeight = 30.f;
	constexpr float MaxHalfHeight = 110.f;
	/** Matériaux créés par Tools/Pokemon/setup_follower_assets.py (sinon matériaux de base d'Unreal) */
	const TCHAR* BubbleMaterial = TEXT("/Game/Pokemon/Follower/M_FollowerBubble.M_FollowerBubble");
	const TCHAR* SparkleMaterial = TEXT("/Game/Pokemon/Follower/M_FollowerSparkle.M_FollowerSparkle");
	constexpr int32 NumSparkles = 20;
	constexpr int32 NumSleepLetters = 3;
	/** Un « Z » met ce temps pour monter */
	constexpr float SleepLetterCycle = 2.4f;
	/** Durée d'un clignement d'yeux */
	constexpr float BlinkDuration = 0.13f;

	/** Composant d'effet : ne touche rien, placé en coordonnées du monde */
	void SetupEffectComponent(USceneComponent* Component, USceneComponent* Parent)
	{
		Component->SetupAttachment(Parent);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->SetHiddenInGame(false);
		Component->SetVisibility(false);
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Primitive->SetCastShadow(false);
		}
	}

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
	// Oiseaux de Kanto qui se posent au sol quand le joueur s'arrête
	for (const TCHAR* Bird : { TEXT("Pidgey"), TEXT("Pidgeotto"), TEXT("Pidgeot"), TEXT("Spearow"), TEXT("Fearow"),
		TEXT("Farfetchd"), TEXT("Articuno"), TEXT("Zapdos"), TEXT("Moltres"), TEXT("Charizard"), TEXT("Aerodactyl"), TEXT("Dragonite") })
	{
		LandingSpecies.Add(FName(Bird));
	}
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
	// Animations mélangées avec fondus (PokemonFollowerAnimInstance), sans Animation Blueprint
	Skeletal->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Skeletal->SetAnimInstanceClass(UPokemonFollowerAnimInstance::StaticClass());
	Skeletal->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	StaticModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticModel"));
	StaticModel->SetupAttachment(Capsule);
	StaticModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticModel->SetRelativeRotation(PokemonFollower::ModelRotation);

	// Effets (seulement affichés chez les joueurs) : bulle et « Z » du sommeil, étoiles
	SleepBubble = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SleepBubble"));
	PokemonFollower::SetupEffectComponent(SleepBubble, Capsule);
	for (int32 Index = 0; Index < PokemonFollower::NumSleepLetters; ++Index)
	{
		UTextRenderComponent* Letter = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("SleepLetter%d"), Index));
		PokemonFollower::SetupEffectComponent(Letter, Capsule);
		Letter->SetText(FText::FromString(TEXT("Z")));
		Letter->SetHorizontalAlignment(EHTA_Center);
		Letter->SetVerticalAlignment(EVRTA_TextCenter);
		Letter->SetTextRenderColor(FColor(225, 238, 255));
		SleepLetters.Add(Letter);
	}
	for (int32 Index = 0; Index < PokemonFollower::NumSparkles; ++Index)
	{
		UStaticMeshComponent* Sparkle = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Sparkle%d"), Index));
		PokemonFollower::SetupEffectComponent(Sparkle, Capsule);
		Sparkles.Add(Sparkle);
	}

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

	// Formes et matériaux des effets
	if (GetNetMode() != NM_DedicatedServer)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		SleepBubble->SetStaticMesh(Sphere);
		if (UMaterialInterface* Bubble = LoadObject<UMaterialInterface>(nullptr, PokemonFollower::BubbleMaterial, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			SleepBubble->SetMaterial(0, Bubble);
		}
		UMaterialInterface* Star = LoadObject<UMaterialInterface>(nullptr, PokemonFollower::SparkleMaterial, nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (UStaticMeshComponent* Sparkle : Sparkles)
		{
			Sparkle->SetStaticMesh(Cube);
			if (Star)
			{
				Sparkle->SetMaterial(0, Star);
			}
		}
		SparkleStates.SetNum(Sparkles.Num());
		BlinkTimeLeft = FMath::FRandRange(BlinkInterval.X, BlinkInterval.Y);
	}
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
		MulticastAppear(GetActorLocation());
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
	const bool bFirstModel = LoadedSpecies.IsNone();
	LoadedSpecies = Species;
	const FString Id = Species.ToString();
	ClosedEyeSlots.Reset();
	ClosedEyeMaterials.Reset();
	bEyeMaterialsSearched = false;
	bEyesClosed = false;

	IdleAnim = WalkAnim = RunAnim = HappyAnim = SleepAnim = LandedAnim = nullptr;
	MeasuredWalkSpeed = MeasuredRunSpeed = 0.f;
	PreviousFeetLocal.Reset();
	PreviousLowestFoot = INDEX_NONE;

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
		if (LandingSpecies.Contains(Species) && !LandedAnimName.IsNone())
		{
			LandedAnim = Cast<UAnimSequence>(LoadModelAsset(Id, FString::Printf(TEXT("A_%s_%s"), *Id, *LandedAnimName.ToString())));
		}
		// Marche, course, attente : le corps reste sur place dans l'animation (c'est le code qui le déplace).
		// Sinon, à chaque boucle de l'animation, le modèle revient d'un coup en arrière.
		for (UAnimSequence* Anim : { IdleAnim.Get(), WalkAnim.Get(), RunAnim.Get() })
		{
			if (Anim)
			{
				Anim->bForceRootLock = true;
			}
		}
		FindFootBones(SkeletalAsset, static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z), static_cast<float>(Bounds.BoxExtent.Z * 2.0));
	}
	else
	{
		FootBones.Reset();
		FootRestHeights.Reset();
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
	float Scale = FMath::Max(SizeMultiplier, 0.1f);
	if (MaxDisplayHeight > 0.f && Height * Scale > MaxDisplayHeight)
	{
		Scale = MaxDisplayHeight / Height;
	}
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

	// Pieds du modèle (le bas de sa boîte, normalement Z = 0) posés en bas de la capsule
	const float FeetZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
	const FVector MeshOffset(0.f, 0.f, -HalfHeight - (bPlaceholder ? 0.f : FeetZ));
	ModelScale = Scale;
	ModelHeight = Height * Scale;
	BaseMeshZ = MeshOffset.Z;
	GroundOffset = 0.f;
	StaticBaseScale = bPlaceholder ? PlaceholderDiameter / 100.f : Scale;
	Skeletal->SetRelativeLocationAndRotation(MeshOffset, ModelRotation);
	StaticModel->SetRelativeLocation(bPlaceholder ? MeshOffset + FVector(0.f, 0.f, PlaceholderDiameter * 0.5f) : MeshOffset);
	AppliedScaleFactor = -1.f;
	ApplyModelScale(1.f);
	if (bFirstModel && GetNetMode() != NM_DedicatedServer)
	{
		// Il sort de sa Ball : il apparaît dans des étoiles
		AppearClock = 0.f;
		BurstSparkles(GetActorLocation(), 10);
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
		UpdateAnimation(DeltaSeconds);
		UpdateEffects(DeltaSeconds);
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
	// Canal Visibilité : le sol et les murs, pas les volumes invisibles (hautes herbes, intérieurs, dialogues)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PokemonFollowerGround), false, this);
	Params.AddIgnoredActor(Leader.Get());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Around + FVector(0.f, 0.f, 400.f), Around - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		OutGroundZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

bool APokemonFollower::IsReachable(const FVector& Target) const
{
	// Ligne droite libre (pas de mur ni de rocher entre lui et le point), à mi-hauteur de son corps
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PokemonFollowerPath), false, this);
	Params.AddIgnoredActor(Leader.Get());
	FVector To = Target;
	if (!IsFlyer())
	{
		float GroundZ = Target.Z;
		FindGround(Target, GroundZ);
		To.Z = GroundZ + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return !GetWorld()->LineTraceTestByChannel(GetActorLocation(), To, ECC_Visibility, Params);
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

	// Disparition en cours : il réapparaît à côté du joueur à la fin
	if (TeleportTimeLeft >= 0.f)
	{
		TeleportTimeLeft -= DeltaSeconds;
		if (TeleportTimeLeft < 0.f)
		{
			TeleportBehindLeader();
			MulticastAppear(GetActorLocation());
		}
		return;
	}

	const FVector Location = GetActorLocation();
	const FVector LeaderLocation = Leader->GetActorLocation();
	if (FVector::DistSquared(Location, LeaderLocation) > FMath::Square(TeleportDistance)
		|| (!IsFlyer() && FMath::Abs(Location.Z - LeaderLocation.Z) > 800.f))
	{
		TeleportBecause(FVector::DistSquared(Location, LeaderLocation) > FMath::Square(TeleportDistance)
			? TEXT("trop loin du joueur") : TEXT("trop au-dessus ou en dessous du joueur (chute ?)"));
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

	if (bHeel || LeaderIdleTime <= 0.f || Distance > MaxDistance + BodyRadius)
	{
		// Au pied, ou le joueur repart (ou il est trop loin) : il se réveille et le suit
		WakeUp();
		if (WakeTimeLeft > 0.f)
		{
			// Il se relève d'abord
			WakeTimeLeft -= DeltaSeconds;
			StopMoving();
			return;
		}
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
	GetCharacterMovement()->bCanWalkOffLedges = true;
	if (!bHeel)
	{
		ReplanTimeLeft -= DeltaSeconds;
		if (ReplanTimeLeft <= 0.f)
		{
			PickFollowSpot();
		}
	}

	const float LeaderSpeed = Leader->GetVelocity().Size2D();
	const bool bLeaderMoving = LeaderSpeed > PokemonFollower::LeaderStillSpeed;
	const float Reach = BodyRadius + PokemonFollower::LeaderRadius;
	// Au pied : place fixe et serrée à côté du joueur
	const float Comfort = bHeel ? HeelDistance + 50.f : ComfortDistance;
	const float TooClose = bHeel ? 0.f : MinDistance * 0.6f;
	const float WantedDistance = bHeel ? HeelDistance : SpotDistance;
	const float WantedAngle = bHeel ? HeelAngle : SpotAngle;

	// Il ne repart que quand le joueur sort de sa zone de confort (ou s'il est trop collé à lui)
	if (!bCatchingUp && (DistanceToLeader > Comfort + Reach || DistanceToLeader < TooClose + Reach))
	{
		bCatchingUp = true;
	}
	if (!bCatchingUp)
	{
		StopMoving();
		// Au pied il regarde dans le même sens que le joueur ; sinon il regarde le joueur, sans tourner la tête au moindre pas
		const float ToLeaderYaw = (Leader->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;
		const float WantedYaw = bHeel ? GetLeaderYaw() : ToLeaderYaw;
		if (bHeel || FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, WantedYaw)) > 45.f)
		{
			TurnTowardsYaw(WantedYaw, DeltaSeconds);
		}
		return;
	}

	// Sa place autour du joueur
	const FRotator SpotRotation(0.f, GetLeaderYaw() + 180.f + WantedAngle, 0.f);
	FVector Spot = GetLeaderFeet() + SpotRotation.Vector() * (WantedDistance + Reach);
	ForceTrailTimeLeft = FMath::Max(ForceTrailTimeLeft - DeltaSeconds, 0.f);
	if (ForceTrailTimeLeft > 0.f || !IsReachable(IsFlyer() ? ToFlightHeight(Spot) : Spot))
	{
		// Mur ou rocher sur la route : il passe par où le joueur est passé
		Spot = GetTrailPoint(WantedDistance + Reach);
	}
	if (IsFlyer())
	{
		Spot = ToFlightHeight(Spot);
	}

	// Vitesse : celle du joueur, plus son retard ; il court au-delà de MaxDistance (ou quand on le siffle)
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
	else if (DistanceToLeader < Comfort + Reach)
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

	const bool bArrived = MoveTowards(Spot, Speed, bHeel ? 25.f : 35.f, DeltaSeconds);
	if (bArrived && !bLeaderMoving)
	{
		// Arrivé et le joueur est arrêté : il se pose
		bCatchingUp = false;
		bRunning = false;
		if (bHeelGreeting)
		{
			// Sifflé : content d'arriver au pied
			bHeelGreeting = false;
			MulticastPlayHappy();
		}
	}

	if (StuckTimer > StuckTime)
	{
		StuckTimer = 0.f;
		if (ForceTrailTimeLeft <= 0.f)
		{
			// 1re fois : il essaie le chemin exact du joueur (là où lui est passé, ça passe)
			ForceTrailTimeLeft = PokemonFollower::TrailRetryTime;
		}
		else if (DistanceToLeader > MaxDistance + Reach || bHeel)
		{
			// Même par le chemin du joueur ça ne passe pas : il réapparaît à côté de lui
			ForceTrailTimeLeft = 0.f;
			TeleportBecause(TEXT("bloqué loin du joueur, même par son chemin"));
		}
		else
		{
			// Assez près quand même : il renonce à cette place et en choisit une autre
			ForceTrailTimeLeft = 0.f;
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
		if (CanLand() && Pose == EFollowerPose::None && LandAfter > 0.f && LeaderIdleTime > LandAfter)
		{
			// Oiseau : le joueur ne bouge plus, il descend se poser
			Behavior = EBehavior::Land;
			return;
		}
		if ((FollowMode == EFollowMode::Walk || Pose == EFollowerPose::Perched) && SleepAnim && SleepAfter > 0.f && LeaderIdleTime > SleepAfter)
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

	case EBehavior::Land:
	{
		// Descente en douceur jusqu'au sol, puis il se pose (il marche au sol, plus en vol)
		float GroundZ = 0.f;
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		UCharacterMovementComponent* Move = GetCharacterMovement();
		if (!FindGround(GetActorLocation(), GroundZ))
		{
			Behavior = EBehavior::Pause;
			ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
			break;
		}
		const float Height = static_cast<float>(GetActorLocation().Z) - (GroundZ + HalfHeight);
		if (Height < 10.f)
		{
			Pose = EFollowerPose::Perched;
			Move->SetMovementMode(MOVE_Walking);
			Behavior = EBehavior::Pause;
			ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
			break;
		}
		Move->MaxFlySpeed = FMath::Clamp(Height * 2.f, 40.f, 150.f);
		AddMovementInput(FVector::DownVector, 1.f);
		break;
	}

	case EBehavior::Wander:
	{
		// En balade, il ne saute pas d'un muret ou d'une falaise (il n'arriverait plus à remonter)
		GetCharacterMovement()->bCanWalkOffLedges = false;
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
	float Roll = FMath::FRand();
	const float Reach = BodyRadius + PokemonFollower::LeaderRadius;
	if (Pose == EFollowerPose::Perched)
	{
		// Posé, il ne se balade pas (ses animations de marche battent des ailes) : il regarde autour de lui
		Roll = FMath::FRandRange(0.45f, 0.85f);
	}

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
	ForceTrailTimeLeft = 0.f;
	WakeTimeLeft = 0.f;
	bCatchingUp = false;
	bRunning = false;
	Behavior = EBehavior::Follow;
	Pose = EFollowerPose::None;
	ApplyMovementMode();
	PickFollowSpot();
	TeleportTimeLeft = -1.f;

	// À côté du joueur : au pied s'il a été sifflé, sinon d'un côté ou de l'autre, tout près
	const float Angle = bHeel ? HeelAngle : (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(70.f, 120.f);
	const float Distance = (bHeel ? HeelDistance : MinDistance) + BodyRadius + PokemonFollower::LeaderRadius;
	FVector Spot = Feet + FRotator(0.f, Rotation.Yaw + 180.f + Angle, 0.f).Vector() * Distance;
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
		// Pas de place à côté (mur) : aux pieds du joueur, qu'il traverse sans le gêner
		TeleportTo(FVector(Feet.X, Feet.Y, Feet.Z + HalfHeight + 2.f), Rotation, false, true);
	}
	GetCharacterMovement()->StopMovementImmediately();
}

void APokemonFollower::TeleportBecause(const TCHAR* Reason)
{
	if (TeleportTimeLeft < 0.f)
	{
		UE_LOG(LogPokemonFollower, Log, TEXT("%s disparaît et revient près du joueur : %s (à %.0f cm du joueur)"),
			*Species.ToString(), Reason, Leader ? FVector::Dist(GetActorLocation(), Leader->GetActorLocation()) : 0.f);
	}
	StartTeleport();
}

void APokemonFollower::StartTeleport()
{
	if (!Leader || !HasAuthority() || TeleportTimeLeft >= 0.f)
	{
		return;
	}
	TeleportTimeLeft = VanishTime;
	Pose = EFollowerPose::None;
	GetCharacterMovement()->StopMovementImmediately();
	MulticastVanish(GetActorLocation());
}

// ---------------------------------------------------------------- Sifflet

void APokemonFollower::Whistle()
{
	if (!Leader || !HasAuthority())
	{
		return;
	}
	if (bHeel)
	{
		// 2e coup de sifflet : il reprend sa liberté
		bHeel = false;
		bHeelGreeting = false;
		PickFollowSpot();
		return;
	}

	bHeel = true;
	bHeelGreeting = true;
	WakeUp();
	Behavior = EBehavior::Follow;
	bCatchingUp = true;
	bRunning = true;
	StuckTimer = 0.f;

	// Trop loin, ou un obstacle entre lui et le joueur : il disparaît et réapparaît au pied
	const float Reach = BodyRadius + PokemonFollower::LeaderRadius;
	const FVector Heel = GetLeaderFeet() + FRotator(0.f, GetLeaderYaw() + 180.f + HeelAngle, 0.f).Vector() * (HeelDistance + Reach);
	const float Distance = FVector::Dist2D(GetActorLocation(), Leader->GetActorLocation());
	if (Distance > MaxDistance * 2.f || !IsReachable(IsFlyer() ? ToFlightHeight(Heel) : Heel))
	{
		TeleportBecause(TEXT("sifflé de loin ou derrière un obstacle"));
	}
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
	// Réveillé par la caresse (il fait sa joie sur place, pas besoin d'attendre) ; un oiseau posé reste posé
	if (Pose == EFollowerPose::Sleep && IsFlyer())
	{
		Pose = EFollowerPose::Perched;
	}
	else if (!IsFlyer())
	{
		Pose = EFollowerPose::None;
	}
	LeaderIdleTime = 0.f;
	Behavior = EBehavior::Pause;
	ActionTimeLeft = FMath::FRandRange(PauseDuration.X, PauseDuration.Y);
	GetCharacterMovement()->StopMovementImmediately();
	MulticastPlayHappy();
}

void APokemonFollower::MulticastPlayHappy_Implementation()
{
	HappyTimeLeft = HappyAnim ? HappyAnim->GetPlayLength() : 0.8f;
	bRestartHappy = true;
	if (!HappyAnim && HasAuthority() && FollowMode == EFollowMode::Walk)
	{
		// Pas d'animation de joie : un petit saut
		LaunchCharacter(FVector(0.f, 0.f, 320.f), false, true);
	}
}

// ---------------------------------------------------------------- Animation (tous les joueurs)

void APokemonFollower::UpdateAnimation(float DeltaSeconds)
{
	UPokemonFollowerAnimInstance* Anim = Cast<UPokemonFollowerAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Anim || !GetMesh()->GetSkeletalMeshAsset())
	{
		return;
	}

	// Se coucher et se relever : fondu lent ; le reste : fondu court
	if (Pose != AnimPose)
	{
		AnimPose = Pose;
		SlowBlendTimeLeft = SleepBlendTime;
	}
	SlowBlendTimeLeft = FMath::Max(SlowBlendTimeLeft - DeltaSeconds, 0.f);
	Anim->SetBlendTime(SlowBlendTimeLeft > 0.f ? SleepBlendTime : MoveBlendTime);

	if (Pose == EFollowerPose::Sleep && SleepAnim)
	{
		Anim->Want(SleepAnim, 1.f, 1.f, true);
		return;
	}
	if (Pose == EFollowerPose::Perched && LandedAnim)
	{
		Anim->Want(LandedAnim, 1.f, 1.f, true);
		return;
	}
	if (HappyTimeLeft > 0.f && HappyAnim)
	{
		if (bRestartHappy)
		{
			Anim->Restart(HappyAnim);
		}
		bRestartHappy = false;
		Anim->Want(HappyAnim, 1.f, 1.f, false);
		return;
	}
	bRestartHappy = false;

	const FVector Velocity = GetVelocity();
	const float Speed = IsFlyer() ? Velocity.Size() : Velocity.Size2D();
	const float MinRate = IsFlyer() ? FMath::Max(static_cast<float>(PlayRateRange.X), 0.8f) : static_cast<float>(PlayRateRange.X);
	const float MaxRate = IsFlyer() ? FMath::Min(static_cast<float>(PlayRateRange.Y), 1.4f) : static_cast<float>(PlayRateRange.Y);
	const float WalkSpeed = GetAnimGaitSpeed(false);
	const float RunSpeed = GetAnimGaitSpeed(true);
	UAnimSequence* Walk = WalkAnim ? WalkAnim.Get() : RunAnim.Get();
	if (!Walk)
	{
		Anim->Want(IdleAnim, 1.f, 1.f, true);
		return;
	}

	// Démarche choisie d'après la vitesse réelle, et jouée au rythme du sol (les pattes ne glissent pas)
	const float SlowestWalk = WalkSpeed * MinRate;
	if (Speed < SlowestWalk && IdleAnim)
	{
		// Démarrage et arrêt : mélange d'attente et de marche lente, les pas suivent la vitesse
		const float WalkWeight = Speed < 5.f ? 0.f : Speed / SlowestWalk;
		Anim->Want(IdleAnim, 1.f, 1.f - WalkWeight, true);
		Anim->Want(Walk, MinRate, WalkWeight, true);
	}
	else if (RunAnim && Speed > FMath::Sqrt(WalkSpeed * RunSpeed))
	{
		Anim->Want(RunAnim, FMath::Clamp(Speed / RunSpeed, MinRate, MaxRate), 1.f, true);
	}
	else
	{
		Anim->Want(Walk, FMath::Clamp(Speed / WalkSpeed, 0.05f, MaxRate), 1.f, true);
	}
}

float APokemonFollower::GetAnimGaitSpeed(bool bRun) const
{
	if (bRun && RunAnim)
	{
		return MeasuredRunSpeed > 0.f ? MeasuredRunSpeed : RunGaitSpeed;
	}
	return MeasuredWalkSpeed > 0.f ? MeasuredWalkSpeed : WalkGaitSpeed;
}

void APokemonFollower::MeasureGait(float DeltaSeconds, const TArray<FVector>& FeetLocal)
{
	// Le pied posé (le plus bas) recule sous le corps à la vitesse du sol × la vitesse de lecture.
	// Mesurer ça donne la vraie vitesse de l'animation : plus besoin de deviner d'après la taille.
	int32 Lowest = INDEX_NONE;
	for (int32 Index = 0; Index < FeetLocal.Num(); ++Index)
	{
		if (Lowest == INDEX_NONE || FeetLocal[Index].Z < FeetLocal[Lowest].Z)
		{
			Lowest = Index;
		}
	}
	const UPokemonFollowerAnimInstance* Anim = Cast<UPokemonFollowerAnimInstance>(GetMesh()->GetAnimInstance());
	if (Anim && !IsFlyer() && DeltaSeconds > KINDA_SMALL_NUMBER && Lowest != INDEX_NONE && Lowest == PreviousLowestFoot
		&& PreviousFeetLocal.Num() == FeetLocal.Num())
	{
		for (const bool bRun : { false, true })
		{
			UAnimSequence* Sequence = bRun ? RunAnim.Get() : WalkAnim.Get();
			const float Rate = Sequence ? Anim->GetPlayRate(Sequence) : 0.f;
			if (!Sequence || Anim->GetWeight(Sequence) < 0.95f || Rate < 0.3f)
			{
				continue;
			}
			const float Backward = static_cast<float>(PreviousFeetLocal[Lowest].X - FeetLocal[Lowest].X) / DeltaSeconds;
			const float Guess = bRun ? RunGaitSpeed : WalkGaitSpeed;
			if (Backward <= 0.f)
			{
				continue;
			}
			const float Sample = FMath::Clamp(Backward / Rate, Guess * 0.3f, Guess * 3.f);
			float& Measured = bRun ? MeasuredRunSpeed : MeasuredWalkSpeed;
			Measured = Measured > 0.f ? FMath::Lerp(Measured, Sample, 0.04f) : Sample;
		}
	}
	PreviousFeetLocal = FeetLocal;
	PreviousLowestFoot = Lowest;
}

void APokemonFollower::WakeUp()
{
	if (Pose == EFollowerPose::Sleep)
	{
		WakeTimeLeft = WakeUpTime;
	}
	if (Pose != EFollowerPose::None && IsFlyer())
	{
		// Oiseau posé : il s'envole
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	Pose = EFollowerPose::None;
}

// ---------------------------------------------------------------- Effets (tous les joueurs)

void APokemonFollower::MulticastVanish_Implementation(FVector_NetQuantize At)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	VanishClock = 0.f;
	AppearClock = -1.f;
	BurstSparkles(At, 10);
}

void APokemonFollower::MulticastAppear_Implementation(FVector_NetQuantize At)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	VanishClock = -1.f;
	AppearClock = 0.f;
	BurstSparkles(At, 10);
}

void APokemonFollower::ApplyModelScale(float Factor)
{
	if (FMath::IsNearlyEqual(Factor, AppliedScaleFactor, 0.001f))
	{
		return;
	}
	AppliedScaleFactor = Factor;
	// Le modèle grandit et rapetisse depuis ses pieds ; à 0 on le cache
	const float Safe = FMath::Max(Factor, 0.001f);
	GetMesh()->SetRelativeScale3D(FVector(ModelScale * Safe));
	StaticModel->SetRelativeScale3D(FVector(StaticBaseScale * Safe));
	const bool bVisible = Factor > 0.01f;
	GetMesh()->SetVisibility(bVisible && GetMesh()->GetSkeletalMeshAsset() != nullptr);
	StaticModel->SetVisibility(bVisible && StaticModel->GetStaticMesh() != nullptr);
}

void APokemonFollower::FindFootBones(const USkeletalMesh* Asset, float BottomZ, float Height)
{
	FootBones.Reset();
	FootRestHeights.Reset();
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	// Les vrais os des pattes (LFoot, RToe, LHand d'un quadrupède...), s'ils sont près du sol en pose de repos.
	// Les points d'effet EffFoot ne servent qu'en dernier recours : ils ne suivent pas toujours les pattes.
	auto Collect = [&](bool bEffectPoints)
	{
		for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
		{
			const FString Bone = Skeleton.GetBoneName(Index).ToString();
			const bool bEffect = Bone.StartsWith(TEXT("Eff"));
			const bool bLimb = Bone.Contains(TEXT("Foot")) || Bone.Contains(TEXT("Toe")) || Bone.Contains(TEXT("Hand"))
				|| Bone.Contains(TEXT("Finger")) || Bone.Contains(TEXT("Paw"));
			if (!bLimb || bEffect != bEffectPoints)
			{
				continue;
			}
			// Hauteur au-dessus du bas du modèle en pose de repos
			const float RestHeight = static_cast<float>(FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Index).GetLocation().Z) - BottomZ;
			if (RestHeight <= Height * 0.25f)
			{
				FootBones.Add(Skeleton.GetBoneName(Index));
				FootRestHeights.Add(RestHeight);
			}
		}
	};
	Collect(false);
	if (FootBones.Num() == 0)
	{
		Collect(true);
	}
	UE_LOG(LogPokemonFollower, Verbose, TEXT("%s : %d os de pattes pour poser les pieds au sol"), *Species.ToString(), FootBones.Num());
}

void APokemonFollower::UpdateGrounding(float DeltaSeconds)
{
	USkeletalMeshComponent* Skeletal = GetMesh();
	if (FootBones.Num() == 0 || !Skeletal->GetSkeletalMeshAsset() || IsAirborne())
	{
		return;
	}
	// Plante des pieds, dans le repère du Pokémon
	const float MeshScaleZ = static_cast<float>(Skeletal->GetComponentScale().Z);
	const FTransform ActorTransform = GetActorTransform();
	TArray<FVector> Feet;
	TArray<FVector> FeetLocal;
	for (int32 Index = 0; Index < FootBones.Num(); ++Index)
	{
		const FVector Foot = Skeletal->GetSocketLocation(FootBones[Index]) - FVector(0.f, 0.f, FootRestHeights[Index] * MeshScaleZ);
		Feet.Add(Foot);
		FeetLocal.Add(ActorTransform.InverseTransformPosition(Foot));
	}
	MeasureGait(DeltaSeconds, FeetLocal);
	// En sautant (joie), en tombant, en disparaissant : on ne touche à rien
	const bool bHold = HappyTimeLeft > 0.f || GetCharacterMovement()->IsFalling() || VanishClock >= 0.f || AppearClock >= 0.f;
	float Target = GroundOffset;
	if (Pose == EFollowerPose::Sleep)
	{
		// Couché, les pieds ne touchent pas forcément le sol : on garde le placement du modèle
		Target = 0.f;
	}
	else if (!bHold)
	{
		// Le pied le plus bas doit toucher le sol juste en dessous de lui
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PokemonFollowerFeet), false, this);
		Params.AddIgnoredActor(Leader.Get());
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float TopZ = static_cast<float>(GetActorLocation().Z) + HalfHeight;
		const float BottomZ = static_cast<float>(GetActorLocation().Z) - HalfHeight - 150.f;
		float Gap = TNumericLimits<float>::Max();
		for (const FVector& Foot : Feet)
		{
			const float SoleZ = static_cast<float>(Foot.Z);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(Foot.X, Foot.Y, TopZ), FVector(Foot.X, Foot.Y, BottomZ), ECC_Visibility, Params))
			{
				Gap = FMath::Min(Gap, SoleZ - static_cast<float>(Hit.ImpactPoint.Z));
			}
		}
		if (Gap < TNumericLimits<float>::Max())
		{
			Target = GroundOffset - Gap;
		}
	}
	const float Limit = FMath::Max(ModelHeight * 0.5f, 10.f);
	Target = FMath::Clamp(Target, -Limit, Limit);
	GroundOffset = FMath::FInterpTo(GroundOffset, Target, DeltaSeconds, 10.f);

	const FVector MeshLocation(0.f, 0.f, BaseMeshZ + GroundOffset);
	Skeletal->SetRelativeLocation(MeshLocation);
	// Le lissage réseau (chez les autres joueurs) repart de cette hauteur
	CacheInitialMeshOffset(MeshLocation, PokemonFollower::ModelRotation);
}

FVector APokemonFollower::GetBodyPoint(FName Socket, float HeightRatio) const
{
	const USkeletalMeshComponent* Skeletal = GetMesh();
	if (Skeletal->GetSkeletalMeshAsset() && Skeletal->DoesSocketExist(Socket))
	{
		return Skeletal->GetSocketLocation(Socket);
	}
	// Pas de point d'effet dans le squelette : estimé d'après sa taille
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return Feet + FVector(0.f, 0.f, ModelHeight * HeightRatio) + GetActorForwardVector() * BodyRadius * 0.5f * (1.f - HeightRatio);
}

void APokemonFollower::UpdateEffects(float DeltaSeconds)
{
	// Disparition : il rapetisse en s'accélérant ; apparition : il grandit avec un petit rebond
	float Factor = 1.f;
	if (VanishClock >= 0.f)
	{
		VanishClock += DeltaSeconds;
		const float Alpha = FMath::Clamp(VanishClock / VanishTime, 0.f, 1.f);
		Factor = 1.f - Alpha * Alpha;
	}
	else if (AppearClock >= 0.f)
	{
		AppearClock += DeltaSeconds;
		const float Alpha = FMath::Clamp(AppearClock / AppearTime, 0.f, 1.f);
		// Easing « back » : dépasse un peu sa taille puis revient
		constexpr float C1 = 1.70158f;
		constexpr float C3 = C1 + 1.f;
		Factor = 1.f + C3 * FMath::Pow(Alpha - 1.f, 3.f) + C1 * FMath::Pow(Alpha - 1.f, 2.f);
		if (Alpha >= 1.f)
		{
			AppearClock = -1.f;
			Factor = 1.f;
		}
	}
	ApplyModelScale(Factor);
	UpdateGrounding(DeltaSeconds);
	UpdateSparkles(DeltaSeconds);

	const bool bAsleep = Pose == EFollowerPose::Sleep && VanishClock < 0.f;
	UpdateSleepEffects(DeltaSeconds, bAsleep);

	// Yeux : fermés pendant le sommeil, clignements de temps en temps sinon
	bool bClose = bAsleep;
	if (!bAsleep && BlinkInterval.Y > 0.f)
	{
		BlinkTimeLeft -= DeltaSeconds;
		if (BlinkTimeLeft <= 0.f)
		{
			BlinkCloseLeft = PokemonFollower::BlinkDuration;
			BlinkTimeLeft = FMath::FRandRange(BlinkInterval.X, BlinkInterval.Y);
		}
		if (BlinkCloseLeft > 0.f)
		{
			BlinkCloseLeft -= DeltaSeconds;
			bClose = true;
		}
	}
	SetEyesClosed(bClose);
}

void APokemonFollower::UpdateSleepEffects(float DeltaSeconds, bool bAsleep)
{
	SleepBubble->SetVisibility(bAsleep);
	if (!bAsleep)
	{
		for (UTextRenderComponent* Letter : SleepLetters)
		{
			Letter->SetVisibility(false);
		}
		SleepClock = 0.f;
		return;
	}
	SleepClock += DeltaSeconds;

	// Bulle au bout du nez : elle gonfle à l'inspiration et dégonfle à l'expiration
	const float Breath = 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * SleepClock / BreathPeriod);
	const float MaxBubbleRadius = FMath::Clamp(ModelHeight * BubbleSizeRatio, 3.f, 18.f);
	const float BubbleRadius = MaxBubbleRadius * (0.2f + 0.8f * Breath);
	const FVector Forward = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const FVector Mouth = GetBodyPoint(TEXT("EffMouth01"), 0.35f);
	SleepBubble->SetWorldLocation(Mouth + Forward * BubbleRadius * 0.8f + Right * BubbleRadius * 0.35f);
	SleepBubble->SetWorldScale3D(FVector(BubbleRadius * 2.f / 100.f));

	// « Z » qui montent au-dessus de la tête en grandissant, tournés vers la caméra
	const FVector Head = GetBodyPoint(TEXT("EffOverHead01"), 1.f) + FVector(0.f, 0.f, 8.f);
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector Camera = CameraManager ? CameraManager->GetCameraLocation() : Head - Forward * 300.f;
	const float SizeScale = FMath::Clamp(ModelHeight / 60.f, 0.7f, 2.5f);
	for (int32 Index = 0; Index < SleepLetters.Num(); ++Index)
	{
		UTextRenderComponent* Letter = SleepLetters[Index];
		const float Phase = FMath::Fmod(SleepClock / PokemonFollower::SleepLetterCycle + static_cast<float>(Index) / SleepLetters.Num(), 1.f);
		const FVector Location = Head
			+ FVector(0.f, 0.f, Phase * 45.f * SizeScale)
			+ Right * (Phase * 14.f + FMath::Sin(Phase * UE_TWO_PI) * 4.f) * SizeScale;
		Letter->SetWorldLocation(Location);
		Letter->SetWorldRotation((Camera - Location).Rotation());
		Letter->SetWorldSize(FMath::Lerp(7.f, 16.f, Phase) * SizeScale);
		// Chaque « Z » apparaît en bas et s'efface en haut
		Letter->SetVisibility(Phase > 0.04f && Phase < 0.9f);
	}
}

void APokemonFollower::FindEyeMaterials()
{
	bEyeMaterialsSearched = true;
	ClosedEyeSlots.Reset();
	ClosedEyeMaterials.Reset();
	const USkeletalMesh* Asset = GetMesh()->GetSkeletalMeshAsset();
	if (!Asset)
	{
		return;
	}
	// Le matériau « yeux fermés » d'un emplacement s'appelle <matériau>_Closed, à côté de lui.
	// Les matériaux des modèles s'appellent M_Pikachu_1, _2... : on essaie chaque emplacement.
	const TArray<FSkeletalMaterial>& Materials = Asset->GetMaterials();
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const UMaterialInterface* Material = Materials[Index].MaterialInterface;
		if (!Material)
		{
			continue;
		}
		const FString Name = Material->GetName() + TEXT("_Closed");
		const FString Path = FString::Printf(TEXT("%s_Closed.%s"), *Material->GetPackage()->GetName(), *Name);
		if (UMaterialInterface* Closed = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			ClosedEyeSlots.Add(Index);
			ClosedEyeMaterials.Add(Closed);
		}
	}
	if (ClosedEyeSlots.Num() == 0)
	{
		UE_LOG(LogPokemonFollower, Warning, TEXT("%s : pas de matériau « yeux fermés » (<matériau>_Closed). Lancer Tools/Pokemon/setup_follower_assets.py dans Unreal."),
			*Species.ToString());
	}
}

void APokemonFollower::SetEyesClosed(bool bClosed)
{
	if (bClosed == bEyesClosed)
	{
		return;
	}
	bEyesClosed = bClosed;
	if (!bEyeMaterialsSearched)
	{
		FindEyeMaterials();
	}
	USkeletalMeshComponent* Skeletal = GetMesh();
	for (int32 Index = 0; Index < ClosedEyeSlots.Num(); ++Index)
	{
		// nullptr : retour au matériau du modèle (yeux ouverts)
		Skeletal->SetMaterial(ClosedEyeSlots[Index], bClosed ? ClosedEyeMaterials[Index].Get() : nullptr);
	}
}

void APokemonFollower::BurstSparkles(const FVector& Center, int32 Count)
{
	if (Sparkles.Num() == 0 || SparkleStates.Num() != Sparkles.Num())
	{
		return;
	}
	const float SizeScale = FMath::Clamp(ModelHeight / 50.f, 0.8f, 2.5f);
	for (int32 Made = 0; Made < Count; ++Made)
	{
		const int32 Index = NextSparkle;
		NextSparkle = (NextSparkle + 1) % Sparkles.Num();
		FSparkle& State = SparkleStates[Index];
		FVector Direction = FMath::VRand();
		Direction.Z = FMath::Abs(Direction.Z) * 0.8f + 0.2f;
		State.Origin = Center + Direction * 10.f * SizeScale;
		State.Velocity = Direction.GetSafeNormal() * FMath::FRandRange(90.f, 190.f) * SizeScale;
		State.Age = 0.f;
		State.Life = FMath::FRandRange(0.5f, 0.9f);
		State.Size = FMath::FRandRange(0.05f, 0.1f) * SizeScale;
		State.Spin = FMath::FRandRange(-540.f, 540.f);
		Sparkles[Index]->SetVisibility(true);
	}
}

void APokemonFollower::UpdateSparkles(float DeltaSeconds)
{
	for (int32 Index = 0; Index < SparkleStates.Num(); ++Index)
	{
		FSparkle& State = SparkleStates[Index];
		if (State.Life <= 0.f)
		{
			continue;
		}
		State.Age += DeltaSeconds;
		UStaticMeshComponent* Sparkle = Sparkles[Index];
		if (State.Age >= State.Life)
		{
			State.Life = 0.f;
			Sparkle->SetVisibility(false);
			continue;
		}
		// Éclat qui s'envole en ralentissant, tourne sur lui-même et scintille avant de s'éteindre
		const float Time = State.Age;
		const float Alpha = Time / State.Life;
		const FVector Location = State.Origin + State.Velocity * (Time - 0.5f * Time * Time) + FVector(0.f, 0.f, -60.f * Time * Time);
		const float Grow = Alpha < 0.15f ? Alpha / 0.15f : 1.f - (Alpha - 0.15f) / 0.85f;
		const float Twinkle = 0.75f + 0.25f * FMath::Sin(Time * 40.f + Index);
		Sparkle->SetWorldLocation(Location);
		// Cube tourné de 45° sur deux axes : un losange, comme une petite étoile
		Sparkle->SetWorldRotation(FRotator(45.f, State.Spin * Time, 45.f));
		Sparkle->SetWorldScale3D(FVector(FMath::Max(State.Size * Grow * Twinkle, 0.001f)));
	}
}
