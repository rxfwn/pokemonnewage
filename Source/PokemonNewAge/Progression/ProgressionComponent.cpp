#include "Progression/ProgressionComponent.h"
#include "Progression/EvolutionScene.h"
#include "UI/Progression/ProgressionWidget.h"
#include "Battle/BattleComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "PokemonNewAgePlayerController.h"
#include "PokemonNewAge.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "Progression"

UProgressionComponent::UProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

USoundBase* UProgressionComponent::LoadSound(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Progression/Sounds/%s.%s"), Name, Name);
	return Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
}

UMaterialInterface* UProgressionComponent::LoadGlowMaterial()
{
	return Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
		TEXT("/Game/Progression/M_EvolutionGlow.M_EvolutionGlow"), nullptr, LOAD_NoWarn | LOAD_Quiet));
}

APokemonNewAgePlayerController* UProgressionComponent::GetController() const
{
	return Cast<APokemonNewAgePlayerController>(GetOwner());
}

APokemonPlayerState* UProgressionComponent::GetPokemonPlayerState() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	return Controller ? Controller->GetPlayerState<APokemonPlayerState>() : nullptr;
}

// ---------------------------------------------------------------- Demandes du joueur

void UProgressionComponent::UseItem(FName Item, int32 PartyIndex) { ServerUseItem(Item, PartyIndex); }
void UProgressionComponent::ConfirmEvolution(int32 PartyIndex, bool bAccept) { ServerConfirmEvolution(PartyIndex, bAccept); }
void UProgressionComponent::LearnMove(int32 PartyIndex, FName Move, int32 ForgetSlot) { ServerLearnMove(PartyIndex, Move, ForgetSlot); }
void UProgressionComponent::GiveExperience(int32 PartyIndex, int32 Amount) { ServerGiveExperience(PartyIndex, Amount); }

void UProgressionComponent::PreviewEvolution(FName FromSpecies, FName ToSpecies)
{
	FStep Step;
	Step.Type = EStepType::Evolution;
	Step.FromSpecies = FromSpecies;
	Step.ToSpecies = ToSpecies;
	Step.bPreview = true;
	Steps.Add(Step);
}

// ---------------------------------------------------------------- Serveur

void UProgressionComponent::ServerUseItem_Implementation(FName Item, int32 PartyIndex)
{
	UseItemOnServer(Item, PartyIndex);
}

bool UProgressionComponent::UseItemOnServer(FName Item, int32 PartyIndex)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const APokemonNewAgePlayerController* Controller = GetController();
	if (!State || !Database || !State->Party.IsValidIndex(PartyIndex) || State->GetItemCount(Item) <= 0
		|| (Controller && Controller->BattleComponent && Controller->BattleComponent->IsInBattle()))
	{
		return false;
	}

	FPokemonInstance& Pokemon = State->Party[PartyIndex];
	const FName OldSpecies = Pokemon.Species;
	FProgressionOutcome Outcome;
	Outcome.PartyIndex = PartyIndex;
	bool bConsumed = false;
	if (!UPokemonProgression::UseItem(Database, Item, Pokemon, Outcome, bConsumed))
	{
		return false;
	}
	if (bConsumed)
	{
		State->RemoveItem(Item, 1);
	}
	SendOutcome(Outcome, OldSpecies);
	return true;
}

void UProgressionComponent::ServerGiveExperience_Implementation(int32 PartyIndex, int32 Amount)
{
#if !UE_BUILD_SHIPPING
	// Commande de test : seulement pour l'hôte (pas pour un client en ligne)
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APokemonPlayerState* State = GetPokemonPlayerState();
	if (!Controller || !Controller->IsLocalController() || !State || !State->Party.IsValidIndex(PartyIndex) || Amount <= 0)
	{
		return;
	}
	FPokemonInstance& Pokemon = State->Party[PartyIndex];
	const FName OldSpecies = Pokemon.Species;
	FProgressionOutcome Outcome;
	Outcome.PartyIndex = PartyIndex;
	UPokemonProgression::GainExperience(UPokemonDataSubsystem::GetDatabase(this), Pokemon, Amount, Outcome);
	SendOutcome(Outcome, OldSpecies);
#endif
}

void UProgressionComponent::SendOutcome(const FProgressionOutcome& Outcome, FName OldSpecies)
{
	if (Outcome.MovesToChoose.Num() > 0)
	{
		AllowedMoves.FindOrAdd(Outcome.PartyIndex).Append(Outcome.MovesToChoose);
	}
	if (!Outcome.EvolvesInto.IsNone())
	{
		const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
		const FItemRow* ItemRow = Database && !Outcome.Item.IsNone() ? Database->FindItem(Outcome.Item) : nullptr;
		FServerEvolution Evolution;
		Evolution.PartyIndex = Outcome.PartyIndex;
		Evolution.FromSpecies = OldSpecies;
		Evolution.ToSpecies = Outcome.EvolvesInto;
		// La pierre ne part du sac que si l'évolution va au bout
		const bool bEvolutionItem = ItemRow && (ItemRow->FieldEffect == EFieldItemEffect::EvolutionItem || ItemRow->FieldEffect == EFieldItemEffect::TradeEvolution);
		Evolution.Item = bEvolutionItem ? Outcome.Item : NAME_None;
		PendingEvolutions.RemoveAll([&Evolution](const FServerEvolution& E) { return E.PartyIndex == Evolution.PartyIndex; });
		PendingEvolutions.Add(Evolution);
	}
	ClientOutcome(Outcome);
}

void UProgressionComponent::AddBattleEvolutions(const TArray<FPendingEvolution>& Evolutions)
{
	const APokemonPlayerState* State = GetPokemonPlayerState();
	for (const FPendingEvolution& Pending : Evolutions)
	{
		if (!State || !State->Party.IsValidIndex(Pending.PartyIndex))
		{
			continue;
		}
		FServerEvolution Evolution;
		Evolution.PartyIndex = Pending.PartyIndex;
		Evolution.FromSpecies = State->Party[Pending.PartyIndex].Species;
		Evolution.ToSpecies = Pending.NewSpecies;
		PendingEvolutions.RemoveAll([&Evolution](const FServerEvolution& E) { return E.PartyIndex == Evolution.PartyIndex; });
		PendingEvolutions.Add(Evolution);
	}
	if (Evolutions.Num() > 0)
	{
		ClientQueueEvolutions(Evolutions);
	}
}

void UProgressionComponent::ServerConfirmEvolution_Implementation(int32 PartyIndex, bool bAccept)
{
	const int32 PendingIndex = PendingEvolutions.IndexOfByPredicate([PartyIndex](const FServerEvolution& E) { return E.PartyIndex == PartyIndex; });
	APokemonPlayerState* State = GetPokemonPlayerState();
	if (PendingIndex == INDEX_NONE || !State || !State->Party.IsValidIndex(PartyIndex))
	{
		return;
	}
	const FServerEvolution Evolution = PendingEvolutions[PendingIndex];
	PendingEvolutions.RemoveAt(PendingIndex);
	FPokemonInstance& Pokemon = State->Party[PartyIndex];
	if (!bAccept || Pokemon.Species != Evolution.FromSpecies)
	{
		return;
	}
	if (!Evolution.Item.IsNone() && !State->RemoveItem(Evolution.Item, 1))
	{
		return;
	}

	FProgressionOutcome Outcome;
	Outcome.PartyIndex = PartyIndex;
	if (!UPokemonProgression::ApplyEvolution(UPokemonDataSubsystem::GetDatabase(this), Pokemon, Evolution.ToSpecies, Outcome))
	{
		return;
	}
	State->MarkCaught(Evolution.ToSpecies);
	SendOutcome(Outcome, Evolution.ToSpecies);
}

void UProgressionComponent::ServerLearnMove_Implementation(int32 PartyIndex, FName Move, int32 ForgetSlot)
{
	TArray<FName>* Allowed = AllowedMoves.Find(PartyIndex);
	APokemonPlayerState* State = GetPokemonPlayerState();
	if (!Allowed || !Allowed->Contains(Move) || !State || !State->Party.IsValidIndex(PartyIndex))
	{
		return;
	}
	Allowed->Remove(Move);
	FPokemonInstance& Pokemon = State->Party[PartyIndex];
	if (ForgetSlot >= 0 || Pokemon.Moves.Num() < 4)
	{
		UPokemonLibrary::LearnMove(UPokemonDataSubsystem::GetDatabase(this), Pokemon, Move, ForgetSlot);
	}
}

// ---------------------------------------------------------------- Client : file d'attente de l'écran

void UProgressionComponent::ClientOutcome_Implementation(const FProgressionOutcome& Outcome)
{
	OnOutcome.Broadcast(Outcome);
	if (!bUseBuiltInUI)
	{
		return;
	}

	const bool bEvolutionDone = !Outcome.EvolvedTo.IsNone();
	// Objet utilisé depuis le Pokédex sans rien à décider : le Pokédex affiche déjà le résultat
	const APokemonNewAgePlayerController* Controller = GetController();
	if (!bEvolutionDone && !Outcome.HasChoices() && Controller && Controller->IsGameMenuOpen())
	{
		return;
	}
	QueueOutcome(Outcome, bEvolutionDone);

	// Les attaques proposées après une évolution passent tout de suite
	if (bEvolutionDone && WaitForServer > 0.f)
	{
		WaitForServer = 0.f;
		ProcessNextStep();
	}
}

void UProgressionComponent::ClientQueueEvolutions_Implementation(const TArray<FPendingEvolution>& Evolutions)
{
	for (const FPendingEvolution& Evolution : Evolutions)
	{
		FProgressionOutcome Outcome;
		Outcome.PartyIndex = Evolution.PartyIndex;
		Outcome.EvolvesInto = Evolution.NewSpecies;
		ClientOutcome_Implementation(Outcome);
	}
}

void UProgressionComponent::QueueOutcome(const FProgressionOutcome& Outcome, bool bFront)
{
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	TArray<FStep> NewSteps;
	for (const FLevelUpInfo& Info : Outcome.LevelUps)
	{
		FStep& Step = NewSteps.AddDefaulted_GetRef();
		Step.Type = EStepType::LevelUp;
		Step.PartyIndex = Outcome.PartyIndex;
		Step.LevelUp = Info;
	}
	for (FName Move : Outcome.MovesLearned)
	{
		const FMoveRow* Row = Database ? Database->FindMove(Move) : nullptr;
		FStep& Step = NewSteps.AddDefaulted_GetRef();
		Step.Type = EStepType::Message;
		Step.PartyIndex = Outcome.PartyIndex;
		Step.Text = FText::Format(LOCTEXT("MoveLearned", "{0} apprend {1} !"),
			GetPokemonName(Outcome.PartyIndex, Outcome.EvolvedTo), Row ? Row->DisplayName : FText::FromName(Move));
	}
	for (FName Move : Outcome.MovesToChoose)
	{
		FStep& Step = NewSteps.AddDefaulted_GetRef();
		Step.Type = EStepType::LearnMove;
		Step.PartyIndex = Outcome.PartyIndex;
		Step.Move = Move;
	}
	if (!Outcome.EvolvesInto.IsNone())
	{
		// Plusieurs Super Bonbons d'affilée : une seule séquence, après le dernier
		Steps.RemoveAll([&Outcome](const FStep& Old) { return Old.Type == EStepType::Evolution && !Old.bPreview && Old.PartyIndex == Outcome.PartyIndex; });
		FStep& Step = NewSteps.AddDefaulted_GetRef();
		Step.Type = EStepType::Evolution;
		Step.PartyIndex = Outcome.PartyIndex;
		Step.ToSpecies = Outcome.EvolvesInto;
	}

	if (bFront)
	{
		Steps.Insert(NewSteps, 0);
	}
	else
	{
		Steps.Append(NewSteps);
	}
}

bool UProgressionComponent::CanShowNow() const
{
	const APokemonNewAgePlayerController* Controller = GetController();
	return Controller && Controller->IsLocalController() && !Controller->IsGameMenuOpen() && !Controller->IsBattleHUDOpen()
		&& !(Controller->BattleComponent && Controller->BattleComponent->IsInBattle());
}

void UProgressionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (WaitForServer > 0.f)
	{
		WaitForServer -= DeltaTime;
		if (WaitForServer <= 0.f)
		{
			ProcessNextStep();
		}
		return;
	}
	if (bProcessing || Steps.Num() == 0)
	{
		bClosingMenu = false;
		return;
	}
	if (CanShowNow())
	{
		bClosingMenu = false;
		ProcessNextStep();
		return;
	}
	// Une évolution ou un choix attend pendant que le Pokédex est ouvert : on le referme (animation), puis on joue
	APokemonNewAgePlayerController* Controller = GetController();
	if (Controller && Controller->IsGameMenuOpen() && !bClosingMenu)
	{
		bClosingMenu = true;
		Controller->CloseGameMenu();
	}
}

void UProgressionComponent::ProcessNextStep()
{
	APokemonNewAgePlayerController* Controller = GetController();
	if (Steps.Num() == 0 || !Controller || !CanShowNow())
	{
		EndProcessing();
		return;
	}

	if (!Widget)
	{
		TSubclassOf<UProgressionWidget> Class = WidgetClass ? WidgetClass : TSubclassOf<UProgressionWidget>(UProgressionWidget::StaticClass());
		Widget = CreateWidget<UProgressionWidget>(Controller, Class);
		if (!Widget)
		{
			Steps.Reset();
			return;
		}
		Widget->AddToPlayerScreen(20);
		Controller->SetShowMouseCursor(true);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Widget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
	}
	Widget->SetKeyboardFocus();
	bProcessing = true;

	const FStep Step = Steps[0];
	Steps.RemoveAt(0);
	TWeakObjectPtr<UProgressionComponent> WeakThis(this);
	auto Next = [WeakThis]()
	{
		if (UProgressionComponent* Self = WeakThis.Get())
		{
			Self->ProcessNextStep();
		}
	};

	const APokemonPlayerState* State = GetPokemonPlayerState();
	switch (Step.Type)
	{
	case EStepType::Message:
		Widget->ShowMessage(Step.Text, true, Next);
		break;

	case EStepType::LevelUp:
		Widget->ShowLevelUp(GetPokemonName(Step.PartyIndex, NAME_None), Step.LevelUp, Next);
		break;

	case EStepType::LearnMove:
	{
		const FPokemonInstance* Pokemon = State && State->Party.IsValidIndex(Step.PartyIndex) ? &State->Party[Step.PartyIndex] : nullptr;
		if (!Pokemon || Pokemon->Moves.Num() < 4)
		{
			// Une case s'est libérée entre-temps
			LearnMove(Step.PartyIndex, Step.Move, -1);
			Next();
			break;
		}
		const int32 PartyIndex = Step.PartyIndex;
		const FName Move = Step.Move;
		Widget->ShowForgetMove(*Pokemon, Move, [WeakThis, PartyIndex, Move, Next](int32 ForgetSlot)
		{
			if (UProgressionComponent* Self = WeakThis.Get())
			{
				Self->LearnMove(PartyIndex, Move, ForgetSlot);
			}
			Next();
		});
		break;
	}

	case EStepType::Evolution:
		StartEvolution(Step);
		break;
	}
}

void UProgressionComponent::StartEvolution(const FStep& Step)
{
	const APokemonPlayerState* State = GetPokemonPlayerState();
	const FPokemonInstance* Pokemon = !Step.bPreview && State && State->Party.IsValidIndex(Step.PartyIndex) ? &State->Party[Step.PartyIndex] : nullptr;
	const FName From = Step.bPreview ? Step.FromSpecies : (Pokemon ? Pokemon->Species : NAME_None);
	if (From.IsNone() || Step.ToSpecies.IsNone())
	{
		ProcessNextStep();
		return;
	}

	APlayerController* Controller = GetController();
	const TSubclassOf<AEvolutionScene> Class = EvolutionSceneClass ? EvolutionSceneClass : TSubclassOf<AEvolutionScene>(AEvolutionScene::StaticClass());
	FActorSpawnParameters Params;
	Params.Owner = Controller;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Scene = GetWorld()->SpawnActor<AEvolutionScene>(Class, FTransform::Identity, Params);
	if (!Scene)
	{
		FinishEvolution(Step.PartyIndex, Step.ToSpecies, true, Step.bPreview);
		return;
	}

	const FText Name = Pokemon ? UPokemonLibrary::GetDisplayName(UPokemonDataSubsystem::GetDatabase(this), *Pokemon) : GetPokemonName(-1, From);
	TWeakObjectPtr<UProgressionComponent> WeakThis(this);
	TWeakObjectPtr<AEvolutionScene> WeakScene(Scene);
	Widget->OnBack = [WeakScene]() { return WeakScene.IsValid() && WeakScene->RequestCancel(); };
	const int32 PartyIndex = Step.PartyIndex;
	const FName To = Step.ToSpecies;
	const bool bPreview = Step.bPreview;
	Scene->Play(Controller, Widget, Name, From, To, [WeakThis, PartyIndex, To, bPreview](bool bEvolved)
	{
		if (UProgressionComponent* Self = WeakThis.Get())
		{
			Self->FinishEvolution(PartyIndex, To, bEvolved, bPreview);
		}
	});
}

void UProgressionComponent::FinishEvolution(int32 PartyIndex, FName ToSpecies, bool bEvolved, bool bPreview)
{
	if (Scene)
	{
		Scene->Destroy();
		Scene = nullptr;
	}
	if (Widget)
	{
		Widget->OnBack = nullptr;
		Widget->SetFlash(0.f);
	}
	OnEvolutionFinished.Broadcast(PartyIndex, ToSpecies, bEvolved);
	if (!bPreview)
	{
		ConfirmEvolution(PartyIndex, bEvolved);
		if (bEvolved)
		{
			// Le serveur répond avec les attaques de la nouvelle espèce (ClientOutcome) ; sinon on continue après ce délai
			WaitForServer = 1.5f;
			return;
		}
	}
	ProcessNextStep();
}

void UProgressionComponent::EndProcessing()
{
	const bool bWasProcessing = bProcessing || Widget != nullptr;
	bProcessing = false;
	WaitForServer = 0.f;
	if (Scene)
	{
		Scene->Destroy();
		Scene = nullptr;
	}
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	// Rend la main au jeu, sauf si un combat ou le menu a pris l'écran entre-temps
	APlayerController* Controller = GetController();
	if (bWasProcessing && Controller && CanShowNow())
	{
		Controller->SetShowMouseCursor(false);
		Controller->SetInputMode(FInputModeGameOnly());
	}
}

void UProgressionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Steps.Reset();
	EndProcessing();
	Super::EndPlay(EndPlayReason);
}

FText UProgressionComponent::GetPokemonName(int32 PartyIndex, FName FallbackSpecies) const
{
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	const APokemonPlayerState* State = GetPokemonPlayerState();
	if (State && State->Party.IsValidIndex(PartyIndex) && (FallbackSpecies.IsNone() || !State->Party[PartyIndex].Nickname.IsEmpty()))
	{
		return UPokemonLibrary::GetDisplayName(Database, State->Party[PartyIndex]);
	}
	const FSpeciesRow* Row = Database && !FallbackSpecies.IsNone() ? Database->FindSpecies(FallbackSpecies) : nullptr;
	return Row ? Row->DisplayName : FText::FromName(FallbackSpecies);
}

#undef LOCTEXT_NAMESPACE
