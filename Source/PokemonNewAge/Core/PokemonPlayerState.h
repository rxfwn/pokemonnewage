#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Pokemon/PokemonInstance.h"
#include "Core/PokemonSaveGame.h"
#include "PokemonPlayerState.generated.h"

class UPokemonDatabase;
struct FItemRow;

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

	/** Serveur : numéro de dresseur tiré au hasard et début du chrono de temps de jeu */
	virtual void BeginPlay() override;

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

	/**
	 *  Dernier Centre Pokémon où l'équipe a été soignée (ou la maison), où le joueur revient si toute son équipe est K.O.
	 *  Id d'un APokemonCenterPoint ; None = jamais soigné (on cherche le point de départ, voir UPokemonCenterComponent)
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	FName LastHealPoint;

	/** Position de ce point, gardée au cas où l'acteur aurait disparu de la map */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	FVector LastHealLocation = FVector::ZeroVector;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	FRotator LastHealRotation = FRotator::ZeroRotator;

	/** Numéro de dresseur (carte Dresseur), tiré au hasard à la première partie */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Story")
	int32 TrainerNumber = 0;

	/** Temps de jeu des sessions précédentes (sauvegarde), en secondes */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Story")
	float PlayTimeBeforeSession = 0.f;

	/** Compteur de pas du Pokédex (compté par le serveur, APokemonNewAgePlayerController::Tick) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Story")
	int32 StepCount = 0;

	/** Longueur d'un pas du compteur, en cm */
	static constexpr float StepLength = 75.f;

	/** Heure du serveur au début de cette session */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Story")
	float SessionStartServerTime = 0.f;

	// ------------------------------------------------ Lecture (partout)

	/** Temps de jeu total en secondes */
	UFUNCTION(BlueprintPure, Category="Story")
	float GetPlayTimeSeconds() const;

	/** Copie de tout ce qui se sauvegarde (sans la position, ajoutée par UPokemonSaveLibrary) */
	FPlayerSaveData MakeSaveData() const;

	/** Vrai si l'objet a un effet hors combat sur ce Pokémon (Potion sur un Pokémon blessé...) */
	static bool CanUseItemOnPokemon(const FItemRow& Item, const FPokemonInstance& Pokemon);

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

	/** Échange deux Pokémon de l'équipe (menu Pokémon > Déplacer) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokemon")
	bool SwapPartyPokemon(int32 IndexA, int32 IndexB);

	/** Fait tenir un objet du sac (l'ancien objet tenu revient dans le sac). Faux si l'objet manque */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokemon")
	bool GiveHeldItem(int32 PartyIndex, FName Item);

	/** Reprend l'objet tenu dans le sac */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pokemon")
	bool TakeHeldItem(int32 PartyIndex);

	/** Utilise un objet du sac sur un Pokémon de l'équipe, hors combat. Faux si sans effet (rien n'est retiré) */
	bool UseItemOnPokemon(const UPokemonDatabase* Database, FName Item, int32 PartyIndex);

	/** Remet une sauvegarde dans ce PlayerState (chargement) */
	void ApplySaveData(const FPlayerSaveData& Data);

	static constexpr int32 MaxPartySize = 6;
};
