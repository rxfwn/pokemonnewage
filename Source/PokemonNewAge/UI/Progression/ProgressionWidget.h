#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Pokemon/PokemonProgression.h"
#include "ProgressionWidget.generated.h"

class UBorder;
class UButton;
class UProgressionWidget;
class UTextBlock;
class UVerticalBox;
class UUniformGridPanel;
class UPokemonDatabase;

/** Relie un bouton du choix d'attaque à sa case (les délégués UMG n'ont pas de paramètre). */
UCLASS()
class UProgressionButtonBinding : public UObject
{
	GENERATED_BODY()

public:

	/** 0 à 3 = attaque à oublier, -1 = ne pas apprendre */
	int32 Slot = -1;
	TWeakObjectPtr<UProgressionWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};

/**
 *  Écran de la progression hors combat (docs/PROGRESSION.md) : boîte de texte, panneau des stats
 *  quand un Pokémon monte de niveau, choix de l'attaque à oublier, et texte + flash de la séquence d'évolution.
 *  Construit en C++ (même style que l'écran de combat), piloté par UProgressionComponent.
 *  Clavier (Entrée/Espace = A, Échap/Retour = B), souris et manette.
 */
UCLASS()
class UProgressionWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Message dans la boîte de texte. bWaitForInput : attend A, sinon passe tout seul. OnDone à la fin */
	void ShowMessage(const FText& Text, bool bWaitForInput, TFunction<void()> OnDone = nullptr);

	/** « X monte au niveau N ! » puis le panneau des stats (gains, puis totaux) */
	void ShowLevelUp(const FText& PokemonName, const FLevelUpInfo& Info, TFunction<void()> OnDone);

	/** 4 attaques déjà connues : le joueur choisit laquelle oublier. OnChosen(case 0 à 3, ou -1 = ne pas apprendre) */
	void ShowForgetMove(const FPokemonInstance& Pokemon, FName NewMove, TFunction<void(int32)> OnChosen);

	/** Texte posé sans attente ni clic (séquence d'évolution) */
	void SetCaption(const FText& Text);

	/** Voile blanc plein écran (0 à 1) : flash de l'évolution */
	void SetFlash(float Alpha);

	/** Appelé quand le joueur appuie sur B hors d'un choix (annuler l'évolution). Vrai = géré */
	TFunction<bool()> OnBack;

	/** Bouton cliqué dans le choix d'attaque */
	void ChooseForgetSlot(int32 MoveSlot);

	bool IsBusy() const { return bMessageActive || bChoosing; }

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:

	void Confirm();
	void FinishMessage();
	void ShowStatsPanel(bool bTotals);
	void ClearMenu();
	const UPokemonDatabase* GetDatabase() const;
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold = false);

	UPROPERTY(Transient) TObjectPtr<UBorder> MessageFrame;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageArrow;
	UPROPERTY(Transient) TObjectPtr<UBorder> StatsFrame;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> StatsGrid;
	UPROPERTY(Transient) TObjectPtr<UBorder> MenuFrame;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MenuBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> FlashBorder;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressionButtonBinding>> Bindings;
	UPROPERTY(Transient) TObjectPtr<UButton> FirstButton;

	// Boîte de texte (machine à écrire)
	FString FullMessage;
	float MessageChars = 0.f;
	float MessageHold = 0.f;
	bool bMessageActive = false;
	bool bWaitForInput = true;
	TFunction<void()> MessageDone;
	float CharsPerSecond = 60.f;
	float AutoHoldTime = 1.2f;

	// Montée de niveau : 0 = message, 1 = gains, 2 = totaux
	FLevelUpInfo LevelUp;
	int32 LevelUpStep = 0;
	TFunction<void()> LevelUpDone;

	// Choix d'attaque
	bool bChoosing = false;
	FPokemonInstance LearningPokemon;
	FName LearningMove;
	TFunction<void(int32)> ForgetChosen;
};
