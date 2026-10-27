#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Dialogue/DialogueRows.h"
#include "DialogueLibrary.generated.h"

class UDataTable;

/**
 *  Fonctions de base pour lire DT_Dialogues, en C++ comme en Blueprint.
 *  Elles ne gardent aucun état : on leur passe les drapeaux du joueur qui parle,
 *  ce qui marche pareil en solo et en multijoueur.
 */
UCLASS()
class UDialogueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Vrai si le joueur a tous les RequiredFlags et aucun des BlockedByFlags */
	UFUNCTION(BlueprintPure, Category="Dialogue")
	static bool AreConditionsMet(const FDialogueRow& Conversation, const TArray<FName>& PlayerFlags);

	/** Conversation à jouer quand le joueur parle à Npc : la Talk valide de plus haute priorité */
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	static bool FindTalkConversation(const UDataTable* Dialogues, FName Npc, const TArray<FName>& PlayerFlags,
		FName& OutConversationId, FDialogueRow& OutConversation);

	/**
	 *  Conversation de fin de combat contre un dresseur (TrainerGroup = Brock, RivalLab...) :
	 *  Trainer_<Groupe>_PlayerWins ou Trainer_<Groupe>_PlayerLoses.
	 *  Défaite sans réplique propre au dresseur : Battle_PlayerBlackout. None si rien ne convient.
	 */
	UFUNCTION(BlueprintPure, Category="Dialogue")
	static FName GetTrainerOutcomeConversation(const UDataTable* Dialogues, FName TrainerGroup, bool bPlayerWon);

	/** Remplace {PlayerName}, {StarterName}... par les valeurs données (clé sans accolades) */
	UFUNCTION(BlueprintPure, Category="Dialogue")
	static FText FormatDialogueText(const FText& Text, const TMap<FString, FText>& Arguments);
};
