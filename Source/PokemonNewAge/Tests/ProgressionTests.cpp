// Tests de la progression hors combat (docs/PROGRESSION.md) : Super Bonbon, pierres, Fil de Liaison, CT, évolution.
// Dans Unreal : Outils > Session Frontend > Automation, filtre « PokemonNewAge.Progression », puis Lancer.
// Sans Unreal : ./Tools/BattleSim/run.sh (même faux moteur que les tests du combat).

#include "Misc/AutomationTest.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonInstance.h"
#include "Pokemon/PokemonProgression.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProgressionTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UDataTable* MakeTable(UScriptStruct* Struct)
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = Struct;
		return Table;
	}

	FSpeciesRow Species(int32 Dex, int32 Base)
	{
		FSpeciesRow Row;
		Row.NationalDex = Dex;
		Row.Type1 = EPokemonType::Normal;
		Row.BaseStats.HP = Base; Row.BaseStats.Attack = Base; Row.BaseStats.Defense = Base;
		Row.BaseStats.SpecialAttack = Base; Row.BaseStats.SpecialDefense = Base; Row.BaseStats.Speed = Base;
		Row.GrowthRate = EGrowthRate::MediumFast;
		Row.BaseExperience = 64;
		return Row;
	}

	FPokemonEvolution Evolution(const TCHAR* Into, EEvolutionTrigger Trigger, int32 Level = 0, const TCHAR* Item = nullptr)
	{
		FPokemonEvolution Evo;
		Evo.Species = Into;
		Evo.Trigger = Trigger;
		Evo.MinLevel = Level;
		Evo.Item = Item ? FName(Item) : FName();
		return Evo;
	}

	FMoveRow Move(int32 Power)
	{
		FMoveRow Row;
		Row.Type = EPokemonType::Normal;
		Row.Category = Power > 0 ? EMoveCategory::Physical : EMoveCategory::Status;
		Row.Power = Power;
		Row.Accuracy = 100;
		Row.PP = 20;
		return Row;
	}

	FItemRow Item(EFieldItemEffect Effect)
	{
		FItemRow Row;
		Row.Category = EItemCategory::Other;
		Row.FieldEffect = Effect;
		return Row;
	}

	/** Bulbizarre (niveau 16), Pikachu (Pierre Foudre), Kadabra (échange), et de quoi tester les CT */
	UPokemonDatabase* MakeDatabase()
	{
		UPokemonDatabase* Db = NewObject<UPokemonDatabase>(GetTransientPackage());
		Db->Species = MakeTable(FSpeciesRow::StaticStruct());
		Db->Moves = MakeTable(FMoveRow::StaticStruct());
		Db->Learnsets = MakeTable(FLearnsetRow::StaticStruct());
		Db->Experience = MakeTable(FExperienceRow::StaticStruct());
		Db->Items = MakeTable(FItemRow::StaticStruct());

		FSpeciesRow Bulbasaur = Species(1, 45);
		Bulbasaur.Evolutions.Add(Evolution(TEXT("Ivysaur"), EEvolutionTrigger::Level, 16));
		Db->Species->AddRow(TEXT("Bulbasaur"), Bulbasaur);
		Db->Species->AddRow(TEXT("Ivysaur"), Species(2, 60));
		FSpeciesRow Pikachu = Species(25, 50);
		Pikachu.Evolutions.Add(Evolution(TEXT("Raichu"), EEvolutionTrigger::Item, 0, TEXT("ThunderStone")));
		Db->Species->AddRow(TEXT("Pikachu"), Pikachu);
		Db->Species->AddRow(TEXT("Raichu"), Species(26, 80));
		FSpeciesRow Kadabra = Species(64, 60);
		Kadabra.Evolutions.Add(Evolution(TEXT("Alakazam"), EEvolutionTrigger::Trade));
		Db->Species->AddRow(TEXT("Kadabra"), Kadabra);
		Db->Species->AddRow(TEXT("Alakazam"), Species(65, 90));

		for (const TCHAR* Name : { TEXT("Tackle"), TEXT("Growl"), TEXT("LeechSeed"), TEXT("VineWhip"), TEXT("PoisonPowder"), TEXT("RazorLeaf"), TEXT("RockTomb") })
		{
			Db->Moves->AddRow(Name, Move(40));
		}

		for (int32 Level = 1; Level <= 100; ++Level)
		{
			FExperienceRow Row;
			Row.MediumFast = Level == 1 ? 0 : Level * Level * Level;
			Db->Experience->AddRow(FName(*FString::FromInt(Level)), Row);
		}

		FLearnsetRow BulbasaurMoves;
		FLevelUpMove Entry;
		Entry.Level = 1; Entry.Move = TEXT("Tackle"); BulbasaurMoves.LevelUpMoves.Add(Entry);
		Entry.Level = 1; Entry.Move = TEXT("Growl"); BulbasaurMoves.LevelUpMoves.Add(Entry);
		Entry.Level = 7; Entry.Move = TEXT("LeechSeed"); BulbasaurMoves.LevelUpMoves.Add(Entry);
		Entry.Level = 10; Entry.Move = TEXT("VineWhip"); BulbasaurMoves.LevelUpMoves.Add(Entry);
		Entry.Level = 15; Entry.Move = TEXT("PoisonPowder"); BulbasaurMoves.LevelUpMoves.Add(Entry);
		BulbasaurMoves.MachineMoves.Add(TEXT("RazorLeaf"));
		Db->Learnsets->AddRow(TEXT("Bulbasaur"), BulbasaurMoves);
		FLearnsetRow IvysaurMoves;
		Entry.Level = 16; Entry.Move = TEXT("RazorLeaf"); IvysaurMoves.LevelUpMoves.Add(Entry);
		Db->Learnsets->AddRow(TEXT("Ivysaur"), IvysaurMoves);

		FItemRow Candy = Item(EFieldItemEffect::LevelUp);
		Candy.LevelsGained = 1;
		Db->Items->AddRow(TEXT("RareCandy"), Candy);
		Db->Items->AddRow(TEXT("ThunderStone"), Item(EFieldItemEffect::EvolutionItem));
		Db->Items->AddRow(TEXT("LinkingCord"), Item(EFieldItemEffect::TradeEvolution));
		FItemRow Tm = Item(EFieldItemEffect::TeachMove);
		Tm.Category = EItemCategory::Machine;
		Tm.TeachesMove = TEXT("RazorLeaf");
		Tm.bReusable = true;
		Db->Items->AddRow(TEXT("TM99"), Tm);
		FItemRow Everstone;
		Everstone.HeldEffect = EHeldItemEffect::PreventEvolution;
		Db->Items->AddRow(TEXT("Everstone"), Everstone);
		FItemRow Potion;
		Potion.Category = EItemCategory::Medicine;
		Potion.HealHP = 20;
		Db->Items->AddRow(TEXT("Potion"), Potion);
		return Db;
	}

	FPokemonInstance Make(const UPokemonDatabase* Db, FName Species, int32 Level)
	{
		FRandomStream Stream(Level * 13 + 5);
		FPokemonInstance Pokemon;
		UPokemonLibrary::CreatePokemon(Db, Species, Level, Stream, Pokemon, 31);
		return Pokemon;
	}
}

using namespace ProgressionTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionRareCandyTest, "PokemonNewAge.Progression.SuperBonbon", ProgressionTest::TestFlags)
bool FProgressionRareCandyTest::RunTest(const FString& Parameters)
{
	const UPokemonDatabase* Db = MakeDatabase();
	FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 15);
	TestEqual(TEXT("4 attaques au niveau 15"), Bulbasaur.Moves.Num(), 4);
	const int32 HPBefore = Bulbasaur.CurrentHP;

	FProgressionOutcome Outcome;
	bool bConsumed = false;
	TestTrue(TEXT("Super Bonbon utilisé"), UPokemonProgression::UseItem(Db, TEXT("RareCandy"), Bulbasaur, Outcome, bConsumed));
	TestTrue(TEXT("Super Bonbon consommé"), bConsumed);
	TestEqual(TEXT("niveau 16"), Bulbasaur.Level, 16);
	TestEqual(TEXT("XP du début du niveau 16"), Bulbasaur.Experience, 16 * 16 * 16);
	TestEqual(TEXT("un niveau gagné"), Outcome.LevelUps.Num(), 1);
	TestTrue(TEXT("stats en hausse"), Outcome.LevelUps.Num() == 1 && Outcome.LevelUps[0].StatsAfter.Attack > Outcome.LevelUps[0].StatsBefore.Attack);
	TestTrue(TEXT("les PV gagnés s'ajoutent"), Bulbasaur.CurrentHP > HPBefore);
	TestEqual(TEXT("veut évoluer en Herbizarre"), Outcome.EvolvesInto, FName(TEXT("Ivysaur")));

	// Pierre Stase : pas d'évolution par niveau
	FPokemonInstance Holder = Make(Db, TEXT("Bulbasaur"), 15);
	Holder.HeldItem = TEXT("Everstone");
	FProgressionOutcome Blocked;
	UPokemonProgression::UseItem(Db, TEXT("RareCandy"), Holder, Blocked, bConsumed);
	TestEqual(TEXT("niveau 16 avec la Pierre Stase"), Holder.Level, 16);
	TestTrue(TEXT("Pierre Stase : pas d'évolution"), Blocked.EvolvesInto.IsNone());

	// Niveau 100 : sans effet
	FPokemonInstance Max = Make(Db, TEXT("Raichu"), 100);
	TestEqual(TEXT("niveau 100 : sans effet"), UPokemonProgression::GetItemCompatibility(Db, TEXT("RareCandy"), Max), EItemCompatibility::NoEffect);
	FProgressionOutcome None;
	TestFalse(TEXT("niveau 100 : refusé"), UPokemonProgression::UseItem(Db, TEXT("RareCandy"), Max, None, bConsumed));
	TestFalse(TEXT("niveau 100 : pas consommé"), bConsumed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionStoneTest, "PokemonNewAge.Progression.Pierres", ProgressionTest::TestFlags)
bool FProgressionStoneTest::RunTest(const FString& Parameters)
{
	const UPokemonDatabase* Db = MakeDatabase();
	FPokemonInstance Pikachu = Make(Db, TEXT("Pikachu"), 12);
	FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 12);
	TestEqual(TEXT("Pikachu : apte"), UPokemonProgression::GetItemCompatibility(Db, TEXT("ThunderStone"), Pikachu), EItemCompatibility::Able);
	TestEqual(TEXT("Bulbizarre : pas apte"), UPokemonProgression::GetItemCompatibility(Db, TEXT("ThunderStone"), Bulbasaur), EItemCompatibility::NotAble);
	TestEqual(TEXT("Potion : pas un objet de progression"), UPokemonProgression::GetItemCompatibility(Db, TEXT("Potion"), Pikachu), EItemCompatibility::NotApplicable);

	FProgressionOutcome Outcome;
	bool bConsumed = true;
	TestTrue(TEXT("Pierre Foudre utilisée"), UPokemonProgression::UseItem(Db, TEXT("ThunderStone"), Pikachu, Outcome, bConsumed));
	TestEqual(TEXT("évolue en Raichu"), Outcome.EvolvesInto, FName(TEXT("Raichu")));
	TestFalse(TEXT("pierre gardée tant que l'évolution n'est pas finie"), bConsumed);
	TestEqual(TEXT("toujours un Pikachu avant la séquence"), Pikachu.Species, FName(TEXT("Pikachu")));

	// Pierre Stase : les pierres marchent quand même
	Pikachu.HeldItem = TEXT("Everstone");
	TestEqual(TEXT("pierre malgré la Pierre Stase"), UPokemonProgression::GetItemCompatibility(Db, TEXT("ThunderStone"), Pikachu), EItemCompatibility::Able);

	// Fil de Liaison : évolution par échange
	FPokemonInstance Kadabra = Make(Db, TEXT("Kadabra"), 20);
	TestEqual(TEXT("Fil de Liaison sur Kadabra"), UPokemonProgression::GetEvolutionForItem(Db, Kadabra, TEXT("LinkingCord")), FName(TEXT("Alakazam")));
	Kadabra.HeldItem = TEXT("Everstone");
	TestEqual(TEXT("Fil de Liaison bloqué par la Pierre Stase"), UPokemonProgression::GetItemCompatibility(Db, TEXT("LinkingCord"), Kadabra), EItemCompatibility::NotAble);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionMachineTest, "PokemonNewAge.Progression.CT", ProgressionTest::TestFlags)
bool FProgressionMachineTest::RunTest(const FString& Parameters)
{
	const UPokemonDatabase* Db = MakeDatabase();
	FPokemonInstance Full = Make(Db, TEXT("Bulbasaur"), 15);
	FPokemonInstance Young = Make(Db, TEXT("Bulbasaur"), 5);
	FPokemonInstance Pikachu = Make(Db, TEXT("Pikachu"), 12);
	TestEqual(TEXT("Pikachu ne peut pas apprendre Tranch'Herbe"), UPokemonProgression::GetItemCompatibility(Db, TEXT("TM99"), Pikachu), EItemCompatibility::NotAble);

	FProgressionOutcome Outcome;
	bool bConsumed = true;
	TestTrue(TEXT("CT sur un Bulbizarre à 2 attaques"), UPokemonProgression::UseItem(Db, TEXT("TM99"), Young, Outcome, bConsumed));
	TestFalse(TEXT("CT réutilisable"), bConsumed);
	TestEqual(TEXT("appris tout de suite"), Outcome.MovesLearned.Num(), 1);
	TestEqual(TEXT("3 attaques"), Young.Moves.Num(), 3);
	TestEqual(TEXT("déjà connue ensuite"), UPokemonProgression::GetItemCompatibility(Db, TEXT("TM99"), Young), EItemCompatibility::AlreadyKnown);

	FProgressionOutcome Choice;
	TestTrue(TEXT("CT sur un Bulbizarre à 4 attaques"), UPokemonProgression::UseItem(Db, TEXT("TM99"), Full, Choice, bConsumed));
	TestEqual(TEXT("le joueur choisit quoi oublier"), Choice.MovesToChoose.Num(), 1);
	TestEqual(TEXT("toujours 4 attaques"), Full.Moves.Num(), 4);
	TestTrue(TEXT("rien d'appris sans choix"), Full.FindMove(TEXT("RazorLeaf")) == INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionEvolutionTest, "PokemonNewAge.Progression.Evolution", ProgressionTest::TestFlags)
bool FProgressionEvolutionTest::RunTest(const FString& Parameters)
{
	const UPokemonDatabase* Db = MakeDatabase();

	// Plusieurs niveaux d'un coup : une ligne par niveau, attaques des niveaux 7 et 10
	FPokemonInstance Young = Make(Db, TEXT("Bulbasaur"), 5);
	FProgressionOutcome Levels;
	UPokemonProgression::GainLevels(Db, Young, 6, Levels);
	TestEqual(TEXT("niveau 11"), Young.Level, 11);
	TestEqual(TEXT("6 niveaux"), Levels.LevelUps.Num(), 6);
	TestTrue(TEXT("stats enchaînées"), Levels.LevelUps.Num() == 6 && Levels.LevelUps[1].StatsBefore.HP == Levels.LevelUps[0].StatsAfter.HP);
	TestEqual(TEXT("Vampigraine et Fouet Lianes appris"), Levels.MovesLearned.Num(), 2);
	TestTrue(TEXT("pas d'évolution avant 16"), Levels.EvolvesInto.IsNone());

	// Évolution : l'attaque de la nouvelle espèce à ce niveau est proposée
	FPokemonInstance Bulbasaur = Make(Db, TEXT("Bulbasaur"), 16);
	const int32 HPBefore = Bulbasaur.Stats.HP;
	FProgressionOutcome Evolved;
	TestTrue(TEXT("évolution faite"), UPokemonProgression::ApplyEvolution(Db, Bulbasaur, TEXT("Ivysaur"), Evolved));
	TestEqual(TEXT("Herbizarre"), Bulbasaur.Species, FName(TEXT("Ivysaur")));
	TestEqual(TEXT("ancienne espèce notée"), Evolved.EvolvedFrom, FName(TEXT("Bulbasaur")));
	TestTrue(TEXT("stats de Herbizarre"), Bulbasaur.Stats.HP > HPBefore);
	TestEqual(TEXT("Tranch'Herbe à choisir (4 attaques)"), Evolved.MovesToChoose.Num(), 1);

	// K.O. : pas d'évolution par niveau
	FPokemonInstance Fainted = Make(Db, TEXT("Bulbasaur"), 16);
	Fainted.CurrentHP = 0;
	TestTrue(TEXT("K.O. : pas d'évolution"), UPokemonProgression::GetLevelUpEvolution(Db, Fainted).IsNone());
	return true;
}

#endif
