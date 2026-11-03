#include "Progression/EvolutionBackdrop.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace EvolutionBackdrop
{
	/** Plan du moteur : 100 × 100 cm, face vers +Z */
	const TCHAR* PlaneMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* SkyPath = TEXT("/Game/Progression/M_EvolutionSky.M_EvolutionSky");
	const TCHAR* FloorPath = TEXT("/Game/Progression/M_EvolutionFloor.M_EvolutionFloor");
	/** Paramètres posés à chaque image sur les deux matériaux (dégradé du ciel, lueur du sol) */
	const FName FloorZParam(TEXT("SolZ"));
	const FName FloorCenterParam(TEXT("CentreSol"));
	const FName FloorRadiusParam(TEXT("RayonSol"));
}

UEvolutionBackdropComponent::UEvolutionBackdropComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Après le déplacement de la caméra par la scène (même image)
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SkyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(EvolutionBackdrop::SkyPath));
	FloorMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(EvolutionBackdrop::FloorPath));
}

UStaticMeshComponent* UEvolutionBackdropComponent::MakePlane(const TCHAR* Name, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutDynamic)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, EvolutionBackdrop::PlaneMesh);
	if (!Mesh || !Material)
	{
		return nullptr;
	}
	UStaticMeshComponent* Plane = NewObject<UStaticMeshComponent>(GetOwner(), Name);
	Plane->SetStaticMesh(Mesh);
	Plane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plane->SetCastShadow(false);
	// Grands panneaux lumineux : hors de l'éclairage global (Lumen en ferait des taches sur le Pokémon)
	Plane->bAffectDynamicIndirectLighting = false;
	Plane->bAffectDistanceFieldLighting = false;
	Plane->SetReceivesDecals(false);
	OutDynamic = UMaterialInstanceDynamic::Create(Material, this);
	Plane->SetMaterial(0, OutDynamic);
	return Plane;
}

void UEvolutionBackdropComponent::OnRegister()
{
	Super::OnRegister();

	// Seulement en jeu (pas dans l'éditeur de Blueprint), une seule fois
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || Sky || !GetOwner())
	{
		return;
	}
	Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
	UMaterialInterface* SkyMat = SkyMaterial.LoadSynchronous();
	UMaterialInterface* FloorMat = FloorMaterial.LoadSynchronous();
	if (!Camera || !SkyMat || !FloorMat)
	{
		UE_LOG(LogTemp, Log, TEXT("Évolution : fond étoilé absent (lancer Tools/Progression/import_evolution_backdrop.py)."));
		return;
	}

	UMaterialInstanceDynamic* SkyMID = nullptr;
	UMaterialInstanceDynamic* FloorMID = nullptr;
	Sky = MakePlane(TEXT("BackdropSky"), SkyMat, SkyMID);
	Floor = MakePlane(TEXT("BackdropFloor"), FloorMat, FloorMID);
	SkyDynamic = SkyMID;
	FloorDynamic = FloorMID;
	if (!Sky || !Floor)
	{
		return;
	}
	// Ciel : accroché à la caméra, face vers elle (le +Z du plan tourné vers -X de la caméra)
	Sky->SetupAttachment(Camera);
	Sky->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Sky->RegisterComponent();
	// Sol : accroché aux pieds (ce composant)
	Floor->SetupAttachment(this);
	Floor->RegisterComponent();
	UpdateLayout();
}

float UEvolutionBackdropComponent::SceneRadius() const
{
	// Taille de ce qui est montré (Pokémon, Ball, effets) autour des pieds, sans le fond lui-même
	float Radius = 100.f;
	const FVector Center = GetComponentLocation();
	TArray<UPrimitiveComponent*> Primitives;
	GetOwner()->GetComponents<UPrimitiveComponent>(Primitives);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive == Sky || Primitive == Floor || !Primitive->IsVisible() || !Primitive->IsRegistered())
		{
			continue;
		}
		const FBoxSphereBounds& Bounds = Primitive->Bounds;
		Radius = FMath::Max(Radius, FVector::Dist(Bounds.Origin, Center) + Bounds.SphereRadius);
	}
	return Radius;
}

void UEvolutionBackdropComponent::UpdateLayout()
{
	if (!Sky || !Floor || !Camera)
	{
		return;
	}
	const FVector Center = GetComponentLocation();
	const float CameraDistance = FVector::Dist(Camera->GetComponentLocation(), Center);
	const float Radius = SceneRadius();

	// Ciel derrière tout ce qui est montré, assez grand pour remplir l'image (champ horizontal de la caméra)
	const float SkyDistance = CameraDistance + FMath::Max(SkyDistanceBehind, Radius * 1.2f);
	const float HalfFov = FMath::DegreesToRadians(FMath::Clamp(Camera->FieldOfView, 5.f, 170.f) * 0.5f);
	const float SkySize = 2.f * SkyDistance * FMath::Tan(HalfFov) * SkyMargin;
	Sky->SetRelativeLocation(FVector(SkyDistance, 0.f, 0.f));
	Sky->SetRelativeScale3D(FVector(SkySize / 100.f, SkySize / 100.f, 1.f));

	// Sol : jusqu'au ciel dans toutes les directions (au-delà, il passe derrière le ciel et ne se voit pas)
	const float FloorHalf = SkyDistance + CameraDistance;
	Floor->SetRelativeLocation(FVector(0.f, 0.f, FloorLift));
	Floor->SetRelativeScale3D(FVector(FloorHalf / 50.f, FloorHalf / 50.f, 1.f));

	// Dégradé du ciel calé sur le sol (même bleu à l'horizon des deux côtés), lueur du sol centrée sous le Pokémon
	const float FloorZ = Center.Z + FloorLift;
	const FLinearColor FloorCenter(Center.X, Center.Y, FloorZ, 0.f);
	for (UMaterialInstanceDynamic* Dynamic : { SkyDynamic.Get(), FloorDynamic.Get() })
	{
		if (Dynamic)
		{
			Dynamic->SetScalarParameterValue(EvolutionBackdrop::FloorZParam, FloorZ);
			Dynamic->SetVectorParameterValue(EvolutionBackdrop::FloorCenterParam, FloorCenter);
			Dynamic->SetScalarParameterValue(EvolutionBackdrop::FloorRadiusParam, FMath::Max(Radius * 1.8f, 250.f));
		}
	}
}

void UEvolutionBackdropComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateLayout();
}
