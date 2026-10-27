// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PokemonNewAgePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UInputAction;
class UGameMenuWidget;
class UBattleComponent;

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

	APokemonNewAgePlayerController();

	/** Combats de ce joueur : envoie ses choix au serveur et reçoit les événements (docs/COMBAT.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Battle")
	TObjectPtr<UBattleComponent> BattleComponent;

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

	// Commandes de test du combat, à taper dans la console (touche ²) tant que l'UI n'existe pas.
	// Voir docs/COMBAT.md. Les numéros commencent à 1 comme à l'écran.

	/** CombatSauvage Pidgey 3 */
	UFUNCTION(Exec)
	void CombatSauvage(FName Species, int32 Level);

	/** CombatRencontre Route1_Walk */
	UFUNCTION(Exec)
	void CombatRencontre(FName EncounterTable);

	/** CombatDresseur Brock (ou RivalLab) */
	UFUNCTION(Exec)
	void CombatDresseur(FName TrainerGroup);

	/** Attaque 1 à 4 */
	UFUNCTION(Exec)
	void Attaque(int32 Slot);

	/** Changer 1 à 6 (aussi pour envoyer un Pokémon après un K.O.) */
	UFUNCTION(Exec)
	void Changer(int32 PartySlot);

	/** Objet PokeBall, Objet Potion 1 */
	UFUNCTION(Exec)
	void Objet(FName Item, int32 PartySlot = 1);

	UFUNCTION(Exec)
	void Fuite();

	/** 5e attaque : Oublier 1 à 4, Oublier 0 pour ne pas l'apprendre */
	UFUNCTION(Exec)
	void Oublier(int32 Slot);

	/** Évolution après le combat : Evoluer 1 (Evoluer 1 0 pour l'annuler) */
	UFUNCTION(Exec)
	void Evoluer(int32 PartySlot, int32 bAccept = 1);

	/** DonnerPokemon Bulbasaur 5 */
	UFUNCTION(Exec)
	void DonnerPokemon(FName Species, int32 Level);

	/** DonnerObjet PokeBall 10 */
	UFUNCTION(Exec)
	void DonnerObjet(FName Item, int32 Count = 1);

	/** Soigne l'équipe */
	UFUNCTION(Exec)
	void Soigner();

	/** Affiche l'équipe dans le journal et à l'écran */
	UFUNCTION(Exec)
	void Equipe();

};
