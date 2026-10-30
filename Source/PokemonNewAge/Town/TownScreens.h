#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

/**
 *  Petits outils communs aux écrans plein écran du Centre et de la Boutique (Boutique, PC de Léo) :
 *  le joueur ne bouge plus et le clavier / la manette pilotent l'écran, comme la boîte de dialogue.
 */
namespace TownScreens
{
	inline void OpenScreen(APlayerController* PlayerController, UUserWidget* Widget)
	{
		if (!PlayerController || !Widget)
		{
			return;
		}
		Widget->SetIsFocusable(true);
		Widget->AddToPlayerScreen(6);
		PlayerController->SetIgnoreMoveInput(true);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Widget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PlayerController->SetInputMode(InputMode);
	}

	inline void CloseScreen(APlayerController* PlayerController, UUserWidget* Widget)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
		if (PlayerController)
		{
			PlayerController->SetIgnoreMoveInput(false);
			PlayerController->SetInputMode(FInputModeGameOnly());
		}
	}
}
