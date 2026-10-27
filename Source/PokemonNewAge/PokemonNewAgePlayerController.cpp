// Copyright Epic Games, Inc. All Rights Reserved.


#include "PokemonNewAgePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "UI/GameMenuWidget.h"
#include "PokemonNewAge.h"
#include "Widgets/Input/SVirtualJoystick.h"

void APokemonNewAgePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// keep the camera from going under the ground or straight above the character
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = CameraPitchMin;
		PlayerCameraManager->ViewPitchMax = CameraPitchMax;
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogPokemonNewAge, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void APokemonNewAgePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// Touche du menu en jeu par défaut (Tab, Start) si aucune n'est réglée dans le Blueprint
			if (!MenuAction)
			{
				MenuAction = NewObject<UInputAction>(this, TEXT("IA_Menu_Default"));
				DefaultMenuContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Menu_Default"));
				DefaultMenuContext->MapKey(MenuAction, EKeys::Tab);
				DefaultMenuContext->MapKey(MenuAction, EKeys::Gamepad_Special_Right);
				Subsystem->AddMappingContext(DefaultMenuContext, 1);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	// Menu en jeu
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MenuAction)
		{
			EnhancedInputComponent->BindAction(MenuAction, ETriggerEvent::Started, this, &APokemonNewAgePlayerController::ToggleGameMenu);
		}
	}
}

void APokemonNewAgePlayerController::ToggleGameMenu()
{
	if (IsGameMenuOpen())
	{
		CloseGameMenu();
	}
	else
	{
		OpenGameMenu();
	}
}

void APokemonNewAgePlayerController::OpenGameMenu()
{
	if (IsGameMenuOpen() || !IsLocalPlayerController())
	{
		return;
	}

	TSubclassOf<UGameMenuWidget> MenuClass = GameMenuClass ? GameMenuClass : TSubclassOf<UGameMenuWidget>(UGameMenuWidget::StaticClass());
	GameMenu = CreateWidget<UGameMenuWidget>(this, MenuClass);
	if (!GameMenu)
	{
		UE_LOG(LogPokemonNewAge, Error, TEXT("Could not spawn the game menu widget."));
		return;
	}
	GameMenu->AddToPlayerScreen(10);

	// Le monde se fige, la souris apparaît, le clavier et la manette pilotent le menu
	SetPause(true);
	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	GameMenu->FocusDefaultEntry();
}

void APokemonNewAgePlayerController::CloseGameMenu()
{
	if (!IsGameMenuOpen())
	{
		return;
	}

	GameMenu->RemoveFromParent();
	GameMenu = nullptr;

	SetPause(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

bool APokemonNewAgePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
