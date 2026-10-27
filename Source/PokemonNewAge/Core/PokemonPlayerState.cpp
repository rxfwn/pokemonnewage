#include "Core/PokemonPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Pokemon/PokemonDatabase.h"

void APokemonPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// L'équipe et les badges se voient chez tout le monde (futur combat entre joueurs), le reste chez le joueur seul
	DOREPLIFETIME(APokemonPlayerState, Party);
	DOREPLIFETIME(APokemonPlayerState, Badges);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, StoragePC, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, Inventory, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, Money, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, StoryFlags, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, PokedexSeen, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, PokedexCaught, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, CurrentLocation, COND_OwnerOnly);
	DOREPLIFETIME(APokemonPlayerState, TrainerNumber);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, PlayTimeBeforeSession, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APokemonPlayerState, SessionStartServerTime, COND_OwnerOnly);
}

void APokemonPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (TrainerNumber == 0)
		{
			TrainerNumber = FMath::RandRange(10000, 65535);
		}
		SessionStartServerTime = GetWorld()->GetTimeSeconds();
	}
}

float APokemonPlayerState::GetPlayTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const float Now = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	return PlayTimeBeforeSession + FMath::Max(0.f, Now - SessionStartServerTime);
}

FPlayerSaveData APokemonPlayerState::MakeSaveData() const
{
	FPlayerSaveData Data;
	Data.TrainerName = GetPlayerName();
	Data.TrainerNumber = TrainerNumber;
	Data.Party = Party;
	Data.StoragePC = StoragePC;
	Data.Inventory = Inventory;
	Data.Money = Money;
	Data.Badges = Badges;
	Data.StoryFlags = StoryFlags;
	Data.PokedexSeen = PokedexSeen;
	Data.PokedexCaught = PokedexCaught;
	Data.CurrentLocation = CurrentLocation;
	Data.PlayTimeSeconds = GetPlayTimeSeconds();
	return Data;
}

void APokemonPlayerState::ApplySaveData(const FPlayerSaveData& Data)
{
	if (!Data.TrainerName.IsEmpty())
	{
		SetPlayerName(Data.TrainerName);
	}
	if (Data.TrainerNumber > 0)
	{
		TrainerNumber = Data.TrainerNumber;
	}
	Party = Data.Party;
	if (Party.Num() > MaxPartySize)
	{
		Party.SetNum(MaxPartySize);
	}
	StoragePC = Data.StoragePC;
	Inventory = Data.Inventory;
	Inventory.RemoveAll([](const FItemStack& S) { return S.Item.IsNone() || S.Count <= 0; });
	Money = FMath::Clamp(Data.Money, 0, 999999);
	Badges = Data.Badges;
	StoryFlags = Data.StoryFlags;
	PokedexSeen = Data.PokedexSeen;
	PokedexCaught = Data.PokedexCaught;
	CurrentLocation = Data.CurrentLocation;
	PlayTimeBeforeSession = FMath::Max(0.f, Data.PlayTimeSeconds);
	SessionStartServerTime = GetWorld()->GetTimeSeconds();
}

int32 APokemonPlayerState::GetItemCount(FName Item) const
{
	const FItemStack* Stack = Inventory.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; });
	return Stack ? Stack->Count : 0;
}

bool APokemonPlayerState::HasPokemonAbleToBattle() const
{
	return Party.ContainsByPredicate([](const FPokemonInstance& Pokemon) { return Pokemon.CanBattle(); });
}

void APokemonPlayerState::AddFlag(FName Flag)
{
	if (!Flag.IsNone())
	{
		StoryFlags.AddUnique(Flag);
	}
}

void APokemonPlayerState::AddItem(FName Item, int32 Count)
{
	if (Item.IsNone() || Count <= 0)
	{
		return;
	}
	if (FItemStack* Stack = Inventory.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; }))
	{
		Stack->Count += Count;
		return;
	}
	FItemStack Stack;
	Stack.Item = Item;
	Stack.Count = Count;
	Inventory.Add(Stack);
}

bool APokemonPlayerState::RemoveItem(FName Item, int32 Count)
{
	FItemStack* Stack = Inventory.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; });
	if (!Stack || Stack->Count < Count)
	{
		return false;
	}
	Stack->Count -= Count;
	Inventory.RemoveAll([](const FItemStack& S) { return S.Count <= 0; });
	return true;
}

void APokemonPlayerState::AddMoney(int32 Amount)
{
	Money = FMath::Clamp(Money + Amount, 0, 999999);
}

bool APokemonPlayerState::AddPokemon(const FPokemonInstance& Pokemon)
{
	if (!Pokemon.IsValid())
	{
		return false;
	}
	MarkCaught(Pokemon.Species);
	if (Party.Num() < MaxPartySize)
	{
		Party.Add(Pokemon);
		return true;
	}
	StoragePC.Add(Pokemon);
	return false;
}

void APokemonPlayerState::HealParty()
{
	for (FPokemonInstance& Pokemon : Party)
	{
		UPokemonLibrary::HealFully(Pokemon);
	}
}

void APokemonPlayerState::MarkSeen(FName Species)
{
	if (!Species.IsNone())
	{
		PokedexSeen.AddUnique(Species);
	}
}

bool APokemonPlayerState::SwapPartyPokemon(int32 IndexA, int32 IndexB)
{
	if (!Party.IsValidIndex(IndexA) || !Party.IsValidIndex(IndexB) || IndexA == IndexB)
	{
		return false;
	}
	Party.Swap(IndexA, IndexB);
	return true;
}

bool APokemonPlayerState::CanUseItemOnPokemon(const FItemRow& Item, const FPokemonInstance& Pokemon)
{
	if (!Item.bUsableInField || Item.IsPokeBall() || !Pokemon.IsValid())
	{
		return false;
	}
	if (Pokemon.IsFainted())
	{
		return Item.ReviveHPPercent > 0;
	}
	const bool bHeals = (Item.HealHP > 0 || Item.bHealFull) && Pokemon.CurrentHP < Pokemon.GetMaxHP();
	const bool bCures = Pokemon.Status != EMajorStatus::None && Item.CuresStatus.Contains(Pokemon.Status);
	return bHeals || bCures;
}

bool APokemonPlayerState::UseItemOnPokemon(const UPokemonDatabase* Database, FName ItemId, int32 PartyIndex)
{
	const FItemRow* Item = Database ? Database->FindItem(ItemId) : nullptr;
	if (!Item || !Party.IsValidIndex(PartyIndex) || GetItemCount(ItemId) <= 0 || !CanUseItemOnPokemon(*Item, Party[PartyIndex]))
	{
		return false;
	}

	// Mêmes règles que les objets en combat (UBattleSession::ExecuteItem)
	FPokemonInstance& Pokemon = Party[PartyIndex];
	if (Pokemon.IsFainted())
	{
		Pokemon.CurrentHP = FMath::Max(1, Pokemon.GetMaxHP() * Item->ReviveHPPercent / 100);
		Pokemon.Status = EMajorStatus::None;
		Pokemon.SleepTurns = 0;
	}
	else
	{
		if (Item->HealHP > 0 || Item->bHealFull)
		{
			Pokemon.CurrentHP = Item->bHealFull ? Pokemon.GetMaxHP() : FMath::Min(Pokemon.GetMaxHP(), Pokemon.CurrentHP + Item->HealHP);
		}
		if (Pokemon.Status != EMajorStatus::None && Item->CuresStatus.Contains(Pokemon.Status))
		{
			Pokemon.Status = EMajorStatus::None;
			Pokemon.SleepTurns = 0;
		}
	}
	RemoveItem(ItemId, 1);
	return true;
}

void APokemonPlayerState::MarkCaught(FName Species)
{
	if (!Species.IsNone())
	{
		MarkSeen(Species);
		PokedexCaught.AddUnique(Species);
	}
}
