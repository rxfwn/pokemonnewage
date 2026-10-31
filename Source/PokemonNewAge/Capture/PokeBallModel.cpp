#include "Capture/PokeBallModel.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace PokeBallModel
{
	const TCHAR* Folder = TEXT("/Game/Capture/Balls");
	const FName DefaultBall(TEXT("PokeBall"));
	const FName GlowParam(TEXT("Eclat"));
	const FName GlowColorParam(TEXT("CouleurEclat"));

	namespace
	{
		UStaticMesh* LoadPart(FName Ball, const TCHAR* Part)
		{
			if (Ball.IsNone())
			{
				return nullptr;
			}
			const FString Id = Ball.ToString();
			const FString Name = FString::Printf(TEXT("SM_%s_%s"), *Id, Part);
			return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
				*FString::Printf(TEXT("%s/%s/%s.%s"), Folder, *Id, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet));
		}

		FParts LoadExact(FName Ball)
		{
			FParts Parts;
			Parts.Top = LoadPart(Ball, TEXT("Haut"));
			Parts.Bottom = Parts.Top ? LoadPart(Ball, TEXT("Bas")) : nullptr;
			Parts.Ball = Ball;
			return Parts;
		}

		void MakeDynamic(UStaticMeshComponent* Mesh)
		{
			if (Mesh && Mesh->GetNumMaterials() > 0 && !Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
			{
				Mesh->CreateAndSetMaterialInstanceDynamic(0);
			}
		}
	}

	FParts Load(FName Ball)
	{
		FParts Parts = LoadExact(Ball);
		if (!Parts.IsValid() && Ball != DefaultBall)
		{
			Parts = LoadExact(DefaultBall);
		}
		return Parts;
	}

	float Apply(FName Ball, UStaticMeshComponent* Bottom, USceneComponent* Hinge, UStaticMeshComponent* Top)
	{
		const FParts Parts = Load(Ball);
		if (!Parts.IsValid() || !Bottom || !Hinge || !Top)
		{
			return 0.f;
		}
		Bottom->SetStaticMesh(Parts.Bottom);
		Top->SetStaticMesh(Parts.Top);
		// Matériaux du modèle (MI_<Ball>), au cas où un ancien matériau aurait été posé sur le composant
		Bottom->EmptyOverrideMaterials();
		Top->EmptyOverrideMaterials();
		MakeDynamic(Bottom);
		MakeDynamic(Top);

		// Charnière à l'arrière (-X), sur l'équateur ; le couvercle revient au centre de la Ball
		const FBox Box = Parts.Bottom->GetBoundingBox() + Parts.Top->GetBoundingBox();
		const float Radius = FMath::Max(Box.GetExtent().X, 0.1f);
		const FVector Center = Box.GetCenter();
		Hinge->SetRelativeLocationAndRotation(FVector(Center.X - Radius, Center.Y, 0.f), FRotator::ZeroRotator);
		Top->SetRelativeLocationAndRotation(FVector(Radius - Center.X, -Center.Y, 0.f), FRotator::ZeroRotator);
		Top->SetVisibility(true);
		return Box.GetSize().GetMax();
	}

	void SetOpenAngle(USceneComponent* Hinge, float Degrees)
	{
		if (Hinge)
		{
			// Tangage positif : l'avant (bouton, +X) se lève
			Hinge->SetRelativeRotation(FRotator(FMath::Max(Degrees, 0.f), 0.f, 0.f));
		}
	}

	void SetGlow(UStaticMeshComponent* Bottom, UStaticMeshComponent* Top, float Amount, const FLinearColor& Color)
	{
		for (UStaticMeshComponent* Mesh : { Bottom, Top })
		{
			if (UMaterialInstanceDynamic* Material = Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr)
			{
				Material->SetScalarParameterValue(GlowParam, FMath::Max(Amount, 0.f));
				Material->SetVectorParameterValue(GlowColorParam, Color);
			}
		}
	}
}
