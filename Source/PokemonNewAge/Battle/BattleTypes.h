#pragma once

#include "CoreMinimal.h"
#include "Pokemon/PokemonInstance.h"
#include "BattleTypes.generated.h"

/**
 *  Types partagés par le système de combat (docs/COMBAT.md).
 *  Côté 0 = le joueur, côté 1 = l'adversaire (Pokémon sauvage, dresseur, ou un autre joueur plus tard).
 *  Tout est fait de FName et de nombres : un combat se réplique en envoyant ces structs.
 */

UENUM(BlueprintType)
enum class EBattleType : uint8
{
	Wild,
	Trainer
};

UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	NotStarted,
	/** Chaque côté choisit son action (Attaque, Sac, Pokémon, Fuite) */
	WaitingForActions,
	/** Un Pokémon est K.O. : son dresseur doit en envoyer un autre */
	WaitingForReplacement,
	/** Un Pokémon veut apprendre une 5e attaque : le joueur choisit laquelle oublier */
	WaitingForMoveLearn,
	Ended
};

UENUM(BlueprintType)
enum class EBattleOutcome : uint8
{
	None,
	/** Le joueur a gagné */
	Won,
	/** Toute l'équipe du joueur est K.O. */
	Lost,
	/** Le joueur a fui (ou Cyclone a mis fin au combat sauvage) */
	Fled,
	/** Le Pokémon sauvage est capturé */
	Caught
};

UENUM(BlueprintType)
enum class EBattleActionType : uint8
{
	None,
	Move,
	Switch,
	Item,
	Run
};

/** Pourquoi une action est refusée (l'UI affiche le message et le joueur rechoisit) */
UENUM(BlueprintType)
enum class EBattleActionError : uint8
{
	None,
	WrongPhase,
	AlreadyChosen,
	InvalidMove,
	NoPPLeft,
	InvalidPokemon,
	AlreadyInBattle,
	PokemonFainted,
	Trapped,
	CannotRunFromTrainer,
	NoItem,
	ItemNotUsable,
	ItemHasNoEffect,
	CannotCatchTrainerPokemon,
	/** Équipe et PC pleins : plus de place pour un Pokémon capturé */
	NoRoomForPokemon
};

/** Ce qu'un côté fait pendant le tour. */
USTRUCT(BlueprintType)
struct FBattleAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	EBattleActionType Type = EBattleActionType::None;

	/** Move : case de l'attaque (0 à 3). Lutte est choisie toute seule quand il n'y a plus de PP */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 MoveSlot = 0;

	/** Switch : Pokémon de l'équipe à envoyer. Item : Pokémon soigné (ignoré pour une Ball) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	int32 PartyIndex = 0;

	/** Item : ligne de DT_Items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	FName Item;

	static FBattleAction MakeMove(int32 Slot) { FBattleAction A; A.Type = EBattleActionType::Move; A.MoveSlot = Slot; return A; }
	static FBattleAction MakeSwitch(int32 Index) { FBattleAction A; A.Type = EBattleActionType::Switch; A.PartyIndex = Index; return A; }
	static FBattleAction MakeItem(FName ItemId, int32 Index = 0) { FBattleAction A; A.Type = EBattleActionType::Item; A.Item = ItemId; A.PartyIndex = Index; return A; }
	static FBattleAction MakeRun() { FBattleAction A; A.Type = EBattleActionType::Run; return A; }
};

/** Ce qui s'est passé : l'UI les joue un par un (texte, animation, barre de PV). */
UENUM(BlueprintType)
enum class EBattleEventType : uint8
{
	BattleStarted,
	SentOut,
	Withdrew,
	MoveUsed,
	/** L'attaque échoue (déjà endormi, PV au max...) */
	MoveFailed,
	Missed,
	/** Immunité de type (Effectiveness = 0) */
	NoEffect,
	/** PV perdus par une attaque. Amount = dégâts, HPAfter = PV restants */
	Damage,
	CriticalHit,
	/** Après les dégâts : Effectiveness = 2 (super efficace), 0.5 (pas très efficace)... */
	Effectiveness,
	/** Amount = nombre de coups */
	MultiHit,
	/** PV rendus (Soin, objet, Vampigraine) */
	Heal,
	/** Contrecoup (Bélier, Lutte) */
	Recoil,
	/** Statut majeur infligé : Status */
	StatusApplied,
	StatusCured,
	/** Dégâts de brûlure ou de poison en fin de tour */
	StatusDamage,
	/** Ne peut pas attaquer : Status = Sleep, Freeze ou Paralysis */
	StatusPrevented,
	WokeUp,
	Thawed,
	/** Effet volatil infligé (Ailment = Confusion, LeechSeed, Trap) */
	VolatileApplied,
	/** Est confus (au moment d'attaquer) */
	IsConfused,
	ConfusionEnded,
	/** Se blesse dans sa confusion */
	HurtByConfusion,
	Flinched,
	/** Stat change de Amount crans (Amount = 0 : ne peut plus monter/baisser) */
	StatChanged,
	/** Abri bloque l'attaque */
	Protected,
	/** Se protège (Abri) */
	ProtectStarted,
	FocusEnergy,
	/** Vampigraine draine Amount PV */
	LeechSeedDrain,
	/** Ligotage, Étreinte : Amount = dégâts de fin de tour */
	TrapDamage,
	TrapEnded,
	Fainted,
	ItemUsed,
	/** Amount = numéro de la secousse (1 à 3) */
	BallShake,
	Caught,
	BrokeFree,
	FleeSucceeded,
	FleeFailed,
	/** Cyclone, Hurlement */
	ForcedOut,
	ExpGained,
	/** Amount = nouveau niveau */
	LevelUp,
	MoveLearned,
	/** 4 attaques déjà connues : le joueur doit choisir (phase WaitingForMoveLearn) */
	MoveLearnPending,
	/** Le joueur a renoncé à apprendre Move */
	MoveNotLearned,
	MoneyWon,
	MoneyLost,
	/** Rien ne se passe (attaque pas encore gérée par le combat) */
	NothingHappened,
	BattleEnded
};

USTRUCT(BlueprintType)
struct FBattleEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EBattleEventType Type = EBattleEventType::BattleStarted;

	/** Côté concerné (0 joueur, 1 adversaire), -1 si aucun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 Side = -1;

	/** Pokémon concerné dans l'équipe de ce côté */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 PartyIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName Item;

	/** MoveLearned : attaque oubliée à la place (None si une case était libre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName ForgottenMove;

	/** Espèce (SentOut, Caught) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName Species;

	/** Dégâts, soin, XP, niveau, crans, argent... selon Type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 Amount = 0;

	/** PV du Pokémon concerné après l'événement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 HPAfter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	float Effectiveness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EPokemonStat Stat = EPokemonStat::Attack;

	/** StatChanged : crans demandés par l'attaque (+2...). Amount = changement réel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 RequestedStages = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EMajorStatus Status = EMajorStatus::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EMoveAilment Ailment = EMoveAilment::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EBattleOutcome Outcome = EBattleOutcome::None;
};

/** Crans de stats en combat, -6 à +6. Remis à 0 quand le Pokémon sort. */
USTRUCT(BlueprintType)
struct FBattleStatStages
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 Attack = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 Defense = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 SpecialAttack = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 SpecialDefense = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 Speed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 Accuracy = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle") int32 Evasion = 0;

	int32 Get(EPokemonStat Stat) const;
	/** Ajoute Delta crans et renvoie le changement réel (0 si déjà au bout) */
	int32 Add(EPokemonStat Stat, int32 Delta);
};

/** État du Pokémon actif qui disparaît quand il rentre (crans, confusion...). */
USTRUCT(BlueprintType)
struct FBattlerVolatile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FBattleStatStages Stages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 ConfusionTurns = 0;

	/** Tours restants de Ligotage / Étreinte (empêche de fuir et de changer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 TrapTurns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	bool bLeechSeeded = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	bool bFocusEnergy = false;

	/** Compteur de Toxik (dégâts N/16) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 ToxicCounter = 0;

	/** Abri réussis d'affilée (chaque nouvel essai a 1 chance sur 3^N) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 ProtectChain = 0;

	// Remis à zéro au début de chaque tour
	bool bProtected = false;
	bool bFlinched = false;
	bool bUsedProtectThisTurn = false;
	/** Son K.O. a déjà été traité */
	bool bFainted = false;
};

/** Un côté du combat : son équipe (copie, réécrite dans le PlayerState à la fin) et son Pokémon actif. */
USTRUCT(BlueprintType)
struct FBattleSide
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	TArray<FPokemonInstance> Party;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 ActiveIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FBattlerVolatile Volatile;

	/** Sac (copie). Vide pour un Pokémon sauvage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	TArray<FItemStack> Inventory;

	/** Ligne de DT_Trainers (dresseur), None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName TrainerId;

	/** Les actions de ce côté sont choisies par UBattleAI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	bool bIsAI = false;

	/** Ses Pokémon gagnent de l'XP (le joueur contre un sauvage ou un dresseur PNJ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	bool bGainsExperience = false;

	/** Argent du joueur et nombre de badges (pour l'argent perdu en cas de défaite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 BadgeCount = 0;

	/** Faux si l'équipe et le PC sont pleins : les Poké Balls sont refusées (SCRUM-42) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	bool bCanStoreCaughtPokemon = true;

	const FPokemonInstance* GetActive() const { return Party.IsValidIndex(ActiveIndex) ? &Party[ActiveIndex] : nullptr; }
	FPokemonInstance* GetActive() { return Party.IsValidIndex(ActiveIndex) ? &Party[ActiveIndex] : nullptr; }
	bool HasPokemonLeft() const;
	int32 GetItemCount(FName Item) const;
};

/** Évolution à jouer après le combat (séquence animée, SCRUM-47). */
USTRUCT(BlueprintType)
struct FPendingEvolution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 PartyIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName NewSpecies;
};

/** Résultat donné à la fin d'un combat. */
USTRUCT(BlueprintType)
struct FBattleResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EBattleOutcome Outcome = EBattleOutcome::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EBattleType BattleType = EBattleType::Wild;

	/** Ligne de DT_Trainers du dresseur battu ou qui a gagné */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName TrainerId;

	/** Argent gagné (positif) ou perdu (négatif) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 MoneyDelta = 0;

	/** Outcome = Caught : le Pokémon capturé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FPokemonInstance CaughtPokemon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	TArray<FPendingEvolution> Evolutions;
};

/** Ce que l'UI a besoin de savoir pour afficher le combat (envoyé au client avec chaque lot d'événements). */
USTRUCT(BlueprintType)
struct FBattleSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EBattleType BattleType = EBattleType::Wild;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	EBattlePhase Phase = EBattlePhase::NotStarted;

	/** Côté de ce joueur (0 en solo) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 LocalSide = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	TArray<FBattleSide> Sides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 TurnNumber = 0;

	/** Phase WaitingForMoveLearn : Pokémon et attaque en attente */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	int32 PendingLearnPartyIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle")
	FName PendingLearnMove;
};
