#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InZOIImportLibrary.generated.h"

class UAnimSequence;
class USkeleton;
class USkeletalMesh;

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
};
