#include "UI/Title/TitleScreenSubsystem.h"
#include "UI/Title/TitleScreenSettings.h"
#include "UI/Title/TitleScreenWidget.h"
#include "UI/GameHUDWidget.h"
#include "UI/Pokedex/PokedexModel.h"
#include "Core/PokemonSaveGame.h"
#include "PokemonNewAge.h"
#include "PokemonNewAgePlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"

namespace TitleScreen
{
	/** Option de map ajoutée par « Nouvelle partie » quand elle recharge la map : pas de deuxième écran de titre */
	const TCHAR* NoTitleOption = TEXT("NoTitle");

	/** Tag de l'acteur qui donne la vue du titre */
	const FName CameraTag(TEXT("TitleCamera"));

	/** Au bout de ce temps sans personnage, l'écran s'affiche quand même */
	constexpr float MaxWaitForPawn = 3.f;

	/** Fondu du noir au début */
	constexpr float FadeInTime = 2.f;

	/** Point que la caméra regarde, au-dessus des pieds du joueur */
	constexpr float LookAtHeight = 150.f;
}

using namespace TitleScreen;

namespace TitleScreen
{
	/** Cache ou remontre le HUD du jeu (il ne doit pas apparaître pendant le vol de la caméra) */
	void SetGameHUDVisible(APlayerController* PC, bool bVisible)
	{
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Found, UGameHUDWidget::StaticClass(), true);
		for (UUserWidget* HUD : Found)
		{
			if (HUD->GetOwningPlayer() == PC)
			{
				HUD->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
		}
	}
}

bool UTitleScreenSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTitleScreenSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ShouldShow(InWorld))
	{
		Stage = EStage::Waiting;
		StageTime = 0.f;
	}
}

bool UTitleScreenSubsystem::ShouldShow(const UWorld& World) const
{
	const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();
	if (!Settings->bEnabled || FParse::Param(FCommandLine::Get(), NoTitleOption))
	{
		return false;
	}
	if (World.WorldType == EWorldType::PIE && !Settings->bShowInEditor)
	{
		return false;
	}
	// Un client qui rejoint une partie arrive directement en jeu
	if (World.GetNetMode() == NM_Client || World.GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	if (const AGameModeBase* GameMode = World.GetAuthGameMode(); GameMode && UGameplayStatics::HasOption(GameMode->OptionsString, NoTitleOption))
	{
		return false;
	}
	if (Settings->Maps.Num() > 0)
	{
		const FString MapName = UGameplayStatics::GetCurrentLevelName(&World, true);
		if (!Settings->Maps.ContainsByPredicate([&MapName](const FString& Name) { return Name.Equals(MapName, ESearchCase::IgnoreCase); }))
		{
			return false;
		}
	}
	return true;
}

APokemonNewAgePlayerController* UTitleScreenSubsystem::FindLocalController() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APokemonNewAgePlayerController* Found = Cast<APokemonNewAgePlayerController>(It->Get());
		if (Found && Found->IsLocalPlayerController())
		{
			return Found;
		}
	}
	return nullptr;
}

void UTitleScreenSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Stage == EStage::Off)
	{
		return;
	}
	// Temps réel : les Options mettent le jeu en pause
	const float RealDelta = FMath::Min(float(FApp::GetDeltaTime()), 0.1f);
	StageTime += RealDelta;

	if (Stage == EStage::Waiting)
	{
		APokemonNewAgePlayerController* Found = FindLocalController();
		if (Found && (Found->GetPawn() || StageTime > MaxWaitForPawn))
		{
			Show(Found);
		}
		else if (StageTime > MaxWaitForPawn * 2.f)
		{
			// Pas de PlayerController du jeu (autre GameMode) : pas d'écran de titre
			Stage = EStage::Off;
		}
		return;
	}

	APokemonNewAgePlayerController* PC = Controller.Get();
	if (!PC)
	{
		Stage = EStage::Off;
		return;
	}

	if (Stage == EStage::Shown)
	{
		// Retour des Options (le menu Paramètres remet les commandes du jeu en se fermant)
		const bool bOpenNow = PC->IsSettingsMenuOpen();
		if (bOptionsOpen && !bOpenNow)
		{
			bOptionsOpen = false;
			if (Widget)
			{
				Widget->SetVisibility(ESlateVisibility::Visible);
				Widget->RefreshSave();
			}
			FocusTitle();
		}
		UpdateCamera(RealDelta);
	}
	else if (Stage == EStage::Leaving && StageTime >= LeaveTime)
	{
		Finish();
	}
}

void UTitleScreenSubsystem::Show(APokemonNewAgePlayerController* PC)
{
	const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();
	UWorld* World = GetWorld();
	Controller = PC;
	Stage = EStage::Shown;
	StageTime = 0.f;

	Widget = CreateWidget<UTitleScreenWidget>(PC, UTitleScreenWidget::StaticClass());
	if (Widget)
	{
		Widget->Owner = this;
		// Sous le menu Paramètres (20), qui s'ouvre par-dessus depuis « Options »
		Widget->AddToPlayerScreen(10);
		Widget->TakeWidget();
	}

	// Sans image de fond : la vue 3D de la map (acteur tagué TitleCamera, sinon un tour lent autour du joueur)
	bUseCamera = !Widget || !Widget->HasBackground();
	if (bUseCamera)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(CameraTag))
			{
				FixedView = *It;
				FixedViewTransform = It->GetActorTransform();
				break;
			}
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (Camera)
		{
			Camera->GetCameraComponent()->SetFieldOfView(Settings->FieldOfView);
			Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
			if (const APawn* Pawn = PC->GetPawn())
			{
				// Départ face au joueur, un peu de côté
				OrbitAngle = Pawn->GetActorRotation().Yaw + 150.f;
			}
			UpdateCamera(0.f);
			PC->SetViewTarget(Camera);
		}
	}
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeInTime, FLinearColor::Black, false, false);
	}
	FocusTitle();
	SetGameHUDVisible(PC, false);

	if (USoundBase* Sound = Settings->Music.LoadSynchronous())
	{
		Music = UGameplayStatics::CreateSound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
		if (Music)
		{
			Music->bIsUISound = true;
			Music->FadeIn(1.5f);
		}
	}
	UE_LOG(LogPokemonNewAge, Log, TEXT("Ecran de titre affiche"));
}

void UTitleScreenSubsystem::FocusTitle()
{
	APokemonNewAgePlayerController* PC = Controller.Get();
	if (!PC || !Widget)
	{
		return;
	}
	PC->SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	Widget->FocusCurrent();
}

void UTitleScreenSubsystem::UpdateCamera(float DeltaTime)
{
	const APokemonNewAgePlayerController* PC = Controller.Get();
	if (!Camera || !PC)
	{
		return;
	}
	const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();

	if (FixedView.IsValid())
	{
		// Vue choisie dans la map, qui respire un peu (balancement lent de quelques degrés)
		FRotator Rotation = FixedViewTransform.Rotator();
		Rotation.Yaw += 2.5f * FMath::Sin(StageTime * 0.12f);
		Rotation.Pitch += 0.8f * FMath::Sin(StageTime * 0.09f);
		Camera->SetActorLocationAndRotation(FixedViewTransform.GetLocation(), Rotation);
		return;
	}

	const APawn* Pawn = PC->GetPawn();
	const FVector Center = (Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector) + FVector(0.f, 0.f, LookAtHeight);
	OrbitAngle = FMath::Fmod(OrbitAngle + Settings->OrbitSpeed * DeltaTime, 360.f);
	const float Radians = FMath::DegreesToRadians(OrbitAngle);
	FVector Location = Center + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.f) * Settings->OrbitRadius;
	Location.Z = Center.Z + Settings->OrbitHeight;

	// Jamais dans une colline : on remonte au-dessus du sol sous la caméra
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TitleCamera), false, Pawn);
	const FVector Up = Location + FVector(0.f, 0.f, 50000.f);
	const FVector Down = Location - FVector(0.f, 0.f, 50000.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Up, Down, ECC_Visibility, Query))
	{
		Location.Z = FMath::Max(Location.Z, Hit.ImpactPoint.Z + Settings->MinGroundClearance);
	}
	// Hauteur lissée : pas de saut quand le relief change sous la caméra
	SmoothedZ = bHasSmoothedZ && DeltaTime > 0.f ? FMath::FInterpTo(SmoothedZ, Location.Z, DeltaTime, 1.5f) : Location.Z;
	bHasSmoothedZ = true;
	Location.Z = SmoothedZ;

	Camera->SetActorLocationAndRotation(Location, (Center - Location).Rotation());
}

void UTitleScreenSubsystem::Continue()
{
	if (Stage == EStage::Shown)
	{
		Leave();
	}
}

void UTitleScreenSubsystem::StartNewGame()
{
	if (Stage != EStage::Shown)
	{
		return;
	}
	if (!UPokemonSaveLibrary::HasSaveGame())
	{
		// Rien de sauvegardé : la partie qui tourne dessous est déjà neuve
		Leave();
		return;
	}
	// Le widget a déjà demandé confirmation : on efface et on recharge la map sans écran de titre
	UPokemonSaveLibrary::DeleteSaveGame();
	UWorld* World = GetWorld();
	const FString MapPath = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	UE_LOG(LogPokemonNewAge, Log, TEXT("Nouvelle partie : sauvegarde effacee, rechargement de %s"), *MapPath);
	Stage = EStage::Off;
	if (Music)
	{
		Music->FadeOut(0.5f, 0.f);
	}
	if (APokemonNewAgePlayerController* PC = Controller.Get(); PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.3f, FLinearColor::Black, false, true);
	}
	UGameplayStatics::OpenLevel(World, FName(*MapPath), true, NoTitleOption);
}

void UTitleScreenSubsystem::OpenOptions()
{
	APokemonNewAgePlayerController* PC = Controller.Get();
	if (Stage != EStage::Shown || !PC)
	{
		return;
	}
	PC->OpenSettingsMenu();
	if (PC->IsSettingsMenuOpen())
	{
		bOptionsOpen = true;
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTitleScreenSubsystem::QuitGame()
{
	if (APokemonNewAgePlayerController* PC = Controller.Get())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

void UTitleScreenSubsystem::PlaySound(bool bConfirm) const
{
	const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();
	USoundBase* Sound = (bConfirm ? Settings->ConfirmSound : Settings->NavSound).LoadSynchronous();
	if (!Sound)
	{
		// Bruits du Pokédex de L'amie s'ils sont importés
		if (const UPokedexModel* Model = UPokedexModel::Get())
		{
			Sound = bConfirm ? Model->ClickSound.Get() : Model->NavSound.Get();
		}
	}
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound, 1.f, 1.f, 0.f, nullptr, nullptr, true);
	}
}

void UTitleScreenSubsystem::Leave()
{
	APokemonNewAgePlayerController* PC = Controller.Get();
	const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();
	Stage = EStage::Leaving;
	StageTime = 0.f;
	// Avec l'image de fond, elle s'efface sur le jeu ; avec la vue 3D, la caméra vole jusqu'au joueur
	LeaveTime = bUseCamera ? Settings->BlendTime : FMath::Min(Settings->BlendTime, 1.2f);
	if (Widget)
	{
		Widget->PlayOutro(bUseCamera ? 0.7f : LeaveTime);
	}
	if (Music)
	{
		Music->FadeOut(LeaveTime + 0.5f, 0.f);
	}
	if (PC)
	{
		// Plus de clics sur le titre pendant le vol, mais toujours pas de commandes de jeu
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeUIOnly());
		if (APawn* Pawn = PC->GetPawn())
		{
			// La caméra du jeu se met derrière le joueur, légèrement au-dessus
			PC->SetControlRotation(FRotator(-12.f, Pawn->GetActorRotation().Yaw, 0.f));
			if (bUseCamera)
			{
				// La vue du titre glisse jusqu'à elle
				PC->SetViewTargetWithBlend(Pawn, Settings->BlendTime, VTBlend_Cubic);
			}
		}
	}
	if (LeaveTime <= 0.f)
	{
		Finish();
	}
}

void UTitleScreenSubsystem::Finish()
{
	APokemonNewAgePlayerController* PC = Controller.Get();
	Stage = EStage::Off;
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
	if (Music)
	{
		Music->Stop();
		Music = nullptr;
	}
	if (!PC)
	{
		return;
	}
	if (PC->GetPawn())
	{
		PC->SetViewTarget(PC->GetPawn());
	}
	PC->SetShowMouseCursor(false);
	PC->SetInputMode(FInputModeGameOnly());
	SetGameHUDVisible(PC, true);

	// Le grand nom du lieu, comme quand on arrive quelque part
	const UPokedexModel* Model = UPokedexModel::Get();
	const int32 Place = PC->GetCurrentPlace();
	if (Model && Model->MapPlaces.IsValidIndex(Place))
	{
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Found, UGameHUDWidget::StaticClass(), true);
		for (UUserWidget* HUD : Found)
		{
			if (HUD->GetOwningPlayer() == PC)
			{
				Cast<UGameHUDWidget>(HUD)->ShowPlaceBanner(Model->MapPlaces[Place].Name);
			}
		}
	}
	UE_LOG(LogPokemonNewAge, Log, TEXT("Ecran de titre ferme, debut de la partie"));
}

void UTitleScreenSubsystem::Deinitialize()
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Stage = EStage::Off;
	Super::Deinitialize();
}

TStatId UTitleScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTitleScreenSubsystem, STATGROUP_Tickables);
}
