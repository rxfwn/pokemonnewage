#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Battle/BattleTypes.h"
#include "BattleSession.generated.h"

class UPokemonDatabase;
struct FMoveRow;
struct FTrainerRow;
struct FItemRow;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattleEventsNative, class UBattleSession* /*Session*/, const TArray<FBattleEvent>& /*Events*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattleEndedNative, class UBattleSession* /*Session*/, const FBattleResult& /*Result*/);

/**
 *  Un combat Pokémon (SCRUM-34 à 38, 41, 44 à 48) : un objet à part, sans acteur ni affichage.
 *  Il ne vit que sur le serveur (UBattleSubsystem), reçoit les choix de chaque côté, résout le tour
 *  et produit une liste d'événements (FBattleEvent) que l'UI de chaque joueur rejoue à son rythme.
 *
 *  Déroulement :
 *   1. InitWildBattle / InitTrainerBattle puis Start.
 *   2. Phase WaitingForActions : SubmitAction pour chaque côté humain (l'IA choisit seule).
 *      Quand tous ont choisi, le tour est résolu : ordre par priorité puis Vitesse.
 *   3. Après le tour : WaitingForMoveLearn (5e attaque), WaitingForReplacement (Pokémon K.O.)
 *      ou de nouveau WaitingForActions, jusqu'à Ended.
 *  Les deux côtés sont symétriques : un combat entre deux joueurs marchera de la même façon.
 */
UCLASS(BlueprintType)
class UBattleSession : public UObject
{
	GENERATED_BODY()

public:

	/** Combat contre un Pokémon sauvage */
	bool InitWildBattle(const UPokemonDatabase* InDatabase, const FBattleSide& Player, const FPokemonInstance& WildPokemon, int32 Seed);

	/** Combat contre le dresseur TrainerId (ligne de DT_Trainers). TrainerIV : IV de ses Pokémon */
	bool InitTrainerBattle(const UPokemonDatabase* InDatabase, const FBattleSide& Player, FName TrainerId, int32 TrainerIV, int32 Seed);

	/** Combat entre deux côtés déjà prêts (futur combat entre joueurs) */
	bool InitBattle(const UPokemonDatabase* InDatabase, EBattleType InType, const FBattleSide& Side0, const FBattleSide& Side1, int32 Seed);

	/** Envoie les premiers Pokémon et passe à WaitingForActions */
	void Start();

	/** Choix d'un côté pour ce tour. Faux (avec la raison) si l'action est impossible : le joueur rechoisit */
	UFUNCTION(BlueprintCallable, Category="Battle")
	bool SubmitAction(int32 Side, const FBattleAction& Action, EBattleActionError& OutError);

	/** Phase WaitingForReplacement : Pokémon à envoyer. En combat sauvage, Action Run est aussi permise */
	UFUNCTION(BlueprintCallable, Category="Battle")
	bool ChooseReplacement(int32 Side, int32 PartyIndex, EBattleActionError& OutError);

	/** Phase WaitingForReplacement, combat sauvage : fuir au lieu d'envoyer un Pokémon */
	UFUNCTION(BlueprintCallable, Category="Battle")
	bool RunInsteadOfReplacement(int32 Side, EBattleActionError& OutError);

	/** Phase WaitingForMoveLearn : case à oublier (0 à 3), -1 = ne pas apprendre */
	UFUNCTION(BlueprintCallable, Category="Battle")
	bool ResolveMoveLearn(int32 Side, int32 ForgetSlot);

	UFUNCTION(BlueprintPure, Category="Battle")
	EBattlePhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Battle")
	EBattleType GetBattleType() const { return BattleType; }

	const FBattleSide& GetSide(int32 Side) const { return Sides[Side == 1 ? 1 : 0]; }

	const FBattleResult& GetResult() const { return Result; }

	UFUNCTION(BlueprintPure, Category="Battle")
	int32 GetTurnNumber() const { return TurnNumber; }

	/** Ce côté doit encore répondre (action, remplaçant ou attaque à oublier) */
	UFUNCTION(BlueprintPure, Category="Battle")
	bool IsWaitingFor(int32 Side) const;

	/** Plus aucun PP : la seule attaque possible est Lutte */
	UFUNCTION(BlueprintPure, Category="Battle")
	bool MustStruggle(int32 Side) const;

	/** Tout ce qui s'est passé depuis le début, dans l'ordre */
	const TArray<FBattleEvent>& GetEventLog() const { return EventLog; }

	/** Ce qu'un joueur voit du combat */
	FBattleSnapshot MakeSnapshot(int32 LocalSide) const;

	const UPokemonDatabase* GetDatabase() const { return Database.Get(); }

	/** Nouveaux événements (après Start, chaque tour, chaque réponse) */
	FOnBattleEventsNative OnEvents;

	/** Le combat est fini (GetResult) */
	FOnBattleEndedNative OnEnded;

private:

	struct FQueuedAction
	{
		int32 Side = 0;
		FBattleAction Action;
		int32 Bracket = 0;
		int32 Speed = 0;
		float TieBreak = 0.f;
	};

	// Déroulement
	void ResolveTurn();
	void Advance();
	void Finish(EBattleOutcome Outcome);
	void Flush();
	void Emit(const FBattleEvent& Event);
	FBattleEvent MakeEvent(EBattleEventType Type, int32 Side) const;
	bool ValidateAction(int32 Side, const FBattleAction& Action, EBattleActionError& OutError) const;
	void SendOut(int32 Side, int32 PartyIndex);
	int32 PickAIReplacement(int32 Side) const;

	// Actions
	void ExecuteAction(const FQueuedAction& Queued);
	void ExecuteMove(int32 Side, int32 MoveSlot);
	void ExecuteSwitch(int32 Side, int32 PartyIndex);
	void ExecuteItem(int32 Side, FName Item, int32 PartyIndex);
	void ExecuteRun(int32 Side);
	bool CanAct(int32 Side);
	void DoDamagingMove(int32 Side, FName MoveId, const FMoveRow& Move);
	void DoUniqueMove(int32 Side, FName MoveId, const FMoveRow& Move);
	int32 DealDamage(int32 Side, int32 Amount, EBattleEventType EventType, FName MoveId = NAME_None);
	void Heal(int32 Side, int32 Amount, FName MoveId = NAME_None);
	bool TryApplyAilment(int32 SourceSide, int32 TargetSide, EMoveAilment Ailment, FName MoveId, const FMoveRow& Move, bool bReportFailure);
	void ApplyStatChanges(int32 TargetSide, const FMoveRow& Move, FName MoveId, bool bReportFailure);
	void EndOfTurn();

	// Objets tenus
	/** Objet tenu par le Pokémon actif du camp s'il a cet effet, sinon nul */
	const FItemRow* GetHeldItem(int32 Side, EHeldItemEffect Effect) const;
	/** Annonce l'objet tenu ; une baie est mangée (bConsume) */
	void ActivateHeldItem(int32 Side, bool bConsume);
	/** Baies des deux Pokémon actifs : soin sous la moitié des PV, statut soigné */
	void CheckHeldBerries();

	// K.O. et expérience
	void CheckFainted();
	void OnFainted(int32 Side);
	void GiveExperience(int32 WinnerSide, const FPokemonInstance& Fainted);
	void QueueLevelUpMoves(int32 Side, int32 PartyIndex, int32 Level);

	// Calculs sur le Pokémon actif
	int32 GetBattleStat(int32 Side, EPokemonStat Stat, bool bIgnorePositive = false, bool bIgnoreNegative = false) const;
	int32 GetSpeed(int32 Side) const;
	void GetActiveTypes(int32 Side, EPokemonType& OutType1, EPokemonType& OutType2) const;
	bool IsActiveAlive(int32 Side) const;
	bool Roll(int32 Percent);

	UPROPERTY(Transient)
	TObjectPtr<UPokemonDatabase> Database;

	EBattleType BattleType = EBattleType::Wild;
	EBattlePhase Phase = EBattlePhase::NotStarted;
	FBattleSide Sides[2];
	FRandomStream Stream;
	int32 TurnNumber = 0;

	/** Actions choisies ce tour */
	TOptional<FBattleAction> Pending[2];
	/** Côtés qui doivent envoyer un Pokémon */
	bool NeedsReplacement[2] = { false, false };
	/** Pokémon de chaque côté qui ont affronté l'adversaire actuel (partage de l'XP) */
	TArray<int32> Participants[2];
	/** Pokémon qui ont pris un niveau (évolution en fin de combat) */
	TArray<int32> LeveledUp;
	int32 FleeAttempts = 0;

	struct FPendingLearn
	{
		int32 Side = 0;
		int32 PartyIndex = 0;
		FName Move;
	};
	TArray<FPendingLearn> PendingLearns;

	/** Fin décidée pendant le tour, appliquée quand les attaques en attente sont réglées */
	EBattleOutcome PendingOutcome = EBattleOutcome::None;

	FBattleResult Result;
	bool bEndBroadcast = false;
	TArray<FBattleEvent> EventLog;
	TArray<FBattleEvent> NewEvents;
};
