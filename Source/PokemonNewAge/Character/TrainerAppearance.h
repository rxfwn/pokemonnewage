#pragma once

#include "CoreMinimal.h"
#include "TrainerAppearance.generated.h"

/** Valeur d'un curseur de l'éditeur de personnage (nez, mâchoire, muscles...) */
USTRUCT(BlueprintType)
struct FTrainerSliderValue
{
	GENERATED_BODY()

	/** Id du curseur, comme dans le catalogue (UTrainerAppearanceData) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FName Slider;

	/** -1 à 1 (ou 0 à 1 si le curseur n'a pas de morph « négatif ») */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	float Value = 0.f;
};

/**
 *  Apparence choisie par un joueur (SCRUM-66/79) : ce qui se sauvegarde et ce qui part sur le réseau.
 *  Seulement des noms et des nombres, jamais d'assets : ajouter un curseur ou un vêtement plus tard
 *  ne casse pas les anciennes sauvegardes (un curseur absent vaut 0, un id inconnu est ignoré).
 *  Les TMap ne se répliquent pas, d'où le tableau de curseurs.
 */
USTRUCT(BlueprintType)
struct FTrainerAppearance
{
	GENERATED_BODY()

	/** Base choisie (Homme, Femme...). None = la première du catalogue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FName Base;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	TArray<FTrainerSliderValue> Sliders;

	/** Teinte de peau (multipliée à la couleur de la peau CC). Blanc = peau d'origine */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor SkinColor = FLinearColor::White;

	/** Couleur de l'iris */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor EyeColor = FLinearColor(0.25f, 0.15f, 0.08f);

	/** Vêtements portés, un id par emplacement (plus tard : armoire, boutiques) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	TArray<FName> Outfit;

	/** Faux tant que le joueur n'est pas passé par l'éditeur (après le discours du Prof. Chen) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	bool bCreated = false;

	float GetSlider(FName Slider) const
	{
		const FTrainerSliderValue* Found = Sliders.FindByPredicate([Slider](const FTrainerSliderValue& S) { return S.Slider == Slider; });
		return Found ? Found->Value : 0.f;
	}

	void SetSlider(FName Slider, float Value)
	{
		if (FTrainerSliderValue* Found = Sliders.FindByPredicate([Slider](const FTrainerSliderValue& S) { return S.Slider == Slider; }))
		{
			Found->Value = Value;
			return;
		}
		Sliders.Add({ Slider, Value });
	}
};
