#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/TrainerAppearance.h"
#include "TrainerAppearanceComponent.generated.h"

class UTrainerAppearanceData;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
struct FTrainerBaseDef;

/**
 *  Habille le personnage selon l'apparence de son joueur (SCRUM-66/79).
 *  L'apparence vit dans le PlayerState (sauvegardée, répliquée à tous les joueurs) ; ce composant ne fait
 *  que l'afficher : mesh de la base, morphs des curseurs, couleurs de peau et d'yeux.
 *  Changer d'apparence (éditeur, armoire, menu Apparence) passe par RequestAppearance, validé par le serveur.
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

	UFUNCTION(BlueprintPure, Category="Appearance")
	UTrainerAppearanceData* GetAppearanceData();

	/** Affiche une apparence (sans la sauvegarder) : aperçu de l'éditeur, ou appelé quand le PlayerState change */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void ApplyAppearance(const FTrainerAppearance& InAppearance);

	/** Demande au serveur de garder cette apparence (bouton Valider de l'éditeur, de l'armoire...) */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void RequestAppearance(const FTrainerAppearance& Appearance);

	/** Remet les curseurs dans leurs bornes et retire ce que le catalogue ne connaît pas */
	FTrainerAppearance Sanitize(const FTrainerAppearance& Appearance);

	/** Apparence du PlayerState du personnage (défaut si pas encore de PlayerState) */
	UFUNCTION(BlueprintPure, Category="Appearance")
	FTrainerAppearance GetCurrentAppearance() const;

protected:

	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSetAppearance(const FTrainerAppearance& Appearance);

private:

	/** Le PlayerState arrive parfois après le personnage (réseau) : on réessaie quelques secondes */
	void TryApplyFromPlayerState();

	void ApplyMorphs(const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, USkeletalMeshComponent* Mesh);

	void ApplyColor(USkeletalMeshComponent* Mesh, const TArray<FName>& Slots, FName Parameter, const FLinearColor& Color);

	FTimerHandle RetryTimer;
	int32 RetryCount = 0;

	/** Base affichée en ce moment (évite de recharger le mesh à chaque curseur) */
	FName AppliedBase;
};
