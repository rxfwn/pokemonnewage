#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PokemonCenterComponent.generated.h"

class APokemonCenterPoint;
class APokemonPlayerState;
class UStorageWidget;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealPointChanged, FName, PointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReturnedToHealPoint);

/**
 *  Le Centre Pokémon d'un joueur (SCRUM-53), sur son PlayerController :
 *  - Soin (événement HealParty de Joëlle ou de maman) : le Centre le plus proche (APokemonCenterPoint) devient
 *    le point de retour, gardé dans le PlayerState et la sauvegarde ; le jingle du point est joué chez le joueur.
 *  - Équipe K.O. (événement ReturnToLastHeal) : écran noir, le joueur réapparaît devant le comptoir du dernier Centre
 *    (ou à la maison s'il n'en a visité aucun), équipe soignée.
 *  - PC de Léo (événement OpenPC) : ouvre l'écran du PC (UStorageWidget), branché sur UCaptureComponent.
 *  Voir docs/CENTRE_ET_BOUTIQUE.md.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UPokemonCenterComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPokemonCenterComponent();

	// ------------------------------------------------ Serveur (appelé par les dialogues)

	/** L'équipe vient d'être soignée : retient le Centre (ou la maison) le plus proche comme point de retour */
	void RecordHealPoint();

	/** Ramène le joueur au dernier point de soin, équipe soignée. Faux si aucun point n'existe dans la map */
	bool ReturnToLastHealPoint();

	/** Ouvre le PC de Léo chez le joueur */
	void OpenPC();

	// ------------------------------------------------ Côté joueur

	/** Ferme l'écran du PC */
	UFUNCTION(BlueprintCallable, Category="Pokemon Center")
	void ClosePC();

	UFUNCTION(BlueprintPure, Category="Pokemon Center")
	bool IsPCOpen() const { return PCWidget != nullptr || bPCWaitingForDialogue; }

	/** Le point de retour a changé (serveur et client du joueur) */
	UPROPERTY(BlueprintAssignable, Category="Pokemon Center")
	FOnHealPointChanged OnHealPointChanged;

	/** Le joueur vient de réapparaître au Centre (client), pour une animation de Joëlle par exemple */
	UPROPERTY(BlueprintAssignable, Category="Pokemon Center")
	FOnReturnedToHealPoint OnReturnedToHealPoint;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Écran du PC (un Widget Blueprint enfant de StorageWidget pour l'habiller) */
	UPROPERTY(EditAnywhere, Category="Pokemon Center|UI")
	TSubclassOf<UStorageWidget> StorageWidgetClass;

	/** Durée du fondu au noir avant et après le retour au Centre */
	UPROPERTY(EditAnywhere, Category="Pokemon Center", meta=(ClampMin=0, Units="s"))
	float FadeDuration = 0.6f;

	/** Attente maximale du terrain (World Partition) sous les pieds du joueur après le retour */
	UPROPERTY(EditAnywhere, Category="Pokemon Center", meta=(ClampMin=1, Units="s"))
	float GroundWaitTimeout = 15.f;

	UFUNCTION(Client, Reliable)
	void ClientHealRecorded(FName PointId, USoundBase* HealSound);

	UFUNCTION(Client, Reliable)
	void ClientFadeOut();

	UFUNCTION(Client, Reliable)
	void ClientReturned();

	UFUNCTION(Client, Reliable)
	void ClientOpenPC();

private:

	APokemonPlayerState* GetPokemonPlayerState() const;
	APlayerController* GetPlayerController() const;

	/** Serveur : place le personnage sur le point, sans gravité tant que le sol n'est pas chargé */
	void TeleportPawn(const FVector& Location, const FRotator& Rotation);

	/** Serveur : vérifie que le sol est là, puis rend la marche au joueur */
	void CheckGround();

	void ShowPC();

	UFUNCTION()
	void HandleConversationEnded();

	FTimerHandle GroundTimer;
	float GroundWaitStart = 0.f;

	bool bPCWaitingForDialogue = false;

	UPROPERTY(Transient)
	TObjectPtr<UStorageWidget> PCWidget;
};
