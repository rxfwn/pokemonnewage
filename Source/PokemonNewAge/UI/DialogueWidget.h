#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogueWidget.generated.h"

class UBorder;
class UDialogueComponent;
class USoundBase;
class UTextBlock;
class UVerticalBox;

/**
 *  Boîte de dialogue en bas de l'écran (SCRUM-26), comme dans Rouge Feu / Vert Feuille :
 *  nom de celui qui parle, texte qui s'écrit lettre par lettre, flèche quand on peut continuer,
 *  et liste de réponses (Oui / Non) quand la réplique pose une question.
 *
 *  Commandes : E, Espace, Entrée, clic, A ou X de la manette = afficher tout le texte, puis continuer.
 *  Haut / Bas (flèches, Z / S, croix de la manette) = changer de réponse. Échap ou B = dernière réponse (« Non »).
 *
 *  Construite entièrement en C++ comme le menu en jeu ; on pourra l'habiller plus tard
 *  avec un WBP_Dialogue enfant de cette classe (réglé dans le DialogueComponent du PlayerController).
 */
UCLASS()
class UDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(UDialogueComponent* InOwner, float InLettersPerSecond, USoundBase* InTypingSound);

	/**
	 *  Affiche une réplique. SpeakerName vide = pas de cartouche de nom (panneau, objet, narration).
	 *  bHasVoice : une voix est jouée, on n'ajoute pas le petit son de frappe.
	 */
	void ShowLine(const FText& SpeakerName, const FText& Text, const TArray<FText>& Choices, bool bHasVoice);

	/** Vrai tant que les lettres s'affichent */
	bool IsTyping() const { return VisibleLetters < FullText.Len(); }

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Hook Blueprint : une réplique s'affiche (pour animer un habillage WBP_Dialogue) */
	UFUNCTION(BlueprintImplementableEvent, Category="Dialogue")
	void OnLineStarted(const FText& SpeakerName, const FText& Text);

private:

	/** Touche « valider » : finit le texte, puis continue ou choisit */
	void Confirm();

	/** Change la réponse surlignée */
	void MoveSelection(int32 Delta);

	void RefreshText();
	void RefreshChoices();

	UPROPERTY(Transient)
	TObjectPtr<UDialogueComponent> Owner;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> TypingSound;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NamePlate;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinueArrow;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ChoicesPanel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChoicesList;

	FString FullText;
	TArray<FText> CurrentChoices;
	int32 SelectedChoice = 0;
	int32 VisibleLetters = 0;
	float LetterProgress = 0.f;
	float LettersPerSecond = 45.f;
	float BlinkTime = 0.f;
	bool bPlayTypingSound = true;
};
