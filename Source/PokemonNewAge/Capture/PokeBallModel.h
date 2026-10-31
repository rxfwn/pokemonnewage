#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Modèles 3D des Poké Balls de L'amie (SCRUM-75, docs/CAPTURE.md) : deux moitiés par Ball, importées par
 *  Tools/Capture/import_pokeball_unreal.py dans /Game/Capture/Balls/<Ball>/SM_<Ball>_Haut et SM_<Ball>_Bas
 *  (<Ball> = ligne de DT_Items : PokeBall, GreatBall...). Bouton vers +X, pivot au centre de la Ball.
 *
 *  Utilisé par le lancer (APokeBallThrow) et la séquence d'évolution (AEvolutionScene). Montage attendu :
 *  Bottom (moitié basse) > Hinge (charnière) > Top (moitié haute). La charnière est placée à l'arrière de la Ball,
 *  sur l'équateur : la tourner en tangage (pitch) ouvre le couvercle.
 */
namespace PokeBallModel
{
	/** Dossier des Balls importées */
	extern const TCHAR* Folder;

	/** Ball utilisée quand celle demandée n'a pas de modèle (ou n'est pas connue) */
	extern const FName DefaultBall;

	/** Paramètres du matériau M_Ball : lueur émissive (0 = éteinte) et sa couleur */
	extern const FName GlowParam;
	extern const FName GlowColorParam;

	struct FParts
	{
		UStaticMesh* Top = nullptr;
		UStaticMesh* Bottom = nullptr;
		FName Ball;

		bool IsValid() const { return Top && Bottom; }
	};

	/** Charge les deux moitiés d'une Ball ; à défaut celles de la Poké Ball. Invalide si rien n'est importé */
	FParts Load(FName Ball);

	/**
	 *  Pose la Ball sur les trois composants (moitiés, charnière placée, couvercle fermé) et crée une instance
	 *  dynamique du matériau sur chaque moitié (pour la lueur). Renvoie le diamètre du modèle (cm), 0 si absent.
	 */
	float Apply(FName Ball, UStaticMeshComponent* Bottom, USceneComponent* Hinge, UStaticMeshComponent* Top);

	/** Ouvre le couvercle d'un angle (degrés, 0 = fermé) */
	void SetOpenAngle(USceneComponent* Hinge, float Degrees);

	/** Allume les deux moitiés (Amount 0 = éteint), si leur matériau a les paramètres de M_Ball */
	void SetGlow(UStaticMeshComponent* Bottom, UStaticMeshComponent* Top, float Amount, const FLinearColor& Color);
}
