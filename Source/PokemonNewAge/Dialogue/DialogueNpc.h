#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "DialogueNpc.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 *  PNJ, panneau ou objet à qui l'on parle avec la touche d'action (SCRUM-26).
 *  On le pose dans la map, on choisit son identifiant (ligne de DT_Npcs) dans la liste
 *  « Npc Id » du panneau Détails, et c'est tout : la conversation est choisie d'après
 *  l'avancement du joueur (docs/DIALOGUES.md).
 *
 *  Tant qu'il n'y a pas de modèle, un cylindre le représente. Pour un vrai personnage,
 *  régler « Skeletal Mesh » et vider « Placeholder Mesh » (ou faire un Blueprint enfant).
 */
UCLASS()
class ADialogueNpc : public AActor, public IInteractable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* Capsule;

	/** Cylindre de blockout, à remplacer par le modèle */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* PlaceholderMesh;

	/** Modèle du personnage (vide par défaut) */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* SkeletalMesh;

	/** Étiquette visible seulement dans l'éditeur, pour retrouver les PNJ dans la map */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* EditorLabel;

public:

	ADialogueNpc();

	/** Ligne de DT_Npcs : ProfOak, PalletTown_Sign, OaksLab_PokeBall_Bulbasaur... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(GetOptions="GetNpcIdOptions"))
	FName NpcId;

	/** Se tourne vers le joueur qui lui parle (pas pour un panneau ou un objet) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	bool bFacePlayer = true;

	/** Touche d'action : lance la conversation du joueur avec ce PNJ */
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	/** Liste déroulante de « Npc Id » (lit DT_Npcs) */
	UFUNCTION()
	TArray<FString> GetNpcIdOptions() const;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
};
