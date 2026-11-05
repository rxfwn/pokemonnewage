#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PokemonFollowerComponent.generated.h"

class APokemonFollower;
class USoundBase;

/**
 *  Fait suivre le Pokémon de tête de l'équipe (SCRUM-78), sur le PlayerController de chaque joueur.
 *  Le serveur regarde l'équipe quatre fois par seconde : le premier Pokémon (s'il n'est pas K.O.)
 *  sort et suit le joueur, selon FollowMode de DT_Species (marche, vole, flotte, ou reste dans sa Ball).
 *  Il rentre pendant les combats et change tout seul après un échange d'ordre ou une évolution.
 *  Voir docs/POKEMON_QUI_SUIT.md.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UPokemonFollowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPokemonFollowerComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Montre ou range le Pokémon qui suit (console : Suiveur) */
	UFUNCTION(BlueprintCallable, Category="Follower")
	void SetFollowerEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Follower")
	bool IsFollowerEnabled() const { return bFollowerEnabled; }

	/** Le Pokémon qui suit ce joueur (serveur seulement, nul chez les clients) */
	UFUNCTION(BlueprintPure, Category="Follower")
	APokemonFollower* GetFollower() const { return Follower; }

	/** Caresse un Pokémon qui suit (le sien ou celui d'un ami) : envoyée au serveur */
	void PetFollower(APokemonFollower* Target);

	/** Siffle son Pokémon (touche F) : il vient au pied ; un 2e coup de sifflet le laisse libre */
	UFUNCTION(BlueprintCallable, Category="Follower")
	void Whistle();

	/** Son du sifflet, joué chez le joueur qui siffle (vide = pas de son) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower")
	TObjectPtr<USoundBase> WhistleSound;

	/** Classe du Pokémon qui suit (un Blueprint enfant pour changer les réglages). Vide = APokemonFollower */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Follower")
	TSubclassOf<APokemonFollower> FollowerClass;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerSetFollowerEnabled(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void ServerPetFollower(APokemonFollower* Target);

	UFUNCTION(Server, Reliable)
	void ServerWhistle();

	virtual void BeginPlay() override;

private:

	/** Serveur : sort, change ou range le Pokémon qui suit */
	void Refresh();
	void DestroyFollower();
	bool IsInBattle() const;

	UPROPERTY(Replicated)
	bool bFollowerEnabled = true;

	UPROPERTY(Transient)
	TObjectPtr<APokemonFollower> Follower;
};
