#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/BattleTypes.h"
#include "Capture/PokemonStorage.h"
#include "CaptureComponent.generated.h"

class APokeBallThrow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPokemonCaught, const FCaughtPokemonInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStorageRefused, EPokemonStorageError, Error);

/**
 *  La capture et le PC d'un joueur (épopée Capture SCRUM-9), sur son PlayerController à côté de UBattleComponent.
 *  - Serveur : range le Pokémon capturé (équipe, ou PC si elle est pleine), le note au Pokédex et prévient le client.
 *  - Client : OnPokemonCaught pour l'UI (fiche du Pokédex, envoi au PC, surnom), puis SetCaughtNickname.
 *  - PC de Léo : déposer, retirer, échanger, relâcher (demandes envoyées au serveur, refusées pendant un combat).
 *  - Lancer de Poké Ball en 3D (SCRUM-40) : joué chez le joueur à partir des événements du combat.
 *  Les règles elles-mêmes sont dans UPokemonStorage (testées sans Unreal). Voir docs/CAPTURE.md.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCaptureComponent();

	// ------------------------------------------------ Côté joueur (UI)

	/** Surnom du Pokémon qui vient d'être capturé. Vide = garder le nom de l'espèce */
	UFUNCTION(BlueprintCallable, Category="Capture")
	void SetCaughtNickname(const FString& Nickname);

	/** PC : équipe -> PC */
	UFUNCTION(BlueprintCallable, Category="Capture|PC")
	void DepositPokemon(int32 PartyIndex);

	/** PC : PC -> équipe */
	UFUNCTION(BlueprintCallable, Category="Capture|PC")
	void WithdrawPokemon(int32 StorageIndex);

	/** PC : échange un Pokémon de l'équipe et un du PC */
	UFUNCTION(BlueprintCallable, Category="Capture|PC")
	void SwapWithStorage(int32 PartyIndex, int32 StorageIndex);

	/** PC : relâche un Pokémon (définitif, demander confirmation avant) */
	UFUNCTION(BlueprintCallable, Category="Capture|PC")
	void ReleasePokemon(int32 StorageIndex);

	/** Rencontres dans les hautes herbes (faux = comme un Repousse sans fin, pour les tests) */
	UFUNCTION(BlueprintCallable, Category="Capture")
	void SetEncountersEnabled(bool bEnabled);

	/** Vrai côté serveur seulement */
	bool AreEncountersEnabled() const { return bEncountersEnabled; }

	/** Un Pokémon vient d'être capturé : messages (UPokemonStorage::GetCaughtMessages), puis proposer un surnom */
	UPROPERTY(BlueprintAssignable, Category="Capture")
	FOnPokemonCaught OnPokemonCaught;

	/** Le serveur a refusé une demande du PC ou un surnom */
	UPROPERTY(BlueprintAssignable, Category="Capture")
	FOnStorageRefused OnStorageRefused;

	/** Écrit les messages à l'écran et dans le journal (tests sans UI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capture")
	bool bPrintToScreen = true;

	// ------------------------------------------------ Lancer de Ball (client)

	/** Acteur du lancer (un Blueprint enfant de PokeBallThrow pour changer le modèle, les sons...). Vide = pas de lancer en 3D */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capture|Ball")
	TSubclassOf<APokeBallThrow> PokeBallThrowClass;

	/** Où tombe la Ball tant que le Pokémon sauvage n'a pas de modèle en 3D : devant le joueur, à cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capture|Ball", meta=(ClampMin=100, Units="cm"))
	float BallTargetDistance = 450.f;

	/** Point visé par la Ball. Si un acteur montre le Pokémon sauvage, l'y placer (SetBallTarget) */
	UFUNCTION(BlueprintCallable, Category="Capture|Ball")
	void SetBallTarget(FVector WorldLocation) { BallTargetOverride = WorldLocation; bHasBallTargetOverride = true; }

	UFUNCTION(BlueprintCallable, Category="Capture|Ball")
	void ClearBallTarget() { bHasBallTargetOverride = false; }

	// ------------------------------------------------ Côté serveur (UBattleSubsystem)

	/** Fin de combat avec capture : range le Pokémon et prévient le client. Faux si plus de place */
	bool HandlePokemonCaught(const FPokemonInstance& Caught);

	UFUNCTION(Client, Reliable)
	void ClientPokemonCaught(const FCaughtPokemonInfo& Info);

	UFUNCTION(Client, Reliable)
	void ClientStorageRefused(EPokemonStorageError Error);

protected:

	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSetCaughtNickname(const FString& Nickname);

	UFUNCTION(Server, Reliable)
	void ServerDepositPokemon(int32 PartyIndex);

	UFUNCTION(Server, Reliable)
	void ServerWithdrawPokemon(int32 StorageIndex);

	UFUNCTION(Server, Reliable)
	void ServerSwapWithStorage(int32 PartyIndex, int32 StorageIndex);

	UFUNCTION(Server, Reliable)
	void ServerReleasePokemon(int32 StorageIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetEncountersEnabled(bool bEnabled);

	/** Client : rejoue le lancer de Ball à partir des événements du tour */
	UFUNCTION()
	void HandleBattleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& Snapshot);

private:

	class APokemonPlayerState* GetPokemonPlayerState() const;
	bool IsInBattle() const;

	/** Serveur : vérifie qu'on n'est pas en combat, sinon prévient le client */
	bool CanUseStorage();

	void Print(const FString& Line, const FColor& Color = FColor::Cyan) const;

	/** Serveur : où se trouve le Pokémon qui attend son surnom (-1 = aucun) */
	bool bPendingNicknameInParty = true;
	int32 PendingNicknameIndex = INDEX_NONE;

	/** Serveur */
	bool bEncountersEnabled = true;

	FVector BallTargetOverride = FVector::ZeroVector;
	bool bHasBallTargetOverride = false;
};
