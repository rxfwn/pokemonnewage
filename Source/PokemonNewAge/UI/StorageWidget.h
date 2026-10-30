#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Capture/PokemonStorage.h"
#include "StorageWidget.generated.h"

class UPokemonCenterComponent;
class UCaptureComponent;
class UTextBlock;
class UVerticalBox;

/**
 *  Écran du PC de Léo (SCRUM-53, PC de la capture SCRUM-42) : l'équipe à gauche, une boîte de 30 places à droite.
 *  Déposer, Retirer, Échanger, Relâcher : chaque demande part au serveur par UCaptureComponent (règles UPokemonStorage).
 *
 *  Commandes : Haut / Bas = choisir, Gauche / Droite (Q / D) = équipe ou boîte, Page préc. / Page suiv., gâchettes LB / RB
 *  ou Tab = changer de boîte. E, Espace, Entrée, clic ou A = actions ; Échap, Retour arrière ou B = revenir, puis éteindre le PC.
 *
 *  Construit entièrement en C++ ; on pourra l'habiller avec un WBP_PC enfant de cette classe
 *  (réglé dans le PokemonCenterComponent du PlayerController).
 */
UCLASS()
class UStorageWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(UPokemonCenterComponent* InOwner);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Hook Blueprint : l'écran a changé, pour animer un habillage WBP_PC */
	UFUNCTION(BlueprintImplementableEvent, Category="PC")
	void OnStorageRefreshed();

private:

	enum class EColumn : uint8 { Party, Box };

	/** Actions du petit menu ouvert sur un Pokémon */
	enum class EAction : uint8 { Deposit, Swap, Withdraw, Release, ConfirmRelease, Cancel };

	void Confirm();
	void Back();
	void MoveSelection(int32 Delta);
	void ChangeColumn();
	void ChangeBox(int32 Delta);
	void RunAction(EAction Action);

	/** Actions proposées pour la case choisie */
	TArray<EAction> GetActions() const;

	void Refresh();
	int32 ComputeSignature() const;

	UFUNCTION()
	void HandleStorageRefused(EPokemonStorageError Error);

	UCaptureComponent* GetCapture() const;
	const TArray<FPokemonInstance>* GetParty() const;
	const TArray<FPokemonInstance>* GetStorage() const;

	UPROPERTY(Transient)
	TObjectPtr<UPokemonCenterComponent> Owner;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoxTitle;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PartyList;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BoxList;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ActionList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	EColumn Column = EColumn::Party;
	int32 PartySelection = 0;
	/** Place dans la boîte (0 à 29) */
	int32 BoxSelection = 0;
	int32 Box = 0;

	/** Menu d'actions ouvert (sinon on choisit un Pokémon) */
	bool bActionsOpen = false;
	int32 ActionSelection = 0;
	TArray<EAction> Actions;

	/** Échange en cours : Pokémon de l'équipe choisi, on cherche celui du PC */
	int32 SwapPartyIndex = INDEX_NONE;

	int32 LastSignature = 0;
	FText Message;
};
