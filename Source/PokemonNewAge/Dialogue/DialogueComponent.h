#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/BattleTypes.h"
#include "Dialogue/DialogueRows.h"
#include "DialogueComponent.generated.h"

class APlayerController;
class APokemonPlayerState;
class UAudioComponent;
class UDataTable;
class UDialogueWidget;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDialogueLineShown, FName, ConversationId, int32, LineIndex, const FDialogueLine&, Line, const FText&, FormattedText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueEvent, EDialogueEvent, Event, FName, EventParam, int32, EventAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEnded);

/**
 *  Joue les conversations de DT_Dialogues pour un joueur (SCRUM-26). Posé sur le PlayerController.
 *
 *  Le serveur mène la conversation : il choisit la conversation d'après les drapeaux du joueur,
 *  applique les événements (objets, Pokémon, argent, combat) et les drapeaux de fin.
 *  Le client affiche chaque réplique dans la boîte de dialogue (UDialogueWidget), joue la voix
 *  si elle existe, et renvoie « suivant » ou le choix du joueur. En solo, tout se passe sur la même machine.
 *
 *  Voir docs/DIALOGUES.md, partie « Boîte de dialogue en jeu ».
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDialogueComponent();

	// ------------------------------------------------ Lancer une conversation

	/** Le joueur parle au PNJ Npc (ligne de DT_Npcs) : joue sa conversation Talk valide de plus haute priorité */
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	void TalkTo(FName Npc);

	/** Joue une conversation par son nom (scène, volume au sol). Côté serveur, ou depuis le client du joueur */
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	void PlayConversation(FName ConversationId);

	/** Vrai pendant une conversation (le joueur ne bouge plus) */
	UFUNCTION(BlueprintPure, Category="Dialogue")
	bool IsInConversation() const { return !ClientConversation.IsNone() || !ServerConversation.IsNone(); }

	/** Tables chargées (nullptr si l'import n'a pas été fait) */
	UDataTable* GetDialoguesTable() const;
	UDataTable* GetNpcsTable() const;

	// ------------------------------------------------ Appelé par la boîte de dialogue (client)

	/** Le joueur a fini de lire la réplique affichée */
	void AdvanceLine();

	/** Le joueur a choisi la réponse ChoiceIndex de la réplique affichée */
	void SelectChoice(int32 ChoiceIndex);

	/** Nom affiché au-dessus du texte (vide pour un panneau, un objet ou la narration) */
	FText GetSpeakerName(FName Speaker) const;

	/** Remplace {PlayerName}, {StarterName}, {RivalStarterName}, {SeenCount}, {CaughtCount}, {MoneyLost} */
	UFUNCTION(BlueprintPure, Category="Dialogue")
	FText FormatLineText(const FText& Text) const;

	// ------------------------------------------------ Pour les Blueprints (client)

	/** Une réplique vient de s'afficher : sous-titres, voix, animation du PNJ... */
	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FOnDialogueLineShown OnLineShown;

	/**
	 *  Événement pas encore géré en C++ (PlayCutscene, OfferNickname ; OpenShop, OpenPC et ReturnToLastHeal
	 *  seulement si le PlayerController n'a pas de ShopComponent / PokemonCenterComponent).
	 *  Diffusé sur le serveur ET sur le client du joueur, pour qu'un Blueprint puisse le brancher en attendant.
	 */
	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FOnDialogueEvent OnUnhandledEvent;

	/** La boîte de dialogue s'est fermée */
	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FOnDialogueEnded OnConversationEnded;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** DT_Dialogues (importé par Tools/Data/import_data_tables.py) */
	UPROPERTY(EditAnywhere, Category="Dialogue|Data")
	TSoftObjectPtr<UDataTable> DialoguesTable;

	/** DT_Npcs */
	UPROPERTY(EditAnywhere, Category="Dialogue|Data")
	TSoftObjectPtr<UDataTable> NpcsTable;

	/** Boîte de dialogue à afficher (un Widget Blueprint enfant de DialogueWidget pour l'habiller) */
	UPROPERTY(EditAnywhere, Category="Dialogue|UI")
	TSubclassOf<UDialogueWidget> DialogueWidgetClass;

	/** Lettres affichées par seconde (0 = tout le texte d'un coup) */
	UPROPERTY(EditAnywhere, Category="Dialogue|UI", meta=(ClampMin=0))
	float LettersPerSecond = 45.f;

	/** Attente entre la fin d'un combat et la réplique du dresseur (laisse l'écran de combat se fermer) */
	UPROPERTY(EditAnywhere, Category="Dialogue|Battle", meta=(ClampMin=0, Units="s"))
	float BattleOutcomeDelay = 1.5f;

	/**
	 *  Voix (SCRUM-72) : dossier où chercher une voix par réplique, nommée VO_<Conversation>_<NN>
	 *  (NN = numéro de la réplique à partir de 01). Exemple : /Game/Audio/Voices/VO_PlayerHouse_Mom_Start_01.
	 *  Une réplique sans fichier reste muette, rien d'autre à faire.
	 */
	UPROPERTY(EditAnywhere, Category="Dialogue|Voice")
	FString VoiceFolder = TEXT("/Game/Audio/Voices");

	/** Petit son joué à chaque lettre qui apparaît, quand la réplique n'a pas de voix (facultatif) */
	UPROPERTY(EditAnywhere, Category="Dialogue|Voice")
	TObjectPtr<USoundBase> TypingSound;

	/** Son quand on passe à la réplique suivante (facultatif) */
	UPROPERTY(EditAnywhere, Category="Dialogue|Voice")
	TObjectPtr<USoundBase> AdvanceSound;

private:

	// ------------------------------------------------ Serveur

	/** Démarre une conversation pour ce joueur (serveur) */
	void ServerPlay(FName ConversationId);

	/** Applique l'événement de la réplique courante (ou bascule sur FailConversation) puis l'envoie au client */
	void ServerShowCurrentLine();

	/** Fin de la conversation : drapeaux, puis conversation suivante (Next, ou celle d'un choix) */
	void ServerFinish(FName Next);

	/** Applique un événement ; faux s'il a échoué (pas assez d'argent, objet manquant) */
	bool ApplyEvent(const FDialogueLine& Line);

	/** Le combat lancé par un dialogue est fini : réplique de victoire ou de défaite */
	UFUNCTION()
	void HandleBattleEnded(APlayerController* Player, FName TrainerGroup, const FBattleResult& Result);

	UFUNCTION(Server, Reliable)
	void ServerTalkTo(FName Npc);

	UFUNCTION(Server, Reliable)
	void ServerPlayConversation(FName ConversationId);

	UFUNCTION(Server, Reliable)
	void ServerAdvance(FName ConversationId, int32 LineIndex);

	UFUNCTION(Server, Reliable)
	void ServerChoose(FName ConversationId, int32 LineIndex, int32 ChoiceIndex);

	// ------------------------------------------------ Client

	/** Affiche la réplique LineIndex de ConversationId */
	UFUNCTION(Client, Reliable)
	void ClientShowLine(FName ConversationId, int32 LineIndex, int32 MoneyLost);

	/** Ferme la boîte de dialogue */
	UFUNCTION(Client, Reliable)
	void ClientEndConversation();

	UFUNCTION(Client, Reliable)
	void ClientUnhandledEvent(EDialogueEvent Event, FName EventParam, int32 EventAmount);

	void OpenWidget();
	void CloseWidget();

	/** Joue VO_<Conversation>_<NN> s'il existe ; vrai si une voix joue */
	bool PlayVoice(FName ConversationId, int32 LineIndex);
	void StopVoice();

	APlayerController* GetPlayerController() const;
	APokemonPlayerState* GetPokemonPlayerState() const;

	/** Conversation menée par le serveur pour ce joueur, et la réplique où il en est */
	FName ServerConversation;
	int32 ServerLine = INDEX_NONE;

	/** Argent perdu au dernier combat ({MoneyLost}) */
	int32 LastMoneyLost = 0;

	/** Ce que le client affiche */
	FName ClientConversation;
	int32 ClientLine = INDEX_NONE;
	int32 ClientMoneyLost = 0;

	/** Drapeaux que la conversation affichée donnera à la fin (pour {RivalStarterName} avant qu'ils arrivent) */
	TArray<FName> ClientPendingFlags;

	UPROPERTY(Transient)
	TObjectPtr<UDialogueWidget> Widget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudio;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedDialogues;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedNpcs;

	FTimerHandle OutcomeTimer;
};
