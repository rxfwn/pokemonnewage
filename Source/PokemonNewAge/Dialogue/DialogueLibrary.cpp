#include "Dialogue/DialogueLibrary.h"

#include "Engine/DataTable.h"

bool UDialogueLibrary::AreConditionsMet(const FDialogueRow& Conversation, const TArray<FName>& PlayerFlags)
{
	for (const FName& Flag : Conversation.RequiredFlags)
	{
		if (!PlayerFlags.Contains(Flag))
		{
			return false;
		}
	}
	for (const FName& Flag : Conversation.BlockedByFlags)
	{
		if (PlayerFlags.Contains(Flag))
		{
			return false;
		}
	}
	return true;
}

bool UDialogueLibrary::FindTalkConversation(const UDataTable* Dialogues, FName Npc, const TArray<FName>& PlayerFlags,
	FName& OutConversationId, FDialogueRow& OutConversation)
{
	OutConversationId = NAME_None;
	if (!Dialogues)
	{
		return false;
	}

	const FDialogueRow* Best = nullptr;
	Dialogues->ForeachRow<FDialogueRow>(TEXT("FindTalkConversation"),
		[&](const FName& Key, const FDialogueRow& Row)
		{
			if (Row.Npc == Npc && Row.Trigger == EDialogueTrigger::Talk && AreConditionsMet(Row, PlayerFlags)
				&& (!Best || Row.Priority > Best->Priority))
			{
				Best = &Row;
				OutConversationId = Key;
			}
		});

	if (!Best)
	{
		return false;
	}
	OutConversation = *Best;
	return true;
}

FName UDialogueLibrary::GetTrainerOutcomeConversation(const UDataTable* Dialogues, FName TrainerGroup, bool bPlayerWon)
{
	if (!Dialogues)
	{
		return NAME_None;
	}

	const FName Id(*FString::Printf(TEXT("Trainer_%s_%s"), *TrainerGroup.ToString(), bPlayerWon ? TEXT("PlayerWins") : TEXT("PlayerLoses")));
	if (Dialogues->GetRowMap().Contains(Id))
	{
		return Id;
	}

	static const FName Blackout(TEXT("Battle_PlayerBlackout"));
	return !bPlayerWon && Dialogues->GetRowMap().Contains(Blackout) ? Blackout : NAME_None;
}

FText UDialogueLibrary::FormatDialogueText(const FText& Text, const TMap<FString, FText>& Arguments)
{
	FFormatNamedArguments Args;
	for (const TPair<FString, FText>& Pair : Arguments)
	{
		Args.Add(Pair.Key, Pair.Value);
	}
	return FText::Format(FTextFormat(Text), Args);
}
