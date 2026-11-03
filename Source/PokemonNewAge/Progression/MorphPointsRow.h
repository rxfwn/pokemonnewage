#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MorphPointsRow.generated.h"

/**
 *  Points de surface d'un modèle de Pokémon, pour la transformation pendant l'évolution (docs/MODELES_POKEMON.md).
 *  Une ligne par espèce (nom de ligne de DT_Species) dans /Game/Pokemon/Models/DT_MorphPoints, remplie par
 *  Tools/Pokemon/import_pokemon_models.py. En cm, dans l'espace du mesh, triés pour que le point i d'une espèce
 *  glisse vers le point i de son évolution.
 */
USTRUCT(BlueprintType)
struct FPokemonMorphPointsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evolution")
	TArray<FVector> Points;
};
