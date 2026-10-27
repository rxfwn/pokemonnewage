#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Pokemon/PokemonInstance.h"
#include "PokemonPlayerState.generated.h"

/**
 *  Tout ce qui appartient à un joueur (SCRUM-68) : son équipe, son PC, son sac, son argent,
 *  ses badges, ses drapeaux d'histoire (dialogues) et son Pokédex.
 *  Modifié seulement par le serveur (combat, dialogues, boutique) ; répliqué vers son client.
 *  Le GameMode du jeu l'utilise par défaut (APokemonNewAgeGameMode).
 */
UCLASS()
class APokemonPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Équipe, 6 au maximum. L'ordre est celui du menu Pokémon */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Pokemon")
	TArray<FPokemonInstance> Party;

	/** PC de Léo (une seule boîte pour l'instant) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Pokemon")
	TArray<FPokemonInstance> StoragePC;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Bag")
	TArray<FItemStack> Inventory;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Bag")
	int32 Money = 3000;

	/** Badges obtenus (BoulderBadge...) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FName> Badges;

	/** Drapeaux d'histoire (GotStarter, Starter_Bulbasaur, Beaten_Brock...), voir docs/DIALOGUES.md */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FName> StoryFlags;

	/** Pokédex : espèces vues et capturées */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Pokedex")
	TArray<FName> PokedexSeen;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Pokedex")
	TArray<FName> PokedexCaught;

	/** Lieu où le joueur se trouve (pour MetLocation et le Centre Pokémon) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	FName CurrentLocation;

	// ------------------------------------------------ Lecture (partout)

	UFUNCTION(BlueprintPure, Category="Story")
	bool HasFlag(FName Flag) const { return StoryFlags.Contains(Flag); }

	UFUNCTION(BlueprintPure, Category="Bag")
	int32 GetItemCount(FName Item) const;

	/** Au moins un Pokémon de l'équipe peut se battre */
	UFUNCTION(BlueprintPure, Category="Pokemon")
	bool HasPokemonAbleToBattle() const;

	// ------------------------------------------------ Modifications (serveur seulement)

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Story")
	void AddFlag(FName Flag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bag")
	void AddItem(FName Item, int32 Count = 1);

	/** Faux si le joueur n'en a pas assez (rien n'est retiré) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bag")
	bool RemoveItem(FName Item, int32 Count = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bag")
	void AddMoney(int32 Amount);

	/** Ajoute à l'équipe, ou au PC si l'équipe est pleine. Vrai si dans l'équipe */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokemon")
	bool AddPokemon(const FPokemonInstance& Pokemon);

	/** Soigne toute l'équipe (Centre Pokémon, maman) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokemon")
	void HealParty();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokedex")
	void MarkSeen(FName Species);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokedex")
	void MarkCaught(FName Species);

	static constexpr int32 MaxPartySize = 6;
};
