#include "Battle/BattleComponent.h"
#include "Battle/BattleSession.h"
#include "Battle/BattleSubsystem.h"
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
			PrintBattleLine(FString::Printf(TEXT("Quoi ? Le Pokemon n.%d evolue en %s ! (sequence apres le combat, ou Evoluer %d / Evoluer %d 0)"),
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
