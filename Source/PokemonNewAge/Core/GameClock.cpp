#include "Core/GameClock.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace PokemonClock
{
	namespace
	{
		const TCHAR* HourNames[] = { TEXT("Heure"), TEXT("Hour"), TEXT("TimeOfDay"), TEXT("HeureDuJour"), TEXT("Time") };

		/** Variable « heure » du BP de cycle jour / nuit, si on la trouve */
		bool ReadCycleActor(const UWorld* World, float& OutHour)
		{
			for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
			{
				const UClass* Class = It->GetClass();
				if (!Class->GetName().Contains(TEXT("CycleJourNuit")) && !Class->GetName().Contains(TEXT("DayNight")))
				{
					continue;
				}
				for (const TCHAR* Name : HourNames)
				{
					for (TFieldIterator<FProperty> Prop(Class); Prop; ++Prop)
					{
						if (!Prop->GetName().Equals(Name, ESearchCase::IgnoreCase))
						{
							continue;
						}
						double Value = -1.0;
						if (const FFloatProperty* Float = CastField<FFloatProperty>(*Prop))
						{
							Value = Float->GetPropertyValue_InContainer(*It);
						}
						else if (const FDoubleProperty* Double = CastField<FDoubleProperty>(*Prop))
						{
							Value = Double->GetPropertyValue_InContainer(*It);
						}
						if (Value >= 0.0 && Value <= 24.0)
						{
							OutHour = float(Value);
							return true;
						}
					}
				}
			}
			return false;
		}

		/** Soleil : le plus fort des soleils de la map */
		const UDirectionalLightComponent* FindSun(const UWorld* World)
		{
			const UDirectionalLightComponent* Best = nullptr;
			for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
			{
				TInlineComponentArray<UDirectionalLightComponent*> Lights(*It);
				for (const UDirectionalLightComponent* Light : Lights)
				{
					if (Light->IsRegistered() && Light->LightingChannels.bChannel0 && (!Best || Light->Intensity > Best->Intensity))
					{
						Best = Light;
					}
				}
			}
			return Best;
		}

		float LastElevation = 0.f;
		bool bRising = true;
		double LastSample = -1.0;

		/** Dernier résultat (parcourir les acteurs coûte : une fois toutes les 0,5 s suffit) */
		TWeakObjectPtr<const UWorld> CachedWorld;
		double CachedAt = -1.0;
		float CachedHour = 0.f;
	}

	float ComputeHour(const UWorld* World);

	float GetHour(const UWorld* World)
	{
		const double Now = World ? World->GetRealTimeSeconds() : 0.0;
		if (World && CachedWorld.Get() == World && CachedAt >= 0.0 && Now - CachedAt < 0.5 && Now >= CachedAt)
		{
			return CachedHour;
		}
		CachedHour = ComputeHour(World);
		CachedWorld = World;
		CachedAt = Now;
		return CachedHour;
	}

	float ComputeHour(const UWorld* World)
	{
		float Hour = 0.f;
		if (World && ReadCycleActor(World, Hour))
		{
			return Hour;
		}
		if (const UDirectionalLightComponent* Sun = World ? FindSun(World) : nullptr)
		{
			// Direction vers le soleil : hauteur -90° (minuit) à 90° (midi)
			const FVector ToSun = -Sun->GetDirection();
			const float Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(ToSun.Z, -1.f, 1.f)));
			const double Now = World->GetRealTimeSeconds();
			if (LastSample >= 0.0 && Now - LastSample > 0.2 && !FMath::IsNearlyEqual(Elevation, LastElevation, 0.01f))
			{
				bRising = Elevation > LastElevation;
			}
			if (LastSample < 0.0 || Now - LastSample > 0.2)
			{
				LastElevation = Elevation;
				LastSample = Now;
			}
			// Midi en haut, minuit en bas ; matin quand le soleil monte
			const float FromNoon = (90.f - Elevation) / 15.f;
			Hour = bRising ? 12.f - FromNoon : 12.f + FromNoon;
			return FMath::Fmod(Hour + 24.f, 24.f);
		}
		const FDateTime Now = FDateTime::Now();
		return Now.GetHour() + Now.GetMinute() / 60.f;
	}

	FText FormatTime(const UWorld* World)
	{
		const float Hour = GetHour(World);
		const int32 Minutes = FMath::FloorToInt(Hour * 60.f) % (24 * 60);
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60));
	}
}
