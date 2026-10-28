#include "Dialogue/DialogueComponent.h"

#include "Battle/BattleSubsystem.h"
#include "Components/AudioComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Pokemon/PokemonInstance.h"
#include "PokemonNewAge.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/DialogueWidget.h"

namespace
{
	/** Événements appliqués quand la réplique s'affiche (le texte annonce ce qui vient de se passer) */
	bool AppliesWhenShown(EDialogueEvent Event)
	{
		switch (Event)
		{
		case EDialogueEvent::GiveItem:
		case EDialogueEvent::TakeItem:
		case EDialogueEvent::TakeMoney:
		case EDialogueEvent::GivePokemon:
		case EDialogueEvent::GiveBadge:
		case EDialogueEvent::HealParty:
		case EDialogueEvent::OfferNickname:
		case EDialogueEvent::PlayCutscene:
			return true;
		default:
			return false;
		}
	}

	FText GetSpeciesName(const UPokemonDatabase* Database, FName Species)
	{
		const FSpeciesRow* Row = Database ? Database->FindSpecies(Species) : nullptr;
		return Row ? Row->DisplayName : FText::FromName(Species);
	}
}

UDialogueComponent::UDialogueComponent()
{
	SetIsReplicatedByDefault(true);
	DialoguesTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/World/Dialogues/DT_Dialogues.DT_Dialogues")));
	NpcsTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/World/Dialogues/DT_Npcs.DT_Npcs")));
	DialogueWidgetClass = UDialogueWidget::StaticClass();
}

void UDialogueComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		if (UBattleSubsystem* Battles = GetWorld()->GetSubsystem<UBattleSubsystem>())
		{
			Battles->OnBattleEnded.AddDynamic(this, &UDialogueComponent::HandleBattleEnded);
		}
	}
}

void UDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(OutcomeTimer);
	StopVoice();
	CloseWidget();
	Super::EndPlay(EndPlayReason);
}

UDataTable* UDialogueComponent::GetDialoguesTable() const
{
	if (!LoadedDialogues)
	{
		const_cast<UDialogueComponent*>(this)->LoadedDialogues = DialoguesTable.LoadSynchronous();
		if (!LoadedDialogues)
		{
			UE_LOG(LogPokemonNewAge, Error, TEXT("Dialogue : DT_Dialogues introuvable (%s). Lancer Tools/Data/import_data_tables.py"), *DialoguesTable.ToString());
		}
	}
	return LoadedDialogues;
}

UDataTable* UDialogueComponent::GetNpcsTable() const
{
	if (!LoadedNpcs)
	{
		const_cast<UDialogueComponent*>(this)->LoadedNpcs = NpcsTable.LoadSynchronous();
	}
	return LoadedNpcs;
}

APlayerController* UDialogueComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

APokemonPlayerState* UDialogueComponent::GetPokemonPlayerState() const
{
	const APlayerController* PlayerController = GetPlayerController();
	return PlayerController ? PlayerController->GetPlayerState<APokemonPlayerState>() : nullptr;
}

// ---------------------------------------------------------------- Lancer

void UDialogueComponent::TalkTo(FName Npc)
{
	if (IsInConversation() || Npc.IsNone())
	{
		return;
	}
	// Sur le serveur (solo, hôte) l'appel est direct ; depuis un client il part au serveur
	ServerTalkTo(Npc);
}

void UDialogueComponent::PlayConversation(FName ConversationId)
{
	if (GetOwner()->HasAuthority())
	{
		ServerPlay(ConversationId);
	}
	else
	{
		ServerPlayConversation(ConversationId);
	}
}

void UDialogueComponent::ServerTalkTo_Implementation(FName Npc)
{
	const APokemonPlayerState* State = GetPokemonPlayerState();
	if (!ServerConversation.IsNone() || !State)
	{
		return;
	}

	FName ConversationId;
	FDialogueRow Conversation;
	if (UDialogueLibrary::FindTalkConversation(GetDialoguesTable(), Npc, State->StoryFlags, ConversationId, Conversation))
	{
		ServerPlay(ConversationId);
	}
	else
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Dialogue : aucune conversation pour le PNJ %s avec ces drapeaux"), *Npc.ToString());
	}
}

void UDialogueComponent::ServerPlayConversation_Implementation(FName ConversationId)
{
	// Demandé par un client : seulement si les conditions de la conversation sont remplies pour lui
	const UDataTable* Dialogues = GetDialoguesTable();
	const FDialogueRow* Row = Dialogues ? Dialogues->FindRow<FDialogueRow>(ConversationId, TEXT("Dialogue")) : nullptr;
	const APokemonPlayerState* State = GetPokemonPlayerState();
	if (Row && State && ServerConversation.IsNone() && UDialogueLibrary::AreConditionsMet(*Row, State->StoryFlags))
	{
		ServerPlay(ConversationId);
	}
}

// ---------------------------------------------------------------- Serveur

void UDialogueComponent::ServerPlay(FName ConversationId)
{
	const UDataTable* Dialogues = GetDialoguesTable();
	const FDialogueRow* Row = Dialogues ? Dialogues->FindRow<FDialogueRow>(ConversationId, TEXT("Dialogue"), false) : nullptr;
	if (!Row)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Dialogue : conversation %s introuvable"), *ConversationId.ToString());
		ServerConversation = NAME_None;
		ServerLine = INDEX_NONE;
		ClientEndConversation();
		return;
	}

	ServerConversation = ConversationId;
	ServerLine = 0;
	if (Row->Lines.Num() == 0)
	{
		ServerFinish(Row->NextConversation);
		return;
	}
	ServerShowCurrentLine();
}

void UDialogueComponent::ServerShowCurrentLine()
{
	const FDialogueRow* Row = GetDialoguesTable()->FindRow<FDialogueRow>(ServerConversation, TEXT("Dialogue"));
	const FDialogueLine& Line = Row->Lines[ServerLine];

	if (AppliesWhenShown(Line.Event) && !ApplyEvent(Line) && !Line.FailConversation.IsNone())
	{
		// Pas assez d'argent... : on passe à la conversation de repli, sans les drapeaux de celle-ci
		ServerPlay(Line.FailConversation);
		return;
	}

	ClientShowLine(ServerConversation, ServerLine, LastMoneyLost);
}

void UDialogueComponent::ServerAdvance_Implementation(FName ConversationId, int32 LineIndex)
{
	// Réponse en retard ou en double : on l'ignore
	if (ConversationId != ServerConversation || LineIndex != ServerLine)
	{
		return;
	}
	const FDialogueRow* Row = GetDialoguesTable()->FindRow<FDialogueRow>(ServerConversation, TEXT("Dialogue"));
	const FDialogueLine& Line = Row->Lines[ServerLine];
	if (Line.Choices.Num() > 0)
	{
		return;
	}

	// Combat : la boîte se ferme puis le combat commence ; la suite vient avec le résultat (HandleBattleEnded)
	if (Line.Event == EDialogueEvent::StartTrainerBattle)
	{
		const FName TrainerGroup = Line.EventParam;
		ServerFinish(NAME_None);
		UBattleSubsystem* Battles = GetWorld()->GetSubsystem<UBattleSubsystem>();
		if (!Battles || !Battles->StartTrainerBattle(GetPlayerController(), TrainerGroup))
		{
			UE_LOG(LogPokemonNewAge, Warning, TEXT("Dialogue : combat contre %s impossible (equipe K.O. ? DT_Trainers ?)"), *TrainerGroup.ToString());
		}
		return;
	}

	// Boutique, PC, retour au Centre : quand le joueur a lu la réplique
	if (!AppliesWhenShown(Line.Event) && Line.Event != EDialogueEvent::None)
	{
		ApplyEvent(Line);
	}

	if (++ServerLine < Row->Lines.Num())
	{
		ServerShowCurrentLine();
	}
	else
	{
		ServerFinish(Row->NextConversation);
	}
}

void UDialogueComponent::ServerChoose_Implementation(FName ConversationId, int32 LineIndex, int32 ChoiceIndex)
{
	if (ConversationId != ServerConversation || LineIndex != ServerLine)
	{
		return;
	}
	const FDialogueRow* Row = GetDialoguesTable()->FindRow<FDialogueRow>(ServerConversation, TEXT("Dialogue"));
	const FDialogueLine& Line = Row->Lines[ServerLine];
	if (Line.Choices.IsValidIndex(ChoiceIndex))
	{
		ServerFinish(Line.Choices[ChoiceIndex].NextConversation);
	}
}

void UDialogueComponent::ServerFinish(FName Next)
{
	if (!ServerConversation.IsNone())
	{
		const FDialogueRow* Row = GetDialoguesTable()->FindRow<FDialogueRow>(ServerConversation, TEXT("Dialogue"));
		APokemonPlayerState* State = GetPokemonPlayerState();
		if (Row && State)
		{
			for (const FName& Flag : Row->SetFlags)
			{
				State->AddFlag(Flag);
			}
		}
	}

	ServerConversation = NAME_None;
	ServerLine = INDEX_NONE;

	if (!Next.IsNone())
	{
		// La boîte reste ouverte, la conversation suivante s'y affiche
		ServerPlay(Next);
	}
	else
	{
		ClientEndConversation();
	}
}

bool UDialogueComponent::ApplyEvent(const FDialogueLine& Line)
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	if (!State)
	{
		return false;
	}

	switch (Line.Event)
	{
	case EDialogueEvent::GiveItem:
		State->AddItem(Line.EventParam, FMath::Max(Line.EventAmount, 1));
		return true;

	case EDialogueEvent::TakeItem:
		return State->RemoveItem(Line.EventParam, FMath::Max(Line.EventAmount, 1));

	case EDialogueEvent::TakeMoney:
		if (State->Money < Line.EventAmount)
		{
			return false;
		}
		State->AddMoney(-Line.EventAmount);
		return true;

	case EDialogueEvent::GivePokemon:
	{
		FRandomStream Stream(FMath::Rand());
		FPokemonInstance Pokemon;
		if (!UPokemonLibrary::CreatePokemon(UPokemonDataSubsystem::GetDatabase(this), Line.EventParam, FMath::Max(Line.EventAmount, 1), Stream, Pokemon))
		{
			UE_LOG(LogPokemonNewAge, Warning, TEXT("Dialogue : espece %s inconnue"), *Line.EventParam.ToString());
			return false;
		}
		Pokemon.Ball = TEXT("PokeBall");
		Pokemon.MetLocation = State->CurrentLocation;
		Pokemon.MetLevel = Pokemon.Level;
		State->MarkSeen(Line.EventParam);
		State->AddPokemon(Pokemon);
		return true;
	}

	case EDialogueEvent::GiveBadge:
		State->Badges.AddUnique(Line.EventParam);
		return true;

	case EDialogueEvent::HealParty:
		State->HealParty();
		return true;

	case EDialogueEvent::ReturnToLastHeal:
		// Le téléport au dernier Centre viendra avec les Centres Pokémon ; l'équipe est déjà soignée
		State->HealParty();
		break;

	default:
		break;
	}

	// Pas encore géré en C++ : on prévient les Blueprints (serveur et client du joueur)
	OnUnhandledEvent.Broadcast(Line.Event, Line.EventParam, Line.EventAmount);
	if (!GetPlayerController()->IsLocalController())
	{
		ClientUnhandledEvent(Line.Event, Line.EventParam, Line.EventAmount);
	}
	UE_LOG(LogPokemonNewAge, Log, TEXT("Dialogue : evenement %s (%s) pas encore code"),
		*UEnum::GetValueAsString(Line.Event), *Line.EventParam.ToString());
	return true;
}

void UDialogueComponent::HandleBattleEnded(APlayerController* Player, FName TrainerGroup, const FBattleResult& Result)
{
	if (Player != GetPlayerController())
	{
		return;
	}

	LastMoneyLost = FMath::Max(0, -Result.MoneyDelta);

	FName Outcome = NAME_None;
	if (!TrainerGroup.IsNone())
	{
		Outcome = UDialogueLibrary::GetTrainerOutcomeConversation(GetDialoguesTable(), TrainerGroup, Result.Outcome == EBattleOutcome::Won);
	}
	else if (Result.Outcome == EBattleOutcome::Lost)
	{
		// Équipe K.O. contre un Pokémon sauvage
		static const FName Blackout(TEXT("Battle_PlayerBlackout"));
		Outcome = Blackout;
	}
	if (Outcome.IsNone())
	{
		return;
	}

	// On laisse l'écran de combat se fermer avant d'afficher la réplique
	GetWorld()->GetTimerManager().SetTimer(OutcomeTimer, FTimerDelegate::CreateWeakLambda(this, [this, Outcome]()
	{
		ServerPlay(Outcome);
	}), FMath::Max(BattleOutcomeDelay, 0.01f), false);
}

// ---------------------------------------------------------------- Client

void UDialogueComponent::ClientShowLine_Implementation(FName ConversationId, int32 LineIndex, int32 MoneyLost)
{
	const UDataTable* Dialogues = GetDialoguesTable();
	const FDialogueRow* Row = Dialogues ? Dialogues->FindRow<FDialogueRow>(ConversationId, TEXT("Dialogue")) : nullptr;
	if (!Row || !Row->Lines.IsValidIndex(LineIndex))
	{
		ClientEndConversation_Implementation();
		return;
	}

	ClientConversation = ConversationId;
	ClientLine = LineIndex;
	ClientMoneyLost = MoneyLost;
	ClientPendingFlags = Row->SetFlags;

	OpenWidget();
	if (!Widget)
	{
		return;
	}

	const FDialogueLine& Line = Row->Lines[LineIndex];
	const FText Text = FormatLineText(Line.Text);
	TArray<FText> Choices;
	for (const FDialogueChoice& Choice : Line.Choices)
	{
		Choices.Add(FormatLineText(Choice.Text));
	}

	StopVoice();
	const bool bHasVoice = PlayVoice(ConversationId, LineIndex);
	Widget->ShowLine(GetSpeakerName(Line.Speaker), Text, Choices, bHasVoice);
	OnLineShown.Broadcast(ConversationId, LineIndex, Line, Text);
}

void UDialogueComponent::ClientEndConversation_Implementation()
{
	StopVoice();
	CloseWidget();
	const bool bWasOpen = !ClientConversation.IsNone();
	ClientConversation = NAME_None;
	ClientLine = INDEX_NONE;
	ClientPendingFlags.Reset();
	if (bWasOpen)
	{
		OnConversationEnded.Broadcast();
	}
}

void UDialogueComponent::ClientUnhandledEvent_Implementation(EDialogueEvent Event, FName EventParam, int32 EventAmount)
{
	OnUnhandledEvent.Broadcast(Event, EventParam, EventAmount);
}

void UDialogueComponent::AdvanceLine()
{
	if (ClientConversation.IsNone())
	{
		return;
	}
	StopVoice();
	if (AdvanceSound)
	{
		UGameplayStatics::PlaySound2D(this, AdvanceSound);
	}
	ServerAdvance(ClientConversation, ClientLine);
}

void UDialogueComponent::SelectChoice(int32 ChoiceIndex)
{
	if (ClientConversation.IsNone())
	{
		return;
	}
	StopVoice();
	if (AdvanceSound)
	{
		UGameplayStatics::PlaySound2D(this, AdvanceSound);
	}
	ServerChoose(ClientConversation, ClientLine, ChoiceIndex);
}

void UDialogueComponent::OpenWidget()
{
	APlayerController* PlayerController = GetPlayerController();
	if (Widget || !PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	Widget = CreateWidget<UDialogueWidget>(PlayerController, DialogueWidgetClass ? DialogueWidgetClass.Get() : UDialogueWidget::StaticClass());
	if (!Widget)
	{
		UE_LOG(LogPokemonNewAge, Error, TEXT("Dialogue : impossible de creer la boite de dialogue"));
		return;
	}
	Widget->Init(this, LettersPerSecond, TypingSound);
	Widget->AddToPlayerScreen(5);

	// Le joueur ne bouge plus, le clavier et la manette pilotent la boîte. Le monde continue (multijoueur)
	PlayerController->SetIgnoreMoveInput(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PlayerController->SetInputMode(InputMode);
}

void UDialogueComponent::CloseWidget()
{
	if (!Widget)
	{
		return;
	}
	Widget->RemoveFromParent();
	Widget = nullptr;

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

// ---------------------------------------------------------------- Textes et voix

FText UDialogueComponent::GetSpeakerName(FName Speaker) const
{
	const UDataTable* Npcs = GetNpcsTable();
	const FNpcRow* Npc = Npcs ? Npcs->FindRow<FNpcRow>(Speaker, TEXT("Dialogue"), false) : nullptr;
	return Npc && Npc->Kind == ENpcKind::Character ? Npc->DisplayName : FText::GetEmpty();
}

FText UDialogueComponent::FormatLineText(const FText& Text) const
{
	const APokemonPlayerState* State = GetPokemonPlayerState();
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);

	// Drapeaux du joueur + ceux que la conversation affichée va donner (le starter est annoncé avant la fin)
	TArray<FName> Flags = State ? State->StoryFlags : TArray<FName>();
	for (const FName& Flag : ClientPendingFlags)
	{
		Flags.AddUnique(Flag);
	}

	TMap<FString, FText> Arguments;
	Arguments.Add(TEXT("PlayerName"), FText::FromString(State ? State->GetPlayerName() : FString()));
	Arguments.Add(TEXT("SeenCount"), FText::AsNumber(State ? State->PokedexSeen.Num() : 0));
	Arguments.Add(TEXT("CaughtCount"), FText::AsNumber(State ? State->PokedexCaught.Num() : 0));
	Arguments.Add(TEXT("MoneyLost"), FText::AsNumber(ClientMoneyLost));

	// Starter du joueur (son surnom s'il en a un), d'après le drapeau Starter_<Espèce>
	for (const FName& Flag : Flags)
	{
		const FString FlagName = Flag.ToString();
		if (FlagName.StartsWith(TEXT("Starter_")) && FlagName.Len() > 8)
		{
			const FString Species = FlagName.Mid(8);
			FText Name = GetSpeciesName(Database, FName(*Species));
			if (State)
			{
				if (const FPokemonInstance* Starter = State->Party.FindByPredicate([&Species](const FPokemonInstance& P) { return P.Species == FName(*Species); }))
				{
					Name = UPokemonLibrary::GetDisplayName(Database, *Starter);
				}
			}
			Arguments.Add(TEXT("StarterName"), Name);
			break;
		}
	}

	// Starter du rival : dernier Pokémon de RivalLab_<starter du joueur> dans DT_Trainers
	static const FName RivalLab(TEXT("RivalLab"));
	const FName RivalId = UBattleSubsystem::ResolveTrainerId(Database, RivalLab, Flags);
	const FTrainerRow* Rival = Database && !RivalId.IsNone() ? Database->FindTrainer(RivalId) : nullptr;
	if (Rival && Rival->Party.Num() > 0)
	{
		Arguments.Add(TEXT("RivalStarterName"), GetSpeciesName(Database, Rival->Party.Last().Species));
	}

	return UDialogueLibrary::FormatDialogueText(Text, Arguments);
}

bool UDialogueComponent::PlayVoice(FName ConversationId, int32 LineIndex)
{
	if (VoiceFolder.IsEmpty())
	{
		return false;
	}
	const FString AssetName = FString::Printf(TEXT("VO_%s_%02d"), *ConversationId.ToString(), LineIndex + 1);
	const FString PackagePath = VoiceFolder / AssetName;
	if (!FPackageName::DoesPackageExist(PackagePath))
	{
		return false;
	}
	USoundBase* Voice = LoadObject<USoundBase>(nullptr, *(PackagePath + TEXT(".") + AssetName));
	if (!Voice)
	{
		return false;
	}
	VoiceAudio = UGameplayStatics::SpawnSound2D(this, Voice);
	return VoiceAudio != nullptr;
}

void UDialogueComponent::StopVoice()
{
	if (VoiceAudio)
	{
		VoiceAudio->Stop();
		VoiceAudio = nullptr;
	}
}
