#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PokedexModel.generated.h"

class UStaticMesh;
class UMaterialInterface;
class USoundBase;
class UTexture2D;

/** Sorte de lieu sur la carte du Pokédex. */
UENUM(BlueprintType)
enum class EPokedexPlaceKind : uint8
{
	Town,
	Route,
	Place
};

/** Un lieu de la carte : nom et position sur l'image (u vers l'est, v vers le sud, de 0 à 1). */
USTRUCT(BlueprintType)
struct FPokedexMapPlace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	EPokedexPlaceKind Kind = EPokedexPlaceKind::Town;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FVector2D UV = FVector2D::ZeroVector;

	/** Rayon du lieu, en fraction de la taille de la carte (« Tu es ici » quand le joueur est dedans) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	float Radius = 0.01f;
};

/**
 *  DA_PokedexModel : les pièces 3D, les matériaux, les icônes, la carte et les sons du Pokédex du menu (SCRUM-64).
 *  Rempli tout seul par Tools/Pokedex/import_pokedex_unreal.py ; on peut remplacer une pièce ou un son à la main.
 *  Pièces (modèle de L'amie, découpé par prepare_pokedex_model.py) : Body, Hinge, Lens, LensRing, LedRed, LedYellow,
 *  LedGreen, ScreenLeds, Screen, MiniScreen0, BackButton, ConfirmButton, HomeButton, DexButton, DPad ; sur le volet :
 *  Lid, LidScreen, MiniScreen1, MiniScreen2, LidLeds, PrevButton, NextButton, LidButton, MenuButton0 à MenuButton9.
 */
UCLASS(BlueprintType)
class UPokedexModel : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Une mesh par pièce, toutes avec leur origine sur l'axe de la charnière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Model")
	TMap<FName, TObjectPtr<UStaticMesh>> Parts;

	/**
	 *  Matériau uni à paramètres (BaseColor, Metallic, Roughness, EmissiveColor, EmissiveStrength) : M_Pokedex.
	 *  Sert aux 10 boutons du volet (sous les icônes) et aux pièces sans texture.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Model")
	TObjectPtr<UMaterialInterface> Material;

	/** Icônes des 10 boutons du volet, dans l'ordre de EPokedexMenu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu")
	TArray<TObjectPtr<UTexture2D>> MenuIcons;

	/** Carte de Kanto, nord en haut (Tools/Pokedex/make_pokedex_map.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	TObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	TArray<FPokedexMapPlace> MapPlaces;

	/** Nombre de points de côté de la heightmap du Landscape (taille du monde = (N - 1) × échelle X) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	int32 MapHeightmapSize = 4033;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> ClickSound;

	/** Une page de l'écran s'ouvre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> BeepSound;

	/** La sélection bouge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> NavSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> ErrorSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> SaveSound;

	/** Chemin de l'asset créé par le script d'import */
	static constexpr const TCHAR* AssetPath = TEXT("/Game/UI/Pokedex/DA_PokedexModel.DA_PokedexModel");
};
