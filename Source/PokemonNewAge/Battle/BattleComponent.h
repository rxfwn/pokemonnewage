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
 *  Après le combat, les évolutions et les attaques proposées passent par UProgressionComponent (docs/PROGRESSION.md).
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

	// ------------------------------------------------ Côté serveur (UBattleSubsystem)

	UFUNCTION(Client, Reliable)
	void ClientBattleStarted(const FBattleSnapshot& InSnapshot);

	UFUNCTION(Client, Reliable)
	void ClientBattleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& InSnapshot);

	UFUNCTION(Client, Reliable)
	void ClientBattleEnded(const FBattleResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientActionRefused(EBattleActionError Error);

	/** Écrit le texte de chaque événement à l'écran et dans le journal (tests sans UI de combat) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle")
	bool bPrintEventsToScreen = true;

protected:

	UFUNCTION(Server, Reliable)
	void ServerSubmitAction(const FBattleAction& Action);

	UFUNCTION(Server, Reliable)
	void ServerChooseReplacement(int32 PartyIndex);

	UFUNCTION(Server, Reliable)
	void ServerRunInsteadOfReplacement();

	UFUNCTION(Server, Reliable)
	void ServerResolveMoveLearn(int32 ForgetSlot);

private:

	class UBattleSubsystem* GetBattleSubsystem() const;
	APlayerController* GetPlayerController() const;

	UPROPERTY(Transient)
	FBattleSnapshot Snapshot;

	bool bInBattle = false;
};
