#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InZOIImportLibrary.generated.h"

class UAnimSequence;
class USkeleton;

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
};
