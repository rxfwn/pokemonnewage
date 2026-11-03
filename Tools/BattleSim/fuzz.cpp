#include "Battle/BattleSession.h"
#include "Battle/BattleText.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonProgression.h"
#include "Engine/DataTable.h"
#include <iostream>
UPokemonDatabase* MakeRealDatabase();

static int Errors = 0;
static void Fail(const std::string& M) { if (++Errors < 30) std::cout << "ERREUR " << M << "\n"; }

static void CheckSide(const FBattleSide& S, const char* Where)
{
	for (const FPokemonInstance& P : S.Party)
	{
		if (P.CurrentHP < 0 || P.CurrentHP > P.GetMaxHP()) Fail(std::string(Where) + " PV hors bornes " + P.Species.ToString().Narrow());
		if (P.Moves.Num() > 4 || P.Moves.Num() == 0) Fail(std::string(Where) + " nb attaques " + std::to_string(P.Moves.Num()) + " " + P.Species.ToString().Narrow());
		for (auto& M : P.Moves) if (M.CurrentPP < 0 || M.CurrentPP > M.MaxPP) Fail("PP hors bornes");
		if (P.Level < 1 || P.Level > 100) Fail("niveau");
	}
	auto& St = S.Volatile.Stages;
	for (int v : {St.Attack, St.Defense, St.SpecialAttack, St.SpecialDefense, St.Speed, St.Accuracy, St.Evasion}) if (v < -6 || v > 6) Fail("cran");
}

static EBattleOutcome RunBattle(UPokemonDatabase* Db, UBattleSession* Session, FRandomStream& R, bool bPrint, int& TurnsOut)
{
	if (bPrint)
	{
		Session->OnEvents.AddLambda([Db](UBattleSession* S, const TArray<FBattleEvent>& Events)
		{
			FBattleSnapshot Snap = S->MakeSnapshot(0);
			for (auto& E : Events) { FString T = UBattleText::MakeEventText(Db, Snap, E).ToString(); if (!T.IsEmpty()) std::wcout << L"  " << *T << L"\n"; }
		});
	}
	Session->Start();
	int Steps = 0;
	while (Session->GetPhase() != EBattlePhase::Ended && Steps++ < 2000)
	{
		EBattleActionError Err;
		const FBattleSide& Me = Session->GetSide(0);
		switch (Session->GetPhase())
		{
		case EBattlePhase::WaitingForActions:
		{
			int Roll = R.RandRange(0, 99);
			FBattleAction A = FBattleAction::MakeMove(R.RandRange(0, 3));
			if (Roll < 6) A = FBattleAction::MakeSwitch(R.RandRange(0, 5));
			else if (Roll < 12) A = FBattleAction::MakeItem(R.RandRange(0, 1) ? FName(L"PokeBall") : FName(L"Potion"), R.RandRange(0, 5));
			else if (Roll < 15) A = FBattleAction::MakeRun();
			if (!Session->SubmitAction(0, A, Err))
			{
				// repli : la première attaque avec des PP
				int Slot = 0; const FPokemonInstance* Act = Me.GetActive();
				for (int i = 0; Act && i < Act->Moves.Num(); ++i) if (Act->Moves[i].CurrentPP > 0) { Slot = i; break; }
				if (!Session->SubmitAction(0, FBattleAction::MakeMove(Slot), Err)) Fail("aucune action acceptée, erreur " + std::to_string((int)Err));
			}
			break;
		}
		case EBattlePhase::WaitingForReplacement:
		{
			if (Session->GetBattleType() == EBattleType::Wild && R.RandRange(0, 9) == 0) { Session->RunInsteadOfReplacement(0, Err); break; }
			int Idx = Me.Party.IndexOfByPredicate([](const FPokemonInstance& P) { return P.CanBattle(); });
			if (!Session->ChooseReplacement(0, Idx, Err)) Fail("remplaçant refusé");
			break;
		}
		case EBattlePhase::WaitingForMoveLearn:
			if (!Session->ResolveMoveLearn(0, R.RandRange(-1, 3))) Fail("ResolveMoveLearn refusé");
			break;
		default: Fail("phase inattendue"); Steps = 99999; break;
		}
		CheckSide(Session->GetSide(0), "joueur"); CheckSide(Session->GetSide(1), "adversaire");
	}
	if (Session->GetPhase() != EBattlePhase::Ended) Fail("combat sans fin");
	if (Session->GetResult().Outcome == EBattleOutcome::None) Fail("pas de résultat");
	// tous les textes se génèrent
	FBattleSnapshot Snap = Session->MakeSnapshot(0);
	for (auto& E : Session->GetEventLog()) UBattleText::MakeEventText(Db, Snap, E);
	TurnsOut = Session->GetTurnNumber();
	return Session->GetResult().Outcome;
}

static FPokemonInstance Mon(UPokemonDatabase* Db, FName S, int L, FRandomStream& R)
{
	FPokemonInstance P; if (!UPokemonLibrary::CreatePokemon(Db, S, L, R, P)) Fail("CreatePokemon " + S.ToString().Narrow()); return P;
}

int main()
{
	std::setlocale(LC_ALL, "C.UTF-8");
	UPokemonDatabase* Db = MakeRealDatabase();
	TArray<FName> V1;
	for (auto& N : Db->Species->GetRowNames()) if (Db->FindSpecies(N)->bAvailableInV1) V1.Add(N);
	std::cout << "Espèces V1 : " << V1.Num() << "\n";
	FRandomStream R(1234);

	// 1. Exemple lisible : Bulbizarre N.12 + Roucool contre Pierre
	{
		FBattleSide P; P.bGainsExperience = true; P.Money = 3000;
		P.Party.Add(Mon(Db, L"Bulbasaur", 13, R)); P.Party.Add(Mon(Db, L"Pidgey", 10, R));
		FItemStack Pot; Pot.Item = L"Potion"; Pot.Count = 3; P.Inventory.Add(Pot);
		UBattleSession* S = NewObject<UBattleSession>();
		S->InitTrainerBattle(Db, P, L"Brock", 25, 99);
		std::cout << "--- Exemple : contre Pierre ---\n"; int T;
		EBattleOutcome O = RunBattle(Db, S, R, true, T);
		std::cout << "--- fin : résultat " << (int)O << " en " << T << " tours ---\n";
	}

	// 2. Tous les dresseurs, avec chaque starter, plusieurs fois
	int Wins = 0, Total = 0;
	for (auto& Tid : Db->Trainers->GetRowNames())
		for (const TCHAR* Starter : { L"Bulbasaur", L"Charmander", L"Squirtle" })
			for (int K = 0; K < 30; ++K)
			{
				FBattleSide P; P.bGainsExperience = true; P.Money = 1000;
				P.Party.Add(Mon(Db, Starter, R.RandRange(6, 16), R));
				for (int n = R.RandRange(0, 3); n > 0; --n) P.Party.Add(Mon(Db, V1[R.RandRange(0, V1.Num() - 1)], R.RandRange(3, 14), R));
				FItemStack Pot; Pot.Item = L"Potion"; Pot.Count = 2; P.Inventory.Add(Pot);
				UBattleSession* S = NewObject<UBattleSession>();
				if (!S->InitTrainerBattle(Db, P, Tid, 15, R.RandRange(0, 1 << 30))) { Fail("InitTrainerBattle " + Tid.ToString().Narrow()); continue; }
				int T; EBattleOutcome O = RunBattle(Db, S, R, false, T); ++Total; Wins += O == EBattleOutcome::Won;
				if (O == EBattleOutcome::Lost && S->GetResult().MoneyDelta > 0) Fail("argent");
			}
	std::cout << "Dresseurs : " << Total << " combats, " << Wins << " gagnés\n";

	// 3. Combats sauvages sur toutes les tables de rencontre
	int Caught = 0, Fled = 0, WonWild = 0, Lost = 0, N = 0, MaxTurns = 0;
	TArray<FName> Tables = Db->Encounters->GetRowNames();
	for (int K = 0; K < 4000; ++K)
	{
		FName Species; int32 Level;
		if (!Db->RollEncounter(Tables[R.RandRange(0, Tables.Num() - 1)], R, Species, Level)) continue;
		FBattleSide P; P.bGainsExperience = true; P.Money = 500;
		for (int n = R.RandRange(1, 6); n > 0; --n) P.Party.Add(Mon(Db, V1[R.RandRange(0, V1.Num() - 1)], R.RandRange(2, 40), R));
		FItemStack B; B.Item = L"PokeBall"; B.Count = 5; P.Inventory.Add(B);
		FItemStack Pot; Pot.Item = L"Potion"; Pot.Count = 2; P.Inventory.Add(Pot);
		UBattleSession* S = NewObject<UBattleSession>();
		if (!S->InitWildBattle(Db, P, Mon(Db, Species, Level, R), R.RandRange(0, 1 << 30))) { Fail("InitWild"); continue; }
		int T; EBattleOutcome O = RunBattle(Db, S, R, false, T); ++N; MaxTurns = std::max(MaxTurns, T);
		Caught += O == EBattleOutcome::Caught; Fled += O == EBattleOutcome::Fled; WonWild += O == EBattleOutcome::Won; Lost += O == EBattleOutcome::Lost;
		if (O == EBattleOutcome::Caught && !S->GetResult().CaughtPokemon.IsValid()) Fail("capture vide");
	}
	std::cout << "Sauvages : " << N << " combats, gagnés " << WonWild << ", capturés " << Caught << ", fuites " << Fled << ", perdus " << Lost << ", tours max " << MaxTurns << "\n";
	// 4. Progression : chaque espèce monte du niveau 2 à 100 au Super Bonbon, apprend, évolue ; puis tous les objets
	int Evolutions = 0, Choices = 0, ItemsUsed = 0;
	TArray<FName> FieldItems;
	for (auto& Id : Db->Items->GetRowNames())
	{
		const FItemRow* Row = Db->FindItem(Id);
		if (Row->FieldEffect == EFieldItemEffect::None) continue;
		FieldItems.Add(Id);
		if (Row->FieldEffect == EFieldItemEffect::TeachMove && !Db->FindMove(Row->TeachesMove)) Fail("CT sans attaque " + Id.ToString().Narrow());
	}
	auto CheckMon = [](const FPokemonInstance& P, const char* Where)
	{
		if (P.Moves.Num() < 1 || P.Moves.Num() > 4) Fail(std::string(Where) + " nb attaques " + P.Species.ToString().Narrow());
		for (int32 I = 0; I < P.Moves.Num(); ++I)
			for (int32 J = I + 1; J < P.Moves.Num(); ++J)
				if (P.Moves[I].Move == P.Moves[J].Move) Fail(std::string(Where) + " attaque en double " + P.Species.ToString().Narrow());
		if (P.Level < 1 || P.Level > 100 || P.Stats.HP <= 0 || P.CurrentHP > P.Stats.HP) Fail(std::string(Where) + " stats " + P.Species.ToString().Narrow());
	};
	auto Resolve = [&](FPokemonInstance& P, FProgressionOutcome& O)
	{
		for (FName M : O.MovesToChoose) { ++Choices; if (R.FRand() < 0.7f) UPokemonLibrary::LearnMove(Db, P, M, R.RandRange(0, 3)); }
		if (!O.EvolvesInto.IsNone())
		{
			if (!Db->FindSpecies(O.EvolvesInto)) Fail("évolution inconnue " + O.EvolvesInto.ToString().Narrow());
			FProgressionOutcome E;
			if (UPokemonProgression::ApplyEvolution(Db, P, O.EvolvesInto, E)) ++Evolutions; else Fail("ApplyEvolution");
			for (FName M : E.MovesToChoose) { ++Choices; UPokemonLibrary::LearnMove(Db, P, M, R.RandRange(0, 3)); }
		}
	};
	for (auto& Sid : Db->Species->GetRowNames())
	{
		FPokemonInstance P = Mon(Db, Sid, 2, R);
		while (P.Level < 100)
		{
			const int32 Before = P.Level;
			FProgressionOutcome O; bool bConsumed = false;
			if (!UPokemonProgression::UseItem(Db, L"RareCandy", P, O, bConsumed) || P.Level != Before + 1 || !bConsumed) { Fail("Super Bonbon " + Sid.ToString().Narrow()); break; }
			if (O.LevelUps.Num() != 1 || O.LevelUps[0].StatsAfter.HP < O.LevelUps[0].StatsBefore.HP) Fail("stats du niveau " + Sid.ToString().Narrow());
			Resolve(P, O);
			CheckMon(P, "progression");
		}
		for (int K = 0; K < 10; ++K)
		{
			FPokemonInstance Q = Mon(Db, Sid, R.RandRange(5, 60), R);
			const FName Item = FieldItems[R.RandRange(0, FieldItems.Num() - 1)];
			const EItemCompatibility C = UPokemonProgression::GetItemCompatibility(Db, Item, Q);
			FProgressionOutcome O; bool bConsumed = false;
			const bool bUsed = UPokemonProgression::UseItem(Db, Item, Q, O, bConsumed);
			if (bUsed != (C == EItemCompatibility::Able)) Fail("compatibilité " + Item.ToString().Narrow() + " " + Sid.ToString().Narrow());
			ItemsUsed += bUsed;
			Resolve(Q, O);
			CheckMon(Q, "objet");
		}
	}
	std::cout << "Progression : 151 espèces au niveau 100, " << Evolutions << " évolutions, " << Choices << " choix d'attaque, " << ItemsUsed << " objets utilisés\n";

	std::cout << (Errors ? "ERREURS : " + std::to_string(Errors) : std::string("Aucune erreur")) << "\n";
	return Errors ? 1 : 0;
}
