#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InZOIImportLibrary.generated.h"

class UAnimSequence;
class USkeleton;
class USkeletalMesh;
class UMaterialExpression;

/**
 *  Outils appelés par Tools/InZOI/import_inzoi.py (éditeur seulement). Import des animations .ueanim de l'exercice
 *  inZOI SANS la fenêtre « Animation Import Options » du plugin UEFormat : le squelette est donné directement, ce qui
 *  permet de tout importer avec import_inzoi.bat (sans éditeur ouvert). Il faut le plugin UEFormat dans Plugins/UEFormat ;
 *  sans lui (ou dans le jeu), les fonctions ne font rien.
 */
UCLASS()
class UInZOIImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Vrai si le plugin UEFormat était là à la compilation (import des .ueanim possible) */
	UFUNCTION(BlueprintCallable, Category="InZOI")
	static bool CanImportUEAnim();

	/**
	 *  Crée l'animation AssetPath (/Game/.../ANIM_X) à partir du fichier .ueanim, sur ce squelette. Ne l'enregistre pas
	 *  (le script appelle save_loaded_asset). Nul si le plugin manque ou si le fichier est illisible.
	 */
	UFUNCTION(BlueprintCallable, Category="InZOI")
	static UAnimSequence* ImportUEAnim(const FString& Filename, USkeleton* Skeleton, const FString& AssetPath);

	/**
	 *  Prépare une coupe de cheveux pour les zones « Mèches » et « Mèches devant » du matériau M_InZOI_Hair : pour chaque
	 *  carte (îlot de sommets connectés), la racine est le sommet le plus proche du centre du crâne ; la couleur de sommet
	 *  reçoit R = 1 si cette racine est sur le devant (frange, mèches qui encadrent le visage), G = numéro de mèche
	 *  aléatoire (le même sur toute la carte), B = 0 (marque « préparée » : sans elle, le matériau garde l'ancien calcul).
	 *  FrontAxis : devant de la tête dans le repère du mesh. Renvoie le nombre de cartes devant / total (« 12/340 »),
	 *  vide si le mesh n'a pas pu être modifié. Éditeur seulement.
	 */
	UFUNCTION(BlueprintCallable, Category="InZOI")
	static FString BakeHairStrands(USkeletalMesh* Mesh, FVector FrontAxis, float FrontAngleDegrees = 70.f, float MinHeightAboveCenter = -6.f,
		float MinForward = 5.f);

	/** Nombre de jeux de coordonnées de texture (UV) du mesh de LOD 0 (diagnostic de l'import des coiffures) */
	UFUNCTION(BlueprintCallable, Category="InZOI")
	static int32 CountUVChannels(USkeletalMesh* Mesh);

	/** Identifiant d'une entrée ou sortie de fonction de matériau (protégé en Python) : pour refaire une fonction
	 *  compatible avec les appels existants (MF_MaskEdge d'inZOI). Faux si l'expression n'est ni l'une ni l'autre. */
	UFUNCTION(BlueprintCallable, Category="InZOI")
	static bool SetFunctionIOId(UMaterialExpression* Expression, const FString& GuidText);
};
