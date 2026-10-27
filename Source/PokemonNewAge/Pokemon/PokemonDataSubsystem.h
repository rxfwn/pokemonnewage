#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PokemonDataSubsystem.generated.h"

class UPokemonDatabase;

/**
 *  Charge DA_PokemonDatabase au lancement du jeu et le rend accessible partout
 *  (C++ : UPokemonDataSubsystem::GetDatabase(this) ; Blueprint : nœud « Get Pokemon Database »).
 *  Existe sur le serveur comme sur chaque client.
 *  Chemin de l'asset réglable dans Config/DefaultGame.ini.
 */
UCLASS(Config=Game)
class UPokemonDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** La base de données Pokémon, nullptr si l'asset est introuvable */
	UFUNCTION(BlueprintPure, Category="Pokemon|Data", meta=(WorldContext="WorldContextObject", DisplayName="Get Pokemon Database"))
	static UPokemonDatabase* GetDatabase(const UObject* WorldContextObject);

private:

	/** Asset DA_PokemonDatabase à charger */
	UPROPERTY(Config)
	TSoftObjectPtr<UPokemonDatabase> DatabaseAsset;

	UPROPERTY(Transient)
	TObjectPtr<UPokemonDatabase> Database;
};
