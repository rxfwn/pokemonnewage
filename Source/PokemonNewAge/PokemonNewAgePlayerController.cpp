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
#include "UI/Battle/BattleHUDWidget.h"
#include "UI/Pokedex/PokedexDevice.h"
#include "UI/Pokedex/PokedexWidgets.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Battle/BattleComponent.h"
#include "Battle/BattleSubsystem.h"
#include "Core/PokemonPlayerState.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PokemonNewAge.h"
#include "Widgets/Input/SVirtualJoystick.h"

APokemonNewAgePlayerController::APokemonNewAgePlayerController()
{
	BattleComponent = CreateDefaultSubobject<UBattleComponent>(TEXT("BattleComponent"));
	PokedexDeviceClass = APokedexDevice::StaticClass();
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

	if (IsLocalPlayerController())
	{
		ApplyGameOptions();

		// Écran de combat : il apparaît à chaque début de combat
		BattleComponent->OnBattleStarted.AddDynamic(this, &APokemonNewAgePlayerController::HandleBattleStarted);

		// Reprendre la partie sauvegardée sur ce PC (SCRUM-52)
		if (bLoadSaveOnStart)
		{
			if (const UPokemonSaveGame* Save = UPokemonSaveLibrary::LoadSaveGame())
			{
				ServerLoadSave(Save->Player);
				UE_LOG(LogPokemonNewAge, Log, TEXT("Sauvegarde chargee (%s)"), *Save->SavedAt.ToString());
			}
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
	if (IsGameMenuOpen() || !IsLocalPlayerController() || BattleComponent->IsInBattle() || BattleHUD)
	{
		return;
	}

	// Le Pokédex 3D (SCRUM-64)
	if (bUsePokedex3D && PokedexDeviceClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APokedexDevice* Device = GetWorld()->SpawnActor<APokedexDevice>(PokedexDeviceClass, FTransform::Identity, Params);
		if (Device && Device->InitializeFor(this))
		{
			PokedexDevice = Device;
			// Pause en solo seulement : en ligne, le monde continue pour les autres (SCRUM-68)
			if (GetNetMode() == NM_Standalone)
			{
				SetPause(true);
			}
			SetShowMouseCursor(true);
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(Device->GetInputWidget()->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			Device->Open();
			return;
		}
		if (Device)
		{
			Device->Destroy();
		}
	}

	TSubclassOf<UGameMenuWidget> MenuClass = GameMenuClass ? GameMenuClass : TSubclassOf<UGameMenuWidget>(UGameMenuWidget::StaticClass());
	GameMenu = CreateWidget<UGameMenuWidget>(this, MenuClass);
	if (!GameMenu)
	{
		UE_LOG(LogPokemonNewAge, Error, TEXT("Could not spawn the game menu widget."));
		return;
	}
	GameMenu->AddToPlayerScreen(10);

	// Le monde se fige (en solo), la souris apparaît, le clavier et la manette pilotent le menu
	if (GetNetMode() == NM_Standalone)
	{
		SetPause(true);
	}
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

	// Le Pokédex se referme en animation, puis appelle OnPokedexClosed
	if (PokedexDevice)
	{
		PokedexDevice->Close();
		return;
	}

	GameMenu->RemoveFromParent();
	GameMenu = nullptr;

	SetPause(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

void APokemonNewAgePlayerController::OnPokedexClosed(APokedexDevice* Device)
{
	if (Device != PokedexDevice)
	{
		return;
	}
	PokedexDevice = nullptr;
	SetPause(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

void APokemonNewAgePlayerController::HandleBattleStarted(const FBattleSnapshot& Snapshot)
{
	// Un combat peut commencer menu ouvert (dresseur qui nous voit en ligne) : on le ferme sans animation
	if (PokedexDevice)
	{
		PokedexDevice->Close(true);
	}
	else if (GameMenu)
	{
		CloseGameMenu();
	}

	if (BattleHUD)
	{
		return;
	}
	TSubclassOf<UBattleHUDWidget> HUDClass = BattleHUDClass ? BattleHUDClass : TSubclassOf<UBattleHUDWidget>(UBattleHUDWidget::StaticClass());
	BattleHUD = CreateWidget<UBattleHUDWidget>(this, HUDClass);
	if (!BattleHUD)
	{
		return;
	}
	// L'écran de combat remplace les messages de test du combat
	BattleComponent->bPrintEventsToScreen = false;
	BattleHUD->AddToPlayerScreen(5);
	BattleHUD->Begin(BattleComponent, Snapshot);

	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(BattleHUD->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void APokemonNewAgePlayerController::OnBattleHUDClosed()
{
	BattleHUD = nullptr;
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

void APokemonNewAgePlayerController::ApplyGameOptions()
{
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	CameraSensitivity = FMath::Clamp(Options->CameraSensitivity, 0.1f, 5.f);
	bInvertCameraY = Options->bInvertCameraY;
	UPokemonSaveLibrary::ApplyOptions(this, Options);
}

void APokemonNewAgePlayerController::ServerSwapPartyPokemon_Implementation(int32 IndexA, int32 IndexB)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && !BattleComponent->IsInBattle())
	{
		State->SwapPartyPokemon(IndexA, IndexB);
	}
}

void APokemonNewAgePlayerController::ServerUseFieldItem_Implementation(FName Item, int32 PartyIndex)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && !BattleComponent->IsInBattle())
	{
		State->UseItemOnPokemon(UPokemonDataSubsystem::GetDatabase(this), Item, PartyIndex);
	}
}

void APokemonNewAgePlayerController::ServerLoadSave_Implementation(const FPlayerSaveData& Data)
{
	APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>();
	if (!State || BattleComponent->IsInBattle())
	{
		return;
	}
	State->ApplySaveData(Data);

	if (Data.bHasTransform)
	{
		bHasPendingTransform = true;
		PendingLocation = Data.PlayerLocation;
		PendingRotation = Data.PlayerRotation;
		ApplyPendingTransform();
	}
}

void APokemonNewAgePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ApplyPendingTransform();
}

void APokemonNewAgePlayerController::ApplyPendingTransform()
{
	ACharacter* PlayerCharacter = Cast<ACharacter>(GetPawn());
	if (!bHasPendingTransform || !PlayerCharacter)
	{
		return;
	}
	bHasPendingTransform = false;

	// Un peu plus haut, et sans gravité le temps que le terrain (World Partition) se charge autour du joueur
	PlayerCharacter->TeleportTo(PendingLocation + FVector(0.f, 0.f, 50.f), FRotator(0.f, PendingRotation.Yaw, 0.f));
	SetControlRotation(FRotator(0.f, PendingRotation.Yaw, 0.f));
	if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
	{
		Movement->DisableMovement();
		TWeakObjectPtr<UCharacterMovementComponent> WeakMovement = Movement;
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, [WeakMovement]()
		{
			if (UCharacterMovementComponent* M = WeakMovement.Get())
			{
				M->SetMovementMode(MOVE_Falling);
			}
		}, 1.5f, false);
	}
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

void APokemonNewAgePlayerController::Sauvegarder()
{
	PrintDebug(UPokemonSaveLibrary::SaveGame(this) ? TEXT("Partie sauvegardee") : TEXT("Echec de la sauvegarde"));
}

void APokemonNewAgePlayerController::NouvellePartie()
{
	UPokemonSaveLibrary::DeleteSaveGame();
	PrintDebug(TEXT("Sauvegarde effacee : relance le jeu pour une nouvelle partie"));
}
