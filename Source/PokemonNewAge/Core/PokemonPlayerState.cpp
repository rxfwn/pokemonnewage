#include "Core/PokemonPlayerState.h"
#include "Net/UnrealNetwork.h"

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

void APokemonPlayerState::MarkCaught(FName Species)
{
	if (!Species.IsNone())
	{
		MarkSeen(Species);
		PokedexCaught.AddUnique(Species);
	}
}
