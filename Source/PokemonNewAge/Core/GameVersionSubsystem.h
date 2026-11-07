#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "GameVersionSubsystem.generated.h"

class UNetDriver;

/**
 *  Version du jeu installé et refus des mondes d'une autre version (docs/MISES_A_JOUR.md).
 *  - La version vient de version.txt, à la racine du build packagé, écrit par Tools/Updater/publier_version.py
 *    (par exemple « 2026.11.07-143005 »). Dans l'éditeur ou sans ce fichier, c'est « dev ».
 *  - En rejoignant un monde, le joueur envoie sa version (?Version=...) ; l'hôte refuse une version différente
 *    (APokemonNewAgeGameMode::PreLogin). « dev » est toujours accepté, pour les tests depuis l'éditeur.
 *  - La raison d'un refus ou d'une coupure est gardée ici et affichée dans le Pokédex > En ligne.
 */
UCLASS()
class UGameVersionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Version de ce build (« dev » dans l'éditeur) */
	static const FString& GetVersion();

	/** Un joueur de cette version peut-il rejoindre notre monde ? */
	static bool IsCompatible(const FString& OtherVersion);

	static UGameVersionSubsystem* Get(const UObject* WorldContextObject);

	/** Dernière connexion refusée ou coupée (vide si aucune) */
	const FText& GetLastNetworkError() const { return LastNetworkError; }
	void ClearLastNetworkError() { LastNetworkError = FText::GetEmpty(); }

private:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FText LastNetworkError;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
