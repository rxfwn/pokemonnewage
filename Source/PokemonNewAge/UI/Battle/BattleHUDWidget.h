#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleTypes.h"
#include "BattleHUDWidget.generated.h"

class UBattleComponent;
class UBattleHUDWidget;
class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UUniformGridPanel;
class UPokemonDatabase;
class UPanelWidget;
class UWidget;

/** Ce que fait un bouton de l'écran de combat. */
UENUM()
enum class EBattleHUDAction : uint8
{
	Fight,
	Bag,
	Pokemon,
	Run,
	Move,
	Item,
	PartyPick,
	ForgetMove,
	KeepMoves,
	EvolveYes,
	EvolveNo,
	Back
};

/** Relie un bouton à son action (les délégués UMG n'ont pas de paramètre). */
UCLASS()
class UBattleHUDButtonBinding : public UObject
{
	GENERATED_BODY()

public:

	EBattleHUDAction Action = EBattleHUDAction::Back;
	int32 Index = 0;
	TWeakObjectPtr<UBattleHUDWidget> HUD;

	UFUNCTION()
	void HandleClicked();
};

/**
 *  Écran de combat (SCRUM-36) : cadres des deux Pokémon (nom, niveau, PV animés, XP), boîte de texte
 *  et menus Attaque / Sac / Pokémon / Fuite. Construit en C++, ouvert par le PlayerController à chaque combat.
 *  Il rejoue les événements envoyés par le serveur (UBattleComponent::OnBattleEvents) un par un, avec les
 *  textes de UBattleText, puis propose les choix selon la phase (action, remplaçant, 5e attaque, évolution).
 *  Il ne calcule rien du combat lui-même (docs/COMBAT.md).
 *  Souris, clavier (flèches, Entrée, Échap) et manette (croix, A, B).
 */
UCLASS()
class UBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Branche l'écran sur le combat qui commence */
	void Begin(UBattleComponent* InBattle, const FBattleSnapshot& Snapshot);

	void OnButton(EBattleHUDAction Action, int32 Index);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION()
	void HandleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& Snapshot);

	UFUNCTION()
	void HandleEnded(const FBattleResult& Result);

	UFUNCTION()
	void HandleRefused(EBattleActionError Error);

	UFUNCTION()
	void HandleMovesToLearn(int32 PartyIndex, const TArray<FName>& Moves);

private:

	/** Ce que l'écran attend du joueur une fois les messages finis */
	enum class EMenu : uint8 { None, Main, Moves, Bag, Party, ForgetMove, Evolution };
	/** Pourquoi on choisit un Pokémon dans l'équipe */
	enum class EPartyMode : uint8 { Switch, Replacement, ItemTarget, AfterEvolutionLearn };

	// Lecture des événements
	void StartNextEvent();
	void ShowMessage(const FText& Text);
	bool IsMessageBusy() const;
	void AdvanceMessage();
	void ApplyEventVisuals(const FBattleEvent& Event);
	void ShowMenuForPhase();
	void FinishBattle();

	// Cadres
	void RefreshInfoBox(int32 Side, bool bInstantHP);
	const FPokemonInstance* GetDisplayedPokemon(int32 Side) const;

	// Menus
	void ClearMenu();
	void ShowMainMenu();
	void ShowMoves();
	void ShowBag();
	void ShowParty(EPartyMode Mode);
	void ShowForgetMove(int32 PartyIndex, FName NewMove, bool bAfterEvolution);
	void ShowEvolution();
	UButton* AddMenuButton(UPanelWidget* Panel, const FText& Text, EBattleHUDAction Action, int32 Index,
		const FLinearColor& Color, bool bEnabled = true, const FText& SubText = FText::GetEmpty());
	void Submit(const FBattleAction& Action);

	const UPokemonDatabase* GetDatabase() const;
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold = false);

	/** Widgets d'un cadre (ils restent en vie par l'arbre de widgets) */
	struct FInfoBox
	{
		TObjectPtr<UBorder> Frame;
		TObjectPtr<UTextBlock> Name;
		TObjectPtr<UTextBlock> Level;
		TObjectPtr<UTextBlock> Status;
		TObjectPtr<UProgressBar> HPBar;
		TObjectPtr<UTextBlock> HPText;
		TObjectPtr<UProgressBar> ExpBar;
	};
	void BuildInfoBox(FInfoBox& Box, bool bPlayer);

	UPROPERTY(Transient) TObjectPtr<UBattleComponent> Battle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageArrow;
	UPROPERTY(Transient) TObjectPtr<UBorder> MenuFrame;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MenuBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UBattleHUDButtonBinding>> Bindings;
	UPROPERTY(Transient) TObjectPtr<UButton> FirstButton;

	FInfoBox Boxes[2];

	/** Dernier état reçu, et celui d'avant le lot d'événements en cours (pour les PV au moment où un Pokémon sort) */
	FBattleSnapshot Snapshot;
	FBattleSnapshot PreviousSnapshot;
	TArray<FBattleEvent> Queue;

	/** Pokémon affiché de chaque côté (index dans l'équipe), -1 = aucun */
	int32 ShownIndex[2] = { -1, -1 };
	int32 ShownLevel[2] = { 0, 0 };
	float DisplayedHP[2] = { 0.f, 0.f };
	float TargetHP[2] = { 0.f, 0.f };
	float MaxHP[2] = { 1.f, 1.f };

	EMenu Menu = EMenu::None;
	EPartyMode PartyMode = EPartyMode::Switch;
	FName PendingItem;

	// Boîte de texte (machine à écrire)
	FString FullMessage;
	float MessageChars = 0.f;
	float MessageHold = 0.f;
	bool bMessageActive = false;
	float CharsPerSecond = 60.f;
	float HoldTime = 0.9f;

	// Fin de combat
	bool bEnded = false;
	FBattleResult Result;
	int32 EvolutionStep = 0;
	TArray<TPair<int32, FName>> MovesToLearn;
	float CloseTimer = -1.f;
	bool bWaitingForServer = false;
};
