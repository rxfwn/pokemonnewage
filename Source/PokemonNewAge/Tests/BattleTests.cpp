// Tests de logique du combat (docs/COMBAT.md).
// Dans Unreal : Outils > Session Frontend > Automation, filtre « PokemonNewAge.Battle », puis Lancer.
// Ils construisent une petite base de données en mémoire : pas besoin des Data Tables importées.

#include "Misc/AutomationTest.h"
#include "Battle/BattleAI.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSession.h"
#include "Capture/PokemonStorage.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonInstance.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BattleTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UDataTable* MakeTable(UScriptStruct* Struct)
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = Struct;
		return Table;
	}

	FSpeciesRow Species(int32 Dex, EPokemonType Type1, EPokemonType Type2, int32 HP, int32 Atk, int32 Def, int32 SpA, int32 SpD, int32 Spe,
		int32 CatchRate = 255, int32 BaseExp = 60)
	{
		FSpeciesRow Row;
		Row.NationalDex = Dex;
		Row.Type1 = Type1;
		Row.Type2 = Type2;
		Row.BaseStats.HP = HP; Row.BaseStats.Attack = Atk; Row.BaseStats.Defense = Def;
		Row.BaseStats.SpecialAttack = SpA; Row.BaseStats.SpecialDefense = SpD; Row.BaseStats.Speed = Spe;
		Row.EVYield.Speed = 1;
		Row.CatchRate = CatchRate;
		Row.BaseExperience = BaseExp;
		Row.GrowthRate = EGrowthRate::MediumFast;
		Row.WeightKg = 10.f;
		return Row;
	}

	FMoveRow Move(EPokemonType Type, EMoveCategory Category, int32 Power, int32 Accuracy, int32 PP, int32 Priority = 0)
	{
		FMoveRow Row;
		Row.Type = Type;
		Row.Category = Category;
		Row.Power = Power;
		Row.Accuracy = Accuracy;
		Row.PP = PP;
		Row.Priority = Priority;
		Row.MetaCategory = Category == EMoveCategory::Status ? EMoveMetaCategory::NetGoodStats : EMoveMetaCategory::Damage;
		return Row;
	}

	/** Petite base : Bulbizarre, Salamèche, Rattata, Racaillou, Onix, et quelques attaques */
	UPokemonDatabase* MakeDatabase()
	{
		UPokemonDatabase* Db = NewObject<UPokemonDatabase>(GetTransientPackage());
		Db->Species = MakeTable(FSpeciesRow::StaticStruct());
		Db->Moves = MakeTable(FMoveRow::StaticStruct());
		Db->TypeChart = MakeTable(FTypeChartRow::StaticStruct());
		Db->Learnsets = MakeTable(FLearnsetRow::StaticStruct());
		Db->Experience = MakeTable(FExperienceRow::StaticStruct());
		Db->Trainers = MakeTable(FTrainerRow::StaticStruct());
		Db->Items = MakeTable(FItemRow::StaticStruct());

		FSpeciesRow Bulbasaur = Species(1, EPokemonType::Grass, EPokemonType::Poison, 45, 49, 49, 65, 65, 45, 45, 64);
		FPokemonEvolution Evolution;
		Evolution.Species = TEXT("Ivysaur");
		Evolution.MinLevel = 16;
		Bulbasaur.Evolutions.Add(Evolution);
		Db->Species->AddRow(TEXT("Bulbasaur"), Bulbasaur);
		Db->Species->AddRow(TEXT("Ivysaur"), Species(2, EPokemonType::Grass, EPokemonType::Poison, 60, 62, 63, 80, 80, 60, 45, 142));
		Db->Species->AddRow(TEXT("Charmander"), Species(4, EPokemonType::Fire, EPokemonType::None, 39, 52, 43, 60, 50, 65, 45, 62));
		Db->Species->AddRow(TEXT("Rattata"), Species(19, EPokemonType::Normal, EPokemonType::None, 30, 56, 35, 25, 35, 72, 255, 51));
		Db->Species->AddRow(TEXT("Geodude"), Species(74, EPokemonType::Rock, EPokemonType::Ground, 40, 80, 100, 30, 30, 20, 255, 60));
		Db->Species->AddRow(TEXT("Onix"), Species(95, EPokemonType::Rock, EPokemonType::Ground, 35, 45, 160, 30, 45, 70, 45, 77));

		Db->Moves->AddRow(TEXT("Tackle"), Move(EPokemonType::Normal, EMoveCategory::Physical, 40, 100, 35));
		Db->Moves->AddRow(TEXT("QuickAttack"), Move(EPokemonType::Normal, EMoveCategory::Physical, 40, 100, 30, 1));
		Db->Moves->AddRow(TEXT("VineWhip"), Move(EPokemonType::Grass, EMoveCategory::Physical, 45, 100, 25));
		Db->Moves->AddRow(TEXT("Ember"), Move(EPokemonType::Fire, EMoveCategory::Special, 40, 100, 25));
		Db->Moves->AddRow(TEXT("Struggle"), Move(EPokemonType::Normal, EMoveCategory::Physical, 50, 0, 1));
		FMoveRow Growl = Move(EPokemonType::Normal, EMoveCategory::Status, 0, 100, 40);
		Growl.Target = EMoveTarget::AllOpponents;
		FMoveStatChange Lower;
		Lower.Stat = EPokemonStat::Attack;
		Lower.Stages = -1;
		Growl.StatChanges.Add(Lower);
		Db->Moves->AddRow(TEXT("Growl"), Growl);
		FMoveRow ThunderWave = Move(EPokemonType::Electric, EMoveCategory::Status, 0, 100, 20);
		ThunderWave.MetaCategory = EMoveMetaCategory::Ailment;
		ThunderWave.Ailment = EMoveAilment::Paralysis;
		Db->Moves->AddRow(TEXT("ThunderWave"), ThunderWave);
		Db->Moves->AddRow(TEXT("RazorLeaf"), Move(EPokemonType::Grass, EMoveCategory::Physical, 55, 100, 25));

		// Table des types réduite : ce qui sert aux tests
		auto TypeRow = [](float Grass, float Fire, float Rock, float Ground, float Normal)
		{
			FTypeChartRow Row;
			Row.Grass = Grass; Row.Fire = Fire; Row.Rock = Rock; Row.Ground = Ground; Row.Normal = Normal;
			return Row;
		};
		Db->TypeChart->AddRow(TEXT("Normal"), TypeRow(1.f, 1.f, 0.5f, 1.f, 1.f));
		Db->TypeChart->AddRow(TEXT("Grass"), TypeRow(0.5f, 0.5f, 2.f, 2.f, 1.f));
		Db->TypeChart->AddRow(TEXT("Fire"), TypeRow(2.f, 0.5f, 0.5f, 1.f, 1.f));
		FTypeChartRow Electric = TypeRow(0.5f, 1.f, 1.f, 0.f, 1.f);
		Db->TypeChart->AddRow(TEXT("Electric"), Electric);

		// XP (courbe moyenne rapide : n³)
		for (int32 Level = 1; Level <= 100; ++Level)
		{
			FExperienceRow Row;
			Row.MediumFast = Level == 1 ? 0 : Level * Level * Level;
			Db->Experience->AddRow(FName(*FString::FromInt(Level)), Row);
		}

		FLearnsetRow Learnset;
		FLevelUpMove Entry;
		Entry.Level = 1; Entry.Move = TEXT("Tackle"); Learnset.LevelUpMoves.Add(Entry);
		Entry.Level = 1; Entry.Move = TEXT("Growl"); Learnset.LevelUpMoves.Add(Entry);
		Entry.Level = 7; Entry.Move = TEXT("VineWhip"); Learnset.LevelUpMoves.Add(Entry);
		Db->Learnsets->AddRow(TEXT("Bulbasaur"), Learnset);

		FItemRow PokeBall;
		PokeBall.Category = EItemCategory::PokeBall;
		PokeBall.CatchRateMultiplier = 1.f;
		Db->Items->AddRow(TEXT("PokeBall"), PokeBall);
		FItemRow MasterBall = PokeBall;
		MasterBall.bGuaranteedCatch = true;
		Db->Items->AddRow(TEXT("MasterBall"), MasterBall);
		FItemRow Potion;
		Potion.Category = EItemCategory::Medicine;
		Potion.HealHP = 20;
		Db->Items->AddRow(TEXT("Potion"), Potion);

		FTrainerRow Brock;
		Brock.PrizeMoney = 1400;
		Brock.bIsGymLeader = true;
		FTrainerPokemon Mon;
		Mon.Species = TEXT("Geodude"); Mon.Level = 12; Mon.Moves = { TEXT("Tackle") }; Brock.Party.Add(Mon);
		Mon.Species = TEXT("Onix"); Mon.Level = 14; Mon.Moves = { TEXT("Tackle") }; Brock.Party.Add(Mon);
		Db->Trainers->AddRow(TEXT("Brock"), Brock);
		return Db;
	}

	FPokemonInstance Make(const UPokemonDatabase* Db, FName Species, int32 Level, TArray<FName> Moves = {})
	{
		FRandomStream Stream(Level * 7 + 1);
		FPokemonInstance Pokemon;
		UPokemonLibrary::CreatePokemon(Db, Species, Level, Stream, Pokemon, 31);
		if (Moves.Num() > 0)
		{
			Pokemon.Moves.Reset();
			for (FName Move : Moves)
			{
				UPokemonLibrary::LearnMove(Db, Pokemon, Move);
			}
		}
		return Pokemon;
	}

	FBattleSide PlayerSide(TArray<FPokemonInstance> Party)
	{
		FBattleSide Side;
		Side.Party = MoveTemp(Party);
		Side.bGainsExperience = true;
		Side.Money = 3000;
		FItemStack Balls;
		Balls.Item = TEXT("PokeBall");
		Balls.Count = 5;
		Side.Inventory.Add(Balls);
		FItemStack Master;
		Master.Item = TEXT("MasterBall");
		Master.Count = 1;
		Side.Inventory.Add(Master);
		return Side;
	}

	bool HasEvent(const UBattleSession* Session, EBattleEventType Type, int32 Side = -2)
	{
		return Session->GetEventLog().ContainsByPredicate([Type, Side](const FBattleEvent& E) { return E.Type == Type && (Side == -2 || E.Side == Side); });
	}

	int32 IndexOf(const UBattleSession* Session, EBattleEventType Type, int32 Side)
	{
		return Session->GetEventLog().IndexOfByPredicate([Type, Side](const FBattleEvent& E) { return E.Type == Type && E.Side == Side; });
	}
}

using namespace BattleTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCaptureStorageTest, "PokemonNewAge.Capture.Storage", BattleTest::TestFlags)
bool FCaptureStorageTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();
	TArray<FPokemonInstance> Party;
	TArray<FPokemonInstance> Storage;
	bool bInParty = false;
	int32 Index = INDEX_NONE;

	// 6 dans l'équipe, le 7e part au PC, soigné
	for (int32 I = 0; I < UPokemonStorage::MaxPartySize; ++I)
	{
		TestTrue(TEXT("rangé dans l'équipe"), UPokemonStorage::Store(Party, Storage, Make(Db, TEXT("Rattata"), 3), bInParty, Index) && bInParty);
	}
	FPokemonInstance Hurt = Make(Db, TEXT("Onix"), 5);
	Hurt.CurrentHP = 1;
	TestTrue(TEXT("7e rangé"), UPokemonStorage::Store(Party, Storage, Hurt, bInParty, Index));
	TestFalse(TEXT("7e au PC"), bInParty);
	TestEqual(TEXT("place au PC"), Index, 0);
	TestEqual(TEXT("soigné au PC"), Storage[0].CurrentHP, Storage[0].GetMaxHP());
	TestFalse(TEXT("Pokémon vide refusé"), UPokemonStorage::Store(Party, Storage, FPokemonInstance(), bInParty, Index));

	// Retirer : équipe pleine
	EPokemonStorageError Error;
	TestFalse(TEXT("retirer, équipe pleine"), UPokemonStorage::Withdraw(Party, Storage, 0, Error));
	TestEqual(TEXT("raison équipe pleine"), Error, EPokemonStorageError::PartyFull);

	// Échange : Onix arrive dans l'équipe, Rattata part au PC
	TestTrue(TEXT("échange"), UPokemonStorage::Swap(Party, Storage, 2, 0, Error));
	TestEqual(TEXT("Onix dans l'équipe"), Party[2].Species, FName(TEXT("Onix")));
	TestEqual(TEXT("Rattata au PC"), Storage[0].Species, FName(TEXT("Rattata")));

	// Déposer : jamais le dernier Pokémon en forme
	TArray<FPokemonInstance> Two = { Make(Db, TEXT("Bulbasaur"), 5), Make(Db, TEXT("Rattata"), 3) };
	Two[1].CurrentHP = 0;
	TestFalse(TEXT("dernier en forme"), UPokemonStorage::Deposit(Two, Storage, 0, Error));
	TestEqual(TEXT("raison dernier en forme"), Error, EPokemonStorageError::LastAblePokemon);
	TestTrue(TEXT("déposer le K.O."), UPokemonStorage::Deposit(Two, Storage, 1, Error));
	TestEqual(TEXT("équipe d'un seul"), Two.Num(), 1);
	TestEqual(TEXT("K.O. soigné au PC"), Storage.Last().CurrentHP, Storage.Last().GetMaxHP());
	TestTrue(TEXT("retirer"), UPokemonStorage::Withdraw(Two, Storage, Storage.Num() - 1, Error));
	TestEqual(TEXT("équipe de deux"), Two.Num(), 2);

	// Relâcher
	const int32 Before = Storage.Num();
	TestTrue(TEXT("relâcher"), UPokemonStorage::Release(Storage, 0, Error));
	TestEqual(TEXT("un de moins"), Storage.Num(), Before - 1);
	TestFalse(TEXT("relâcher une case vide"), UPokemonStorage::Release(Storage, 99, Error));

	// PC plein : plus de place pour capturer
	TArray<FPokemonInstance> FullStorage;
	for (int32 I = 0; I < UPokemonStorage::MaxStorageSize; ++I)
	{
		FullStorage.Add(Make(Db, TEXT("Rattata"), 2));
	}
	TestFalse(TEXT("plus de place"), UPokemonStorage::HasRoom(Party, FullStorage));
	TestFalse(TEXT("rangement refusé"), UPokemonStorage::Store(Party, FullStorage, Make(Db, TEXT("Rattata"), 2), bInParty, Index));

	// Surnoms
	FString Nickname;
	TestTrue(TEXT("surnom simple"), UPokemonStorage::CleanNickname(TEXT("  Bulbi  "), Nickname) && Nickname == FString(TEXT("Bulbi")));
	TestTrue(TEXT("surnom vide = nom de l'espèce"), UPokemonStorage::CleanNickname(TEXT(""), Nickname) && Nickname.IsEmpty());
	TestTrue(TEXT("12 caractères"), UPokemonStorage::CleanNickname(TEXT("Mr Rattatata"), Nickname));
	TestFalse(TEXT("13 caractères"), UPokemonStorage::CleanNickname(TEXT("Mr Rattatatat"), Nickname));
	TestFalse(TEXT("balise refusée"), UPokemonStorage::CleanNickname(TEXT("<b>Bulbi"), Nickname));
	TestFalse(TEXT("double espace"), UPokemonStorage::CleanNickname(TEXT("Bul  bi"), Nickname));

	// Messages après capture
	FCaughtPokemonInfo Info;
	Info.Pokemon = Make(Db, TEXT("Onix"), 5);
	Info.bInParty = false;
	Info.Index = 31;
	Info.bNewInPokedex = true;
	const TArray<FText> Messages = UPokemonStorage::GetCaughtMessages(Db, Info);
	TestEqual(TEXT("3 messages"), Messages.Num(), 3);
	int32 Found = INDEX_NONE;
	TestTrue(TEXT("boîte 2"), Messages.Num() == 3 && Messages[1].ToString().FindChar(TEXT('2'), Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCaptureBattleTest, "PokemonNewAge.Capture.Battle", BattleTest::TestFlags)
bool FCaptureBattleTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();

	// Équipe et PC pleins : la Ball est refusée et n'est pas consommée
	FBattleSide Side = PlayerSide({ Make(Db, TEXT("Bulbasaur"), 5, { TEXT("Tackle") }) });
	Side.bCanStoreCaughtPokemon = false;
	UBattleSession* Session = NewObject<UBattleSession>();
	Session->InitWildBattle(Db, Side, Make(Db, TEXT("Rattata"), 3, { TEXT("Tackle") }), 31);
	Session->Start();
	EBattleActionError Error;
	TestFalse(TEXT("Ball refusée"), Session->SubmitAction(0, FBattleAction::MakeItem(TEXT("MasterBall")), Error));
	TestEqual(TEXT("raison plus de place"), Error, EBattleActionError::NoRoomForPokemon);
	TestEqual(TEXT("Ball gardée"), Session->GetSide(0).GetItemCount(TEXT("MasterBall")), 1);

	// Hautes herbes : environ 11,7 % par pas avec le taux 21 des routes
	TestTrue(TEXT("taux 21"), FMath::Abs(UBattleRules::GetEncounterChancePerStep(21) - 336.f / 2880.f) < 0.0001f);
	TestEqual(TEXT("taux 0 = jamais"), UBattleRules::GetEncounterChancePerStep(0), 0.f);
	FRandomStream Stream(7);
	int32 Encounters = 0;
	for (int32 Step = 0; Step < 20000; ++Step)
	{
		Encounters += UBattleRules::RollEncounterStep(21, Stream) ? 1 : 0;
	}
	TestTrue(TEXT("fréquence des rencontres"), FMath::Abs(Encounters / 20000.f - UBattleRules::GetEncounterChancePerStep(21)) < 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleDamageFormulaTest, "PokemonNewAge.Battle.Rules.DamageFormula", BattleTest::TestFlags)
bool FBattleDamageFormulaTest::RunTest(const FString& Parameters)
{
	// N.50, puissance 80, Attaque 100 contre Défense 100 : 22 x 80 x 100 / 100 / 50 + 2 = 37
	FDamageInput Input;
	Input.AttackerLevel = 50;
	Input.Power = 80;
	Input.Attack = 100;
	Input.Defense = 100;
	TestEqual(TEXT("dégâts de base"), UBattleRules::CalculateDamage(Input), 37);

	Input.bSTAB = true;
	TestEqual(TEXT("STAB x1,5"), UBattleRules::CalculateDamage(Input), 55);

	Input.TypeEffectiveness = 2.f;
	TestEqual(TEXT("super efficace"), UBattleRules::CalculateDamage(Input), 110);

	Input.bCritical = true;
	TestEqual(TEXT("critique x1,5"), UBattleRules::CalculateDamage(Input), 164); // 37 -> 55 (critique) -> 82 (STAB) -> 164

	Input = FDamageInput();
	Input.AttackerLevel = 50; Input.Power = 80; Input.Attack = 100; Input.Defense = 100;
	Input.RandomPercent = 85;
	TestEqual(TEXT("tirage minimum 85 %"), UBattleRules::CalculateDamage(Input), 31);

	Input.RandomPercent = 100;
	Input.bBurned = true;
	TestEqual(TEXT("brûlure / 2"), UBattleRules::CalculateDamage(Input), 18);

	Input.TypeEffectiveness = 0.f;
	TestEqual(TEXT("immunité"), UBattleRules::CalculateDamage(Input), 0);

	Input = FDamageInput();
	Input.AttackerLevel = 2; Input.Power = 10; Input.Attack = 5; Input.Defense = 300; Input.TypeEffectiveness = 0.25f;
	TestEqual(TEXT("au moins 1"), UBattleRules::CalculateDamage(Input), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleStatFormulaTest, "PokemonNewAge.Battle.Rules.Stats", BattleTest::TestFlags)
bool FBattleStatFormulaTest::RunTest(const FString& Parameters)
{
	// Base 100, IV 31, EV 0, N.100
	TestEqual(TEXT("PV"), UPokemonLibrary::CalculateStat(EPokemonStat::HP, 100, 31, 0, 100), 341);
	TestEqual(TEXT("stat neutre"), UPokemonLibrary::CalculateStat(EPokemonStat::Attack, 100, 31, 0, 100), 236);
	TestEqual(TEXT("nature +"), UPokemonLibrary::CalculateStat(EPokemonStat::Attack, 100, 31, 0, 100, 110), 259);
	TestEqual(TEXT("nature -"), UPokemonLibrary::CalculateStat(EPokemonStat::Attack, 100, 31, 0, 100, 90), 212);
	TestEqual(TEXT("252 EV"), UPokemonLibrary::CalculateStat(EPokemonStat::Attack, 100, 31, 252, 100), 299);
	// Bulbizarre N.5, IV 31 : PV = (90 + 31) x 5 / 100 + 15 = 21
	TestEqual(TEXT("Bulbizarre N.5 PV"), UPokemonLibrary::CalculateStat(EPokemonStat::HP, 45, 31, 0, 5), 21);

	TestEqual(TEXT("cran +1"), UBattleRules::GetStatStageMultiplier(1), 1.5f);
	TestEqual(TEXT("cran -2"), UBattleRules::GetStatStageMultiplier(-2), 0.5f);
	TestEqual(TEXT("cran +6"), UBattleRules::GetStatStageMultiplier(6), 4.f);
	TestEqual(TEXT("précision -1"), UBattleRules::GetAccuracyStageMultiplier(-1), 0.75f);
	TestEqual(TEXT("critique cran 0"), UBattleRules::GetCriticalHitDenominator(0), 24);
	TestEqual(TEXT("critique cran 1"), UBattleRules::GetCriticalHitDenominator(1), 8);
	TestEqual(TEXT("paralysie"), UBattleRules::GetEffectiveSpeed(100, 0, true), 50);

	FBattleStatStages Stages;
	TestEqual(TEXT("+2"), Stages.Add(EPokemonStat::Attack, 2), 2);
	TestEqual(TEXT("bloqué à +6"), Stages.Add(EPokemonStat::Attack, 6), 4);
	TestEqual(TEXT("déjà au max"), Stages.Add(EPokemonStat::Attack, 1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleCaptureFormulaTest, "PokemonNewAge.Battle.Rules.Capture", BattleTest::TestFlags)
bool FBattleCaptureFormulaTest::RunTest(const FString& Parameters)
{
	// PV pleins, taux 255, Poké Ball : a = 85
	TestEqual(TEXT("a PV pleins"), UBattleRules::GetCatchValue(30, 30, 255, 1.f, EMajorStatus::None), 85.f);
	TestEqual(TEXT("a endormi"), UBattleRules::GetCatchValue(30, 30, 255, 1.f, EMajorStatus::Sleep), 212.5f);
	TestTrue(TEXT("a >= 255 : sûr"), UBattleRules::GetCatchProbability(UBattleRules::GetCatchValue(30, 1, 255, 2.f, EMajorStatus::None)) >= 1.f);

	const float FullHP = UBattleRules::GetCatchProbability(UBattleRules::GetCatchValue(100, 100, 45, 1.f, EMajorStatus::None));
	const float LowHP = UBattleRules::GetCatchProbability(UBattleRules::GetCatchValue(100, 1, 45, 1.f, EMajorStatus::None));
	const float LowHPUltra = UBattleRules::GetCatchProbability(UBattleRules::GetCatchValue(100, 1, 45, 2.f, EMajorStatus::None));
	TestTrue(TEXT("PV bas = plus facile"), LowHP > FullHP);
	TestTrue(TEXT("Hyper Ball = plus facile"), LowHPUltra > LowHP);
	// Formule actuelle (6G et plus) : (a / 255)^0,75, soit ~12 % pour un starter à PV pleins
	TestTrue(TEXT("starter PV pleins ~ 12 %"), FullHP > 0.10f && FullHP < 0.14f);

	FRandomStream Stream(1);
	int32 Shakes = 0;
	TestTrue(TEXT("Master Ball"), UBattleRules::RollCapture(1.f, true, Stream, Shakes));

	// Sur beaucoup de lancers, la fréquence suit la probabilité
	int32 Caught = 0;
	for (int32 I = 0; I < 4000; ++I)
	{
		Caught += UBattleRules::RollCapture(85.f, false, Stream, Shakes) ? 1 : 0;
	}
	const float Expected = UBattleRules::GetCatchProbability(85.f);
	TestTrue(TEXT("fréquence de capture"), FMath::Abs(Caught / 4000.f - Expected) < 0.03f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleExperienceFormulaTest, "PokemonNewAge.Battle.Rules.Experience", BattleTest::TestFlags)
bool FBattleExperienceFormulaTest::RunTest(const FString& Parameters)
{
	// Rattata (51) N.3 battu par un N.5 : 51 x 3 / 5 x (16/18)^2,5 + 1 = 23
	TestEqual(TEXT("sauvage"), UBattleRules::GetExperienceGain(51, 3, 5, false, true), 23);
	TestEqual(TEXT("dresseur x1,5"), UBattleRules::GetExperienceGain(51, 3, 5, true, true), 35);
	TestEqual(TEXT("sans combattre : moitié"), UBattleRules::GetExperienceGain(51, 3, 5, false, false), 11);
	TestTrue(TEXT("plus bas niveau = plus d'XP"), UBattleRules::GetExperienceGain(51, 10, 5, false, true) > UBattleRules::GetExperienceGain(51, 10, 15, false, true));

	FRandomStream Stream(3);
	TestTrue(TEXT("fuite plus rapide"), UBattleRules::RollEscape(50, 40, 1, Stream));
	TestEqual(TEXT("argent perdu sans badge"), UBattleRules::GetMoneyLost(0, 10, 3000), 80);
	TestEqual(TEXT("argent perdu borné"), UBattleRules::GetMoneyLost(8, 100, 500), 500);
	TestTrue(TEXT("Feu ne brûle pas"), UBattleRules::IsImmuneToStatus(EMajorStatus::Burn, EPokemonType::Fire, EPokemonType::None));
	TestTrue(TEXT("Acier pas empoisonné"), UBattleRules::IsImmuneToStatus(EMajorStatus::BadlyPoisoned, EPokemonType::Rock, EPokemonType::Steel));
	TestFalse(TEXT("Normal peut être paralysé"), UBattleRules::IsImmuneToStatus(EMajorStatus::Paralysis, EPokemonType::Normal, EPokemonType::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattlePokemonInstanceTest, "PokemonNewAge.Battle.PokemonInstance", BattleTest::TestFlags)
bool FBattlePokemonInstanceTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();
	FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 5);
	TestEqual(TEXT("PV max"), Bulbasaur.GetMaxHP(), 21);
	TestEqual(TEXT("PV pleins"), Bulbasaur.CurrentHP, 21);
	TestEqual(TEXT("XP du niveau 5"), Bulbasaur.Experience, 125);
	TestEqual(TEXT("attaques par défaut"), Bulbasaur.Moves.Num(), 2);
	TestEqual(TEXT("PP de Charge"), Bulbasaur.Moves[0].MaxPP, 35);

	TArray<int32> Levels;
	UPokemonLibrary::AddExperience(Db, Bulbasaur, 216 - 125, Levels);
	TestEqual(TEXT("monte au N.6"), Bulbasaur.Level, 6);
	TestEqual(TEXT("un niveau"), Levels.Num(), 1);
	TestTrue(TEXT("PV gagnés"), Bulbasaur.GetMaxHP() > 21 && Bulbasaur.CurrentHP == Bulbasaur.GetMaxHP());

	TestTrue(TEXT("apprend une 3e attaque"), UPokemonLibrary::LearnMove(Db, Bulbasaur, TEXT("VineWhip")));
	TestFalse(TEXT("pas deux fois"), UPokemonLibrary::LearnMove(Db, Bulbasaur, TEXT("VineWhip")));
	UPokemonLibrary::LearnMove(Db, Bulbasaur, TEXT("RazorLeaf"));
	TestFalse(TEXT("5e attaque sans oublier : refusé"), UPokemonLibrary::LearnMove(Db, Bulbasaur, TEXT("QuickAttack")));
	TestTrue(TEXT("5e attaque en oubliant la 2e"), UPokemonLibrary::LearnMove(Db, Bulbasaur, TEXT("QuickAttack"), 1));
	TestEqual(TEXT("case 2 remplacée"), Bulbasaur.Moves[1].Move, FName(TEXT("QuickAttack")));

	TestTrue(TEXT("pas encore d'évolution"), UPokemonLibrary::GetLevelEvolution(Db, Bulbasaur).IsNone());
	Bulbasaur.Level = 16;
	TestEqual(TEXT("évolue au N.16"), UPokemonLibrary::GetLevelEvolution(Db, Bulbasaur), FName(TEXT("Ivysaur")));
	TArray<FName> NewMoves;
	TestTrue(TEXT("Evolve"), UPokemonLibrary::Evolve(Db, Bulbasaur, TEXT("Ivysaur"), NewMoves));
	TestEqual(TEXT("espèce"), Bulbasaur.Species, FName(TEXT("Ivysaur")));

	FPokemonInstance Full = Make(Db, TEXT("Rattata"), 5);
	FPokemonBaseStats Yield;
	Yield.Speed = 300;
	UPokemonLibrary::AddEVs(Full, Yield);
	TestEqual(TEXT("EV bornés à 252"), Full.EVs.Speed, 252);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleTurnOrderTest, "PokemonNewAge.Battle.Session.TurnOrder", BattleTest::TestFlags)
bool FBattleTurnOrderTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();

	// Rattata (Vitesse 72) est plus rapide que Bulbizarre (45) : il attaque en premier
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 5, { TEXT("Tackle") }) }), Make(Db, TEXT("Rattata"), 5, { TEXT("Tackle") }), 11);
		Session->Start();
		EBattleActionError Error;
		TestTrue(TEXT("action acceptée"), Session->SubmitAction(0, FBattleAction::MakeMove(0), Error));
		const int32 First = IndexOf(Session, EBattleEventType::MoveUsed, 1);
		const int32 Second = IndexOf(Session, EBattleEventType::MoveUsed, 0);
		TestTrue(TEXT("le plus rapide d'abord"), First != INDEX_NONE && Second != INDEX_NONE && First < Second);
		TestEqual(TEXT("PP dépensé"), Session->GetSide(0).Party[0].Moves[0].CurrentPP, 34);
		TestEqual(TEXT("tour suivant"), Session->GetTurnNumber(), 2);
	}

	// Vive-Attaque (priorité +1) passe avant, même plus lent
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 5, { TEXT("QuickAttack") }) }), Make(Db, TEXT("Rattata"), 5, { TEXT("Tackle") }), 12);
		Session->Start();
		EBattleActionError Error;
		Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
		TestTrue(TEXT("priorité"), IndexOf(Session, EBattleEventType::MoveUsed, 0) < IndexOf(Session, EBattleEventType::MoveUsed, 1));
	}

	// Paralysé, Rattata passe derrière (Vitesse / 2)
	{
		FPokemonInstance Rattata = Make(Db, TEXT("Rattata"), 5, { TEXT("Tackle") });
		Rattata.Status = EMajorStatus::Paralysis;
		FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 5, { TEXT("Tackle") });
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Bulbasaur }), Rattata, 13);
		Session->Start();
		EBattleActionError Error;
		Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
		TestTrue(TEXT("paralysie : plus lent"), IndexOf(Session, EBattleEventType::MoveUsed, 0) < IndexOf(Session, EBattleEventType::MoveUsed, 1)
			|| IndexOf(Session, EBattleEventType::MoveUsed, 1) == INDEX_NONE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleWildFlowTest, "PokemonNewAge.Battle.Session.WildBattle", BattleTest::TestFlags)
bool FBattleWildFlowTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();

	// Un Bulbizarre N.20 bat un Rattata N.2, gagne de l'XP et des EV
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 20, { TEXT("VineWhip") }) }), Make(Db, TEXT("Rattata"), 2, { TEXT("Tackle") }), 21);
		Session->Start();
		EBattleActionError Error;
		for (int32 Turn = 0; Turn < 5 && Session->GetPhase() == EBattlePhase::WaitingForActions; ++Turn)
		{
			Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
		}
		TestEqual(TEXT("gagné"), Session->GetResult().Outcome, EBattleOutcome::Won);
		TestTrue(TEXT("K.O. annoncé"), HasEvent(Session, EBattleEventType::Fainted, 1));
		TestTrue(TEXT("XP gagnée"), HasEvent(Session, EBattleEventType::ExpGained, 0));
		TestEqual(TEXT("EV de Vitesse"), Session->GetSide(0).Party[0].EVs.Speed, 1);
		TestEqual(TEXT("phase finale"), Session->GetPhase(), EBattlePhase::Ended);
	}

	// Fuite : toujours réussie si on est plus rapide
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Rattata"), 10, { TEXT("Tackle") }) }), Make(Db, TEXT("Bulbasaur"), 3, { TEXT("Tackle") }), 22);
		Session->Start();
		EBattleActionError Error;
		TestTrue(TEXT("fuite acceptée"), Session->SubmitAction(0, FBattleAction::MakeRun(), Error));
		TestEqual(TEXT("fui"), Session->GetResult().Outcome, EBattleOutcome::Fled);
	}

	// Master Ball : capture, Ball consommée, Pokémon rendu dans le résultat
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 5, { TEXT("Tackle") }) }), Make(Db, TEXT("Onix"), 5, { TEXT("Tackle") }), 23);
		Session->Start();
		EBattleActionError Error;
		TestTrue(TEXT("Ball acceptée"), Session->SubmitAction(0, FBattleAction::MakeItem(TEXT("MasterBall")), Error));
		TestEqual(TEXT("capturé"), Session->GetResult().Outcome, EBattleOutcome::Caught);
		TestEqual(TEXT("espèce capturée"), Session->GetResult().CaughtPokemon.Species, FName(TEXT("Onix")));
		TestEqual(TEXT("Ball notée"), Session->GetResult().CaughtPokemon.Ball, FName(TEXT("MasterBall")));
		TestEqual(TEXT("Master Ball consommée"), Session->GetSide(0).GetItemCount(TEXT("MasterBall")), 0);
	}

	// Actions refusées
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 5, { TEXT("Tackle") }) }), Make(Db, TEXT("Rattata"), 5, { TEXT("Tackle") }), 24);
		Session->Start();
		EBattleActionError Error;
		TestFalse(TEXT("attaque inexistante"), Session->SubmitAction(0, FBattleAction::MakeMove(3), Error));
		TestEqual(TEXT("raison"), Error, EBattleActionError::InvalidMove);
		TestFalse(TEXT("Potion sans effet (PV pleins)"), Session->SubmitAction(0, FBattleAction::MakeItem(TEXT("Potion"), 0), Error));
		TestFalse(TEXT("changer vers soi"), Session->SubmitAction(0, FBattleAction::MakeSwitch(0), Error));
		TestEqual(TEXT("raison changement"), Error, EBattleActionError::AlreadyInBattle);
	}

	// Plus de PP : Lutte
	{
		FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 5, { TEXT("Tackle") });
		Bulbasaur.Moves[0].CurrentPP = 0;
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Bulbasaur }), Make(Db, TEXT("Rattata"), 5, { TEXT("Tackle") }), 25);
		Session->Start();
		TestTrue(TEXT("doit lutter"), Session->MustStruggle(0));
		EBattleActionError Error;
		TestTrue(TEXT("Lutte acceptée"), Session->SubmitAction(0, FBattleAction::MakeMove(0), Error));
		TestTrue(TEXT("Lutte utilisée"), Session->GetEventLog().ContainsByPredicate([](const FBattleEvent& E)
		{
			return E.Type == EBattleEventType::MoveUsed && E.Side == 0 && E.Move == FName(TEXT("Struggle"));
		}));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleStatusTest, "PokemonNewAge.Battle.Session.Status", BattleTest::TestFlags)
bool FBattleStatusTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();

	// Cage-Éclair ne touche pas Racaillou (Sol)
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Rattata"), 10, { TEXT("ThunderWave") }) }), Make(Db, TEXT("Geodude"), 3, { TEXT("Tackle") }), 31);
		Session->Start();
		EBattleActionError Error;
		Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
		TestTrue(TEXT("sans effet"), HasEvent(Session, EBattleEventType::NoEffect, 1));
		TestEqual(TEXT("pas paralysé"), Session->GetSide(1).Party[0].Status, EMajorStatus::None);
	}

	// Cage-Éclair paralyse Rattata ; Rugissement baisse son Attaque
	{
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 10, { TEXT("ThunderWave"), TEXT("Growl") }) }), Make(Db, TEXT("Rattata"), 3, { TEXT("Tackle") }), 32);
		Session->Start();
		EBattleActionError Error;
		Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
		TestEqual(TEXT("paralysé"), Session->GetSide(1).Party[0].Status, EMajorStatus::Paralysis);
		Session->SubmitAction(0, FBattleAction::MakeMove(1), Error);
		TestEqual(TEXT("Attaque -1"), Session->GetSide(1).Volatile.Stages.Attack, -1);
	}

	// Brûlure : 1/16 des PV en fin de tour
	{
		FPokemonInstance Rattata = Make(Db, TEXT("Rattata"), 20, { TEXT("Growl") });
		Rattata.Status = EMajorStatus::Burn;
		UBattleSession* Session = NewObject<UBattleSession>();
		Session->InitWildBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 20, { TEXT("Growl") }) }), Rattata, 33);
		Session->Start();
		EBattleActionError Error;
		Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
		const FPokemonInstance& After = Session->GetSide(1).Party[0];
		TestEqual(TEXT("dégâts de brûlure"), After.GetMaxHP() - After.CurrentHP, After.GetMaxHP() / 16);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleTrainerTest, "PokemonNewAge.Battle.Session.TrainerBattle", BattleTest::TestFlags)
bool FBattleTrainerTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();
	UBattleSession* Session = NewObject<UBattleSession>();
	TestTrue(TEXT("dresseur trouvé"), Session->InitTrainerBattle(Db, PlayerSide({ Make(Db, TEXT("Bulbasaur"), 40, { TEXT("RazorLeaf") }) }), TEXT("Brock"), 25, 41));
	TestEqual(TEXT("2 Pokémon"), Session->GetSide(1).Party.Num(), 2);
	Session->Start();

	EBattleActionError Error;
	TestFalse(TEXT("pas de fuite"), Session->SubmitAction(0, FBattleAction::MakeRun(), Error));
	TestEqual(TEXT("raison fuite"), Error, EBattleActionError::CannotRunFromTrainer);
	TestFalse(TEXT("pas de capture"), Session->SubmitAction(0, FBattleAction::MakeItem(TEXT("PokeBall")), Error));
	TestEqual(TEXT("raison Ball"), Error, EBattleActionError::CannotCatchTrainerPokemon);

	for (int32 Turn = 0; Turn < 10 && Session->GetPhase() != EBattlePhase::Ended; ++Turn)
	{
		if (Session->GetPhase() == EBattlePhase::WaitingForMoveLearn)
		{
			Session->ResolveMoveLearn(0, -1);
			continue;
		}
		Session->SubmitAction(0, FBattleAction::MakeMove(0), Error);
	}
	TestEqual(TEXT("gagné"), Session->GetResult().Outcome, EBattleOutcome::Won);
	TestEqual(TEXT("argent gagné"), Session->GetResult().MoneyDelta, 1400);
	TestEqual(TEXT("Onix envoyé ensuite"), Session->GetEventLog().FilterByPredicate([](const FBattleEvent& E)
	{
		return E.Type == EBattleEventType::SentOut && E.Side == 1;
	}).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleLevelUpTest, "PokemonNewAge.Battle.Session.LevelUpAndEvolution", BattleTest::TestFlags)
bool FBattleLevelUpTest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();

	// Bulbizarre N.6 avec 4 attaques bat un Onix N.70 : il passe le N.7 (veut Fouet Lianes) et atteint le N.16
	FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 15, { TEXT("Tackle"), TEXT("Growl"), TEXT("QuickAttack"), TEXT("Ember") });
	Bulbasaur.Level = 6;
	Bulbasaur.Experience = 216;
	UPokemonLibrary::RecalculateStats(Db, Bulbasaur);
	Bulbasaur.Stats.Attack = 999; // assez fort pour gagner en un coup
	FPokemonInstance Onix = Make(Db, TEXT("Onix"), 70, { TEXT("Tackle") });
	Onix.CurrentHP = 1;
	Onix.Stats.Attack = 1;

	UBattleSession* Session = NewObject<UBattleSession>();
	Session->InitWildBattle(Db, PlayerSide({ Bulbasaur }), Onix, 51);
	Session->Start();
	EBattleActionError Error;
	Session->SubmitAction(0, FBattleAction::MakeMove(2), Error); // Vive-Attaque : passe en premier

	TestEqual(TEXT("attend le choix de l'attaque"), Session->GetPhase(), EBattlePhase::WaitingForMoveLearn);
	TestTrue(TEXT("niveau gagné"), HasEvent(Session, EBattleEventType::LevelUp, 0));
	TestTrue(TEXT("oublier Rugissement"), Session->ResolveMoveLearn(0, 1));
	TestEqual(TEXT("Fouet Lianes appris"), Session->GetSide(0).Party[0].Moves[1].Move, FName(TEXT("VineWhip")));
	TestEqual(TEXT("fin du combat"), Session->GetResult().Outcome, EBattleOutcome::Won);
	TestTrue(TEXT("au moins N.16"), Session->GetSide(0).Party[0].Level >= 16);
	TestEqual(TEXT("évolution proposée"), Session->GetResult().Evolutions.Num(), 1);
	if (Session->GetResult().Evolutions.Num() == 1)
	{
		TestEqual(TEXT("en Herbizarre"), Session->GetResult().Evolutions[0].NewSpecies, FName(TEXT("Ivysaur")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleAITest, "PokemonNewAge.Battle.AI", BattleTest::TestFlags)
bool FBattleAITest::RunTest(const FString& Parameters)
{
	UPokemonDatabase* Db = MakeDatabase();
	// Le dresseur a Charge (pas très efficace sur Racaillou) et Fouet Lianes (x4) : il choisit surtout Fouet Lianes
	UBattleSession* Session = NewObject<UBattleSession>();
	FBattleSide Player = PlayerSide({ Make(Db, TEXT("Geodude"), 10, { TEXT("Tackle") }) });
	FBattleSide Trainer;
	Trainer.Party.Add(Make(Db, TEXT("Bulbasaur"), 10, { TEXT("Tackle"), TEXT("VineWhip"), TEXT("ThunderWave") }));
	Trainer.bIsAI = true;
	Trainer.TrainerId = TEXT("Test");
	Session->InitBattle(Db, EBattleType::Trainer, Player, Trainer, 61);

	TestTrue(TEXT("Fouet Lianes mieux noté que Charge"), FBattleAI::ScoreMove(*Session, 1, 1) > FBattleAI::ScoreMove(*Session, 1, 0));
	TestEqual(TEXT("Cage-Éclair inutile sur Sol"), FBattleAI::ScoreMove(*Session, 1, 2), 0.f);

	FRandomStream Stream(7);
	int32 VineWhip = 0;
	for (int32 I = 0; I < 200; ++I)
	{
		VineWhip += FBattleAI::ChooseAction(*Session, 1, Stream).MoveSlot == 1 ? 1 : 0;
	}
	TestTrue(TEXT("choisit surtout la meilleure attaque"), VineWhip > 150);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
