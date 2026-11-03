#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/BattleTypes.h"
#include "Pokemon/PokemonProgression.h"
#include "ProgressionComponent.generated.h"

class AEvolutionScene;
class UProgressionWidget;
class USoundBase;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgressionOutcome, const FProgressionOutcome&, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEvolutionFinished, int32, PartyIndex, FName, NewSpecies, bool, bEvolved);

/**
 *  Progression des Pokémon d'un joueur hors combat (SCRUM-12, docs/PROGRESSION.md). Sur son PlayerController.
 *
 *  Serveur : utilise les objets de progression (Super Bonbon, pierres, Fil de Liaison, CT), garde les évolutions
 *  proposées (fin de combat ou pierre) et les attaques que le joueur a le droit d'apprendre, puis applique ses choix.
 *  Client : rejoue dans l'ordre ce qui demande l'écran (montée de niveau, attaque à oublier, séquence d'évolution 3D),
 *  dès que le joueur est libre (pas en combat, Pokédex fermé).
 *
 *  Le combat garde ses propres règles (UBattleSession) ; à la fin, UBattleSubsystem passe ici les évolutions.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UProgressionComponent();

	// ------------------------------------------------ Côté joueur (UI, console)

	/** Sac > objet de progression sur un Pokémon de l'équipe (le serveur vérifie tout) */
	UFUNCTION(BlueprintCallable, Category="Progression")
	void UseItem(FName Item, int32 PartyIndex);

	/** Fin de la séquence d'évolution : bAccept = faux si le joueur a appuyé sur B */
	UFUNCTION(BlueprintCallable, Category="Progression")
	void ConfirmEvolution(int32 PartyIndex, bool bAccept);

	/** Attaque proposée : la mettre à la place de ForgetSlot (0 à 3), -1 = ne pas l'apprendre */
	UFUNCTION(BlueprintCallable, Category="Progression")
	void LearnMove(int32 PartyIndex, FName Move, int32 ForgetSlot);

	/** Rejoue la séquence d'évolution sans rien changer (réglages visuels, console VoirEvolution) */
	UFUNCTION(BlueprintCallable, Category="Progression")
	void PreviewEvolution(FName FromSpecies, FName ToSpecies);

	/** Tests : donne de l'XP à un Pokémon de l'équipe hors combat (console DonnerXP) */
	UFUNCTION(BlueprintCallable, Category="Progression")
	void GiveExperience(int32 PartyIndex, int32 Amount);

	/** Une montée de niveau, un choix ou une évolution est à l'écran ou attend */
	UFUNCTION(BlueprintPure, Category="Progression")
	bool IsBusy() const { return bProcessing || Steps.Num() > 0; }

	/** Chaque résultat envoyé par le serveur (objet utilisé, évolution faite) */
	UPROPERTY(BlueprintAssignable, Category="Progression")
	FOnProgressionOutcome OnOutcome;

	/** Fin d'une séquence d'évolution (bEvolved = faux si annulée) */
	UPROPERTY(BlueprintAssignable, Category="Progression")
	FOnEvolutionFinished OnEvolutionFinished;

	/** Faux : pas d'écran ni de séquence, tout passe par les délégués (UI faite en Blueprint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression")
	bool bUseBuiltInUI = true;

	/** Séquence d'évolution (un Blueprint enfant d'EvolutionScene pour régler lumières, caméra, effets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression")
	TSubclassOf<AEvolutionScene> EvolutionSceneClass;

	/** Écran de la progression (un Widget Blueprint enfant de ProgressionWidget pour l'habiller) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression")
	TSubclassOf<UProgressionWidget> WidgetClass;

	/** Sons et matériau importés par Tools/Progression/import_progression_unreal.py (nul s'ils manquent) */
	static USoundBase* LoadSound(const TCHAR* Name);
	static UMaterialInterface* LoadGlowMaterial();

	// ------------------------------------------------ Côté serveur

	/** Fin de combat : évolutions par niveau à proposer (UBattleSubsystem) */
	void AddBattleEvolutions(const TArray<FPendingEvolution>& Evolutions);

	/** Applique un objet de progression. Faux si sans effet (rien n'est retiré du sac) */
	bool UseItemOnServer(FName Item, int32 PartyIndex);

	UFUNCTION(Client, Reliable)
	void ClientOutcome(const FProgressionOutcome& Outcome);

	/** Fin de combat : les évolutions à jouer une fois l'écran de combat fermé */
	UFUNCTION(Client, Reliable)
	void ClientQueueEvolutions(const TArray<FPendingEvolution>& Evolutions);

protected:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerUseItem(FName Item, int32 PartyIndex);

	UFUNCTION(Server, Reliable)
	void ServerConfirmEvolution(int32 PartyIndex, bool bAccept);

	UFUNCTION(Server, Reliable)
	void ServerLearnMove(int32 PartyIndex, FName Move, int32 ForgetSlot);

	UFUNCTION(Server, Reliable)
	void ServerGiveExperience(int32 PartyIndex, int32 Amount);

private:

	class APokemonPlayerState* GetPokemonPlayerState() const;
	class APokemonNewAgePlayerController* GetController() const;

	/** Serveur : retient ce que le joueur doit encore décider, puis l'envoie à son client */
	void SendOutcome(const FProgressionOutcome& Outcome, FName OldSpecies);

	// ------------------------------------------------ Serveur

	struct FServerEvolution
	{
		int32 PartyIndex = 0;
		/** Espèce attendue au moment de confirmer (l'équipe a pu bouger) */
		FName FromSpecies;
		FName ToSpecies;
		/** Pierre ou Fil de Liaison à retirer du sac si l'évolution va au bout */
		FName Item;
	};
	TArray<FServerEvolution> PendingEvolutions;
	TMap<int32, TArray<FName>> AllowedMoves;

	// ------------------------------------------------ Client : ce qui attend l'écran

	enum class EStepType : uint8 { Message, LevelUp, LearnMove, Evolution };
	struct FStep
	{
		EStepType Type = EStepType::Message;
		int32 PartyIndex = -1;
		FText Text;
		FLevelUpInfo LevelUp;
		FName Move;
		FName FromSpecies;
		FName ToSpecies;
		bool bPreview = false;
	};
	TArray<FStep> Steps;

	/** Ajoute les étapes d'un résultat (à la suite, ou en tête après une évolution) */
	void QueueOutcome(const FProgressionOutcome& Outcome, bool bFront);
	bool CanShowNow() const;
	void ProcessNextStep();
	void StartEvolution(const FStep& Step);
	void FinishEvolution(int32 PartyIndex, FName ToSpecies, bool bEvolved, bool bPreview);
	void EndProcessing();
	FText GetPokemonName(int32 PartyIndex, FName FallbackSpecies) const;

	UPROPERTY(Transient)
	TObjectPtr<UProgressionWidget> Widget;

	UPROPERTY(Transient)
	TObjectPtr<AEvolutionScene> Scene;

	bool bProcessing = false;
	/** Après une évolution confirmée : on attend un instant les attaques que le serveur propose */
	float WaitForServer = 0.f;
	bool bClosingMenu = false;
};
