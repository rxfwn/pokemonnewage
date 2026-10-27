#include "Battle/BattleComponent.h"
#include "Battle/BattleSession.h"
#include "Battle/BattleSubsystem.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Battle/BattleText.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PokemonNewAge.h"

namespace
{
	void PrintBattleLine(const FString& Line, const FColor& Color = FColor::White)
	{
		if (Line.IsEmpty())
		{
			return;
		}
		UE_LOG(LogPokemonNewAge, Log, TEXT("[Combat] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, Color, Line);
		}
	}
}

UBattleComponent::UBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UBattleSubsystem* UBattleComponent::GetBattleSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
}

APlayerController* UBattleComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

// ---------------------------------------------------------------- Demandes de l'UI

void UBattleComponent::SubmitAction(const FBattleAction& Action) { ServerSubmitAction(Action); }
void UBattleComponent::ChooseReplacement(int32 PartyIndex) { ServerChooseReplacement(PartyIndex); }
void UBattleComponent::RunInsteadOfReplacement() { ServerRunInsteadOfReplacement(); }
void UBattleComponent::ResolveMoveLearn(int32 ForgetSlot) { ServerResolveMoveLearn(ForgetSlot); }
void UBattleComponent::ConfirmEvolution(int32 PartyIndex, bool bAccept) { ServerConfirmEvolution(PartyIndex, bAccept); }
void UBattleComponent::LearnPendingMove(int32 PartyIndex, FName Move, int32 ForgetSlot) { ServerLearnPendingMove(PartyIndex, Move, ForgetSlot); }

// ---------------------------------------------------------------- Serveur

void UBattleComponent::ServerSubmitAction_Implementation(const FBattleAction& Action)
{
	UBattleSubsystem* Subsystem = GetBattleSubsystem();
	UBattleSession* Session = Subsystem ? Subsystem->GetBattle(GetPlayerController()) : nullptr;
	EBattleActionError Error = EBattleActionError::WrongPhase;
	if (!Session || !Session->SubmitAction(0, Action, Error))
	{
		ClientActionRefused(Error);
	}
}

void UBattleComponent::ServerChooseReplacement_Implementation(int32 PartyIndex)
{
	UBattleSubsystem* Subsystem = GetBattleSubsystem();
	UBattleSession* Session = Subsystem ? Subsystem->GetBattle(GetPlayerController()) : nullptr;
	EBattleActionError Error = EBattleActionError::WrongPhase;
	if (!Session || !Session->ChooseReplacement(0, PartyIndex, Error))
	{
		ClientActionRefused(Error);
	}
}

void UBattleComponent::ServerRunInsteadOfReplacement_Implementation()
{
	UBattleSubsystem* Subsystem = GetBattleSubsystem();
	UBattleSession* Session = Subsystem ? Subsystem->GetBattle(GetPlayerController()) : nullptr;
	EBattleActionError Error = EBattleActionError::WrongPhase;
	if (!Session || !Session->RunInsteadOfReplacement(0, Error))
	{
		ClientActionRefused(Error);
	}
}

void UBattleComponent::ServerResolveMoveLearn_Implementation(int32 ForgetSlot)
{
	UBattleSubsystem* Subsystem = GetBattleSubsystem();
	UBattleSession* Session = Subsystem ? Subsystem->GetBattle(GetPlayerController()) : nullptr;
	if (!Session || !Session->ResolveMoveLearn(0, ForgetSlot))
	{
		ClientActionRefused(EBattleActionError::WrongPhase);
	}
}

void UBattleComponent::ServerConfirmEvolution_Implementation(int32 PartyIndex, bool bAccept)
{
	const int32 PendingIndex = PendingEvolutions.IndexOfByPredicate([PartyIndex](const FPendingEvolution& E) { return E.PartyIndex == PartyIndex; });
	APlayerController* Player = GetPlayerController();
	APokemonPlayerState* State = Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (PendingIndex == INDEX_NONE || !State || !State->Party.IsValidIndex(PartyIndex))
	{
		return;
	}
	const FName NewSpecies = PendingEvolutions[PendingIndex].NewSpecies;
	PendingEvolutions.RemoveAt(PendingIndex);
	if (!bAccept)
	{
		return;
	}

	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	TArray<FName> MovesToLearn;
	if (!UPokemonLibrary::Evolve(Database, State->Party[PartyIndex], NewSpecies, MovesToLearn))
	{
		return;
	}
	State->MarkCaught(NewSpecies);

	// Case libre : appris tout de suite ; sinon le joueur choisit (LearnPendingMove)
	TArray<FName> ToChoose;
	for (FName Move : MovesToLearn)
	{
		if (!UPokemonLibrary::LearnMove(Database, State->Party[PartyIndex], Move))
		{
			ToChoose.Add(Move);
		}
	}
	if (ToChoose.Num() > 0)
	{
		AllowedMovesToLearn.FindOrAdd(PartyIndex).Append(ToChoose);
		ClientMovesToLearn(PartyIndex, ToChoose);
	}
}

void UBattleComponent::ServerLearnPendingMove_Implementation(int32 PartyIndex, FName Move, int32 ForgetSlot)
{
	TArray<FName>* Allowed = AllowedMovesToLearn.Find(PartyIndex);
	APlayerController* Player = GetPlayerController();
	APokemonPlayerState* State = Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (!Allowed || !Allowed->Contains(Move) || !State || !State->Party.IsValidIndex(PartyIndex))
	{
		return;
	}
	Allowed->Remove(Move);
	if (ForgetSlot >= 0 || State->Party[PartyIndex].Moves.Num() < 4)
	{
		UPokemonLibrary::LearnMove(UPokemonDataSubsystem::GetDatabase(this), State->Party[PartyIndex], Move, ForgetSlot);
	}
}

// ---------------------------------------------------------------- Client

void UBattleComponent::ClientBattleStarted_Implementation(const FBattleSnapshot& InSnapshot)
{
	bInBattle = true;
	Snapshot = InSnapshot;
	OnBattleStarted.Broadcast(Snapshot);
}

void UBattleComponent::ClientBattleEvents_Implementation(const TArray<FBattleEvent>& Events, const FBattleSnapshot& InSnapshot)
{
	Snapshot = InSnapshot;
	if (bPrintEventsToScreen)
	{
		for (const FBattleEvent& Event : Events)
		{
			PrintBattleLine(UBattleText::GetEventText(this, Snapshot, Event).ToString());
		}
		for (const FBattleSide& Side : Snapshot.Sides)
		{
			if (const FPokemonInstance* Active = Side.GetActive())
			{
				PrintBattleLine(FString::Printf(TEXT("  %s N.%d : %d/%d PV"), *Active->Species.ToString(), Active->Level, Active->CurrentHP, Active->GetMaxHP()), FColor::Yellow);
			}
		}
		if (Snapshot.Phase == EBattlePhase::WaitingForActions)
		{
			PrintBattleLine(TEXT("A toi : Attaque 1-4, Changer 1-6, Objet PokeBall, Fuite"), FColor::Green);
		}
		else if (Snapshot.Phase == EBattlePhase::WaitingForReplacement)
		{
			PrintBattleLine(TEXT("Envoie un Pokemon : Changer 1-6 (ou Fuite en combat sauvage)"), FColor::Green);
		}
		else if (Snapshot.Phase == EBattlePhase::WaitingForMoveLearn)
		{
			PrintBattleLine(TEXT("Oublier 1-4, ou Oublier 0 pour ne pas apprendre"), FColor::Green);
		}
	}
	OnBattleEvents.Broadcast(Events, Snapshot);
}

void UBattleComponent::ClientBattleEnded_Implementation(const FBattleResult& Result)
{
	bInBattle = false;
	if (bPrintEventsToScreen)
	{
		for (const FPendingEvolution& Evolution : Result.Evolutions)
		{
			PrintBattleLine(FString::Printf(TEXT("Quoi ? Le Pokemon n.%d evolue en %s ! (Evoluer %d, ou Evoluer %d 0 pour annuler)"),
				Evolution.PartyIndex + 1, *Evolution.NewSpecies.ToString(), Evolution.PartyIndex + 1, Evolution.PartyIndex + 1), FColor::Green);
		}
	}
	OnBattleEnded.Broadcast(Result);
}

void UBattleComponent::ClientActionRefused_Implementation(EBattleActionError Error)
{
	if (bPrintEventsToScreen)
	{
		PrintBattleLine(UBattleText::GetActionErrorText(Error).ToString(), FColor::Red);
	}
	OnActionRefused.Broadcast(Error);
}

void UBattleComponent::ClientMovesToLearn_Implementation(int32 PartyIndex, const TArray<FName>& Moves)
{
	MovesToLearnPartyIndex = PartyIndex;
	MovesToLearnAfterEvolution = Moves;
	if (bPrintEventsToScreen)
	{
		for (FName Move : Moves)
		{
			PrintBattleLine(FString::Printf(TEXT("Le Pokemon n.%d veut apprendre %s (4 capacites deja connues)"), PartyIndex + 1, *Move.ToString()), FColor::Green);
		}
	}
}
