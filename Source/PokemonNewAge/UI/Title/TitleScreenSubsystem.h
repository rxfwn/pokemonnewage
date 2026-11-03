#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TitleScreenSubsystem.generated.h"

class ACameraActor;
class APokemonNewAgePlayerController;
class UAudioComponent;
class UTitleScreenWidget;

/**
 *  Écran de titre (SCRUM-76). Il s'affiche par-dessus la map de jeu elle-même (L_Kanto), avec l'image de fond
 *  de L'amie ; « Continuer » ou « Nouvelle partie » l'efface en fondu sur le jeu. Pas de map à part : le monde
 *  ouvert charge pendant qu'on est sur le titre.
 *
 *  Sans image de fond, c'est la map en 3D : la caméra tourne lentement au-dessus du joueur (ou prend la vue d'un
 *  acteur tagué TitleCamera), puis vole jusque derrière lui. Réglages : Paramètres du projet > Jeu > Écran de titre.
 *  Voir docs/ECRAN_TITRE.md.
 */
UCLASS()
class UTitleScreenSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Vrai tant que l'écran de titre est là (le jeu peut s'en servir pour ne rien lancer dessous) */
	UFUNCTION(BlueprintPure, Category="Title Screen")
	bool IsTitleScreenActive() const { return Stage != EStage::Off; }

	/** Appelés par le widget */
	void Continue();
	void StartNewGame();
	void OpenOptions();
	void QuitGame();

	/** Petit bruit (déplacement de la sélection ou validation) */
	void PlaySound(bool bConfirm) const;

	// UTickableWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:

	enum class EStage : uint8
	{
		/** Pas d'écran de titre */
		Off,
		/** On attend le PlayerController et son personnage */
		Waiting,
		/** L'écran est affiché */
		Shown,
		/** La caméra vole jusqu'au joueur */
		Leaving
	};

	/** Vrai si cette map, ce mode et ce lancement veulent l'écran de titre */
	bool ShouldShow(const UWorld& World) const;

	APokemonNewAgePlayerController* FindLocalController() const;

	void Show(APokemonNewAgePlayerController* Controller);

	/** Souris visible, clavier et manette sur l'écran de titre */
	void FocusTitle();

	/** Fait voler la caméra jusque derrière le joueur et rend les commandes */
	void Leave();
	void Finish();

	/** Place la caméra du titre (tour autour du joueur, ou acteur TitleCamera) */
	void UpdateCamera(float DeltaTime);

	EStage Stage = EStage::Off;

	/** Temps réel écoulé depuis le début de l'étape (le jeu peut être en pause sous les Options) */
	float StageTime = 0.f;
	float OrbitAngle = 0.f;
	float SmoothedZ = 0.f;
	bool bHasSmoothedZ = false;
	bool bOptionsOpen = false;

	/** Vue 3D de la map derrière le titre (pas d'image de fond) */
	bool bUseCamera = true;

	/** Durée du passage au jeu */
	float LeaveTime = 1.f;

	TWeakObjectPtr<APokemonNewAgePlayerController> Controller;

	/** Acteur tagué TitleCamera dans la map, s'il y en a un */
	TWeakObjectPtr<AActor> FixedView;
	FTransform FixedViewTransform;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UTitleScreenWidget> Widget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;
};
