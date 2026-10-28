#include "Battle/BattleSession.h"
#include "Battle/BattleAI.h"
#include "Battle/BattleRules.h"
#include "Pokemon/PokemonDatabase.h"

namespace BattleMoves
{
	// Attaques dont l'effet ne tient pas dans les colonnes de DT_Moves
	const FName Struggle(TEXT("Struggle"));
	const FName Toxic(TEXT("Toxic"));
	const FName SeismicToss(TEXT("SeismicToss"));
	const FName NightShade(TEXT("NightShade"));
	const FName SonicBoom(TEXT("SonicBoom"));
	const FName DragonRage(TEXT("DragonRage"));
	const FName SuperFang(TEXT("SuperFang"));
	const FName LowKick(TEXT("LowKick"));
	const FName Magnitude(TEXT("Magnitude"));
	const FName SelfDestruct(TEXT("SelfDestruct"));
	const FName Explosion(TEXT("Explosion"));
	const FName FocusEnergy(TEXT("FocusEnergy"));
	const FName Protect(TEXT("Protect"));
	const FName Detect(TEXT("Detect"));
	const FName Rest(TEXT("Rest"));
	const FName Haze(TEXT("Haze"));

	/** Poudres et spores : sans effet sur le type Plante (6G et plus) */
	bool IsPowder(FName Move)
	{
		static const FName Powders[] = {
			TEXT("StunSpore"), TEXT("SleepPowder"), TEXT("PoisonPowder"), TEXT("Spore"), TEXT("CottonSpore")
		};
		for (const FName& Powder : Powders)
		{
			if (Powder == Move) return true;
		}
		return false;
	}

	/** Puissance de Balayage selon le poids de la cible */
	int32 LowKickPower(float WeightKg)
	{
		return WeightKg < 10.f ? 20 : WeightKg < 25.f ? 40 : WeightKg < 50.f ? 60 : WeightKg < 100.f ? 80 : WeightKg < 200.f ? 100 : 120;
	}

	/** Puissance d'Ampleur (4 à 10) */
	int32 MagnitudePower(FRandomStream& Stream)
	{
		const int32 Roll = Stream.RandRange(0, 99);
		return Roll < 5 ? 10 : Roll < 15 ? 30 : Roll < 35 ? 50 : Roll < 65 ? 70 : Roll < 85 ? 90 : Roll < 95 ? 110 : 150;
	}
}

namespace
{
	bool TargetsOpponent(EMoveTarget Target)
	{
		switch (Target)
		{
		case EMoveTarget::SelectedPokemon:
		case EMoveTarget::SelectedPokemonMeFirst:
		case EMoveTarget::RandomOpponent:
		case EMoveTarget::AllOpponents:
		case EMoveTarget::AllOtherPokemon:
		case EMoveTarget::AllPokemon:
			return true;
		default:
			return false;
		}
	}

	EMajorStatus ToMajorStatus(EMoveAilment Ailment)
	{
		switch (Ailment)
		{
		case EMoveAilment::Paralysis: return EMajorStatus::Paralysis;
		case EMoveAilment::Sleep:     return EMajorStatus::Sleep;
		case EMoveAilment::Freeze:    return EMajorStatus::Freeze;
		case EMoveAilment::Burn:      return EMajorStatus::Burn;
		case EMoveAilment::Poison:    return EMajorStatus::Poison;
		default:                      return EMajorStatus::None;
		}
	}

	constexpr int32 PlayerSideIndex = 0;
	int32 Other(int32 Side) { return 1 - Side; }
}

// ---------------------------------------------------------------- Mise en place

bool UBattleSession::InitWildBattle(const UPokemonDatabase* InDatabase, const FBattleSide& Player, const FPokemonInstance& WildPokemon, int32 Seed)
{
	FBattleSide Wild;
	Wild.Party.Add(WildPokemon);
	Wild.bIsAI = true;
	return InitBattle(InDatabase, EBattleType::Wild, Player, Wild, Seed);
}

bool UBattleSession::InitTrainerBattle(const UPokemonDatabase* InDatabase, const FBattleSide& Player, FName TrainerId, int32 TrainerIV, int32 Seed)
{
	const FTrainerRow* Trainer = InDatabase ? InDatabase->FindTrainer(TrainerId) : nullptr;
	if (!Trainer)
	{
		return false;
	}

	FRandomStream TrainerStream(Seed ^ 0x5EED);
	FBattleSide Opponent;
	Opponent.TrainerId = TrainerId;
	Opponent.bIsAI = true;
	for (const FTrainerPokemon& Entry : Trainer->Party)
	{
		FPokemonInstance Pokemon;
		if (!UPokemonLibrary::CreatePokemon(InDatabase, Entry.Species, Entry.Level, TrainerStream, Pokemon, TrainerIV))
		{
			continue;
		}
		if (Entry.Moves.Num() > 0)
		{
			Pokemon.Moves.Reset();
			for (FName Move : Entry.Moves)
			{
				UPokemonLibrary::LearnMove(InDatabase, Pokemon, Move);
			}
		}
		Opponent.Party.Add(Pokemon);
	}
	return InitBattle(InDatabase, EBattleType::Trainer, Player, Opponent, Seed);
}

bool UBattleSession::InitBattle(const UPokemonDatabase* InDatabase, EBattleType InType, const FBattleSide& Side0, const FBattleSide& Side1, int32 Seed)
{
	if (!InDatabase || !Side0.HasPokemonLeft() || !Side1.HasPokemonLeft())
	{
		return false;
	}

	Database = const_cast<UPokemonDatabase*>(InDatabase);
	BattleType = InType;
	Sides[0] = Side0;
	Sides[1] = Side1;
	Stream.Initialize(Seed);
	Phase = EBattlePhase::NotStarted;
	Result = FBattleResult();
	Result.BattleType = InType;
	Result.TrainerId = Side1.TrainerId;

	for (FBattleSide& Side : Sides)
	{
		// Le premier Pokémon en état de se battre ouvre le combat
		Side.ActiveIndex = Side.Party.IndexOfByPredicate([](const FPokemonInstance& P) { return P.CanBattle(); });
		Side.Volatile = FBattlerVolatile();
	}
	return true;
}

void UBattleSession::Start()
{
	if (Phase != EBattlePhase::NotStarted)
	{
		return;
	}

	FBattleEvent Started = MakeEvent(EBattleEventType::BattleStarted, -1);
	Emit(Started);
	// L'adversaire apparaît d'abord, puis le joueur envoie son Pokémon (comme dans les jeux)
	SendOut(1, Sides[1].ActiveIndex);
	SendOut(0, Sides[0].ActiveIndex);

	TurnNumber = 1;
	Phase = EBattlePhase::WaitingForActions;
	Flush();
}

// ---------------------------------------------------------------- Choix des joueurs

bool UBattleSession::IsWaitingFor(int32 Side) const
{
	switch (Phase)
	{
	case EBattlePhase::WaitingForActions:     return !Sides[Side].bIsAI && !Pending[Side].IsSet();
	case EBattlePhase::WaitingForReplacement: return NeedsReplacement[Side] && !Sides[Side].bIsAI;
	case EBattlePhase::WaitingForMoveLearn:   return PendingLearns.Num() > 0 && PendingLearns[0].Side == Side;
	default:                                  return false;
	}
}

bool UBattleSession::MustStruggle(int32 Side) const
{
	const FPokemonInstance* Active = Sides[Side].GetActive();
	return Active && !Active->Moves.ContainsByPredicate([](const FPokemonMoveSlot& Slot) { return Slot.CurrentPP > 0; });
}

bool UBattleSession::ValidateAction(int32 Side, const FBattleAction& Action, EBattleActionError& OutError) const
{
	const FBattleSide& S = Sides[Side];
	const FPokemonInstance* Active = S.GetActive();
	OutError = EBattleActionError::None;

	switch (Action.Type)
	{
	case EBattleActionType::Move:
		if (MustStruggle(Side))
		{
			return true;
		}
		if (!Active || !Active->Moves.IsValidIndex(Action.MoveSlot))
		{
			OutError = EBattleActionError::InvalidMove;
		}
		else if (Active->Moves[Action.MoveSlot].CurrentPP <= 0)
		{
			OutError = EBattleActionError::NoPPLeft;
		}
		break;

	case EBattleActionType::Switch:
		if (!S.Party.IsValidIndex(Action.PartyIndex) || !S.Party[Action.PartyIndex].IsValid())
		{
			OutError = EBattleActionError::InvalidPokemon;
		}
		else if (Action.PartyIndex == S.ActiveIndex)
		{
			OutError = EBattleActionError::AlreadyInBattle;
		}
		else if (S.Party[Action.PartyIndex].IsFainted())
		{
			OutError = EBattleActionError::PokemonFainted;
		}
		else if (S.Volatile.TrapTurns > 0)
		{
			OutError = EBattleActionError::Trapped;
		}
		break;

	case EBattleActionType::Item:
	{
		const FItemRow* Item = Database ? Database->FindItem(Action.Item) : nullptr;
		if (!Item || S.GetItemCount(Action.Item) <= 0)
		{
			OutError = EBattleActionError::NoItem;
		}
		else if (!Item->bUsableInBattle)
		{
			OutError = EBattleActionError::ItemNotUsable;
		}
		else if (Item->IsPokeBall())
		{
			if (BattleType != EBattleType::Wild)
			{
				OutError = EBattleActionError::CannotCatchTrainerPokemon;
			}
		}
		else if (!S.Party.IsValidIndex(Action.PartyIndex) || !S.Party[Action.PartyIndex].IsValid())
		{
			OutError = EBattleActionError::InvalidPokemon;
		}
		else
		{
			const FPokemonInstance& Target = S.Party[Action.PartyIndex];
			const bool bRevives = Item->ReviveHPPercent > 0 && Target.IsFainted();
			const bool bHeals = (Item->HealHP > 0 || Item->bHealFull) && !Target.IsFainted() && Target.CurrentHP < Target.GetMaxHP();
			const bool bCures = !Target.IsFainted() && Target.Status != EMajorStatus::None && Item->CuresStatus.Contains(Target.Status);
			if (!bRevives && !bHeals && !bCures)
			{
				OutError = EBattleActionError::ItemHasNoEffect;
			}
		}
		break;
	}

	case EBattleActionType::Run:
		if (BattleType != EBattleType::Wild)
		{
			OutError = EBattleActionError::CannotRunFromTrainer;
		}
		else if (S.Volatile.TrapTurns > 0)
		{
			OutError = EBattleActionError::Trapped;
		}
		break;

	default:
		OutError = EBattleActionError::InvalidMove;
		break;
	}
	return OutError == EBattleActionError::None;
}

bool UBattleSession::SubmitAction(int32 Side, const FBattleAction& Action, EBattleActionError& OutError)
{
	Side = Side == 1 ? 1 : 0;
	if (Phase != EBattlePhase::WaitingForActions)
	{
		OutError = EBattleActionError::WrongPhase;
		return false;
	}
	if (Pending[Side].IsSet())
	{
		OutError = EBattleActionError::AlreadyChosen;
		return false;
	}
	if (!ValidateAction(Side, Action, OutError))
	{
		return false;
	}

	Pending[Side] = Action;

	// L'IA choisit quand tous les humains ont choisi (elle ne voit pas leur choix)
	for (int32 S = 0; S < 2; ++S)
	{
		if (Sides[S].bIsAI && !Pending[S].IsSet())
		{
			Pending[S] = FBattleAI::ChooseAction(*this, S, Stream);
		}
	}

	if (Pending[0].IsSet() && Pending[1].IsSet())
	{
		ResolveTurn();
	}
	return true;
}

bool UBattleSession::ChooseReplacement(int32 Side, int32 PartyIndex, EBattleActionError& OutError)
{
	Side = Side == 1 ? 1 : 0;
	OutError = EBattleActionError::None;
	if (Phase != EBattlePhase::WaitingForReplacement || !NeedsReplacement[Side])
	{
		OutError = EBattleActionError::WrongPhase;
		return false;
	}
	const FBattleSide& S = Sides[Side];
	if (!S.Party.IsValidIndex(PartyIndex) || !S.Party[PartyIndex].IsValid())
	{
		OutError = EBattleActionError::InvalidPokemon;
		return false;
	}
	if (S.Party[PartyIndex].IsFainted())
	{
		OutError = EBattleActionError::PokemonFainted;
		return false;
	}

	NeedsReplacement[Side] = false;
	SendOut(Side, PartyIndex);
	if (!NeedsReplacement[0] && !NeedsReplacement[1])
	{
		++TurnNumber;
		Phase = EBattlePhase::WaitingForActions;
	}
	Flush();
	return true;
}

bool UBattleSession::RunInsteadOfReplacement(int32 Side, EBattleActionError& OutError)
{
	Side = Side == 1 ? 1 : 0;
	OutError = EBattleActionError::None;
	if (Phase != EBattlePhase::WaitingForReplacement || !NeedsReplacement[Side])
	{
		OutError = EBattleActionError::WrongPhase;
		return false;
	}
	if (BattleType != EBattleType::Wild)
	{
		OutError = EBattleActionError::CannotRunFromTrainer;
		return false;
	}

	// Le Pokémon sauvage ne bouge pas pendant qu'on choisit : la fuite se calcule avec le prochain Pokémon en forme
	const int32 Next = Sides[Side].Party.IndexOfByPredicate([](const FPokemonInstance& P) { return P.CanBattle(); });
	const int32 PlayerSpeed = Next != INDEX_NONE ? Sides[Side].Party[Next].Stats.Speed : 1;
	++FleeAttempts;
	if (UBattleRules::RollEscape(PlayerSpeed, GetSpeed(Other(Side)), FleeAttempts, Stream))
	{
		Emit(MakeEvent(EBattleEventType::FleeSucceeded, Side));
		Finish(EBattleOutcome::Fled);
	}
	else
	{
		// Raté : il faut quand même envoyer un Pokémon
		Emit(MakeEvent(EBattleEventType::FleeFailed, Side));
	}
	Flush();
	return true;
}

bool UBattleSession::ResolveMoveLearn(int32 Side, int32 ForgetSlot)
{
	if (Phase != EBattlePhase::WaitingForMoveLearn || PendingLearns.Num() == 0 || PendingLearns[0].Side != Side)
	{
		return false;
	}

	const FPendingLearn Learn = PendingLearns[0];
	PendingLearns.RemoveAt(0);

	FPokemonInstance& Pokemon = Sides[Learn.Side].Party[Learn.PartyIndex];
	FBattleEvent Event = MakeEvent(EBattleEventType::MoveNotLearned, Learn.Side);
	Event.PartyIndex = Learn.PartyIndex;
	Event.Move = Learn.Move;
	if (Pokemon.Moves.IsValidIndex(ForgetSlot))
	{
		const FName Forgotten = Pokemon.Moves[ForgetSlot].Move;
		if (UPokemonLibrary::LearnMove(Database, Pokemon, Learn.Move, ForgetSlot))
		{
			Event.Type = EBattleEventType::MoveLearned;
			Event.ForgottenMove = Forgotten;
		}
	}
	Emit(Event);
	Advance();
	return true;
}

// ---------------------------------------------------------------- Résolution du tour

void UBattleSession::ResolveTurn()
{
	TArray<FQueuedAction> Queue;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FBattlerVolatile& V = Sides[Side].Volatile;
		V.bProtected = false;
		V.bFlinched = false;
		V.bUsedProtectThisTurn = false;

		FQueuedAction Queued;
		Queued.Side = Side;
		Queued.Action = Pending[Side].GetValue();
		Queued.Speed = GetSpeed(Side);
		Queued.TieBreak = Stream.FRand();

		// Fuite, puis changements et objets, passent avant les attaques
		switch (Queued.Action.Type)
		{
		case EBattleActionType::Run:    Queued.Bracket = 8; break;
		case EBattleActionType::Switch:
		case EBattleActionType::Item:   Queued.Bracket = 7; break;
		default:
		{
			const FPokemonInstance* Active = Sides[Side].GetActive();
			const FName MoveId = (Active && Active->Moves.IsValidIndex(Queued.Action.MoveSlot) && !MustStruggle(Side))
				? Active->Moves[Queued.Action.MoveSlot].Move : BattleMoves::Struggle;
			const FMoveRow* Move = Database->FindMove(MoveId);
			Queued.Bracket = Move ? Move->Priority : 0;
			// Vive Griffe : passe devant tout le monde de la même priorité
			if (const FItemRow* Claw = GetHeldItem(Side, EHeldItemEffect::QuickClaw); Claw && Roll(Claw->HeldValue))
			{
				Queued.Speed = TNumericLimits<int32>::Max();
				ActivateHeldItem(Side, false);
			}
			break;
		}
		}
		Queue.Add(Queued);
	}
	Pending[0].Reset();
	Pending[1].Reset();

	Queue.Sort([](const FQueuedAction& A, const FQueuedAction& B)
	{
		if (A.Bracket != B.Bracket) return A.Bracket > B.Bracket;
		if (A.Speed != B.Speed) return A.Speed > B.Speed;
		return A.TieBreak > B.TieBreak;
	});

	for (const FQueuedAction& Queued : Queue)
	{
		if (Phase == EBattlePhase::Ended || PendingOutcome != EBattleOutcome::None)
		{
			break;
		}
		ExecuteAction(Queued);
		CheckFainted();
		CheckHeldBerries();
	}

	if (Phase != EBattlePhase::Ended && PendingOutcome == EBattleOutcome::None)
	{
		EndOfTurn();
	}

	Advance();
}

void UBattleSession::Advance()
{
	if (Phase == EBattlePhase::Ended)
	{
		Flush();
		return;
	}

	if (PendingLearns.Num() > 0)
	{
		Phase = EBattlePhase::WaitingForMoveLearn;
		FBattleEvent Event = MakeEvent(EBattleEventType::MoveLearnPending, PendingLearns[0].Side);
		Event.PartyIndex = PendingLearns[0].PartyIndex;
		Event.Move = PendingLearns[0].Move;
		Emit(Event);
		Flush();
		return;
	}

	if (PendingOutcome != EBattleOutcome::None)
	{
		Finish(PendingOutcome);
		Flush();
		return;
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FPokemonInstance* Active = Sides[Side].GetActive();
		if (Active && Active->IsFainted())
		{
			if (Sides[Side].bIsAI)
			{
				SendOut(Side, PickAIReplacement(Side));
			}
			else
			{
				NeedsReplacement[Side] = true;
			}
		}
	}

	if (NeedsReplacement[0] || NeedsReplacement[1])
	{
		Phase = EBattlePhase::WaitingForReplacement;
	}
	else
	{
		++TurnNumber;
		Phase = EBattlePhase::WaitingForActions;
	}
	Flush();
}

void UBattleSession::Finish(EBattleOutcome Outcome)
{
	if (Phase == EBattlePhase::Ended)
	{
		return;
	}

	Result.Outcome = Outcome;
	FBattleSide& Player = Sides[PlayerSideIndex];

	if (Outcome == EBattleOutcome::Won && BattleType == EBattleType::Trainer)
	{
		const FTrainerRow* Trainer = Database->FindTrainer(Sides[1].TrainerId);
		Result.MoneyDelta = Trainer ? Trainer->PrizeMoney : 0;
		// Pièce Rune tenue par un Pokémon qui a combattu
		for (const int32 Index : Participants[PlayerSideIndex])
		{
			const FName Held = Player.Party.IsValidIndex(Index) ? Player.Party[Index].HeldItem : NAME_None;
			const FItemRow* Coin = Held.IsNone() ? nullptr : Database->FindItem(Held);
			if (Coin && Coin->HeldEffect == EHeldItemEffect::MoneyBoost)
			{
				Result.MoneyDelta = Result.MoneyDelta * Coin->HeldValue / 100;
				break;
			}
		}
		FBattleEvent Event = MakeEvent(EBattleEventType::MoneyWon, PlayerSideIndex);
		Event.Amount = Result.MoneyDelta;
		Emit(Event);
	}
	else if (Outcome == EBattleOutcome::Lost)
	{
		int32 HighestLevel = 1;
		for (const FPokemonInstance& Pokemon : Player.Party)
		{
			HighestLevel = FMath::Max(HighestLevel, Pokemon.Level);
		}
		const int32 Lost = UBattleRules::GetMoneyLost(Player.BadgeCount, HighestLevel, Player.Money);
		Result.MoneyDelta = -Lost;
		FBattleEvent Event = MakeEvent(EBattleEventType::MoneyLost, PlayerSideIndex);
		Event.Amount = Lost;
		Emit(Event);
	}

	// Évolutions par niveau des Pokémon qui ont pris un niveau (pas après une défaite)
	if (Outcome != EBattleOutcome::Lost)
	{
		for (int32 Index : LeveledUp)
		{
			const FPokemonInstance& Pokemon = Player.Party[Index];
			const FName NewSpecies = Pokemon.CanBattle() ? UPokemonLibrary::GetLevelEvolution(Database, Pokemon) : NAME_None;
			if (!NewSpecies.IsNone())
			{
				FPendingEvolution Evolution;
				Evolution.PartyIndex = Index;
				Evolution.NewSpecies = NewSpecies;
				Result.Evolutions.Add(Evolution);
			}
		}
	}

	// La confusion, les crans... disparaissent ; PV, PP et statuts restent
	for (FBattleSide& Side : Sides)
	{
		Side.Volatile = FBattlerVolatile();
	}

	FBattleEvent Ended = MakeEvent(EBattleEventType::BattleEnded, -1);
	Ended.Outcome = Outcome;
	Emit(Ended);
	Phase = EBattlePhase::Ended;
}

void UBattleSession::Flush()
{
	if (NewEvents.Num() > 0)
	{
		const TArray<FBattleEvent> Batch = MoveTemp(NewEvents);
		NewEvents.Reset();
		OnEvents.Broadcast(this, Batch);
	}
	if (Phase == EBattlePhase::Ended && !bEndBroadcast)
	{
		bEndBroadcast = true;
		OnEnded.Broadcast(this, Result);
	}
}

void UBattleSession::Emit(const FBattleEvent& Event)
{
	EventLog.Add(Event);
	NewEvents.Add(Event);
}

FBattleEvent UBattleSession::MakeEvent(EBattleEventType Type, int32 Side) const
{
	FBattleEvent Event;
	Event.Type = Type;
	Event.Side = Side;
	if (Side == 0 || Side == 1)
	{
		Event.PartyIndex = Sides[Side].ActiveIndex;
		if (const FPokemonInstance* Active = Sides[Side].GetActive())
		{
			Event.Species = Active->Species;
			Event.HPAfter = Active->CurrentHP;
		}
	}
	return Event;
}

void UBattleSession::SendOut(int32 Side, int32 PartyIndex)
{
	FBattleSide& S = Sides[Side];
	if (!S.Party.IsValidIndex(PartyIndex))
	{
		return;
	}

	S.ActiveIndex = PartyIndex;
	S.Volatile = FBattlerVolatile();
	Emit(MakeEvent(EBattleEventType::SentOut, Side));

	// Partage de l'XP : ce Pokémon a affronté l'adversaire actuel ; l'adversaire repart de zéro face à lui
	Participants[Side].AddUnique(PartyIndex);
	Participants[Other(Side)].Reset();
	Participants[Other(Side)].Add(Sides[Other(Side)].ActiveIndex);
}

int32 UBattleSession::PickAIReplacement(int32 Side) const
{
	// Comme dans Rouge Feu : le dresseur envoie ses Pokémon dans l'ordre
	return Sides[Side].Party.IndexOfByPredicate([](const FPokemonInstance& P) { return P.CanBattle(); });
}

// ---------------------------------------------------------------- Actions

void UBattleSession::ExecuteAction(const FQueuedAction& Queued)
{
	switch (Queued.Action.Type)
	{
	case EBattleActionType::Run:    ExecuteRun(Queued.Side); break;
	case EBattleActionType::Switch: ExecuteSwitch(Queued.Side, Queued.Action.PartyIndex); break;
	case EBattleActionType::Item:   ExecuteItem(Queued.Side, Queued.Action.Item, Queued.Action.PartyIndex); break;
	case EBattleActionType::Move:
		if (IsActiveAlive(Queued.Side))
		{
			ExecuteMove(Queued.Side, Queued.Action.MoveSlot);
		}
		break;
	default:
		break;
	}
}

void UBattleSession::ExecuteRun(int32 Side)
{
	++FleeAttempts;
	if (UBattleRules::RollEscape(GetSpeed(Side), GetSpeed(Other(Side)), FleeAttempts, Stream))
	{
		Emit(MakeEvent(EBattleEventType::FleeSucceeded, Side));
		Finish(EBattleOutcome::Fled);
	}
	else
	{
		Emit(MakeEvent(EBattleEventType::FleeFailed, Side));
	}
}

void UBattleSession::ExecuteSwitch(int32 Side, int32 PartyIndex)
{
	if (IsActiveAlive(Side))
	{
		Emit(MakeEvent(EBattleEventType::Withdrew, Side));
	}
	SendOut(Side, PartyIndex);
}

void UBattleSession::ExecuteItem(int32 Side, FName ItemId, int32 PartyIndex)
{
	FBattleSide& S = Sides[Side];
	const FItemRow* Item = Database->FindItem(ItemId);
	FItemStack* Stack = S.Inventory.FindByPredicate([ItemId](const FItemStack& St) { return St.Item == ItemId; });
	if (!Item || !Stack || Stack->Count <= 0)
	{
		return;
	}
	if (--Stack->Count <= 0)
	{
		S.Inventory.RemoveAll([](const FItemStack& St) { return St.Count <= 0; });
	}

	FBattleEvent Used = MakeEvent(EBattleEventType::ItemUsed, Side);
	Used.Item = ItemId;

	if (Item->IsPokeBall())
	{
		Emit(Used);
		const int32 Target = Other(Side);
		FPokemonInstance& Wild = *Sides[Target].GetActive();
		const FSpeciesRow* Species = Database->FindSpecies(Wild.Species);
		const float CatchValue = UBattleRules::GetCatchValue(Wild.GetMaxHP(), Wild.CurrentHP, Species ? Species->CatchRate : 255,
			Item->CatchRateMultiplier, Wild.Status);

		int32 Shakes = 0;
		const bool bCaught = UBattleRules::RollCapture(CatchValue, Item->bGuaranteedCatch, Stream, Shakes);
		for (int32 Shake = 1; Shake <= Shakes; ++Shake)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::BallShake, Target);
			Event.Item = ItemId;
			Event.Amount = Shake;
			Emit(Event);
		}

		if (bCaught)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::Caught, Target);
			Event.Item = ItemId;
			Emit(Event);

			Result.CaughtPokemon = Wild;
			Result.CaughtPokemon.Ball = ItemId;
			Result.CaughtPokemon.MetLevel = Wild.Level;
			Finish(EBattleOutcome::Caught);
		}
		else
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::BrokeFree, Target);
			Event.Item = ItemId;
			Emit(Event);
		}
		return;
	}

	// Soins : sur le Pokémon choisi de l'équipe
	if (!S.Party.IsValidIndex(PartyIndex))
	{
		return;
	}
	FPokemonInstance& Pokemon = S.Party[PartyIndex];
	Used.PartyIndex = PartyIndex;
	Used.Species = Pokemon.Species;
	Emit(Used);

	if (Item->ReviveHPPercent > 0 && Pokemon.IsFainted())
	{
		Pokemon.CurrentHP = FMath::Max(1, Pokemon.GetMaxHP() * Item->ReviveHPPercent / 100);
		Pokemon.Status = EMajorStatus::None;
		FBattleEvent Event = MakeEvent(EBattleEventType::Heal, Side);
		Event.PartyIndex = PartyIndex;
		Event.Species = Pokemon.Species;
		Event.Amount = Pokemon.CurrentHP;
		Event.HPAfter = Pokemon.CurrentHP;
		Emit(Event);
		return;
	}

	if (Pokemon.IsFainted())
	{
		return;
	}

	if (Item->HealHP > 0 || Item->bHealFull)
	{
		const int32 Before = Pokemon.CurrentHP;
		Pokemon.CurrentHP = Item->bHealFull ? Pokemon.GetMaxHP() : FMath::Min(Pokemon.GetMaxHP(), Pokemon.CurrentHP + Item->HealHP);
		FBattleEvent Event = MakeEvent(EBattleEventType::Heal, Side);
		Event.PartyIndex = PartyIndex;
		Event.Species = Pokemon.Species;
		Event.Amount = Pokemon.CurrentHP - Before;
		Event.HPAfter = Pokemon.CurrentHP;
		Emit(Event);
	}

	if (Pokemon.Status != EMajorStatus::None && Item->CuresStatus.Contains(Pokemon.Status))
	{
		FBattleEvent Event = MakeEvent(EBattleEventType::StatusCured, Side);
		Event.PartyIndex = PartyIndex;
		Event.Species = Pokemon.Species;
		Event.Status = Pokemon.Status;
		Pokemon.Status = EMajorStatus::None;
		Pokemon.SleepTurns = 0;
		if (PartyIndex == S.ActiveIndex)
		{
			S.Volatile.ToxicCounter = 0;
		}
		Emit(Event);
	}
}

bool UBattleSession::CanAct(int32 Side)
{
	FBattleSide& S = Sides[Side];
	FPokemonInstance& User = *S.GetActive();
	FBattlerVolatile& V = S.Volatile;

	if (User.Status == EMajorStatus::Sleep)
	{
		if (--User.SleepTurns > 0)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::StatusPrevented, Side);
			Event.Status = EMajorStatus::Sleep;
			Emit(Event);
			return false;
		}
		User.Status = EMajorStatus::None;
		User.SleepTurns = 0;
		Emit(MakeEvent(EBattleEventType::WokeUp, Side));
	}

	if (User.Status == EMajorStatus::Freeze)
	{
		if (!Roll(20))
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::StatusPrevented, Side);
			Event.Status = EMajorStatus::Freeze;
			Emit(Event);
			return false;
		}
		User.Status = EMajorStatus::None;
		Emit(MakeEvent(EBattleEventType::Thawed, Side));
	}

	if (V.bFlinched)
	{
		Emit(MakeEvent(EBattleEventType::Flinched, Side));
		return false;
	}

	if (V.ConfusionTurns > 0)
	{
		if (--V.ConfusionTurns == 0)
		{
			Emit(MakeEvent(EBattleEventType::ConfusionEnded, Side));
		}
		else
		{
			Emit(MakeEvent(EBattleEventType::IsConfused, Side));
			if (Roll(33))
			{
				// Attaque de 40 de puissance, physique, sans type, contre soi-même
				FDamageInput Input;
				Input.AttackerLevel = User.Level;
				Input.Power = 40;
				Input.Attack = GetBattleStat(Side, EPokemonStat::Attack);
				Input.Defense = GetBattleStat(Side, EPokemonStat::Defense);
				Input.RandomPercent = Stream.RandRange(85, 100);
				DealDamage(Side, UBattleRules::CalculateDamage(Input), EBattleEventType::HurtByConfusion);
				return false;
			}
		}
	}

	if (User.Status == EMajorStatus::Paralysis && Roll(25))
	{
		FBattleEvent Event = MakeEvent(EBattleEventType::StatusPrevented, Side);
		Event.Status = EMajorStatus::Paralysis;
		Emit(Event);
		return false;
	}
	return true;
}

void UBattleSession::ExecuteMove(int32 Side, int32 MoveSlot)
{
	if (!CanAct(Side))
	{
		return;
	}

	FPokemonInstance& User = *Sides[Side].GetActive();
	FName MoveId = BattleMoves::Struggle;
	if (!MustStruggle(Side) && User.Moves.IsValidIndex(MoveSlot) && User.Moves[MoveSlot].CurrentPP > 0)
	{
		MoveId = User.Moves[MoveSlot].Move;
		--User.Moves[MoveSlot].CurrentPP;
	}

	FBattleEvent Used = MakeEvent(EBattleEventType::MoveUsed, Side);
	Used.Move = MoveId;
	Emit(Used);

	const FMoveRow* Move = Database->FindMove(MoveId);
	if (!Move)
	{
		Emit(MakeEvent(EBattleEventType::NothingHappened, Side));
		return;
	}

	const int32 Target = Other(Side);
	const bool bTargetsOpponent = TargetsOpponent(Move->Target);
	const bool bSelfDestructs = MoveId == BattleMoves::SelfDestruct || MoveId == BattleMoves::Explosion;

	if (bTargetsOpponent)
	{
		if (!IsActiveAlive(Target))
		{
			Emit(MakeEvent(EBattleEventType::MoveFailed, Side));
			return;
		}
		if (Sides[Target].Volatile.bProtected)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::Protected, Target);
			Event.Move = MoveId;
			Emit(Event);
			if (bSelfDestructs)
			{
				DealDamage(Side, User.CurrentHP, EBattleEventType::Recoil, MoveId);
			}
			return;
		}

		bool bHits;
		if (Move->MetaCategory == EMoveMetaCategory::OHKO)
		{
			const int32 LevelGap = User.Level - Sides[Target].GetActive()->Level;
			bHits = LevelGap >= 0 && Roll(Move->Accuracy + LevelGap);
		}
		else
		{
			const float Chance = UBattleRules::GetHitChance(Move->Accuracy,
				Sides[Side].Volatile.Stages.Accuracy, Sides[Target].Volatile.Stages.Evasion);
			bHits = Stream.FRand() * 100.f < Chance;
		}
		if (!bHits)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::Missed, Side);
			Event.Move = MoveId;
			Emit(Event);
			if (bSelfDestructs)
			{
				DealDamage(Side, User.CurrentHP, EBattleEventType::Recoil, MoveId);
			}
			return;
		}
	}

	switch (Move->MetaCategory)
	{
	case EMoveMetaCategory::Damage:
	case EMoveMetaCategory::DamageAilment:
	case EMoveMetaCategory::DamageLower:
	case EMoveMetaCategory::DamageRaise:
	case EMoveMetaCategory::DamageHeal:
	case EMoveMetaCategory::OHKO:
		DoDamagingMove(Side, MoveId, *Move);
		break;

	case EMoveMetaCategory::Ailment:
		TryApplyAilment(Side, bTargetsOpponent ? Target : Side, Move->Ailment, MoveId, *Move, true);
		break;

	case EMoveMetaCategory::NetGoodStats:
		ApplyStatChanges(bTargetsOpponent ? Target : Side, *Move, MoveId, true);
		break;

	case EMoveMetaCategory::Heal:
		if (User.CurrentHP >= User.GetMaxHP())
		{
			Emit(MakeEvent(EBattleEventType::MoveFailed, Side));
		}
		else
		{
			Heal(Side, FMath::Max(1, User.GetMaxHP() * Move->Healing / 100), MoveId);
		}
		break;

	case EMoveMetaCategory::Swagger:
		ApplyStatChanges(Target, *Move, MoveId, false);
		TryApplyAilment(Side, Target, EMoveAilment::Confusion, MoveId, *Move, true);
		break;

	case EMoveMetaCategory::ForceSwitch:
	{
		if (BattleType == EBattleType::Wild)
		{
			// Cyclone / Hurlement en combat sauvage : le combat s'arrête
			Emit(MakeEvent(EBattleEventType::ForcedOut, Target));
			Finish(EBattleOutcome::Fled);
			break;
		}
		TArray<int32> Choices;
		for (int32 Index = 0; Index < Sides[Target].Party.Num(); ++Index)
		{
			if (Index != Sides[Target].ActiveIndex && Sides[Target].Party[Index].CanBattle())
			{
				Choices.Add(Index);
			}
		}
		if (Choices.Num() == 0)
		{
			Emit(MakeEvent(EBattleEventType::MoveFailed, Side));
			break;
		}
		Emit(MakeEvent(EBattleEventType::ForcedOut, Target));
		SendOut(Target, Choices[Stream.RandRange(0, Choices.Num() - 1)]);
		break;
	}

	default:
		DoUniqueMove(Side, MoveId, *Move);
		break;
	}

	if (bSelfDestructs && User.CurrentHP > 0)
	{
		DealDamage(Side, User.CurrentHP, EBattleEventType::Recoil, MoveId);
	}
}

void UBattleSession::DoDamagingMove(int32 Side, FName MoveId, const FMoveRow& Move)
{
	const int32 Target = Other(Side);
	FPokemonInstance& User = *Sides[Side].GetActive();
	FPokemonInstance& Defender = *Sides[Target].GetActive();

	EPokemonType UserType1, UserType2, TargetType1, TargetType2;
	GetActiveTypes(Side, UserType1, UserType2);
	GetActiveTypes(Target, TargetType1, TargetType2);

	// Lutte n'a pas de type (efficacité neutre)
	const float Effectiveness = MoveId == BattleMoves::Struggle ? 1.f
		: Database->GetTypeEffectiveness(Move.Type, TargetType1, TargetType2);
	if (Effectiveness <= 0.f)
	{
		FBattleEvent Event = MakeEvent(EBattleEventType::NoEffect, Target);
		Event.Move = MoveId;
		Event.Effectiveness = 0.f;
		Emit(Event);
		return;
	}

	// Dégâts fixes
	int32 FixedDamage = 0;
	if (MoveId == BattleMoves::SeismicToss || MoveId == BattleMoves::NightShade) FixedDamage = User.Level;
	else if (MoveId == BattleMoves::SonicBoom) FixedDamage = 20;
	else if (MoveId == BattleMoves::DragonRage) FixedDamage = 40;
	else if (MoveId == BattleMoves::SuperFang) FixedDamage = FMath::Max(1, Defender.CurrentHP / 2);
	else if (Move.MetaCategory == EMoveMetaCategory::OHKO) FixedDamage = Defender.CurrentHP;

	int32 TotalDamage = 0;
	if (FixedDamage > 0)
	{
		TotalDamage = DealDamage(Target, FixedDamage, EBattleEventType::Damage, MoveId);
	}
	else
	{
		int32 Power = Move.Power;
		if (MoveId == BattleMoves::LowKick)
		{
			const FSpeciesRow* Species = Database->FindSpecies(Defender.Species);
			Power = BattleMoves::LowKickPower(Species ? Species->WeightKg : 50.f);
		}
		else if (MoveId == BattleMoves::Magnitude)
		{
			Power = BattleMoves::MagnitudePower(Stream);
		}
		if (Power <= 0)
		{
			Emit(MakeEvent(EBattleEventType::NothingHappened, Side));
			return;
		}
		// Objet tenu qui renforce un type (Charbon : Feu...)
		if (const FItemRow* Booster = GetHeldItem(Side, EHeldItemEffect::TypeBoost); Booster && Booster->HeldBoostType == Move.Type)
		{
			Power = FMath::Max(1, Power * Booster->HeldValue / 100);
		}

		const bool bPhysical = Move.Category == EMoveCategory::Physical;
		const EPokemonStat AttackStat = bPhysical ? EPokemonStat::Attack : EPokemonStat::SpecialAttack;
		const EPokemonStat DefenseStat = bPhysical ? EPokemonStat::Defense : EPokemonStat::SpecialDefense;
		const int32 CritStage = Move.CritStage + (Sides[Side].Volatile.bFocusEnergy ? 2 : 0);
		const int32 Hits = UBattleRules::RollHitCount(Move.MinHits, Move.MaxHits, Stream);

		int32 HitsDone = 0;
		for (int32 Hit = 0; Hit < Hits && Defender.CurrentHP > 0 && User.CurrentHP > 0; ++Hit)
		{
			const bool bCritical = Stream.RandRange(1, UBattleRules::GetCriticalHitDenominator(CritStage)) == 1;

			FDamageInput Input;
			Input.AttackerLevel = User.Level;
			Input.Power = Power;
			// Un critique ignore les baisses d'attaque du lanceur et les hausses de défense de la cible
			Input.Attack = GetBattleStat(Side, AttackStat, false, bCritical);
			Input.Defense = GetBattleStat(Target, DefenseStat, bCritical, false);
			Input.bSTAB = MoveId != BattleMoves::Struggle && (Move.Type == UserType1 || Move.Type == UserType2);
			Input.TypeEffectiveness = Effectiveness;
			Input.bCritical = bCritical;
			Input.RandomPercent = Stream.RandRange(85, 100);
			Input.bBurned = bPhysical && User.Status == EMajorStatus::Burn;

			if (bCritical)
			{
				Emit(MakeEvent(EBattleEventType::CriticalHit, Target));
			}
			TotalDamage += DealDamage(Target, UBattleRules::CalculateDamage(Input), EBattleEventType::Damage, MoveId);
			++HitsDone;
		}

		if (Effectiveness != 1.f)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::Effectiveness, Target);
			Event.Move = MoveId;
			Event.Effectiveness = Effectiveness;
			Emit(Event);
		}
		if (Hits > 1)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::MultiHit, Target);
			Event.Amount = HitsDone;
			Emit(Event);
		}
	}

	// Un Pokémon gelé touché par une attaque Feu dégèle
	if (Move.Type == EPokemonType::Fire && Defender.CurrentHP > 0 && Defender.Status == EMajorStatus::Freeze)
	{
		Defender.Status = EMajorStatus::None;
		Emit(MakeEvent(EBattleEventType::Thawed, Target));
	}

	// Drain (Vol-Vie) ou contrecoup (Bélier), en % des dégâts infligés
	if (Move.Drain > 0 && TotalDamage > 0 && User.CurrentHP > 0)
	{
		Heal(Side, FMath::Max(1, TotalDamage * Move.Drain / 100), MoveId);
	}
	else if (Move.Drain < 0 && TotalDamage > 0)
	{
		DealDamage(Side, FMath::Max(1, TotalDamage * -Move.Drain / 100), EBattleEventType::Recoil, MoveId);
	}
	// Grelot Coque : rend une part des dégâts infligés
	if (const FItemRow* Bell = GetHeldItem(Side, EHeldItemEffect::DamageHeal); Bell && TotalDamage > 0 && User.CurrentHP > 0 && User.CurrentHP < User.GetMaxHP())
	{
		ActivateHeldItem(Side, false);
		Heal(Side, FMath::Max(1, TotalDamage * Bell->HeldValue / 100), MoveId);
	}
	// Contrecoup en % des PV max (Lutte : -25)
	if (Move.Healing < 0 && User.CurrentHP > 0)
	{
		DealDamage(Side, FMath::Max(1, User.GetMaxHP() * -Move.Healing / 100), EBattleEventType::Recoil, MoveId);
	}

	// Effets secondaires (0 % dans les données = toujours)
	if (Defender.CurrentHP > 0)
	{
		if (Move.Ailment != EMoveAilment::None && Roll(Move.AilmentChance > 0 ? Move.AilmentChance : 100))
		{
			TryApplyAilment(Side, Target, Move.Ailment, MoveId, Move, false);
		}
		if (Move.FlinchChance > 0 && Roll(Move.FlinchChance))
		{
			Sides[Target].Volatile.bFlinched = true;
		}
		if (Move.MetaCategory == EMoveMetaCategory::DamageLower && Roll(Move.StatChance > 0 ? Move.StatChance : 100))
		{
			ApplyStatChanges(Target, Move, MoveId, false);
		}
	}
	if (Move.MetaCategory == EMoveMetaCategory::DamageRaise && User.CurrentHP > 0 && Roll(Move.StatChance > 0 ? Move.StatChance : 100))
	{
		ApplyStatChanges(Side, Move, MoveId, false);
	}
}

void UBattleSession::DoUniqueMove(int32 Side, FName MoveId, const FMoveRow& Move)
{
	FBattlerVolatile& V = Sides[Side].Volatile;
	FPokemonInstance& User = *Sides[Side].GetActive();

	if (MoveId == BattleMoves::FocusEnergy)
	{
		if (V.bFocusEnergy)
		{
			Emit(MakeEvent(EBattleEventType::MoveFailed, Side));
			return;
		}
		V.bFocusEnergy = true;
		Emit(MakeEvent(EBattleEventType::FocusEnergy, Side));
		return;
	}

	if (MoveId == BattleMoves::Protect || MoveId == BattleMoves::Detect)
	{
		// Chaque Abri d'affilée a 3 fois moins de chances de réussir
		int32 Odds = 1;
		for (int32 I = 0; I < V.ProtectChain && Odds < 729; ++I) Odds *= 3;
		if (Stream.RandRange(1, Odds) == 1)
		{
			V.bProtected = true;
			V.bUsedProtectThisTurn = true;
			++V.ProtectChain;
			Emit(MakeEvent(EBattleEventType::ProtectStarted, Side));
		}
		else
		{
			V.ProtectChain = 0;
			Emit(MakeEvent(EBattleEventType::MoveFailed, Side));
		}
		return;
	}

	if (MoveId == BattleMoves::Rest)
	{
		if (User.CurrentHP >= User.GetMaxHP() || User.Status == EMajorStatus::Sleep)
		{
			Emit(MakeEvent(EBattleEventType::MoveFailed, Side));
			return;
		}
		Heal(Side, User.GetMaxHP() - User.CurrentHP, MoveId);
		User.Status = EMajorStatus::Sleep;
		User.SleepTurns = 3; // dort 2 tours, se réveille au 3e
		FBattleEvent Event = MakeEvent(EBattleEventType::StatusApplied, Side);
		Event.Status = EMajorStatus::Sleep;
		Emit(Event);
		return;
	}

	if (MoveId == BattleMoves::Haze)
	{
		Sides[0].Volatile.Stages = FBattleStatStages();
		Sides[1].Volatile.Stages = FBattleStatStages();
		FBattleEvent Event = MakeEvent(EBattleEventType::StatChanged, -1);
		Event.Move = MoveId;
		Emit(Event);
		return;
	}

	// Effet de terrain, météo... pas encore géré
	FBattleEvent Event = MakeEvent(EBattleEventType::NothingHappened, Side);
	Event.Move = MoveId;
	Emit(Event);
}

int32 UBattleSession::DealDamage(int32 Side, int32 Amount, EBattleEventType EventType, FName MoveId)
{
	FPokemonInstance* Pokemon = Sides[Side].GetActive();
	if (!Pokemon || Pokemon->CurrentHP <= 0 || Amount <= 0)
	{
		return 0;
	}
	int32 Dealt = FMath::Min(Amount, Pokemon->CurrentHP);
	// Bandeau : une attaque qui devait le mettre K.O. le laisse à 1 PV
	bool bHeldOn = false;
	if (EventType == EBattleEventType::Damage && Dealt >= Pokemon->CurrentHP)
	{
		if (const FItemRow* Band = GetHeldItem(Side, EHeldItemEffect::FocusBand); Band && Roll(Band->HeldValue))
		{
			Dealt = Pokemon->CurrentHP - 1;
			bHeldOn = true;
		}
	}
	Pokemon->CurrentHP -= Dealt;

	FBattleEvent Event = MakeEvent(EventType, Side);
	Event.Move = MoveId;
	Event.Amount = Dealt;
	Emit(Event);
	if (bHeldOn)
	{
		ActivateHeldItem(Side, false);
	}
	return Dealt;
}

void UBattleSession::Heal(int32 Side, int32 Amount, FName MoveId)
{
	FPokemonInstance* Pokemon = Sides[Side].GetActive();
	if (!Pokemon || Pokemon->CurrentHP <= 0)
	{
		return;
	}
	const int32 Healed = FMath::Min(Amount, Pokemon->GetMaxHP() - Pokemon->CurrentHP);
	Pokemon->CurrentHP += Healed;

	FBattleEvent Event = MakeEvent(EBattleEventType::Heal, Side);
	Event.Move = MoveId;
	Event.Amount = Healed;
	Emit(Event);
}

bool UBattleSession::TryApplyAilment(int32 SourceSide, int32 TargetSide, EMoveAilment Ailment, FName MoveId, const FMoveRow& Move, bool bReportFailure)
{
	FPokemonInstance* Target = Sides[TargetSide].GetActive();
	FBattlerVolatile& V = Sides[TargetSide].Volatile;
	if (!Target || Target->CurrentHP <= 0)
	{
		return false;
	}

	EPokemonType Type1, Type2;
	GetActiveTypes(TargetSide, Type1, Type2);

	auto Fail = [&](EBattleEventType Type)
	{
		if (bReportFailure)
		{
			FBattleEvent Event = MakeEvent(Type, Type == EBattleEventType::NoEffect ? TargetSide : SourceSide);
			Event.Move = MoveId;
			Emit(Event);
		}
		return false;
	};

	// Immunités des attaques de statut : Cage-Éclair ne touche pas le type Sol, les poudres pas le type Plante
	if (Move.Category == EMoveCategory::Status && TargetSide != SourceSide)
	{
		if (Move.Type == EPokemonType::Electric && Database->GetTypeEffectiveness(Move.Type, Type1, Type2) <= 0.f)
		{
			return Fail(EBattleEventType::NoEffect);
		}
		if (BattleMoves::IsPowder(MoveId) && (Type1 == EPokemonType::Grass || Type2 == EPokemonType::Grass))
		{
			return Fail(EBattleEventType::NoEffect);
		}
	}

	EMajorStatus Status = ToMajorStatus(Ailment);
	if (MoveId == BattleMoves::Toxic && Status == EMajorStatus::Poison)
	{
		Status = EMajorStatus::BadlyPoisoned;
	}

	if (Status != EMajorStatus::None)
	{
		if (Target->Status != EMajorStatus::None)
		{
			return Fail(EBattleEventType::MoveFailed);
		}
		if (UBattleRules::IsImmuneToStatus(Status, Type1, Type2))
		{
			return Fail(EBattleEventType::NoEffect);
		}
		Target->Status = Status;
		if (Status == EMajorStatus::Sleep)
		{
			// Dort 1 à 3 tours (5G et plus) : le compteur baisse à chaque tentative d'attaque
			Target->SleepTurns = Stream.RandRange(2, 4);
		}
		if (Status == EMajorStatus::BadlyPoisoned)
		{
			V.ToxicCounter = 0;
		}
		FBattleEvent Event = MakeEvent(EBattleEventType::StatusApplied, TargetSide);
		Event.Status = Status;
		Event.Move = MoveId;
		Emit(Event);
		return true;
	}

	switch (Ailment)
	{
	case EMoveAilment::Confusion:
		if (V.ConfusionTurns > 0)
		{
			return Fail(EBattleEventType::MoveFailed);
		}
		V.ConfusionTurns = Stream.RandRange(2, 5);
		break;
	case EMoveAilment::LeechSeed:
		if (V.bLeechSeeded || Type1 == EPokemonType::Grass || Type2 == EPokemonType::Grass)
		{
			return Fail(V.bLeechSeeded ? EBattleEventType::MoveFailed : EBattleEventType::NoEffect);
		}
		V.bLeechSeeded = true;
		break;
	case EMoveAilment::Trap:
		if (V.TrapTurns > 0)
		{
			return false;
		}
		// 4 ou 5 tours de dégâts (le compteur baisse avant chaque coup de fin de tour)
		V.TrapTurns = Stream.RandRange(5, 6);
		break;
	default:
		return Fail(EBattleEventType::NothingHappened);
	}

	FBattleEvent Event = MakeEvent(EBattleEventType::VolatileApplied, TargetSide);
	Event.Ailment = Ailment;
	Event.Move = MoveId;
	Emit(Event);
	return true;
}

void UBattleSession::ApplyStatChanges(int32 TargetSide, const FMoveRow& Move, FName MoveId, bool bReportFailure)
{
	if (!IsActiveAlive(TargetSide))
	{
		return;
	}
	if (Move.StatChanges.Num() == 0 && bReportFailure)
	{
		Emit(MakeEvent(EBattleEventType::NothingHappened, TargetSide));
		return;
	}
	for (const FMoveStatChange& Change : Move.StatChanges)
	{
		FBattleEvent Event = MakeEvent(EBattleEventType::StatChanged, TargetSide);
		Event.Move = MoveId;
		Event.Stat = Change.Stat;
		Event.RequestedStages = Change.Stages;
		Event.Amount = Sides[TargetSide].Volatile.Stages.Add(Change.Stat, Change.Stages);
		// Amount = 0 : « ne peut plus monter / baisser ». On ne le dit que pour une attaque de statut
		if (Event.Amount != 0 || bReportFailure)
		{
			Emit(Event);
		}
	}
}

void UBattleSession::EndOfTurn()
{
	// Le plus rapide d'abord
	const int32 First = GetSpeed(0) >= GetSpeed(1) ? 0 : 1;
	for (int32 Side : { First, Other(First) })
	{
		if (!IsActiveAlive(Side))
		{
			continue;
		}
		FPokemonInstance& Pokemon = *Sides[Side].GetActive();
		FBattlerVolatile& V = Sides[Side].Volatile;
		const int32 MaxHP = Pokemon.GetMaxHP();

		int32 StatusDamage = 0;
		switch (Pokemon.Status)
		{
		case EMajorStatus::Burn:          StatusDamage = FMath::Max(1, MaxHP / 16); break;
		case EMajorStatus::Poison:        StatusDamage = FMath::Max(1, MaxHP / 8); break;
		case EMajorStatus::BadlyPoisoned: StatusDamage = FMath::Max(1, MaxHP * FMath::Min(++V.ToxicCounter, 15) / 16); break;
		default: break;
		}
		if (StatusDamage > 0)
		{
			FBattleEvent Event = MakeEvent(EBattleEventType::StatusDamage, Side);
			Event.Status = Pokemon.Status;
			const int32 Dealt = FMath::Min(StatusDamage, Pokemon.CurrentHP);
			Pokemon.CurrentHP -= Dealt;
			Event.Amount = Dealt;
			Event.HPAfter = Pokemon.CurrentHP;
			Emit(Event);
		}

		if (V.bLeechSeeded && Pokemon.CurrentHP > 0)
		{
			const int32 Drained = FMath::Min(FMath::Max(MaxHP / 8, 1), Pokemon.CurrentHP);
			Pokemon.CurrentHP -= Drained;
			FBattleEvent Event = MakeEvent(EBattleEventType::LeechSeedDrain, Side);
			Event.Amount = Drained;
			Event.HPAfter = Pokemon.CurrentHP;
			Emit(Event);
			Heal(Other(Side), Drained);
		}

		if (V.TrapTurns > 0 && Pokemon.CurrentHP > 0)
		{
			if (--V.TrapTurns > 0)
			{
				const int32 Dealt = FMath::Min(FMath::Max(MaxHP / 8, 1), Pokemon.CurrentHP);
				Pokemon.CurrentHP -= Dealt;
				FBattleEvent Event = MakeEvent(EBattleEventType::TrapDamage, Side);
				Event.Amount = Dealt;
				Event.HPAfter = Pokemon.CurrentHP;
				Emit(Event);
			}
			else
			{
				Emit(MakeEvent(EBattleEventType::TrapEnded, Side));
			}
		}

		// Restes
		if (const FItemRow* Leftovers = GetHeldItem(Side, EHeldItemEffect::EndTurnHeal); Leftovers && Pokemon.CurrentHP > 0 && Pokemon.CurrentHP < MaxHP)
		{
			ActivateHeldItem(Side, false);
			Heal(Side, FMath::Max(1, MaxHP * Leftovers->HeldValue / 100));
		}

		if (!V.bUsedProtectThisTurn)
		{
			V.ProtectChain = 0;
		}
		CheckFainted();
	}
	CheckHeldBerries();
}

const FItemRow* UBattleSession::GetHeldItem(int32 Side, EHeldItemEffect Effect) const
{
	const FPokemonInstance* Active = (Side == 0 || Side == 1) ? Sides[Side].GetActive() : nullptr;
	if (!Active || Active->HeldItem.IsNone() || !Database)
	{
		return nullptr;
	}
	const FItemRow* Item = Database->FindItem(Active->HeldItem);
	return Item && Item->HeldEffect == Effect ? Item : nullptr;
}

void UBattleSession::ActivateHeldItem(int32 Side, bool bConsume)
{
	FPokemonInstance* Active = Sides[Side].GetActive();
	if (!Active || Active->HeldItem.IsNone())
	{
		return;
	}
	FBattleEvent Event = MakeEvent(EBattleEventType::HeldItemActivated, Side);
	Event.Item = Active->HeldItem;
	Emit(Event);
	if (bConsume)
	{
		Active->HeldItem = NAME_None;
	}
}

void UBattleSession::CheckHeldBerries()
{
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!IsActiveAlive(Side))
		{
			continue;
		}
		FPokemonInstance& Pokemon = *Sides[Side].GetActive();
		const int32 MaxHP = Pokemon.GetMaxHP();

		if (const FItemRow* Berry = GetHeldItem(Side, EHeldItemEffect::BerryCure); Berry && Berry->CuresStatus.Contains(Pokemon.Status))
		{
			ActivateHeldItem(Side, true);
			Pokemon.Status = EMajorStatus::None;
			Pokemon.SleepTurns = 0;
			Sides[Side].Volatile.ToxicCounter = 0;
			Emit(MakeEvent(EBattleEventType::StatusCured, Side));
		}

		if (Pokemon.CurrentHP * 2 > MaxHP)
		{
			continue;
		}
		if (const FItemRow* Berry = GetHeldItem(Side, EHeldItemEffect::BerryHeal))
		{
			ActivateHeldItem(Side, true);
			Heal(Side, Berry->HeldValue);
		}
		else if (const FItemRow* PercentBerry = GetHeldItem(Side, EHeldItemEffect::BerryHealPercent))
		{
			ActivateHeldItem(Side, true);
			Heal(Side, FMath::Max(1, MaxHP * PercentBerry->HeldValue / 100));
		}
	}
}

// ---------------------------------------------------------------- K.O. et expérience

void UBattleSession::CheckFainted()
{
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FPokemonInstance* Active = Sides[Side].GetActive();
		if (Active && Active->CurrentHP <= 0 && !Sides[Side].Volatile.bFainted)
		{
			Sides[Side].Volatile.bFainted = true;
			OnFainted(Side);
		}
	}
}

void UBattleSession::OnFainted(int32 Side)
{
	FPokemonInstance& Fainted = *Sides[Side].GetActive();
	Fainted.CurrentHP = 0;
	Fainted.Status = EMajorStatus::None;
	Fainted.SleepTurns = 0;
	Emit(MakeEvent(EBattleEventType::Fainted, Side));

	const int32 Winner = Other(Side);
	if (Sides[Winner].bGainsExperience && Sides[Winner].HasPokemonLeft())
	{
		GiveExperience(Winner, Fainted);
	}

	if (!Sides[Side].HasPokemonLeft() && PendingOutcome == EBattleOutcome::None)
	{
		// Si les deux tombent en même temps (Destruction), le joueur perd
		PendingOutcome = Side == PlayerSideIndex || !Sides[PlayerSideIndex].HasPokemonLeft() ? EBattleOutcome::Lost : EBattleOutcome::Won;
	}
	else if (!Sides[PlayerSideIndex].HasPokemonLeft())
	{
		PendingOutcome = EBattleOutcome::Lost;
	}
}

void UBattleSession::GiveExperience(int32 WinnerSide, const FPokemonInstance& Fainted)
{
	const FSpeciesRow* Species = Database->FindSpecies(Fainted.Species);
	if (!Species)
	{
		return;
	}

	FBattleSide& Winner = Sides[WinnerSide];
	for (int32 Index = 0; Index < Winner.Party.Num(); ++Index)
	{
		FPokemonInstance& Pokemon = Winner.Party[Index];
		if (!Pokemon.CanBattle() || Pokemon.Level >= 100)
		{
			continue;
		}
		const bool bParticipated = Participants[WinnerSide].Contains(Index);
		int32 Experience = UBattleRules::GetExperienceGain(Species->BaseExperience, Fainted.Level, Pokemon.Level,
			BattleType == EBattleType::Trainer, bParticipated);
		// Œuf Chance (le Pokémon n'a pas besoin d'être au combat)
		const FItemRow* Egg = Pokemon.HeldItem.IsNone() ? nullptr : Database->FindItem(Pokemon.HeldItem);
		if (Egg && Egg->HeldEffect == EHeldItemEffect::ExpBoost)
		{
			Experience = Experience * Egg->HeldValue / 100;
		}

		UPokemonLibrary::AddEVs(Pokemon, Species->EVYield);

		TArray<int32> NewLevels;
		UPokemonLibrary::AddExperience(Database, Pokemon, Experience, NewLevels);

		FBattleEvent Gained = MakeEvent(EBattleEventType::ExpGained, WinnerSide);
		Gained.PartyIndex = Index;
		Gained.Species = Pokemon.Species;
		Gained.Amount = Experience;
		Gained.HPAfter = Pokemon.CurrentHP;
		Emit(Gained);

		for (int32 Level : NewLevels)
		{
			FBattleEvent LevelUp = MakeEvent(EBattleEventType::LevelUp, WinnerSide);
			LevelUp.PartyIndex = Index;
			LevelUp.Species = Pokemon.Species;
			LevelUp.Amount = Level;
			LevelUp.HPAfter = Pokemon.CurrentHP;
			Emit(LevelUp);
			QueueLevelUpMoves(WinnerSide, Index, Level);
		}
		if (NewLevels.Num() > 0)
		{
			LeveledUp.AddUnique(Index);
		}
	}
}

void UBattleSession::QueueLevelUpMoves(int32 Side, int32 PartyIndex, int32 Level)
{
	FPokemonInstance& Pokemon = Sides[Side].Party[PartyIndex];
	for (FName Move : Database->GetMovesLearnedAtLevel(Pokemon.Species, Level))
	{
		if (Pokemon.FindMove(Move) != INDEX_NONE)
		{
			continue;
		}
		if (Pokemon.Moves.Num() < 4 && !PendingLearns.ContainsByPredicate([&](const FPendingLearn& L) { return L.Side == Side && L.PartyIndex == PartyIndex; }))
		{
			UPokemonLibrary::LearnMove(Database, Pokemon, Move);
			FBattleEvent Event = MakeEvent(EBattleEventType::MoveLearned, Side);
			Event.PartyIndex = PartyIndex;
			Event.Species = Pokemon.Species;
			Event.Move = Move;
			Emit(Event);
		}
		else if (!Sides[Side].bIsAI)
		{
			FPendingLearn Learn;
			Learn.Side = Side;
			Learn.PartyIndex = PartyIndex;
			Learn.Move = Move;
			PendingLearns.Add(Learn);
		}
	}
}

// ---------------------------------------------------------------- Calculs

int32 UBattleSession::GetBattleStat(int32 Side, EPokemonStat Stat, bool bIgnorePositive, bool bIgnoreNegative) const
{
	const FPokemonInstance* Pokemon = Sides[Side].GetActive();
	if (!Pokemon)
	{
		return 1;
	}

	int32 Base = 1;
	switch (Stat)
	{
	case EPokemonStat::Attack:         Base = Pokemon->Stats.Attack; break;
	case EPokemonStat::Defense:        Base = Pokemon->Stats.Defense; break;
	case EPokemonStat::SpecialAttack:  Base = Pokemon->Stats.SpecialAttack; break;
	case EPokemonStat::SpecialDefense: Base = Pokemon->Stats.SpecialDefense; break;
	case EPokemonStat::Speed:          Base = Pokemon->Stats.Speed; break;
	default:                           return Pokemon->Stats.HP;
	}

	int32 Stage = Sides[Side].Volatile.Stages.Get(Stat);
	if ((bIgnorePositive && Stage > 0) || (bIgnoreNegative && Stage < 0))
	{
		Stage = 0;
	}
	return FMath::Max(1, FMath::FloorToInt(Base * UBattleRules::GetStatStageMultiplier(Stage)));
}

int32 UBattleSession::GetSpeed(int32 Side) const
{
	const FPokemonInstance* Pokemon = Sides[Side].GetActive();
	if (!Pokemon)
	{
		return 0;
	}
	return UBattleRules::GetEffectiveSpeed(Pokemon->Stats.Speed, Sides[Side].Volatile.Stages.Speed, Pokemon->Status == EMajorStatus::Paralysis);
}

void UBattleSession::GetActiveTypes(int32 Side, EPokemonType& OutType1, EPokemonType& OutType2) const
{
	if (const FPokemonInstance* Pokemon = Sides[Side].GetActive())
	{
		UPokemonLibrary::GetTypes(Database, *Pokemon, OutType1, OutType2);
		return;
	}
	OutType1 = EPokemonType::Normal;
	OutType2 = EPokemonType::None;
}

bool UBattleSession::IsActiveAlive(int32 Side) const
{
	const FPokemonInstance* Pokemon = Sides[Side].GetActive();
	return Pokemon && Pokemon->CurrentHP > 0;
}

bool UBattleSession::Roll(int32 Percent)
{
	return Percent >= 100 || Stream.RandRange(0, 99) < Percent;
}

FBattleSnapshot UBattleSession::MakeSnapshot(int32 LocalSide) const
{
	FBattleSnapshot Snapshot;
	Snapshot.BattleType = BattleType;
	Snapshot.Phase = Phase;
	Snapshot.LocalSide = LocalSide;
	Snapshot.Sides.Add(Sides[0]);
	Snapshot.Sides.Add(Sides[1]);
	Snapshot.TurnNumber = TurnNumber;
	if (Phase == EBattlePhase::WaitingForMoveLearn && PendingLearns.Num() > 0 && PendingLearns[0].Side == LocalSide)
	{
		Snapshot.PendingLearnPartyIndex = PendingLearns[0].PartyIndex;
		Snapshot.PendingLearnMove = PendingLearns[0].Move;
	}
	// L'adversaire n'a pas à voir le sac de l'autre
	Snapshot.Sides[Other(LocalSide)].Inventory.Reset();
	return Snapshot;
}
