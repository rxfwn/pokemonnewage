#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TrainerAppearanceData.generated.h"

class USkeletalMesh;
class UAnimInstance;

/**
 *  Un curseur de l'éditeur. Il pilote un ou deux morph targets exportés de Character Creator :
 *  vers la droite MorphPositive monte de 0 à 1, vers la gauche MorphNegative (s'il existe).
 *  Le même nom de morph est appliqué au corps ET aux vêtements qui l'ont (CC les exporte ensemble).
 */
USTRUCT(BlueprintType)
struct FTrainerSliderDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Id;

	/** Texte à l'écran (« Largeur du nez ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	/** Onglet de l'éditeur : Visage, Yeux, Nez, Bouche, Corps... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Category;

	/** Nom exact du morph dans le Skeletal Mesh (onglet Morph Target Preview de l'éditeur de mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName MorphPositive;

	/** Optionnel : morph de l'autre sens. Vide = curseur de 0 à 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName MorphNegative;

	/** Bornes pour qu'on ne puisse pas « casser » le visage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=(ClampMin=0, ClampMax=1))
	float Limit = 1.f;
};

/** Une base de personnage (Homme, Femme), exportée de Character Creator avec Auto Setup */
USTRUCT(BlueprintType)
struct FTrainerBaseDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Animation Blueprint retargeté (vide = garder celui du personnage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSubclassOf<UAnimInstance> AnimClass;

	/** Curseurs propres à cette base (les curseurs communs sont dans CommonSliders) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerSliderDef> Sliders;
};

/**
 *  Catalogue de l'éditeur de personnage : bases, curseurs, réglages des matériaux.
 *  Un seul asset dans le jeu : Content/Characters/Player/DA_TrainerAppearance.
 *  Ajouter un curseur = une ligne ici, pas de code.
 */
UCLASS(BlueprintType)
class UTrainerAppearanceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerBaseDef> Bases;

	/** Curseurs valables pour toutes les bases (même noms de morphs chez l'homme et la femme) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerSliderDef> CommonSliders;

	/** Emplacements de matériau qui reçoivent la couleur de peau (Std_Skin_Head, Std_Skin_Body...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	TArray<FName> SkinMaterialSlots;

	/** Paramètre couleur des matériaux de peau à teinter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	FName SkinColorParameter = TEXT("SkinTint");

	/** Emplacements des yeux (Std_Eye_L, Std_Eye_R...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	TArray<FName> EyeMaterialSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	FName EyeColorParameter = TEXT("IrisColor");

	const FTrainerBaseDef* FindBase(FName Id) const;

	/** Curseur de la base ou commun, nullptr si inconnu */
	const FTrainerSliderDef* FindSlider(const FTrainerBaseDef& Base, FName Slider) const;

	/** Tous les curseurs d'une base (communs puis propres) */
	TArray<FTrainerSliderDef> GetSliders(const FTrainerBaseDef& Base) const;
};
