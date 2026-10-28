#include "Capture/PokemonStorage.h"
#include "Pokemon/PokemonDatabase.h"

namespace
{
	/** Un autre Pokémon que celui-ci peut encore se battre */
	bool HasOtherAblePokemon(const TArray<FPokemonInstance>& Party, int32 ExceptIndex)
	{
		for (int32 Index = 0; Index < Party.Num(); ++Index)
		{
			if (Index != ExceptIndex && Party[Index].CanBattle())
			{
				return true;
			}
		}
		return false;
	}

	void PutInStorage(TArray<FPokemonInstance>& Storage, const FPokemonInstance& Pokemon)
	{
		FPokemonInstance Stored = Pokemon;
		UPokemonLibrary::HealFully(Stored);
		Storage.Add(Stored);
	}
}

bool UPokemonStorage::HasRoom(const TArray<FPokemonInstance>& Party, const TArray<FPokemonInstance>& Storage)
{
	return Party.Num() < MaxPartySize || Storage.Num() < MaxStorageSize;
}

bool UPokemonStorage::Store(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, const FPokemonInstance& Pokemon,
	bool& bOutInParty, int32& OutIndex)
{
	bOutInParty = false;
	OutIndex = INDEX_NONE;
	if (!Pokemon.IsValid() || !HasRoom(Party, Storage))
	{
		return false;
	}
	if (Party.Num() < MaxPartySize)
	{
		bOutInParty = true;
		OutIndex = Party.Add(Pokemon);
		return true;
	}
	PutInStorage(Storage, Pokemon);
	OutIndex = Storage.Num() - 1;
	return true;
}

bool UPokemonStorage::Deposit(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, int32 PartyIndex, EPokemonStorageError& OutError)
{
	OutError = EPokemonStorageError::None;
	if (!Party.IsValidIndex(PartyIndex))
	{
		OutError = EPokemonStorageError::InvalidSlot;
	}
	else if (Storage.Num() >= MaxStorageSize)
	{
		OutError = EPokemonStorageError::StorageFull;
	}
	else if (!HasOtherAblePokemon(Party, PartyIndex))
	{
		OutError = EPokemonStorageError::LastAblePokemon;
	}
	if (OutError != EPokemonStorageError::None)
	{
		return false;
	}
	PutInStorage(Storage, Party[PartyIndex]);
	Party.RemoveAt(PartyIndex);
	return true;
}

bool UPokemonStorage::Withdraw(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, int32 StorageIndex, EPokemonStorageError& OutError)
{
	OutError = EPokemonStorageError::None;
	if (!Storage.IsValidIndex(StorageIndex))
	{
		OutError = EPokemonStorageError::InvalidSlot;
		return false;
	}
	if (Party.Num() >= MaxPartySize)
	{
		OutError = EPokemonStorageError::PartyFull;
		return false;
	}
	Party.Add(Storage[StorageIndex]);
	Storage.RemoveAt(StorageIndex);
	return true;
}

bool UPokemonStorage::Swap(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, int32 PartyIndex, int32 StorageIndex, EPokemonStorageError& OutError)
{
	OutError = EPokemonStorageError::None;
	if (!Party.IsValidIndex(PartyIndex) || !Storage.IsValidIndex(StorageIndex))
	{
		OutError = EPokemonStorageError::InvalidSlot;
		return false;
	}
	// Celui qui arrive du PC est soigné : il peut toujours se battre, l'équipe n'est jamais vidée
	const FPokemonInstance FromStorage = Storage[StorageIndex];
	FPokemonInstance ToStorage = Party[PartyIndex];
	UPokemonLibrary::HealFully(ToStorage);
	Party[PartyIndex] = FromStorage;
	Storage[StorageIndex] = ToStorage;
	return true;
}

bool UPokemonStorage::Release(TArray<FPokemonInstance>& Storage, int32 StorageIndex, EPokemonStorageError& OutError)
{
	OutError = EPokemonStorageError::None;
	if (!Storage.IsValidIndex(StorageIndex))
	{
		OutError = EPokemonStorageError::InvalidSlot;
		return false;
	}
	Storage.RemoveAt(StorageIndex);
	return true;
}

bool UPokemonStorage::CleanNickname(const FString& Wanted, FString& OutNickname)
{
	OutNickname = Wanted.TrimStartAndEnd();
	if (OutNickname.Len() > MaxNicknameLength)
	{
		return false;
	}
	for (int32 Index = 0; Index < OutNickname.Len(); ++Index)
	{
		const TCHAR Char = OutNickname[Index];
		// Pas de caractères de contrôle ni de balises de texte enrichi (<, >) ; un seul espace d'affilée
		if (Char < 32 || Char == TEXT('<') || Char == TEXT('>')
			|| (FChar::IsWhitespace(Char) && Index > 0 && FChar::IsWhitespace(OutNickname[Index - 1])))
		{
			return false;
		}
	}
	return true;
}

TArray<FText> UPokemonStorage::GetCaughtMessages(const UPokemonDatabase* Database, const FCaughtPokemonInfo& Info)
{
	TArray<FText> Messages;
	const FString Name = UPokemonLibrary::GetDisplayName(Database, Info.Pokemon).ToString();
	if (Info.bNewInPokedex)
	{
		Messages.Add(FText::FromString(FString::Printf(TEXT("Les données de %s ont été ajoutées au Pokédex."), *Name)));
	}
	if (!Info.bInParty)
	{
		Messages.Add(FText::FromString(FString::Printf(TEXT("%s a été envoyé au PC de Léo (boîte %d)."), *Name, Info.Index / BoxSize + 1)));
	}
	Messages.Add(FText::FromString(FString::Printf(TEXT("Voulez-vous donner un surnom à %s ?"), *Name)));
	return Messages;
}

FText UPokemonStorage::GetErrorText(EPokemonStorageError Error)
{
	switch (Error)
	{
	case EPokemonStorageError::PartyFull:       return FText::FromString(TEXT("Votre équipe est pleine !"));
	case EPokemonStorageError::StorageFull:     return FText::FromString(TEXT("Le PC est plein !"));
	case EPokemonStorageError::LastAblePokemon: return FText::FromString(TEXT("C'est votre dernier Pokémon en forme !"));
	case EPokemonStorageError::InBattle:        return FText::FromString(TEXT("Pas pendant un combat !"));
	case EPokemonStorageError::InvalidNickname: return FText::FromString(TEXT("Ce surnom ne convient pas (12 caractères au plus)."));
	case EPokemonStorageError::InvalidSlot:     return FText::FromString(TEXT("Il n'y a pas de Pokémon ici."));
	case EPokemonStorageError::None:            return FText::GetEmpty();
	default:                                    return FText::FromString(TEXT("Impossible pour l'instant."));
	}
}
