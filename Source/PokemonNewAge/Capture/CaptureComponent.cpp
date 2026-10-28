#include "Capture/CaptureComponent.h"
#include "Capture/PokeBallThrow.h"
#include "Battle/BattleComponent.h"
#include "Battle/BattleSubsystem.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PokemonNewAge.h"

UCaptureComponent::UCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	PokeBallThrowClass = APokeBallThrow::StaticClass();
}

void UCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	// OnBattleEvents n'est diffusé que chez le joueur concerné : le lancer de Ball ne se voit que chez lui
	if (UBattleComponent* Battle = GetOwner()->FindComponentByClass<UBattleComponent>())
	{
		Battle->OnBattleEvents.AddDynamic(this, &UCaptureComponent::HandleBattleEvents);
	}
}

APokemonPlayerState* UCaptureComponent::GetPokemonPlayerState() const
{
	const APlayerController* Player = Cast<APlayerController>(GetOwner());
	return Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
}

bool UCaptureComponent::IsInBattle() const
{
	const UWorld* World = GetWorld();
	const UBattleSubsystem* Battles = World ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
	return Battles && Battles->GetBattle(Cast<APlayerController>(GetOwner())) != nullptr;
}

void UCaptureComponent::Print(const FString& Line, const FColor& Color) const
{
	if (!bPrintToScreen || Line.IsEmpty())
	{
		return;
	}
	UE_LOG(LogPokemonNewAge, Log, TEXT("[Capture] %s"), *Line);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, Color, Line);
	}
}

// ---------------------------------------------------------------- Demandes de l'UI

void UCaptureComponent::SetCaughtNickname(const FString& Nickname) { ServerSetCaughtNickname(Nickname); }
void UCaptureComponent::DepositPokemon(int32 PartyIndex) { ServerDepositPokemon(PartyIndex); }
void UCaptureComponent::WithdrawPokemon(int32 StorageIndex) { ServerWithdrawPokemon(StorageIndex); }
void UCaptureComponent::SwapWithStorage(int32 PartyIndex, int32 StorageIndex) { ServerSwapWithStorage(PartyIndex, StorageIndex); }
void UCaptureComponent::ReleasePokemon(int32 StorageIndex) { ServerReleasePokemon(StorageIndex); }
void UCaptureComponent::SetEncountersEnabled(bool bEnabled) { ServerSetEncountersEnabled(bEnabled); }

// ---------------------------------------------------------------- Serveur

bool UCaptureComponent::HandlePokemonCaught(const FPokemonInstance& Caught)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	if (!State)
	{
		return false;
	}

	FCaughtPokemonInfo Info;
	Info.Pokemon = Caught;
	Info.bNewInPokedex = !State->PokedexCaught.Contains(Caught.Species);
	if (!UPokemonStorage::Store(State->Party, State->StoragePC, Caught, Info.bInParty, Info.Index))
	{
		// Ne devrait pas arriver : le combat refuse les Balls quand tout est plein (bCanStoreCaughtPokemon)
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Capture : plus de place pour %s"), *Caught.Species.ToString());
		return false;
	}
	State->MarkCaught(Caught.Species);

	bPendingNicknameInParty = Info.bInParty;
	PendingNicknameIndex = Info.Index;
	ClientPokemonCaught(Info);
	return true;
}

void UCaptureComponent::ServerSetCaughtNickname_Implementation(const FString& Nickname)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	TArray<FPokemonInstance>* List = State ? (bPendingNicknameInParty ? &State->Party : &State->StoragePC) : nullptr;
	if (!List || !List->IsValidIndex(PendingNicknameIndex))
	{
		return;
	}
	FString Clean;
	if (!UPokemonStorage::CleanNickname(Nickname, Clean))
	{
		// On garde la question ouverte : le joueur peut proposer un autre surnom
		ClientStorageRefused(EPokemonStorageError::InvalidNickname);
		return;
	}
	(*List)[PendingNicknameIndex].Nickname = Clean;
	PendingNicknameIndex = INDEX_NONE;
}

bool UCaptureComponent::CanUseStorage()
{
	// Toute modification de l'équipe ou du PC ferme la question du surnom (les places ont pu bouger)
	PendingNicknameIndex = INDEX_NONE;
	if (IsInBattle())
	{
		ClientStorageRefused(EPokemonStorageError::InBattle);
		return false;
	}
	return GetPokemonPlayerState() != nullptr;
}

void UCaptureComponent::ServerDepositPokemon_Implementation(int32 PartyIndex)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	EPokemonStorageError Error;
	if (CanUseStorage() && !UPokemonStorage::Deposit(State->Party, State->StoragePC, PartyIndex, Error))
	{
		ClientStorageRefused(Error);
	}
}

void UCaptureComponent::ServerWithdrawPokemon_Implementation(int32 StorageIndex)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	EPokemonStorageError Error;
	if (CanUseStorage() && !UPokemonStorage::Withdraw(State->Party, State->StoragePC, StorageIndex, Error))
	{
		ClientStorageRefused(Error);
	}
}

void UCaptureComponent::ServerSwapWithStorage_Implementation(int32 PartyIndex, int32 StorageIndex)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	EPokemonStorageError Error;
	if (CanUseStorage() && !UPokemonStorage::Swap(State->Party, State->StoragePC, PartyIndex, StorageIndex, Error))
	{
		ClientStorageRefused(Error);
	}
}

void UCaptureComponent::ServerReleasePokemon_Implementation(int32 StorageIndex)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	EPokemonStorageError Error;
	if (CanUseStorage() && !UPokemonStorage::Release(State->StoragePC, StorageIndex, Error))
	{
		ClientStorageRefused(Error);
	}
}

void UCaptureComponent::ServerSetEncountersEnabled_Implementation(bool bEnabled)
{
	bEncountersEnabled = bEnabled;
}

// ---------------------------------------------------------------- Client

void UCaptureComponent::ClientPokemonCaught_Implementation(const FCaughtPokemonInfo& Info)
{
	for (const FText& Message : UPokemonStorage::GetCaughtMessages(UPokemonDataSubsystem::GetDatabase(this), Info))
	{
		Print(Message.ToString());
	}
	Print(TEXT("Console : Surnom <nom> (ou rien pour garder le nom)"), FColor::Green);
	OnPokemonCaught.Broadcast(Info);
}

void UCaptureComponent::ClientStorageRefused_Implementation(EPokemonStorageError Error)
{
	Print(UPokemonStorage::GetErrorText(Error).ToString(), FColor::Red);
	OnStorageRefused.Broadcast(Error);
}

void UCaptureComponent::HandleBattleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& Snapshot)
{
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const APlayerController* Player = Cast<APlayerController>(GetOwner());
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Database || !Pawn || !PokeBallThrowClass)
	{
		return;
	}

	// Un lot d'événements contient au plus un lancer : la Ball, ses secousses, puis capturé ou libéré
	FName Ball;
	int32 Shakes = 0;
	bool bCaught = false;
	bool bResolved = false;
	for (const FBattleEvent& Event : Events)
	{
		if (Event.Type == EBattleEventType::ItemUsed && Event.Side == Snapshot.LocalSide)
		{
			const FItemRow* Item = Database->FindItem(Event.Item);
			if (Item && Item->IsPokeBall())
			{
				Ball = Event.Item;
			}
		}
		else if (!Ball.IsNone() && Event.Type == EBattleEventType::BallShake)
		{
			Shakes = FMath::Max(Shakes, Event.Amount);
		}
		else if (!Ball.IsNone() && (Event.Type == EBattleEventType::Caught || Event.Type == EBattleEventType::BrokeFree))
		{
			bCaught = Event.Type == EBattleEventType::Caught;
			bResolved = true;
			break;
		}
	}
	if (!bResolved)
	{
		return;
	}

	// Départ : main droite du joueur (environ) ; arrivée : le Pokémon sauvage, ou devant le joueur
	const FVector Forward = Pawn->GetActorForwardVector();
	const FVector Start = Pawn->GetActorLocation() + Forward * 40.f + Pawn->GetActorRightVector() * 25.f + FVector(0.f, 0.f, 60.f);
	const FVector Target = bHasBallTargetOverride ? BallTargetOverride : Pawn->GetActorLocation() + Forward * BallTargetDistance;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APokeBallThrow* Throw = GetWorld()->SpawnActor<APokeBallThrow>(PokeBallThrowClass, Start, FRotator::ZeroRotator, Params))
	{
		Throw->Play(Start, Target, Ball, Shakes, bCaught, Pawn);
	}
}
