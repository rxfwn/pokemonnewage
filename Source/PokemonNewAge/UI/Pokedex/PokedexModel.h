#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PokedexModel.generated.h"

class UStaticMesh;
class UMaterialInterface;
class USoundBase;

/**
 *  DA_PokedexModel : les pièces 3D, le matériau et les sons du Pokédex du menu (SCRUM-64).
 *  Rempli tout seul par Tools/Pokedex/import_pokedex_unreal.py ; on peut remplacer une pièce ou un son à la main.
 *  Clés des pièces : Body, TopBand, Bezel, LensRing, Lens, LedRed, LedYellow, LedGreen, ButtonRound, PillRed,
 *  PillBlue, DPad, Panel, Hinge, Lid, LidPanel, MenuButton0 à MenuButton5 (voir docs/MENU_POKEDEX.md).
 */
UCLASS(BlueprintType)
class UPokedexModel : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Une mesh par pièce, toutes avec leur origine sur l'axe de la charnière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Model")
	TMap<FName, TObjectPtr<UStaticMesh>> Parts;

	/** Matériau à paramètres (BaseColor, Metallic, Roughness, EmissiveColor, EmissiveStrength) : M_Pokedex */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Model")
	TObjectPtr<UMaterialInterface> Material;

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
};
