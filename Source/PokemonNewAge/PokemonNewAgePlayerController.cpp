// Copyright Epic Games, Inc. All Rights Reserved.


#include "PokemonNewAgePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PokemonNewAgeCharacter.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "UI/GameMenuWidget.h"
#include "UI/Battle/BattleHUDWidget.h"
#include "UI/Pokedex/PokedexDevice.h"
#include "UI/Pokedex/PokedexWidgets.h"
#include "UI/Pokedex/PokedexScreenWidget.h"
#include "UI/ShortcutBarWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Battle/BattleComponent.h"
#include "Battle/BattleSubsystem.h"
#include "Capture/CaptureComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueComponent.h"
#include "Town/PokemonCenterComponent.h"
#include "Town/ShopComponent.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/InputSettings.h"
#include "PokemonNewAge.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace PokemonShortcuts
{
	/** Raccourcis du menu PC classique : onglet, commande (KeyBindings), touche d'origine */
	struct FShortcut { EPokedexMenu Menu; const TCHAR* Id; const TCHAR* Key; };
	const FShortcut Shortcuts[] = {
		{ EPokedexMenu::Pokemon, TEXT("Party"), TEXT("R") },
		{ EPokedexMenu::Pokedex, TEXT("Pokedex"), TEXT("P") },
		{ EPokedexMenu::Bag, TEXT("Bag"), TEXT("B") },
		{ EPokedexMenu::Trainer, TEXT("Trainer"), TEXT("C") },
		{ EPokedexMenu::Map, TEXT("Map"), TEXT("M") },
		{ EPokedexMenu::Save, TEXT("Save"), TEXT("F5") },
		{ EPokedexMenu::Online, TEXT("Online"), TEXT("L") },
		{ EPokedexMenu::Options, TEXT("Options"), TEXT("O") },
	};
}

APokemonNewAgePlayerController::APokemonNewAgePlayerController()
{
	CaptureComponent = CreateDefaultSubobject<UCaptureComponent>(TEXT("CaptureComponent"));
	BattleComponent = CreateDefaultSubobject<UBattleComponent>(TEXT("BattleComponent"));
	PokedexDeviceClass = APokedexDevice::StaticClass();
	DialogueComponent = CreateDefaultSubobject<UDialogueComponent>(TEXT("DialogueComponent"));
	PokemonCenterComponent = CreateDefaultSubobject<UPokemonCenterComponent>(TEXT("PokemonCenterComponent"));
	ShopComponent = CreateDefaultSubobject<UShopComponent>(TEXT("ShopComponent"));
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

		// Icônes de menu en bas de l'écran, avec leur raccourci (masquables dans Options)
		TSubclassOf<UShortcutBarWidget> BarClass = ShortcutBarClass ? ShortcutBarClass : TSubclassOf<UShortcutBarWidget>(UShortcutBarWidget::StaticClass());
		ShortcutBar = CreateWidget<UShortcutBarWidget>(this, BarClass);
		if (ShortcutBar)
		{
			ShortcutBar->AddToPlayerScreen(1);
		}

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

			// Raccourcis du menu (P Pokédex, B Sac...)
			ShortcutContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Shortcuts"));
			ShortcutActions.Reset();
			for (const PokemonShortcuts::FShortcut& Shortcut : PokemonShortcuts::Shortcuts)
			{
				UInputAction* Action = NewObject<UInputAction>(this, FName(FString(TEXT("IA_Shortcut_")) + Shortcut.Id));
				ShortcutActions.Add(Action);
				ShortcutContext->MapKey(Action, FKey(Shortcut.Key));
			}
			Subsystem->AddMappingContext(ShortcutContext, 1);

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
		for (UInputAction* Action : ShortcutActions)
		{
			EnhancedInputComponent->BindAction(Action, ETriggerEvent::Started, this, &APokemonNewAgePlayerController::HandleShortcut);
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
	OpenGameMenuAt(EPokedexMenu::Pokemon);
}

void APokemonNewAgePlayerController::OpenGameMenuAt(EPokedexMenu Menu)
{
	if (IsGameMenuOpen() || !IsLocalPlayerController() || BattleComponent->IsInBattle() || BattleHUD)
	{
		return;
	}

	// L'onglet Pokédex s'ouvre en 3D (SCRUM-64), le reste dans le menu classique
	if (Menu == EPokedexMenu::Pokedex && SpawnPokedexDevice())
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
	GameMenu->OpenTab(Menu);
}

bool APokemonNewAgePlayerController::SpawnPokedexDevice()
{
	if (!bUsePokedex3D || !PokedexDeviceClass)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APokedexDevice* Device = GetWorld()->SpawnActor<APokedexDevice>(PokedexDeviceClass, FTransform::Identity, Params);
	if (!Device || !Device->InitializeFor(this))
	{
		if (Device)
		{
			Device->Destroy();
		}
		return false;
	}
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
	Device->OpenOnMenu(EPokedexMenu::Pokedex);
	return true;
}

bool APokemonNewAgePlayerController::SwitchToPokedex3D()
{
	if (!GameMenu || PokedexDevice || !SpawnPokedexDevice())
	{
		return false;
	}
	GameMenu->RemoveFromParent();
	GameMenu = nullptr;
	return true;
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

void APokemonNewAgePlayerController::HandleShortcut(const FInputActionInstance& Instance)
{
	const UInputAction* Source = Instance.GetSourceAction();
	const int32 Index = ShortcutActions.IndexOfByPredicate([Source](const TObjectPtr<UInputAction>& Action) { return Action.Get() == Source; });
	if (Index != INDEX_NONE && !IsGameMenuOpen())
	{
		OpenGameMenuAt(PokemonShortcuts::Shortcuts[Index].Menu);
	}
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

	// Le joueur ne bouge pas, mais la caméra tourne : clic droit maintenu à la souris, stick droit à la manette
	SetShowMouseCursor(true);
	SetIgnoreMoveInput(true);
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(BattleHUD->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(true);
	SetInputMode(InputMode);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
	}
}

void APokemonNewAgePlayerController::OnBattleHUDClosed()
{
	BattleHUD = nullptr;
	SetShowMouseCursor(false);
	SetIgnoreMoveInput(false);
	SetInputMode(FInputModeGameOnly());
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->SetMouseCaptureMode(GetDefault<UInputSettings>()->DefaultViewportMouseCaptureMode);
	}
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

void APokemonNewAgePlayerController::ServerGiveHeldItem_Implementation(int32 PartyIndex, FName Item)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && !BattleComponent->IsInBattle())
	{
		// Les objets rares ne se tiennent pas
		const UPokemonDatabase* Database = UPokemonDataSubsystem::GetDatabase(this);
		const FItemRow* Row = Database ? Database->FindItem(Item) : nullptr;
		if (Row && Row->Category != EItemCategory::KeyItem)
		{
			State->GiveHeldItem(PartyIndex, Item);
		}
	}
}

void APokemonNewAgePlayerController::ServerTakeHeldItem_Implementation(int32 PartyIndex)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && !BattleComponent->IsInBattle())
	{
		State->TakeHeldItem(PartyIndex);
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

void APokemonNewAgePlayerController::Sauvegarder()
{
	PrintDebug(UPokemonSaveLibrary::SaveGame(this) ? TEXT("Partie sauvegardee") : TEXT("Echec de la sauvegarde"));
}

void APokemonNewAgePlayerController::NouvellePartie()
{
	UPokemonSaveLibrary::DeleteSaveGame();
	PrintDebug(TEXT("Sauvegarde effacee : relance le jeu pour une nouvelle partie"));
}

// ---------------------------------------------------------------- Touches (Options > Commandes)

namespace PokemonKeys
{
	/** Commandes, dans l'ordre de la page Commandes */
	struct FCommand { const TCHAR* Id; const TCHAR* Name; };
	const FCommand Commands[] = {
		{ TEXT("Forward"), TEXT("Avancer") },
		{ TEXT("Back"), TEXT("Reculer") },
		{ TEXT("Left"), TEXT("Aller à gauche") },
		{ TEXT("Right"), TEXT("Aller à droite") },
		{ TEXT("Sprint"), TEXT("Courir") },
		{ TEXT("Jump"), TEXT("Sauter") },
		{ TEXT("Interact"), TEXT("Parler, ouvrir") },
		{ TEXT("Menu"), TEXT("Ouvrir le menu") },
		{ TEXT("Party"), TEXT("Menu : Pokémon") },
		{ TEXT("Pokedex"), TEXT("Menu : Pokédex") },
		{ TEXT("Bag"), TEXT("Menu : Sac") },
		{ TEXT("Trainer"), TEXT("Menu : Dresseur") },
		{ TEXT("Map"), TEXT("Menu : Carte") },
		{ TEXT("Save"), TEXT("Menu : Sauvegarder") },
		{ TEXT("Online"), TEXT("Menu : En ligne") },
		{ TEXT("Options"), TEXT("Menu : Options") },
	};

	/** Sens donné par une touche de déplacement : on rejoue ses modificateurs (Inverser, Échanger les axes) sur (1, 0, 0) */
	FVector GetMoveDirection(const FEnhancedActionKeyMapping& Mapping)
	{
		FVector Value(1.f, 0.f, 0.f);
		for (const UInputModifier* Modifier : Mapping.Modifiers)
		{
			if (const UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(Modifier))
			{
				switch (Swizzle->Order)
				{
				case EInputAxisSwizzle::YXZ: Value = FVector(Value.Y, Value.X, Value.Z); break;
				case EInputAxisSwizzle::ZYX: Value = FVector(Value.Z, Value.Y, Value.X); break;
				case EInputAxisSwizzle::XZY: Value = FVector(Value.X, Value.Z, Value.Y); break;
				case EInputAxisSwizzle::YZX: Value = FVector(Value.Y, Value.Z, Value.X); break;
				case EInputAxisSwizzle::ZXY: Value = FVector(Value.Z, Value.X, Value.Y); break;
				default: break;
				}
			}
			else if (const UInputModifierNegate* Negate = Cast<UInputModifierNegate>(Modifier))
			{
				Value = FVector(Negate->bX ? -Value.X : Value.X, Negate->bY ? -Value.Y : Value.Y, Negate->bZ ? -Value.Z : Value.Z);
			}
		}
		return Value;
	}

	int32 CommandOrder(FName Id)
	{
		const FString Base = Id.ToString().LeftChop(4);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Commands); ++Index)
		{
			if (Base == Commands[Index].Id)
			{
				return Index;
			}
		}
		return UE_ARRAY_COUNT(Commands);
	}
}

void APokemonNewAgePlayerController::GetSourceContexts(TArray<TPair<UInputMappingContext*, int32>>& OutContexts) const
{
	for (UInputMappingContext* Context : DefaultMappingContexts)
	{
		if (Context)
		{
			OutContexts.Emplace(Context, 0);
		}
	}
	if (!ShouldUseTouchControls())
	{
		for (UInputMappingContext* Context : MobileExcludedMappingContexts)
		{
			if (Context)
			{
				OutContexts.Emplace(Context, 0);
			}
		}
	}
	if (DefaultMenuContext)
	{
		OutContexts.Emplace(DefaultMenuContext, 1);
	}
	if (ShortcutContext)
	{
		OutContexts.Emplace(ShortcutContext, 1);
	}
	if (const APokemonNewAgeCharacter* Hero = Cast<APokemonNewAgeCharacter>(GetPawn()); Hero && Hero->GetInteractContext())
	{
		OutContexts.Emplace(Hero->GetInteractContext(), 1);
	}
}

FName APokemonNewAgePlayerController::GetBindingId(const FEnhancedActionKeyMapping& Mapping) const
{
	const FKey& Key = Mapping.Key;
	const UInputAction* Action = Mapping.Action;
	if (!Action || !Key.IsValid() || Key.IsAnalog() || Key.IsMouseButton() || Key.IsTouch()
		|| Key.ToString().Contains(TEXT("Stick_")))
	{
		return NAME_None;
	}

	FString Base;
	const APokemonNewAgeCharacter* Hero = Cast<APokemonNewAgeCharacter>(GetPawn());
	if (Action == MenuAction)
	{
		Base = TEXT("Menu");
	}
	else if (const int32 Shortcut = ShortcutActions.IndexOfByPredicate([Action](const TObjectPtr<UInputAction>& Candidate) { return Candidate.Get() == Action; });
		Shortcut != INDEX_NONE)
	{
		Base = PokemonShortcuts::Shortcuts[Shortcut].Id;
	}
	else if (Hero && Action == Hero->GetJumpAction())
	{
		Base = TEXT("Jump");
	}
	else if (Hero && Action == Hero->GetSprintAction())
	{
		Base = TEXT("Sprint");
	}
	else if (Hero && Action == Hero->GetInteractAction())
	{
		Base = TEXT("Interact");
	}
	else if (Hero && Action == Hero->GetMoveAction())
	{
		// Déplacement : X = droite, Y = avant (APokemonNewAgeCharacter::Move)
		const FVector Direction = PokemonKeys::GetMoveDirection(Mapping);
		Base = FMath::Abs(Direction.Y) > FMath::Abs(Direction.X)
			? (Direction.Y > 0.f ? TEXT("Forward") : TEXT("Back"))
			: (Direction.X > 0.f ? TEXT("Right") : TEXT("Left"));
	}
	if (Base.IsEmpty())
	{
		return NAME_None;
	}
	return FName(Base + (Key.IsGamepadKey() ? TEXT("_Pad") : TEXT("_Key")));
}

TArray<FPokemonBindableKey> APokemonNewAgePlayerController::GetBindableKeys() const
{
	TArray<FPokemonBindableKey> Keys;
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	TArray<TPair<UInputMappingContext*, int32>> Contexts;
	GetSourceContexts(Contexts);

	// La première touche trouvée pour chaque commande est celle qu'on change (les flèches restent en plus de ZQSD)
	for (const TPair<UInputMappingContext*, int32>& Context : Contexts)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context.Key->GetMappings())
		{
			const FName Id = GetBindingId(Mapping);
			if (Id.IsNone() || Keys.ContainsByPredicate([Id](const FPokemonBindableKey& Existing) { return Existing.Id == Id; }))
			{
				continue;
			}
			FPokemonBindableKey& Entry = Keys.AddDefaulted_GetRef();
			Entry.Id = Id;
			Entry.bGamepad = Mapping.Key.IsGamepadKey();
			Entry.DefaultKey = Mapping.Key;
			const FKey* Custom = Options ? Options->KeyBindings.Find(Id) : nullptr;
			Entry.Key = Custom ? *Custom : Mapping.Key;
			const int32 Order = PokemonKeys::CommandOrder(Id);
			Entry.Name = Order < UE_ARRAY_COUNT(PokemonKeys::Commands) ? FText::FromString(PokemonKeys::Commands[Order].Name) : FText::FromName(Id);
		}
	}

	Keys.Sort([](const FPokemonBindableKey& A, const FPokemonBindableKey& B)
	{
		if (A.bGamepad != B.bGamepad)
		{
			return !A.bGamepad;
		}
		return PokemonKeys::CommandOrder(A.Id) < PokemonKeys::CommandOrder(B.Id);
	});
	return Keys;
}

FKey APokemonNewAgePlayerController::GetShortcutKey(EPokedexMenu Menu) const
{
	for (const PokemonShortcuts::FShortcut& Shortcut : PokemonShortcuts::Shortcuts)
	{
		if (Shortcut.Menu != Menu)
		{
			continue;
		}
		const FName Id(FString(Shortcut.Id) + TEXT("_Key"));
		const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
		const FKey* Custom = Options ? Options->KeyBindings.Find(Id) : nullptr;
		return Custom ? *Custom : FKey(Shortcut.Key);
	}
	return FKey();
}

bool APokemonNewAgePlayerController::FindShortcut(const FKey& Key, EPokedexMenu& OutMenu) const
{
	for (const PokemonShortcuts::FShortcut& Shortcut : PokemonShortcuts::Shortcuts)
	{
		if (Key.IsValid() && GetShortcutKey(Shortcut.Menu) == Key)
		{
			OutMenu = Shortcut.Menu;
			return true;
		}
	}
	return false;
}

bool APokemonNewAgePlayerController::IsMenuKey(const FKey& Key) const
{
	if (Key == EKeys::Tab || Key == EKeys::Gamepad_Special_Right)
	{
		return true;
	}
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	for (const TCHAR* Id : { TEXT("Menu_Key"), TEXT("Menu_Pad") })
	{
		if (const FKey* Custom = Options ? Options->KeyBindings.Find(FName(Id)) : nullptr; Custom && *Custom == Key)
		{
			return true;
		}
	}
	return false;
}

void APokemonNewAgePlayerController::SetKeyBinding(FName Id, const FKey& Key)
{
	UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	const TArray<FPokemonBindableKey> Keys = GetBindableKeys();
	const FPokemonBindableKey* Changed = Keys.FindByPredicate([Id](const FPokemonBindableKey& Entry) { return Entry.Id == Id; });
	if (!Changed || !Options)
	{
		return;
	}

	auto Store = [Options](const FPokemonBindableKey& Entry, const FKey& NewKey)
	{
		if (NewKey == Entry.DefaultKey)
		{
			Options->KeyBindings.Remove(Entry.Id);
		}
		else
		{
			Options->KeyBindings.Add(Entry.Id, NewKey);
		}
	};

	// La commande qui avait déjà cette touche prend l'ancienne touche de celle qu'on change
	for (const FPokemonBindableKey& Other : Keys)
	{
		if (Other.Id != Id && Other.bGamepad == Changed->bGamepad && Other.Key == Key)
		{
			Store(Other, Changed->Key);
		}
	}
	Store(*Changed, Key);
	UPokemonSaveLibrary::SaveOptions(Options);
	ApplyKeyBindings();
}

void APokemonNewAgePlayerController::ResetKeyBindings()
{
	UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	Options->KeyBindings.Reset();
	UPokemonSaveLibrary::SaveOptions(Options);
	ApplyKeyBindings();
}

void APokemonNewAgePlayerController::ApplyKeyBindings()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalPlayerController() && GetLocalPlayer()
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	if (!Subsystem)
	{
		return;
	}
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	TArray<TPair<UInputMappingContext*, int32>> Contexts;
	GetSourceContexts(Contexts);

	// On ne touche jamais aux assets (IMC_Default...) : chaque contexte est recopié avec les touches du joueur
	TSet<FName> Done;
	for (const TPair<UInputMappingContext*, int32>& Context : Contexts)
	{
		UInputMappingContext* Original = Context.Key;
		UInputMappingContext* Copy = NewObject<UInputMappingContext>(this);
		for (const FEnhancedActionKeyMapping& Mapping : Original->GetMappings())
		{
			FKey Key = Mapping.Key;
			const FName Id = GetBindingId(Mapping);
			if (!Id.IsNone() && !Done.Contains(Id))
			{
				Done.Add(Id);
				if (const FKey* Custom = Options->KeyBindings.Find(Id))
				{
					Key = *Custom;
				}
			}
			FEnhancedActionKeyMapping& NewMapping = Copy->MapKey(Mapping.Action, Key);
			NewMapping.Modifiers = Mapping.Modifiers;
			NewMapping.Triggers = Mapping.Triggers;
		}

		if (const TObjectPtr<UInputMappingContext>* Previous = BoundContexts.Find(Original))
		{
			Subsystem->RemoveMappingContext(*Previous);
		}
		Subsystem->RemoveMappingContext(Original);
		Subsystem->AddMappingContext(Copy, Context.Value);
		BoundContexts.Add(Original, Copy);
	}
}

void APokemonNewAgePlayerController::Parler(FName Npc)
{
	DialogueComponent->TalkTo(Npc);
}

void APokemonNewAgePlayerController::Dialogue(FName ConversationId)
{
	if (HasAuthority())
	{
		DialogueComponent->PlayConversation(ConversationId);
	}
}

void APokemonNewAgePlayerController::Drapeau(FName Flag)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && HasAuthority())
	{
		State->AddFlag(Flag);
		Drapeaux();
	}
}

void APokemonNewAgePlayerController::Drapeaux()
{
	if (const APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>())
	{
		FString Flags;
		for (const FName& Flag : State->StoryFlags)
		{
			Flags += Flag.ToString() + TEXT(" ");
		}
		PrintDebug(FString::Printf(TEXT("Drapeaux : %s"), Flags.IsEmpty() ? TEXT("(aucun)") : *Flags));
	}
}

// ---------------------------------------------------------------- Commandes de test du Centre et de la Boutique (console)

void APokemonNewAgePlayerController::Boutique(FName ShopId)
{
	if (HasAuthority())
	{
		PrintDebug(ShopComponent->OpenShop(ShopId) ? FString::Printf(TEXT("Boutique %s ouverte"), *ShopId.ToString())
			: FString::Printf(TEXT("Boutique %s introuvable dans DT_Shops"), *ShopId.ToString()));
	}
}

void APokemonNewAgePlayerController::Acheter(FName Item, int32 Count)
{
	ShopComponent->Buy(Item, Count);
}

void APokemonNewAgePlayerController::Vendre(FName Item, int32 Count)
{
	ShopComponent->Sell(Item, Count);
}

void APokemonNewAgePlayerController::Argent(int32 Amount)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && HasAuthority())
	{
		State->AddMoney(Amount - State->Money);
		PrintDebug(FString::Printf(TEXT("Argent : %d"), State->Money));
	}
}

void APokemonNewAgePlayerController::Badge(FName BadgeId)
{
	if (APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>(); State && HasAuthority() && !BadgeId.IsNone())
	{
		State->Badges.AddUnique(BadgeId);
		PrintDebug(FString::Printf(TEXT("Badges : %d"), State->Badges.Num()));
	}
}

void APokemonNewAgePlayerController::AllumerPC()
{
	if (HasAuthority())
	{
		PokemonCenterComponent->OpenPC();
	}
}

void APokemonNewAgePlayerController::NoterCentre()
{
	if (HasAuthority())
	{
		PokemonCenterComponent->RecordHealPoint();
		if (const APokemonPlayerState* State = GetPlayerState<APokemonPlayerState>())
		{
			PrintDebug(FString::Printf(TEXT("Point de retour : %s"), State->LastHealPoint.IsNone() ? TEXT("(aucun Centre proche)") : *State->LastHealPoint.ToString()));
		}
	}
}

void APokemonNewAgePlayerController::RetourCentre()
{
	if (HasAuthority() && !PokemonCenterComponent->ReturnToLastHealPoint())
	{
		PrintDebug(TEXT("Aucun PokemonCenterPoint ni Player Start dans la map"));
	}
}
