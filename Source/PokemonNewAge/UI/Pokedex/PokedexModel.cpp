#include "UI/Pokedex/PokedexModel.h"
#include "EngineUtils.h"
#include "Landscape.h"

const UPokedexModel* UPokedexModel::Get()
{
	// Le PlayerController garde une référence (PokedexModel) pour qu'il ne soit pas déchargé
	static TWeakObjectPtr<const UPokedexModel> Cached;
	if (!Cached.IsValid())
	{
		Cached = LoadObject<UPokedexModel>(nullptr, AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	return Cached.Get();
}

bool UPokedexModel::WorldToMap(const UWorld* World, const FVector& Location, FVector2D& OutUV) const
{
	if (!World)
	{
		return false;
	}
	const float Size = float(FMath::Max(2, MapHeightmapSize) - 1);
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		// Repère du Landscape : un point de heightmap par unité, X = colonne = nord, Y = ligne = est
		const FVector Local = It->GetActorTransform().InverseTransformPosition(Location);
		OutUV = FVector2D(Local.Y / Size, 1.f - Local.X / Size);
		return OutUV.X >= 0.f && OutUV.X <= 1.f && OutUV.Y >= 0.f && OutUV.Y <= 1.f;
	}
	return false;
}

bool UPokedexModel::MapToWorld(const UWorld* World, const FVector2D& UV, FVector& OutLocation) const
{
	if (!World)
	{
		return false;
	}
	const float Size = float(FMath::Max(2, MapHeightmapSize) - 1);
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		OutLocation = It->GetActorTransform().TransformPosition(FVector((1.f - UV.Y) * Size, UV.X * Size, 0.f));
		return true;
	}
	return false;
}

int32 UPokedexModel::FindPlaceAt(const FVector2D& UV) const
{
	int32 Best = -1;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < MapPlaces.Num(); ++Index)
	{
		const FPokedexMapPlace& Place = MapPlaces[Index];
		const float Radius = FMath::Max(Place.Radius, 0.002f);
		const float Distance = float(FVector2D::Distance(UV, Place.UV));
		if (Distance > Radius)
		{
			continue;
		}
		// Les villes passent avant les routes qui y arrivent
		const float Score = Distance / Radius + (Place.Kind == EPokedexPlaceKind::Town ? 0.f : 10.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Index;
		}
	}
	return Best;
}
