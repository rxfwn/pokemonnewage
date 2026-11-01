#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleTypes.h"
#include "BattleHUDWidget.generated.h"

class UBattleComponent;
class UBattleHUDWidget;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UScrollBox;
class UTexture2D;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UUniformGridPanel;
class UPokemonDatabase;
class UPanelWidget;
class UWidget;
struct FItemRow;

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
	/** Onglet du Sac (Index = EBagTab) */
	BagTab,
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
 *  et menus Attaquer / Sac / Pokémon / Fuite. Disposition au pixel d'après la maquette Combat_Hud de L'amie
 *  (2026-11-01) : panneaux en verre, boutons en dégradé, cartes d'attaque (nom, type, PP), barres de PV
 *  verte / orange / rouge (images de Tools/HUD/make_battle_assets.py). Construit en C++, ouvert par le PlayerController à chaque combat.
 *  Il rejoue les événements envoyés par le serveur (UBattleComponent::OnBattleEvents) un par un, avec les
 *  textes de UBattleText, puis propose les choix selon la phase (action, remplaçant, 5e attaque).
 *  À la montée de niveau : barre d'XP qui se remplit et panneau des stats gagnées. Les évolutions se jouent
 *  après la fermeture de l'écran (UProgressionComponent, docs/PROGRESSION.md).
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
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION()
	void HandleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& Snapshot);

	UFUNCTION()
	void HandleEnded(const FBattleResult& Result);

	UFUNCTION()
	void HandleRefused(EBattleActionError Error);

private:

	/** Ce que l'écran attend du joueur une fois les messages finis */
	enum class EMenu : uint8 { None, Main, Moves, Bag, Party, ForgetMove };
	/** Pourquoi on choisit un Pokémon dans l'équipe */
	enum class EPartyMode : uint8 { Switch, Replacement, ItemTarget };
	/** Onglets du Sac, de haut en bas (maquette Combat_Hud_Bag) : Soins, Balls, objets de combat, autres, Baies */
	enum class EBagTab : uint8 { Medicine, Balls, Battle, Other, Berries, Count };

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
	/** Boîte de texte sur toute la largeur (pas de menu à droite) ou à sa place à gauche du menu */
	void SetMessageWide(bool bWide);
	void ShowMainMenu();
	void ShowMoves();
	void ShowBag();
	/** Grand panneau du Sac (onglets à gauche, liste qui défile à droite), construit une fois et caché */
	void BuildBag();
	void ShowBagTab(EBagTab Tab);
	/** Onglet où ranger un objet ; Count = pas dans le Sac de combat */
	EBagTab GetBagTab(FName ItemId, const FItemRow& Item) const;
	void UpdateBagThumb();
	void ShowParty(EPartyMode Mode);
	void ShowForgetMove(int32 PartyIndex, FName NewMove);
	void ShowLevelUpStats(const FBattleEvent& Event);
	/** Montre le panneau du menu et ses cartes (bBackButton : bouton retour au milieu) */
	void OpenMenu(EMenu NewMenu, bool bBackButton);
	/** Carte 265 x 85 du menu : ButtonImage 0 à 3 = Attaquer, Sac, Pokémon, Fuite (dégradés) ; -1 = carte en verre
	 *  avec étiquette (type, niveau) et texte à droite (PP...) */
	UButton* AddCard(const FText& Title, EBattleHUDAction Action, int32 Index, int32 ButtonImage, bool bEnabled = true,
		const FText& Tag = FText::GetEmpty(), const FLinearColor& TagColor = FLinearColor::Gray, const FText& RightText = FText::GetEmpty());
	UButton* MakeCardButton(UWidget* Content);
	FText GetTypeName(EPokemonType Type) const;
	/** Barre d'XP du joueur : position animée vers TargetExp (XP totale), sur l'échelle du niveau affiché */
	void UpdateExpBar(float DeltaTime);
	bool IsExpBarMoving() const;
	void Submit(const FBattleAction& Action);

	const UPokemonDatabase* GetDatabase() const;
	/** Size en unités UMG (px Figma x 0,75) */
	UTextBlock* MakeText(const FText& Text, float Size, const FLinearColor& Color, bool bBold = false);

	/** Widgets d'un cadre (ils restent en vie par l'arbre de widgets) */
	struct FInfoBox
	{
		TObjectPtr<UWidget> Frame;
		TObjectPtr<UTextBlock> Name;
		TObjectPtr<UTextBlock> Level;
		TObjectPtr<UTextBlock> Status;
		/** Barre de PV : image en dégradé dont la largeur suit les PV */
		TObjectPtr<UImage> HPFill;
		TObjectPtr<UCanvasPanelSlot> HPFillSlot;
		int32 HPColor = -1;
		TObjectPtr<UProgressBar> ExpBar;
	};
	void BuildInfoBox(FInfoBox& Box, bool bPlayer);
	void SetHPBar(FInfoBox& Box, float Ratio);

	UPROPERTY(Transient) TObjectPtr<UBattleComponent> Battle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> MessageFrameSlot;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> MessageTextSlot;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UWidget> MenuFrame;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> MenuScroll;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> MenuScrollSlot;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> MenuGrid;
	UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
	UPROPERTY(Transient) TObjectPtr<UBattleHUDButtonBinding> BackBindingObject;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> MenuButtons;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> HPFillTextures[3];
	UPROPERTY(Transient) TObjectPtr<UTexture2D> HPTrackTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> ButtonTextures[4];
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BackTexture;
	int32 CardCount = 0;
	UPROPERTY(Transient) TArray<TObjectPtr<UBattleHUDButtonBinding>> Bindings;
	UPROPERTY(Transient) TObjectPtr<UButton> FirstButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> StatsFrame;
	// Sac
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BagFrame;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> BagList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> BagItems;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BagEmptyText;
	UPROPERTY(Transient) TObjectPtr<UImage> BagThumb;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> BagThumbSlot;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> BagTabShades;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> BagTabButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UBattleHUDButtonBinding>> BagBindings;
	UPROPERTY(Transient) TObjectPtr<UButton> BagBackButton;
	EBagTab BagTab = EBagTab::Count;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> StatsGrid;

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
	float DisplayedExp = 0.f;
	float TargetExp = 0.f;

	EMenu Menu = EMenu::None;
	/** Le joueur choisit au clavier ou à la manette : la carte qui a le focus grossit (à la souris, seule celle survolée) */
	bool bKeyboardNavigation = false;
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
	float CloseTimer = -1.f;
	bool bWaitingForServer = false;
};
