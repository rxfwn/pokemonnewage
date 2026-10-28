#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Battle/BattleTypes.h"
#include "BattleText.generated.h"

class UPokemonDatabase;

/**
 *  Textes du combat en français, à partir des événements (« Bulbizarre utilise Charge ! »).
 *  L'UI du combat peut s'en servir telle quelle ou écrire les siens.
 */
UCLASS()
class UBattleText : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Texte d'un événement, vide si l'événement n'a pas de texte (BattleStarted...) */
	UFUNCTION(BlueprintPure, Category="Battle|Text", meta=(WorldContext="WorldContextObject"))
	static FText GetEventText(const UObject* WorldContextObject, const FBattleSnapshot& Snapshot, const FBattleEvent& Event);

	static FText MakeEventText(const UPokemonDatabase* Database, const FBattleSnapshot& Snapshot, const FBattleEvent& Event);

	/** Message quand le serveur refuse une action */
	UFUNCTION(BlueprintPure, Category="Battle|Text")
	static FText GetActionErrorText(EBattleActionError Error);
};
