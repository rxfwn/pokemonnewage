#include "Battle/BattleSubsystem.h"
#include "Battle/BattleComponent.h"
#include "Battle/BattleSession.h"
#include "Core/PokemonPlayerState.h"
#include "Progression/ProgressionComponent.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "PokemonNewAge.h"

namespace
{
	APokemonPlayerState* GetPokemonPlayerState(const APlayerController* Player)
	{
		return Player ? Player->GetPlayerState<APokemonPlayerState>() : nullptr;
	}

	UBattleComponent* GetBattleComponent(const APlayerController* Player)
	{
		return Player ? Player->FindComponentByClass<UBattleComponent>() : nullptr;
	}
}

bool UBattleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

const UPokemonDatabase* UBattleSubsystem::GetDatabase() const
{
	return UPokemonDataSubsystem::GetDatabase(this);
}

UBattleSession* UBattleSubsystem::GetBattle(const APlayerController* Player) const
{
	const FActiveBattle* Battle = Battles.FindByPredicate([Player](const FActiveBattle& B) { return B.Player == Player; });
	return Battle ? Battle->Session.Get() : nullptr;
}

const FActiveBattle* UBattleSubsystem::FindBattle(const UBattleSession* Session) const
{
	return Battles.FindByPredicate([Session](const FActiveBattle& B) { return B.Session == Session; });
}

FName UBattleSubsystem::ResolveTrainerId(const UPokemonDatabase* Database, FName TrainerGroup, const TArray<FName>& PlayerFlags)
{
	if (!Database || !Database->Trainers || TrainerGroup.IsNone())
	{
		return NAME_None;
	}
	if (Database->FindTrainer(TrainerGroup))
	{
		return TrainerGroup;
	}

	// Rival : RivalLab_<starter du joueur>, d'après le drapeau Starter_<Espèce>
	const FString Prefix = TrainerGroup.ToString() + TEXT("_");
	for (const FName& RowName : Database->Trainers->GetRowNames())
	{
		if (!RowName.ToString().StartsWith(Prefix))
		{
			continue;
		}
		const FTrainerRow* Row = Database->FindTrainer(RowName);
		if (Row && (Row->RequiredPlayerStarter.IsNone()
			|| PlayerFlags.Contains(FName(*(TEXT("Starter_") + Row->RequiredPlayerStarter.ToString())))))
		{
			return RowName;
		}
	}
	return NAME_None;
}

bool UBattleSubsystem::MakePlayerSide(APlayerController* Player, FBattleSide& OutSide) const
{
	const APokemonPlayerState* State = GetPokemonPlayerState(Player);
	if (!State || !State->HasAuthority() || !State->HasPokemonAbleToBattle() || GetBattle(Player))
	{
		return false;
	}
	OutSide = FBattleSide();
	OutSide.Party = State->Party;
	OutSide.Inventory = State->Inventory;
	OutSide.Money = State->Money;
	OutSide.BadgeCount = State->Badges.Num();
	OutSide.bGainsExperience = true;
	return true;
}

UBattleSession* UBattleSubsystem::StartWildBattle(APlayerController* Player, FName Species, int32 Level)
{
	const UPokemonDatabase* Database = GetDatabase();
	FBattleSide PlayerSide;
	if (!Database || !MakePlayerSide(Player, PlayerSide))
	{
		return nullptr;
	}

	FRandomStream Stream(FMath::Rand());
	FPokemonInstance Wild;
	if (!UPokemonLibrary::CreatePokemon(Database, Species, Level, Stream, Wild))
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("StartWildBattle : espèce %s introuvable"), *Species.ToString());
		return nullptr;
	}
	if (const APokemonPlayerState* State = GetPokemonPlayerState(Player))
	{
		Wild.MetLocation = State->CurrentLocation;
	}

	UBattleSession* Session = NewObject<UBattleSession>(this);
	if (!Session->InitWildBattle(Database, PlayerSide, Wild, static_cast<int32>(Stream.GetUnsignedInt())))
	{
		return nullptr;
	}
	return BeginBattle(Player, Session, NAME_None);
}

UBattleSession* UBattleSubsystem::StartWildEncounter(APlayerController* Player, FName EncounterTableId)
{
	const UPokemonDatabase* Database = GetDatabase();
	if (!Database)
	{
		return nullptr;
	}
	FRandomStream Stream(FMath::Rand());
	FName Species;
	int32 Level = 1;
	if (!Database->RollEncounter(EncounterTableId, Stream, Species, Level))
	{
		return nullptr;
	}
	return StartWildBattle(Player, Species, Level);
}

UBattleSession* UBattleSubsystem::StartTrainerBattle(APlayerController* Player, FName TrainerGroup)
{
	const UPokemonDatabase* Database = GetDatabase();
	const APokemonPlayerState* State = GetPokemonPlayerState(Player);
	FBattleSide PlayerSide;
	if (!Database || !State || !MakePlayerSide(Player, PlayerSide))
	{
		return nullptr;
	}

	const FName TrainerId = ResolveTrainerId(Database, TrainerGroup, State->StoryFlags);
	const FTrainerRow* Trainer = Database->FindTrainer(TrainerId);
	if (!Trainer)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("StartTrainerBattle : dresseur %s introuvable dans DT_Trainers"), *TrainerGroup.ToString());
		return nullptr;
	}

	UBattleSession* Session = NewObject<UBattleSession>(this);
	if (!Session->InitTrainerBattle(Database, PlayerSide, TrainerId, Trainer->bIsGymLeader ? GymLeaderIV : TrainerIV, FMath::Rand()))
	{
		return nullptr;
	}
	return BeginBattle(Player, Session, TrainerGroup);
}

UBattleSession* UBattleSubsystem::BeginBattle(APlayerController* Player, UBattleSession* Session, FName TrainerGroup)
{
	FActiveBattle& Battle = Battles.AddDefaulted_GetRef();
	Battle.Player = Player;
	Battle.Session = Session;
	Battle.TrainerGroup = TrainerGroup;

	Session->OnEvents.AddUObject(this, &UBattleSubsystem::HandleEvents);
	Session->OnEnded.AddUObject(this, &UBattleSubsystem::HandleEnded);

	if (APokemonPlayerState* State = GetPokemonPlayerState(Player))
	{
		for (const FPokemonInstance& Pokemon : Session->GetSide(1).Party)
		{
			// Sauvage : vu tout de suite ; dresseur : chaque Pokémon est vu quand il sort (voir HandleEvents)
			if (Session->GetBattleType() == EBattleType::Wild)
			{
				State->MarkSeen(Pokemon.Species);
			}
		}
	}

	if (UBattleComponent* Component = GetBattleComponent(Player))
	{
		Component->ClientBattleStarted(Session->MakeSnapshot(0));
	}
	Session->Start();
	return Session;
}

void UBattleSubsystem::HandleEvents(UBattleSession* Session, const TArray<FBattleEvent>& Events)
{
	const FActiveBattle* Battle = FindBattle(Session);
	if (!Battle)
	{
		return;
	}

	if (APokemonPlayerState* State = GetPokemonPlayerState(Battle->Player))
	{
		for (const FBattleEvent& Event : Events)
		{
			if (Event.Type == EBattleEventType::SentOut && Event.Side == 1)
			{
				State->MarkSeen(Event.Species);
			}
		}
	}

	if (UBattleComponent* Component = GetBattleComponent(Battle->Player))
	{
		Component->ClientBattleEvents(Events, Session->MakeSnapshot(0));
	}
}

void UBattleSubsystem::HandleEnded(UBattleSession* Session, const FBattleResult& Result)
{
	const int32 Index = Battles.IndexOfByPredicate([Session](const FActiveBattle& B) { return B.Session == Session; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	const FActiveBattle Battle = Battles[Index];
	Battles.RemoveAt(Index);

	if (APokemonPlayerState* State = GetPokemonPlayerState(Battle.Player))
	{
		const FBattleSide& PlayerSide = Session->GetSide(0);
		State->Party = PlayerSide.Party;
		State->Inventory = PlayerSide.Inventory;
		State->AddMoney(Result.MoneyDelta);

		if (Result.Outcome == EBattleOutcome::Won && !Battle.TrainerGroup.IsNone())
		{
			State->AddFlag(FName(*(TEXT("Beaten_") + Battle.TrainerGroup.ToString())));
		}
		if (Result.Outcome == EBattleOutcome::Caught)
		{
			FPokemonInstance Caught = Result.CaughtPokemon;
			if (Caught.MetLocation.IsNone())
			{
				Caught.MetLocation = State->CurrentLocation;
			}
			State->AddPokemon(Caught);
		}
	}

	if (UBattleComponent* Component = GetBattleComponent(Battle.Player))
	{
		Component->ClientBattleEnded(Result);
	}
	// Évolutions : séquence 3D et confirmation une fois l'écran de combat fermé (docs/PROGRESSION.md)
	if (UProgressionComponent* Progression = Battle.Player ? Battle.Player->FindComponentByClass<UProgressionComponent>() : nullptr)
	{
		Progression->AddBattleEvolutions(Result.Evolutions);
	}

	OnBattleEnded.Broadcast(Battle.Player, Battle.TrainerGroup, Result);
}
