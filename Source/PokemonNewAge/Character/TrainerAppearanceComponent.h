#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/TrainerAppearance.h"
#include "TrainerAppearanceComponent.generated.h"

class UTrainerAppearanceData;
class USkeletalMeshComponent;
class ACharacterStudio;
class UCharacterCreatorWidget;

/** D'où l'éditeur de personnage est ouvert */
UENUM(BlueprintType)
enum class ETrainerEditorMode : uint8
{
	/** Première fois (après le discours du Prof. Chen) : pas de bouton Retour, prénom obligatoire */
	Creation,
	/** Menu Apparence (Pokédex) : tout se change, Retour annule */
	Appearance,
	/** Armoire de la chambre : s'ouvre sur l'onglet Tenue */
	Wardrobe
};

/**
 *  Habille le personnage selon l'apparence de son joueur (SCRUM-66/79) et ouvre l'éditeur de personnage.
 *  L'apparence vit dans le PlayerState (sauvegardée, répliquée à tous les joueurs) ; ce composant ne fait
 *  que l'afficher : mesh de la base, morphs des curseurs, couleurs de peau et d'yeux.
 *  Changer d'apparence passe par RequestAppearance, validé par le serveur.
 */
UCLASS(ClassGroup=(Pokemon), meta=(BlueprintSpawnableComponent))
class UTrainerAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UTrainerAppearanceComponent();

	/** Catalogue. Vide = /Game/Characters/Player/DA_TrainerAppearance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TObjectPtr<UTrainerAppearanceData> AppearanceData;

	/**
	 *  Ouvre l'éditeur tout seul au début d'une partie tant que le joueur n'a pas créé son personnage.
	 *  À couper quand le discours du Prof. Chen l'ouvrira lui-même (événement de dialogue OpenCharacterCreator).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bAutoOpenEditor = true;

	/** Hauteur du studio de l'éditeur au-dessus du joueur (cm) : loin du décor et de sa lumière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	float StudioHeight = 300000.f;

	UFUNCTION(BlueprintPure, Category="Appearance")
	UTrainerAppearanceData* GetAppearanceData();

	/** Affiche une apparence (sans la sauvegarder) ; appelé quand le PlayerState change */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void ApplyAppearance(const FTrainerAppearance& Appearance);

	/** Demande au serveur de garder cette apparence */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void RequestAppearance(const FTrainerAppearance& Appearance);

	/** Demande au serveur de garder le prénom et la phrase d'accroche (carte dresseur) */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void RequestProfile(const FString& TrainerName, const FString& Catchphrase);

	/** Apparence du PlayerState du personnage (défaut si pas encore de PlayerState) */
	UFUNCTION(BlueprintPure, Category="Appearance")
	FTrainerAppearance GetCurrentAppearance() const;

	// ------------------------------------------------ Éditeur (côté joueur)

	/** Ouvre l'éditeur sur ce PC (studio + écran). Faux si déjà ouvert ou pas de catalogue */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	bool OpenEditor(ETrainerEditorMode Mode);

	/** Ferme l'éditeur ; bKeep = garder ce qui a été choisi (bouton Terminer) */
	void CloseEditor(bool bKeep, const FTrainerAppearance& Appearance, const FString& TrainerName, const FString& Catchphrase);

	UFUNCTION(BlueprintPure, Category="Appearance")
	bool IsEditorOpen() const { return Editor != nullptr; }

	/** Serveur : ouvre l'éditeur chez le joueur (événement de dialogue), après la fin de la conversation */
	UFUNCTION(Client, Reliable)
	void ClientOpenEditor(ETrainerEditorMode Mode);

	static constexpr int32 MaxNameLength = 12;
	static constexpr int32 MaxCatchphraseLength = 60;

	/** Prénom / phrase nettoyés (espaces, longueur) */
	static FString CleanText(const FString& Text, int32 MaxLength);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerSetAppearance(const FTrainerAppearance& Appearance);

	UFUNCTION(Server, Reliable)
	void ServerSetProfile(const FString& TrainerName, const FString& Catchphrase);

private:

	/** Le PlayerState arrive parfois après le personnage (réseau) : on réessaie quelques secondes */
	void TryApplyFromPlayerState();

	/** Début de partie : éditeur si le personnage n'a jamais été créé */
	void CheckFirstCreation();

	UFUNCTION()
	void HandleConversationEnded();

	USkeletalMeshComponent* GetTargetMesh() const;

	FTimerHandle RetryTimer;
	FTimerHandle FirstCreationTimer;
	int32 RetryCount = 0;

	/** Base affichée en ce moment (évite de recharger le mesh à chaque curseur) */
	FName AppliedBase;

	UPROPERTY(Transient)
	TObjectPtr<ACharacterStudio> Studio;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterCreatorWidget> Editor;

	/** Ouverture demandée pendant un dialogue : attend sa fin */
	TOptional<ETrainerEditorMode> PendingMode;
};
