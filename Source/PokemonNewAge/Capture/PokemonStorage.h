#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pokemon/PokemonInstance.h"
#include "PokemonStorage.generated.h"

/** Pourquoi le PC refuse une demande (texte : UPokemonStorage::GetErrorText). */
UENUM(BlueprintType)
enum class EPokemonStorageError : uint8
{
	None,
	InvalidSlot,
	/** Équipe pleine : impossible de retirer un Pokémon du PC */
	PartyFull,
	/** Le PC est plein */
	StorageFull,
	/** On ne dépose pas son dernier Pokémon en forme */
	LastAblePokemon,
	/** Pas pendant un combat */
	InBattle,
	/** Surnom trop long ou caractères interdits */
	InvalidNickname
};

/** Ce que le joueur apprend après une capture (envoyé par le serveur à son client). */
USTRUCT(BlueprintType)
struct FCaughtPokemonInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	FPokemonInstance Pokemon;

	/** Vrai : dans l'équipe ; faux : envoyé au PC */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	bool bInParty = true;

	/** Place dans l'équipe ou dans le PC */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	int32 Index = INDEX_NONE;

	/** Première capture de cette espèce : afficher sa fiche du Pokédex */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	bool bNewInPokedex = false;
};

/**
 *  Règles de l'équipe et du PC de Léo (SCRUM-42), sans dépendre d'un acteur : le PlayerState garde les
 *  tableaux, UCaptureComponent appelle ces fonctions côté serveur. Testées sans Unreal (Tools/BattleSim).
 *
 *  Le PC est un seul tableau ; l'UI le découpe en boîtes de BoxSize (index / BoxSize = numéro de boîte).
 *  Comme dans les jeux depuis la 3G : un Pokémon déposé au PC est soigné.
 */
UCLASS()
class UPokemonStorage : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static constexpr int32 MaxPartySize = 6;
	static constexpr int32 BoxSize = 30;
	static constexpr int32 BoxCount = 12;
	static constexpr int32 MaxStorageSize = BoxSize * BoxCount;
	static constexpr int32 MaxNicknameLength = 12;

	/** Encore de la place pour un Pokémon capturé (équipe ou PC) */
	static bool HasRoom(const TArray<FPokemonInstance>& Party, const TArray<FPokemonInstance>& Storage);

	/**
	 *  Range un Pokémon capturé ou reçu : dans l'équipe s'il reste une place, sinon au PC (soigné).
	 *  OutInParty / OutIndex disent où il est. Faux si tout est plein ou si le Pokémon est vide.
	 */
	static bool Store(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, const FPokemonInstance& Pokemon,
		bool& bOutInParty, int32& OutIndex);

	/** Équipe -> PC. Refusé pour le dernier Pokémon capable de se battre */
	static bool Deposit(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, int32 PartyIndex, EPokemonStorageError& OutError);

	/** PC -> équipe (à la fin) */
	static bool Withdraw(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, int32 StorageIndex, EPokemonStorageError& OutError);

	/** Échange un Pokémon de l'équipe et un du PC (le premier va au PC, soigné) */
	static bool Swap(TArray<FPokemonInstance>& Party, TArray<FPokemonInstance>& Storage, int32 PartyIndex, int32 StorageIndex, EPokemonStorageError& OutError);

	/** Relâche un Pokémon du PC (définitif) */
	static bool Release(TArray<FPokemonInstance>& Storage, int32 StorageIndex, EPokemonStorageError& OutError);

	/** Surnom nettoyé (espaces en trop retirés). Vide = nom de l'espèce. Faux s'il est trop long ou bizarre */
	static bool CleanNickname(const FString& Wanted, FString& OutNickname);

	/**
	 *  Messages à afficher après la capture, dans l'ordre : fiche du Pokédex (première capture),
	 *  envoi au PC, question du surnom.
	 */
	static TArray<FText> GetCaughtMessages(const class UPokemonDatabase* Database, const FCaughtPokemonInfo& Info);

	UFUNCTION(BlueprintPure, Category="Pokemon|Storage")
	static FText GetErrorText(EPokemonStorageError Error);

	UFUNCTION(BlueprintPure, Category="Pokemon|Storage")
	static int32 GetBoxSize() { return BoxSize; }

	UFUNCTION(BlueprintPure, Category="Pokemon|Storage")
	static int32 GetBoxCount() { return BoxCount; }
};
