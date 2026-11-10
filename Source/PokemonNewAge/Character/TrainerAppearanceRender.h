#pragma once

#include "CoreMinimal.h"
#include "Character/TrainerAppearance.h"

class AActor;
class UTexture2D;
class USkeletalMeshComponent;
class UTrainerAppearanceData;

/**
 *  Habille un Skeletal Mesh selon une apparence : mesh de la base, morphs des curseurs, couleurs de peau et d'yeux.
 *  Utilisé par le personnage en jeu (UTrainerAppearanceComponent) et par l'aperçu du studio de l'éditeur (ACharacterStudio).
 */
namespace TrainerAppearanceRender
{
	/** Remet les curseurs dans leurs bornes et retire ce que le catalogue ne connaît pas */
	FTrainerAppearance Sanitize(const UTrainerAppearanceData* Data, const FTrainerAppearance& Appearance);

	/**
	 *  Applique Appearance à Mesh et aux Skeletal Mesh de Owner qui le suivent (vêtements, cheveux en Leader Pose).
	 *  InOutAppliedBase évite de recharger le mesh quand seule une valeur change. Faux si rien n'a pu être appliqué.
	 */
	bool Apply(const UTrainerAppearanceData* Data, AActor* Owner, USkeletalMeshComponent* Mesh, const FTrainerAppearance& Appearance, FName& InOutAppliedBase);

	/** Texture d'un motif : id du catalogue (Data->Patterns) ou « Fichier:nom.png » (Saved/Motifs). Nul si introuvable */
	UTexture2D* ResolvePattern(const UTrainerAppearanceData* Data, FName Pattern);

	/** Images du joueur utilisables comme motifs : fichiers .png / .jpg de Saved/Motifs, en ids « Fichier:nom » */
	TArray<FName> ListUserPatterns();

	/** Dossier des images du joueur (Saved/Motifs), créé s'il manque */
	FString UserPatternFolder();

	/** Catalogue du jeu : /Game/Characters/Player/DA_TrainerAppearance (nul s'il n'existe pas encore) */
	UTrainerAppearanceData* LoadDefaultData();
}
