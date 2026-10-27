#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Battle/BattleTypes.h"
#include "BattleSubsystem.generated.h"

class APlayerController;
class APokemonPlayerState;
class UBattleSession;
class UPokemonDatabase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnServerBattleEnded, APlayerController*, Player, FName, TrainerGroup, const FBattleResult&, Result);

/** Un combat en cours et le joueur qui le mène. */
USTRUCT()
struct FActiveBattle
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerController> Player;

	UPROPERTY()
	TObjectPtr<UBattleSession> Session;

	/** Dresseur tel que demandé par le dialogue (RivalLab, Brock), pour le drapeau Beaten_<Groupe> */
	UPROPERTY()
	FName TrainerGroup;
};

/**
 *  Lance et suit les combats, sur le serveur uniquement (SCRUM-68).
 *  Chaque combat est un UBattleSession à part : plusieurs joueurs peuvent combattre en même temps.
 *  À la fin, le résultat est écrit dans le PlayerState du joueur : équipe (PV, PP, XP, attaques),
 *  sac, argent, drapeau Beaten_<Dresseur>, Pokémon capturé, Pokédex.
 */
UCLASS(Config=Game)
class UBattleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Combat contre un Pokémon sauvage précis (scripts, légendaires) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle")
	UBattleSession* StartWildBattle(APlayerController* Player, FName Species, int32 Level);

	/** Rencontre dans les hautes herbes : tire l'espèce et le niveau dans DT_Encounters (Route1_Walk...) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle")
	UBattleSession* StartWildEncounter(APlayerController* Player, FName EncounterTableId);

	/**
	 *  Combat contre un dresseur (événement StartTrainerBattle des dialogues).
	 *  TrainerGroup = ligne de DT_Trainers (Brock) ou groupe du rival (RivalLab : on prend RivalLab_<starter du joueur>).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle")
	UBattleSession* StartTrainerBattle(APlayerController* Player, FName TrainerGroup);

	UFUNCTION(BlueprintPure, Category="Battle")
	UBattleSession* GetBattle(const APlayerController* Player) const;

	/** Ligne de DT_Trainers pour ce groupe et les drapeaux du joueur (Starter_Bulbasaur...), None si aucune */
	static FName ResolveTrainerId(const UPokemonDatabase* Database, FName TrainerGroup, const TArray<FName>& PlayerFlags);

	/** Fin d'un combat, sur le serveur (enchaîner la conversation Trainer_<Groupe>_PlayerWins...) */
	UPROPERTY(BlueprintAssignable, Category="Battle")
	FOnServerBattleEnded OnBattleEnded;

	/** IV des Pokémon des dresseurs, et des champions d'arène */
	UPROPERTY(Config)
	int32 TrainerIV = 15;

	UPROPERTY(Config)
	int32 GymLeaderIV = 25;

protected:

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	UBattleSession* BeginBattle(APlayerController* Player, UBattleSession* Session, FName TrainerGroup);
	bool MakePlayerSide(APlayerController* Player, FBattleSide& OutSide) const;
	void HandleEvents(UBattleSession* Session, const TArray<FBattleEvent>& Events);
	void HandleEnded(UBattleSession* Session, const FBattleResult& Result);
	const FActiveBattle* FindBattle(const UBattleSession* Session) const;
	const UPokemonDatabase* GetDatabase() const;

	UPROPERTY(Transient)
	TArray<FActiveBattle> Battles;
};
