// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokemonNewAgePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UInputAction;
class UGameMenuWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class APokemonNewAgePlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Lowest camera pitch, looking down at the character (degrees) */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = -89, ClampMax = 0, Units = "Degrees"))
	float CameraPitchMin = -60.f;

	/** Highest camera pitch, looking up from behind the character (degrees) */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 89, Units = "Degrees"))
	float CameraPitchMax = 30.f;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Touche du menu en jeu (Tab, Start). Créée par le code si elle n'est pas réglée dans le Blueprint */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MenuAction;

	/** Contexte créé par le code pour la touche du menu par défaut */
	UPROPERTY(Transient)
	UInputMappingContext* DefaultMenuContext;

	/** Menu à afficher (un Widget Blueprint enfant de GameMenuWidget pour l'habiller) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UGameMenuWidget> GameMenuClass;

	/** Menu ouvert, sinon nul */
	UPROPERTY(Transient)
	TObjectPtr<UGameMenuWidget> GameMenu;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

public:

	/** Ouvre le menu en jeu s'il est fermé, sinon le ferme */
	UFUNCTION(BlueprintCallable, Category="UI")
	void ToggleGameMenu();

	/** Ouvre le menu en jeu et met le jeu en pause */
	UFUNCTION(BlueprintCallable, Category="UI")
	void OpenGameMenu();

	/** Ferme le menu en jeu et relance le jeu */
	UFUNCTION(BlueprintCallable, Category="UI")
	void CloseGameMenu();

	/** Vrai si le menu en jeu est ouvert */
	UFUNCTION(BlueprintPure, Category="UI")
	bool IsGameMenuOpen() const { return GameMenu != nullptr; }

};
