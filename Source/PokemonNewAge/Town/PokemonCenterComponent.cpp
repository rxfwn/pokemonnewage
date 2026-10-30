#include "Town/PokemonCenterComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "PokemonNewAge.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Town/PokemonCenterPoint.h"
#include "Town/TownScreens.h"
#include "UI/StorageWidget.h"

namespace
{
	/** Conversations jouées en réapparaissant (Tools/Data/dialogues_v1.py) */
	const FName NurseAfterBlackout(TEXT("PokemonCenter_Nurse_AfterBlackout"));
	const FName MomAfterBlackout(TEXT("PlayerHouse_Mom_AfterBlackout"));
}

UPokemonCenterComponent::UPokemonCenterComponent()
{
	SetIsReplicatedByDefault(true);
	StorageWidgetClass = UStorageWidget::StaticClass();
}

void UPokemonCenterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (PCWidget)
	{
		PCWidget->RemoveFromParent();
		PCWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

APlayerController* UPokemonCenterComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

APokemonPlayerState* UPokemonCenterComponent::GetPokemonPlayerState() const
{
	const APlayerController* PlayerController = GetPlayerController();
	return PlayerController ? PlayerController->GetPlayerState<APokemonPlayerState>() : nullptr;
}

// ---------------------------------------------------------------- Soin et point de retour (serveur)

void UPokemonCenterComponent::RecordHealPoint()
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	const APawn* Pawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	APokemonCenterPoint* Point = Pawn ? APokemonCenterPoint::FindNearest(this, Pawn->GetActorLocation()) : nullptr;
	if (!State || !Point)
	{
		// Soin loin de tout Centre (ex. un PNJ sur une route) : le point de retour ne change pas
		return;
	}

	State->LastHealPoint = Point->PointId;
	State->LastHealLocation = Point->GetActorLocation();
	State->LastHealRotation = Point->GetActorRotation();
	if (!Point->Location.IsNone())
	{
		State->CurrentLocation = Point->Location;
	}
	OnHealPointChanged.Broadcast(Point->PointId);
	ClientHealRecorded(Point->PointId, Point->HealSound);
}

bool UPokemonCenterComponent::ReturnToLastHealPoint()
{
	APokemonPlayerState* State = GetPokemonPlayerState();
	if (!State)
	{
		return false;
	}
	State->HealParty();

	// Le dernier Centre, sinon la maison, sinon la position gardée, sinon un Player Start
	FVector Location = State->LastHealLocation;
	FRotator Rotation = State->LastHealRotation;
	bool bAtHome = false;
	bool bFound = false;
	if (const APokemonCenterPoint* Point = APokemonCenterPoint::FindById(this, State->LastHealPoint))
	{
		Location = Point->GetActorLocation();
		Rotation = Point->GetActorRotation();
		bAtHome = Point->bIsHome;
		bFound = true;
	}
	else if (!State->LastHealPoint.IsNone() && !State->LastHealLocation.IsNearlyZero())
	{
		bFound = true;
	}
	else if (const APokemonCenterPoint* Home = APokemonCenterPoint::FindHome(this))
	{
		Location = Home->GetActorLocation();
		Rotation = Home->GetActorRotation();
		bAtHome = true;
		bFound = true;
	}
	else
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			Location = It->GetActorLocation();
			Rotation = It->GetActorRotation();
			bAtHome = true;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Centre Pokemon : aucun PokemonCenterPoint ni Player Start dans la map, le joueur reste sur place"));
		return false;
	}

	// Écran noir, téléport pendant le noir, puis Joëlle (ou maman) parle
	ClientFadeOut();
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Location, Rotation, bAtHome]()
	{
		TeleportPawn(Location, Rotation);
		ClientReturned();
		if (UDialogueComponent* Dialogue = GetOwner()->FindComponentByClass<UDialogueComponent>())
		{
			if (!Dialogue->IsInConversation())
			{
				Dialogue->PlayConversation(bAtHome ? MomAfterBlackout : NurseAfterBlackout);
			}
		}
	}), FMath::Max(FadeDuration, 0.01f), false);
	return true;
}

void UPokemonCenterComponent::TeleportPawn(const FVector& Location, const FRotator& Rotation)
{
	APlayerController* PlayerController = GetPlayerController();
	ACharacter* PlayerCharacter = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr;
	if (!PlayerCharacter)
	{
		return;
	}

	// Comme au chargement d'une sauvegarde : un peu plus haut, sans gravité le temps que le terrain se charge
	const FRotator Facing(0.f, Rotation.Yaw, 0.f);
	PlayerCharacter->TeleportTo(Location + FVector(0.f, 0.f, PlayerCharacter->GetSimpleCollisionHalfHeight() + 20.f), Facing);
	PlayerController->SetControlRotation(Facing);
	if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	GroundWaitStart = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(GroundTimer, this, &UPokemonCenterComponent::CheckGround, 0.25f, true, 0.25f);
}

void UPokemonCenterComponent::CheckGround()
{
	const APlayerController* PlayerController = GetPlayerController();
	ACharacter* PlayerCharacter = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr;
	if (!PlayerCharacter)
	{
		GetWorld()->GetTimerManager().ClearTimer(GroundTimer);
		return;
	}

	const FVector Start = PlayerCharacter->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, PlayerCharacter->GetSimpleCollisionHalfHeight() + 500.f);
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("PokemonCenterGround"), false, PlayerCharacter);
	const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - GroundWaitStart > GroundWaitTimeout;
	if (!bGround && !bTimedOut)
	{
		return;
	}
	if (bTimedOut && !bGround)
	{
		UE_LOG(LogPokemonNewAge, Warning, TEXT("Centre Pokemon : pas de sol sous le point de retour apres %.0f s"), GroundWaitTimeout);
	}
	GetWorld()->GetTimerManager().ClearTimer(GroundTimer);
	if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Falling);
	}
}

// ---------------------------------------------------------------- Client

void UPokemonCenterComponent::ClientHealRecorded_Implementation(FName PointId, USoundBase* HealSound)
{
	if (HealSound)
	{
		UGameplayStatics::PlaySound2D(this, HealSound);
	}
	if (!GetOwner()->HasAuthority())
	{
		OnHealPointChanged.Broadcast(PointId);
	}
}

void UPokemonCenterComponent::ClientFadeOut_Implementation()
{
	APlayerController* PlayerController = GetPlayerController();
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeDuration, FLinearColor::Black, false, true);
	}
}

void UPokemonCenterComponent::ClientReturned_Implementation()
{
	APlayerController* PlayerController = GetPlayerController();
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeDuration * 1.5f, FLinearColor::Black, false, false);
	}
	OnReturnedToHealPoint.Broadcast();
}

// ---------------------------------------------------------------- PC de Léo

void UPokemonCenterComponent::OpenPC()
{
	ClientOpenPC();
}

void UPokemonCenterComponent::ClientOpenPC_Implementation()
{
	// Allumé par le dialogue du PC : on attend que la boîte de dialogue se ferme
	UDialogueComponent* Dialogue = GetOwner()->FindComponentByClass<UDialogueComponent>();
	if (Dialogue && Dialogue->IsInConversation())
	{
		bPCWaitingForDialogue = true;
		Dialogue->OnConversationEnded.AddUniqueDynamic(this, &UPokemonCenterComponent::HandleConversationEnded);
		return;
	}
	ShowPC();
}

void UPokemonCenterComponent::HandleConversationEnded()
{
	if (UDialogueComponent* Dialogue = GetOwner()->FindComponentByClass<UDialogueComponent>())
	{
		Dialogue->OnConversationEnded.RemoveDynamic(this, &UPokemonCenterComponent::HandleConversationEnded);
	}
	if (bPCWaitingForDialogue)
	{
		bPCWaitingForDialogue = false;
		ShowPC();
	}
}

void UPokemonCenterComponent::ShowPC()
{
	APlayerController* PlayerController = GetPlayerController();
	if (PCWidget || !PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	PCWidget = CreateWidget<UStorageWidget>(PlayerController, StorageWidgetClass ? StorageWidgetClass.Get() : UStorageWidget::StaticClass());
	if (!PCWidget)
	{
		UE_LOG(LogPokemonNewAge, Error, TEXT("PC : impossible de creer l'ecran"));
		return;
	}
	PCWidget->Init(this);
	TownScreens::OpenScreen(PlayerController, PCWidget);
}

void UPokemonCenterComponent::ClosePC()
{
	bPCWaitingForDialogue = false;
	if (PCWidget)
	{
		TownScreens::CloseScreen(GetPlayerController(), PCWidget);
		PCWidget = nullptr;
	}
}
