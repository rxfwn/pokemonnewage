#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/BattleTypes.h"
#include "BattleComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleStarted, const FBattleSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBattleEvents, const TArray<FBattleEvent>&, Events, const FBattleSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleEnded, const FBattleResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleActionRefused, EBattleActionError, Error);

/**
 *  Le lien entre un joueur et son combat (sur son PlayerController, créé par le code).
 *  - L'UI du combat appelle SubmitAction, ChooseReplacement... : la demande part au serveur.
 *  - Le serveur renvoie les événements du tour : OnBattleEvents, à rejouer dans l'ordre (texte, animations, PV).
 *  Marche pareil en solo (le joueur est son propre serveur) et en multijoueur.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UBattleComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBattleComponent();

	// ------------------------------------------------ Côté joueur (UI)

	UFUNCTION(BlueprintCallable, Category="Battle")
	void SubmitAction(const FBattleAction& Action);

	UFUNCTION(BlueprintCallable, Category="Battle")
	void ChooseReplacement(int32 PartyIndex);

	/** Combat sauvage, Pokémon K.O. : fuir au lieu d'en envoyer un autre */
	UFUNCTION(BlueprintCallable, Category="Battle")
	void RunInsteadOfReplacement();

	/** 5e attaque : case à oublier (0 à 3), -1 = ne pas l'apprendre */
	UFUNCTION(BlueprintCallable, Category="Battle")
	void ResolveMoveLearn(int32 ForgetSlot);

	/** Après le combat (séquence d'évolution) : bAccept = faux si le joueur appuie sur B */
	UFUNCTION(BlueprintCallable, Category="Battle")
	void ConfirmEvolution(int32 PartyIndex, bool bAccept);

	/** Hors combat (après une évolution) : apprendre Move à la place de ForgetSlot (-1 = case libre) */
	UFUNCTION(BlueprintCallable, Category="Battle")
	void LearnPendingMove(int32 PartyIndex, FName Move, int32 ForgetSlot);

	UFUNCTION(BlueprintPure, Category="Battle")
	bool IsInBattle() const { return bInBattle; }

	/** Dernier état connu du combat */
	UFUNCTION(BlueprintPure, Category="Battle")
	FBattleSnapshot GetSnapshot() const { return Snapshot; }

	UPROPERTY(BlueprintAssignable, Category="Battle")
	FOnBattleStarted OnBattleStarted;

	UPROPERTY(BlueprintAssignable, Category="Battle")
	FOnBattleEvents OnBattleEvents;

	UPROPERTY(BlueprintAssignable, Category="Battle")
	FOnBattleEnded OnBattleEnded;

	/** Le serveur a refusé l'action (fuite impossible, plus de PP...) : afficher le message et rechoisir */
	UPROPERTY(BlueprintAssignable, Category="Battle")
	FOnBattleActionRefused OnActionRefused;

	/** Après une évolution : attaques que le Pokémon peut apprendre (proposer au joueur, puis LearnPendingMove) */
	UPROPERTY(BlueprintReadOnly, Category="Battle")
	TArray<FName> MovesToLearnAfterEvolution;

	/** Pokémon de l'équipe concerné par MovesToLearnAfterEvolution */
	UPROPERTY(BlueprintReadOnly, Category="Battle")
	int32 MovesToLearnPartyIndex = -1;

	// ------------------------------------------------ Côté serveur (UBattleSubsystem)

	UFUNCTION(Client, Reliable)
	void ClientBattleStarted(const FBattleSnapshot& InSnapshot);

	UFUNCTION(Client, Reliable)
	void ClientBattleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& InSnapshot);

	UFUNCTION(Client, Reliable)
	void ClientBattleEnded(const FBattleResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientActionRefused(EBattleActionError Error);

	UFUNCTION(Client, Reliable)
	void ClientMovesToLearn(int32 PartyIndex, const TArray<FName>& Moves);

	/** Écrit le texte de chaque événement à l'écran et dans le journal (tests sans UI de combat) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	bool bPrintEventsToScreen = true;

	/** Évolutions proposées à la fin du dernier combat (le serveur vérifie avant d'appliquer) */
	TArray<FPendingEvolution> PendingEvolutions;

	/** Attaques que le serveur autorise après une évolution */
	TMap<int32, TArray<FName>> AllowedMovesToLearn;

protected:

	UFUNCTION(Server, Reliable)
	void ServerSubmitAction(const FBattleAction& Action);

	UFUNCTION(Server, Reliable)
	void ServerChooseReplacement(int32 PartyIndex);

	UFUNCTION(Server, Reliable)
	void ServerRunInsteadOfReplacement();

	UFUNCTION(Server, Reliable)
	void ServerResolveMoveLearn(int32 ForgetSlot);

	UFUNCTION(Server, Reliable)
	void ServerConfirmEvolution(int32 PartyIndex, bool bAccept);

	UFUNCTION(Server, Reliable)
	void ServerLearnPendingMove(int32 PartyIndex, FName Move, int32 ForgetSlot);

private:

	class UBattleSubsystem* GetBattleSubsystem() const;
	APlayerController* GetPlayerController() const;

	UPROPERTY(Transient)
	FBattleSnapshot Snapshot;

	bool bInBattle = false;
};
