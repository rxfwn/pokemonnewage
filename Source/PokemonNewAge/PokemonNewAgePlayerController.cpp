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
#include "Battle/BattleComponent.h"
#include "Battle/BattleSubsystem.h"
#include "Capture/CaptureComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PokemonNewAge.h"
#include "Widgets/Input/SVirtualJoystick.h"

APokemonNewAgePlayerController::APokemonNewAgePlayerController()
{
	CaptureComponent = CreateDefaultSubobject<UCaptureComponent>(TEXT("CaptureComponent"));
	BattleComponent = CreateDefaultSubobject<UBattleComponent>(TEXT("BattleComponent"));
}

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

// ---------------------------------------------------------------- Commandes de test du combat (console)

namespace
{
	void PrintDebug(const FString& Message)
	{
		UE_LOG(LogPokemonNewAge, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Message);
		}
	}
}

void APokemonNewAgePlayerController::CombatSauvage(FName Species, int32 Level)
{
	UBattleSubsystem* Battles = GetWorld()->GetSubsystem<UBattleSubsystem>();
	if (!HasAuthority() || !Battles || !Battles->StartWildBattle(this, Species, FMath::Max(Level, 1)))
	{
		PrintDebug(TEXT("Combat impossible (pas d'equipe en forme ? espece inconnue ? deja en combat ?)"));
	}
}

void APokemonNewAgePlayerController::CombatRencontre(FName EncounterTable)
{
	UBattleSubsystem* Battles = GetWorld()->GetSubsystem<UBattleSubsystem>();
	if (!HasAuthority() || !Battles || !Battles->StartWildEncounter(this, EncounterTable))
	{
		PrintDebug(TEXT("Rencontre impossible (table inconnue ou pas d'equipe en forme)"));
	}
}

void APokemonNewAgePlayerController::CombatDresseur(FName TrainerGroup)
{
	UBattleSubsystem* Battles = GetWorld()->GetSubsystem<UBattleSubsystem>();
	if (!HasAuthority() || !Battles || !Battles->StartTrainerBattle(this, TrainerGroup))
	{
		PrintDebug(TEXT("Combat impossible (dresseur inconnu, rival sans drapeau Starter_, ou pas d'equipe en forme)"));
	}
}

void APokemonNewAgePlayerController::Attaque(int32 Slot)
{
	BattleComponent->SubmitAction(FBattleAction::MakeMove(Slot - 1));
}

void APokemonNewAgePlayerController::Changer(int32 PartySlot)
{
	if (BattleComponent->GetSnapshot().Phase == EBattlePhase::WaitingForReplacement)
	{
		BattleComponent->ChooseReplacement(PartySlot - 1);
	}
	else
	{
		BattleComponent->SubmitAction(FBattleAction::MakeSwitch(PartySlot - 1));
	}
}

void APokemonNewAgePlayerController::Objet(FName Item, int32 PartySlot)
{
	BattleComponent->SubmitAction(FBattleAction::MakeItem(Item, PartySlot - 1));
}

void APokemonNewAgePlayerController::Fuite()
{
	if (BattleComponent->GetSnapshot().Phase == EBattlePhase::WaitingForReplacement)
	{
		BattleComponent->RunInsteadOfReplacement();
	}
	else
	{
		BattleComponent->SubmitAction(FBattleAction::MakeRun());
	}
}

void APokemonNewAgePlayerController::Oublier(int32 Slot)
{
	BattleComponent->ResolveMoveLearn(Slot - 1);
}

void APokemonNewAgePlayerController::Evoluer(int32 PartySlot, int32 bAccept)
{
	BattleComponent->ConfirmEvolution(PartySlot - 1, bAccept != 0);
}

void APokemonNewAgePlayerController::Surnom(const FString& Nickname)
{
	CaptureComponent->SetCaughtNickname(Nickname);
}

void APokemonNewAgePlayerController::VoirPC()
{
	const APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>();
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	if (!State)
	{
		return;
	}
	PrintDebug(FString::Printf(TEXT("PC de Leo : %d Pokemon"), State->StoragePC.Num()));
	for (int32 Index = 0; Index < State->StoragePC.Num(); ++Index)
	{
		const FPokemonInstance& Pokemon = State->StoragePC[Index];
		PrintDebug(FString::Printf(TEXT("%d. (boite %d) %s N.%d"), Index + 1, Index / UPokemonStorage::BoxSize + 1,
			*UPokemonLibrary::GetDisplayName(Database, Pokemon).ToString(), Pokemon.Level));
	}
}

void APokemonNewAgePlayerController::Deposer(int32 PartySlot)
{
	CaptureComponent->DepositPokemon(PartySlot - 1);
}

void APokemonNewAgePlayerController::Retirer(int32 StorageSlot)
{
	CaptureComponent->WithdrawPokemon(StorageSlot - 1);
}

void APokemonNewAgePlayerController::Echanger(int32 PartySlot, int32 StorageSlot)
{
	CaptureComponent->SwapWithStorage(PartySlot - 1, StorageSlot - 1);
}

void APokemonNewAgePlayerController::Relacher(int32 StorageSlot)
{
	CaptureComponent->ReleasePokemon(StorageSlot - 1);
}

void APokemonNewAgePlayerController::Rencontres(int32 bEnabled)
{
	CaptureComponent->SetEncountersEnabled(bEnabled != 0);
	PrintDebug(bEnabled != 0 ? TEXT("Rencontres dans les herbes : oui") : TEXT("Rencontres dans les herbes : non"));
}

void APokemonNewAgePlayerController::DonnerPokemon(FName Species, int32 Level)
{
	APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>();
	FRandomStream Stream(FMath::Rand());
	FPokemonInstance Pokemon;
	if (HasAuthority() && State && UPokemonLibrary::CreatePokemon(UPokemonDataSubsystem::GetDatabase(this), Species, FMath::Max(Level, 1), Stream, Pokemon))
	{
		Pokemon.Ball = TEXT("PokeBall");
		State->AddPokemon(Pokemon);
		Equipe();
	}
	else
	{
		PrintDebug(TEXT("DonnerPokemon : espece inconnue ou PlayerState absent (GameMode du jeu ?)"));
	}
}

void APokemonNewAgePlayerController::DonnerObjet(FName Item, int32 Count)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && HasAuthority())
	{
		State->AddItem(Item, Count);
		PrintDebug(FString::Printf(TEXT("%s x%d"), *Item.ToString(), State->GetItemCount(Item)));
	}
}

void APokemonNewAgePlayerController::Soigner()
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && HasAuthority())
	{
		State->HealParty();
		Equipe();
	}
}

void APokemonNewAgePlayerController::Equipe()
{
	const APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>();
	const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
	if (!State)
	{
		return;
	}
	for (int32 Index = 0; Index < State->Party.Num(); ++Index)
	{
		const FPokemonInstance& Pokemon = State->Party[Index];
		FString Moves;
		for (const FPokemonMoveSlot& Slot : Pokemon.Moves)
		{
			Moves += FString::Printf(TEXT(" %s %d/%d"), *Slot.Move.ToString(), Slot.CurrentPP, Slot.MaxPP);
		}
		PrintDebug(FString::Printf(TEXT("%d. %s N.%d PV %d/%d XP %d -%s"), Index + 1,
			*UPokemonLibrary::GetDisplayName(Database, Pokemon).ToString(), Pokemon.Level, Pokemon.CurrentHP, Pokemon.GetMaxHP(),
			Pokemon.Experience, *Moves));
	}
}
