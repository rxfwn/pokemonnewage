#include "Battle/BattleText.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"

namespace
{
	FString StatName(EPokemonStat Stat)
	{
		switch (Stat)
		{
		case EPokemonStat::Attack:         return TEXT("l'Attaque");
		case EPokemonStat::Defense:        return TEXT("la Défense");
		case EPokemonStat::SpecialAttack:  return TEXT("l'Attaque Spéciale");
		case EPokemonStat::SpecialDefense: return TEXT("la Défense Spéciale");
		case EPokemonStat::Speed:          return TEXT("la Vitesse");
		case EPokemonStat::Accuracy:       return TEXT("la Précision");
		case EPokemonStat::Evasion:        return TEXT("l'Esquive");
		default:                           return TEXT("les PV");
		}
	}

	FString StatusApplied(EMajorStatus Status)
	{
		switch (Status)
		{
		case EMajorStatus::Paralysis:     return TEXT("est paralysé ! Il aura du mal à attaquer !");
		case EMajorStatus::Sleep:         return TEXT("s'endort !");
		case EMajorStatus::Freeze:        return TEXT("est gelé !");
		case EMajorStatus::Burn:          return TEXT("est brûlé !");
		case EMajorStatus::Poison:        return TEXT("est empoisonné !");
		case EMajorStatus::BadlyPoisoned: return TEXT("est gravement empoisonné !");
		default:                          return FString();
		}
	}

	FString MoveName(const UPokemonDatabase* Database, FName Move)
	{
		const FMoveRow* Row = Database ? Database->FindMove(Move) : nullptr;
		return Row ? Row->DisplayName.ToString() : Move.ToString();
	}

	FString ItemName(const UPokemonDatabase* Database, FName Item)
	{
		const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
		return Row ? Row->DisplayName.ToString() : Item.ToString();
	}

	/** Nom du Pokémon concerné, avec « sauvage » / « ennemi » pour l'adversaire */
	FString PokemonName(const UPokemonDatabase* Database, const FBattleSnapshot& Snapshot, const FBattleEvent& Event)
	{
		FString Name;
		if (Snapshot.Sides.IsValidIndex(Event.Side) && Snapshot.Sides[Event.Side].Party.IsValidIndex(Event.PartyIndex))
		{
			const FPokemonInstance& Pokemon = Snapshot.Sides[Event.Side].Party[Event.PartyIndex];
			Name = UPokemonLibrary::GetDisplayName(Database, Pokemon).ToString();
		}
		else if (const FSpeciesRow* Row = Database ? Database->FindSpecies(Event.Species) : nullptr)
		{
			Name = Row->DisplayName.ToString();
		}
		if (Event.Side >= 0 && Event.Side != Snapshot.LocalSide)
		{
			Name += Snapshot.BattleType == EBattleType::Wild ? TEXT(" sauvage") : TEXT(" ennemi");
		}
		return Name;
	}

	/** « de Racaillou », « d'Onix » */
	FString Of(const FString& Name)
	{
		static const FString Vowels(TEXT("AEIOUYÉÈÊÂÎH"));
		int32 Index = 0;
		const bool bElide = !Name.IsEmpty() && Vowels.FindChar(FChar::ToUpper(Name[0]), Index);
		return (bElide ? TEXT("d'") : TEXT("de ")) + Name;
	}

	FString TrainerName(const UPokemonDatabase* Database, const FBattleSnapshot& Snapshot)
	{
		const FName TrainerId = Snapshot.Sides.IsValidIndex(1) ? Snapshot.Sides[1].TrainerId : NAME_None;
		const FTrainerRow* Row = Database ? Database->FindTrainer(TrainerId) : nullptr;
		return Row ? FString::Printf(TEXT("%s %s"), *Row->ClassDisplayName.ToString(), *Row->DisplayName.ToString()) : FString(TEXT("Le dresseur"));
	}
}

FText UBattleText::GetEventText(const UObject* WorldContextObject, const FBattleSnapshot& Snapshot, const FBattleEvent& Event)
{
	return MakeEventText(UPokemonDataSubsystem::GetDatabase(WorldContextObject), Snapshot, Event);
}

FText UBattleText::MakeEventText(const UPokemonDatabase* Database, const FBattleSnapshot& Snapshot, const FBattleEvent& Event)
{
	const FString Name = PokemonName(Database, Snapshot, Event);
	const bool bLocal = Event.Side == Snapshot.LocalSide;
	FString Text;

	switch (Event.Type)
	{
	case EBattleEventType::BattleStarted:
		Text = Snapshot.BattleType == EBattleType::Trainer ? TrainerName(Database, Snapshot) + TEXT(" veut se battre !") : FString();
		break;
	case EBattleEventType::SentOut:
		if (bLocal) Text = FString::Printf(TEXT("En avant, %s !"), *Name);
		else if (Snapshot.BattleType == EBattleType::Wild) Text = FString::Printf(TEXT("Un %s apparaît !"), *Name);
		else Text = FString::Printf(TEXT("%s envoie %s !"), *TrainerName(Database, Snapshot), *Name);
		break;
	case EBattleEventType::Withdrew:       Text = FString::Printf(TEXT("%s, reviens !"), *Name); break;
	case EBattleEventType::MoveUsed:       Text = FString::Printf(TEXT("%s utilise %s !"), *Name, *MoveName(Database, Event.Move)); break;
	case EBattleEventType::MoveFailed:     Text = TEXT("Mais cela échoue !"); break;
	case EBattleEventType::Missed:         Text = FString::Printf(TEXT("%s rate son attaque !"), *Name); break;
	case EBattleEventType::NoEffect:       Text = FString::Printf(TEXT("Ça n'affecte pas %s..."), *Name); break;
	case EBattleEventType::CriticalHit:    Text = TEXT("Coup critique !"); break;
	case EBattleEventType::Effectiveness:
		Text = Event.Effectiveness > 1.f ? TEXT("C'est super efficace !") : TEXT("Ce n'est pas très efficace...");
		break;
	case EBattleEventType::MultiHit:       Text = FString::Printf(TEXT("Touché %d fois !"), Event.Amount); break;
	case EBattleEventType::Heal:           Text = FString::Printf(TEXT("%s récupère des PV."), *Name); break;
	case EBattleEventType::Recoil:         Text = FString::Printf(TEXT("%s se blesse en frappant !"), *Name); break;
	case EBattleEventType::StatusApplied:  Text = FString::Printf(TEXT("%s %s"), *Name, *StatusApplied(Event.Status)); break;
	case EBattleEventType::StatusCured:    Text = FString::Printf(TEXT("%s est soigné !"), *Name); break;
	case EBattleEventType::StatusDamage:
		Text = Event.Status == EMajorStatus::Burn ? FString::Printf(TEXT("%s souffre de sa brûlure !"), *Name)
			: FString::Printf(TEXT("%s souffre du poison !"), *Name);
		break;
	case EBattleEventType::StatusPrevented:
		Text = Event.Status == EMajorStatus::Sleep ? FString::Printf(TEXT("%s dort profondément."), *Name)
			: Event.Status == EMajorStatus::Freeze ? FString::Printf(TEXT("%s est gelé !"), *Name)
			: FString::Printf(TEXT("%s est paralysé ! Il ne peut pas attaquer !"), *Name);
		break;
	case EBattleEventType::WokeUp:         Text = FString::Printf(TEXT("%s se réveille !"), *Name); break;
	case EBattleEventType::Thawed:         Text = FString::Printf(TEXT("%s dégèle !"), *Name); break;
	case EBattleEventType::VolatileApplied:
		Text = Event.Ailment == EMoveAilment::Confusion ? FString::Printf(TEXT("%s devient confus !"), *Name)
			: Event.Ailment == EMoveAilment::LeechSeed ? FString::Printf(TEXT("%s est infecté !"), *Name)
			: FString::Printf(TEXT("%s est piégé !"), *Name);
		break;
	case EBattleEventType::IsConfused:     Text = FString::Printf(TEXT("%s est confus !"), *Name); break;
	case EBattleEventType::ConfusionEnded: Text = FString::Printf(TEXT("%s n'est plus confus !"), *Name); break;
	case EBattleEventType::HurtByConfusion: Text = TEXT("Il se blesse dans sa confusion !"); break;
	case EBattleEventType::Flinched:       Text = FString::Printf(TEXT("%s a peur ! Il ne peut pas attaquer !"), *Name); break;
	case EBattleEventType::StatChanged:
		if (Event.Side < 0)
		{
			Text = TEXT("Les changements de stats sont annulés !");
		}
		else if (Event.Amount == 0)
		{
			Text = FString::Printf(TEXT("%s %s ne peut plus %s !"), *StatName(Event.Stat), *Of(Name),
				Event.RequestedStages > 0 ? TEXT("augmenter") : TEXT("baisser"));
		}
		else
		{
			const int32 Size = FMath::Abs(Event.Amount);
			const TCHAR* Verb = Event.Amount > 0 ? (Size >= 2 ? TEXT("augmente beaucoup") : TEXT("augmente"))
				: (Size >= 2 ? TEXT("baisse beaucoup") : TEXT("baisse"));
			Text = FString::Printf(TEXT("%s %s %s !"), *StatName(Event.Stat), *Of(Name), Verb);
			Text[0] = FChar::ToUpper(Text[0]);
		}
		break;
	case EBattleEventType::Protected:      Text = FString::Printf(TEXT("%s est protégé !"), *Name); break;
	case EBattleEventType::ProtectStarted: Text = FString::Printf(TEXT("%s se protège !"), *Name); break;
	case EBattleEventType::FocusEnergy:    Text = FString::Printf(TEXT("%s se concentre !"), *Name); break;
	case EBattleEventType::LeechSeedDrain: Text = FString::Printf(TEXT("Vampigraine draine l'énergie de %s !"), *Name); break;
	case EBattleEventType::TrapDamage:     Text = FString::Printf(TEXT("%s est blessé par le piège !"), *Name); break;
	case EBattleEventType::TrapEnded:      Text = FString::Printf(TEXT("%s est libéré !"), *Name); break;
	case EBattleEventType::Fainted:        Text = FString::Printf(TEXT("%s est K.O. !"), *Name); break;
	case EBattleEventType::ItemUsed:       Text = FString::Printf(TEXT("Utilisation de %s !"), *ItemName(Database, Event.Item)); break;
	case EBattleEventType::BallShake:      Text = Event.Amount == 1 ? TEXT("...") : Event.Amount == 2 ? TEXT("... ...") : TEXT("... ... ..."); break;
	case EBattleEventType::Caught:         Text = FString::Printf(TEXT("Et hop ! %s est attrapé !"), *Name.Replace(TEXT(" sauvage"), TEXT(""))); break;
	case EBattleEventType::BrokeFree:      Text = TEXT("Oh non ! Le Pokémon s'est libéré !"); break;
	case EBattleEventType::FleeSucceeded:  Text = TEXT("Vous prenez la fuite !"); break;
	case EBattleEventType::FleeFailed:     Text = TEXT("Impossible de fuir !"); break;
	case EBattleEventType::ForcedOut:      Text = FString::Printf(TEXT("%s est éjecté !"), *Name); break;
	case EBattleEventType::ExpGained:      Text = FString::Printf(TEXT("%s gagne %d points Exp. !"), *Name, Event.Amount); break;
	case EBattleEventType::LevelUp:        Text = FString::Printf(TEXT("%s monte au N. %d !"), *Name, Event.Amount); break;
	case EBattleEventType::MoveLearned:
		Text = Event.ForgottenMove.IsNone()
			? FString::Printf(TEXT("%s apprend %s !"), *Name, *MoveName(Database, Event.Move))
			: FString::Printf(TEXT("1, 2 et... Tadaa ! %s oublie %s et apprend %s !"), *Name, *MoveName(Database, Event.ForgottenMove), *MoveName(Database, Event.Move));
		break;
	case EBattleEventType::MoveLearnPending:
		Text = FString::Printf(TEXT("%s veut apprendre %s, mais il connaît déjà quatre capacités. Faut-il en oublier une ?"), *Name, *MoveName(Database, Event.Move));
		break;
	case EBattleEventType::MoveNotLearned: Text = FString::Printf(TEXT("%s n'a pas appris %s."), *Name, *MoveName(Database, Event.Move)); break;
	case EBattleEventType::MoneyWon:       Text = FString::Printf(TEXT("Vous remportez %d ₽ !"), Event.Amount); break;
	case EBattleEventType::MoneyLost:      Text = FString::Printf(TEXT("Vous n'avez plus de Pokémon en forme ! Vous perdez %d ₽..."), Event.Amount); break;
	case EBattleEventType::NothingHappened: Text = TEXT("Mais rien ne se passe !"); break;
	case EBattleEventType::BattleEnded:
		Text = Event.Outcome == EBattleOutcome::Won && Snapshot.BattleType == EBattleType::Trainer
			? FString::Printf(TEXT("Vous avez battu %s !"), *TrainerName(Database, Snapshot)) : FString();
		break;
	default:
		break;
	}
	return FText::FromString(Text);
}

FText UBattleText::GetActionErrorText(EBattleActionError Error)
{
	switch (Error)
	{
	case EBattleActionError::NoPPLeft:                  return FText::FromString(TEXT("Il n'y a plus de PP pour cette capacité !"));
	case EBattleActionError::AlreadyInBattle:           return FText::FromString(TEXT("Ce Pokémon est déjà au combat !"));
	case EBattleActionError::PokemonFainted:            return FText::FromString(TEXT("Ce Pokémon n'a plus d'énergie pour se battre !"));
	case EBattleActionError::Trapped:                   return FText::FromString(TEXT("Impossible de s'échapper !"));
	case EBattleActionError::CannotRunFromTrainer:      return FText::FromString(TEXT("On ne s'enfuit pas d'un combat de dresseurs !"));
	case EBattleActionError::NoItem:                    return FText::FromString(TEXT("Vous n'avez pas cet objet."));
	case EBattleActionError::ItemNotUsable:             return FText::FromString(TEXT("Ce n'est pas le moment d'utiliser ça !"));
	case EBattleActionError::ItemHasNoEffect:           return FText::FromString(TEXT("Ça n'aura aucun effet."));
	case EBattleActionError::CannotCatchTrainerPokemon: return FText::FromString(TEXT("Le dresseur dévie la Ball ! Voler, c'est mal !"));
	case EBattleActionError::None:                      return FText::GetEmpty();
	default:                                            return FText::FromString(TEXT("Impossible pour l'instant."));
	}
}
