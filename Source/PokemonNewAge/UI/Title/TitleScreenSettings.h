#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TitleScreenSettings.generated.h"

class USoundBase;
class UTexture2D;

/**
 *  Réglages de l'écran de titre (SCRUM-76), dans Modifier > Paramètres du projet > Jeu > Écran de titre.
 *  Enregistrés dans Config/DefaultGame.ini. Voir docs/ECRAN_TITRE.md.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Écran de titre"))
class UTitleScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UTitleScreenSettings();

	/** Montrer l'écran de titre au lancement */
	UPROPERTY(config, EditAnywhere, Category="Général")
	bool bEnabled = true;

	/** Le montrer aussi quand on clique sur Jouer dans l'éditeur (sinon : seulement en jeu autonome et en jeu final) */
	UPROPERTY(config, EditAnywhere, Category="Général")
	bool bShowInEditor = true;

	/** Maps qui ont l'écran de titre (nom court, ex. L_Kanto). Vide = toutes les maps */
	UPROPERTY(config, EditAnywhere, Category="Général")
	TArray<FString> Maps;

	/**
	 *  Images de fond (maquette de L'amie : 1920 × 1080, recadrées pour remplir l'écran). Plusieurs = fondu de l'une
	 *  à l'autre. Vide ou pas importée = vue 3D de la map qui tourne autour du joueur.
	 */
	UPROPERTY(config, EditAnywhere, Category="Apparence")
	TArray<TSoftObjectPtr<UTexture2D>> Backgrounds;

	/** Temps passé sur chaque image de fond quand il y en a plusieurs, en secondes */
	UPROPERTY(config, EditAnywhere, Category="Apparence", meta=(ClampMin=3, Units="s"))
	float BackgroundDuration = 15.f;

	/** Logo en haut à gauche (PNG à fond transparent). Vide ou pas importé = titre écrit */
	UPROPERTY(config, EditAnywhere, Category="Apparence")
	TSoftObjectPtr<UTexture2D> Logo;

	/** Titre écrit à la place du logo */
	UPROPERTY(config, EditAnywhere, Category="Apparence")
	FText Title;

	UPROPERTY(config, EditAnywhere, Category="Apparence")
	FText Subtitle;

	/** Les deux lignes en bas à droite */
	UPROPERTY(config, EditAnywhere, Category="Apparence")
	FText VersionText;

	UPROPERTY(config, EditAnywhere, Category="Apparence")
	FText CopyrightText;

	/** Musique de l'écran de titre (mettre la classe de son SC_Music dessus pour le réglage du volume) */
	UPROPERTY(config, EditAnywhere, Category="Son")
	TSoftObjectPtr<USoundBase> Music;

	/** Bruit quand la sélection bouge et quand on valide (vide = le bip du Pokédex s'il est importé) */
	UPROPERTY(config, EditAnywhere, Category="Son")
	TSoftObjectPtr<USoundBase> NavSound;

	UPROPERTY(config, EditAnywhere, Category="Son")
	TSoftObjectPtr<USoundBase> ConfirmSound;

	/**
	 *  Caméra : sans acteur tagué TitleCamera dans la map, elle tourne lentement autour du joueur.
	 *  Distance horizontale au joueur, en cm.
	 */
	UPROPERTY(config, EditAnywhere, Category="Caméra", meta=(ClampMin=300, Units="cm"))
	float OrbitRadius = 2800.f;

	/** Hauteur de la caméra au-dessus du joueur, en cm */
	UPROPERTY(config, EditAnywhere, Category="Caméra", meta=(Units="cm"))
	float OrbitHeight = 1100.f;

	/** Vitesse de rotation, en degrés par seconde */
	UPROPERTY(config, EditAnywhere, Category="Caméra", meta=(ClampMin=0, ClampMax=30))
	float OrbitSpeed = 3.f;

	/** Jamais plus près du sol que ça, en cm */
	UPROPERTY(config, EditAnywhere, Category="Caméra", meta=(ClampMin=50, Units="cm"))
	float MinGroundClearance = 400.f;

	UPROPERTY(config, EditAnywhere, Category="Caméra", meta=(ClampMin=20, ClampMax=120, Units="Degrees"))
	float FieldOfView = 55.f;

	/** Durée du passage au jeu (vol de la caméra, ou fondu de l'image de fond), en secondes */
	UPROPERTY(config, EditAnywhere, Category="Caméra", meta=(ClampMin=0, ClampMax=10, Units="s"))
	float BlendTime = 2.5f;
};
