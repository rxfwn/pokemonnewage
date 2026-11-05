#include "Character/TrainerAppearanceComponent.h"
#include "Character/TrainerAppearanceData.h"
#include "Character/TrainerAppearanceRender.h"
#include "Character/CharacterStudio.h"
#include "UI/CharacterCreatorWidget.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "PokemonNewAge.h"

UTrainerAppearanceComponent::UTrainerAppearanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UTrainerAppearanceData* UTrainerAppearanceComponent::GetAppearanceData()
{
	if (!AppearanceData)
	{
		AppearanceData = TrainerAppearanceRender::LoadDefaultData();
	}
	return AppearanceData;
}

USkeletalMeshComponent* UTrainerAppearanceComponent::GetTargetMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UTrainerAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	TryApplyFromPlayerState();
	if (bAutoOpenEditor && GetWorld())
	{
		// Laisse le temps à la sauvegarde de revenir du serveur avant de décider si c'est une nouvelle partie
		GetWorld()->GetTimerManager().SetTimer(FirstCreationTimer, this, &UTrainerAppearanceComponent::CheckFirstCreation, 3.f, false);
	}
}

void UTrainerAppearanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Editor || Studio)
	{
		CloseEditor(false, FTrainerAppearance(), FString(), FString());
	}
	Super::EndPlay(EndPlayReason);
}

void UTrainerAppearanceComponent::TryApplyFromPlayerState()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr)
	{
		ApplyAppearance(State->Appearance);
		return;
	}
	// PNJ ou PlayerState pas encore arrivé : 10 essais sur 5 s (le PlayerState rappelle de toute façon à chaque changement)
	if (++RetryCount <= 10 && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(RetryTimer, this, &UTrainerAppearanceComponent::TryApplyFromPlayerState, 0.5f, false);
	}
}

void UTrainerAppearanceComponent::CheckFirstCreation()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (Pawn && Pawn->IsLocallyControlled() && State && !State->Appearance.bCreated && GetAppearanceData())
	{
		OpenEditor(ETrainerEditorMode::Creation);
	}
}

FTrainerAppearance UTrainerAppearanceComponent::GetCurrentAppearance() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr;
	return State ? State->Appearance : FTrainerAppearance();
}

void UTrainerAppearanceComponent::ApplyAppearance(const FTrainerAppearance& Appearance)
{
	// Pas encore de catalogue : on laisse le Dresseur provisoire
	TrainerAppearanceRender::Apply(GetAppearanceData(), GetOwner(), GetTargetMesh(), Appearance, AppliedBase);
}

void UTrainerAppearanceComponent::RequestAppearance(const FTrainerAppearance& Appearance)
{
	ApplyAppearance(Appearance); // aperçu immédiat chez soi
	ServerSetAppearance(Appearance); // en solo ou sur l'hôte, l'appel est direct
}

void UTrainerAppearanceComponent::ServerSetAppearance_Implementation(const FTrainerAppearance& Appearance)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr)
	{
		State->SetAppearance(TrainerAppearanceRender::Sanitize(GetAppearanceData(), Appearance));
	}
}

FString UTrainerAppearanceComponent::CleanText(const FString& Text, int32 MaxLength)
{
	FString Clean = Text.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")).TrimStartAndEnd();
	return Clean.Left(MaxLength);
}

void UTrainerAppearanceComponent::RequestProfile(const FString& TrainerName, const FString& Catchphrase)
{
	ServerSetProfile(TrainerName, Catchphrase);
}

void UTrainerAppearanceComponent::ServerSetProfile_Implementation(const FString& TrainerName, const FString& Catchphrase)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APokemonPlayerState* State = Pawn ? Pawn->GetPlayerState<APokemonPlayerState>() : nullptr)
	{
		const FString Name = CleanText(TrainerName, MaxNameLength);
		if (!Name.IsEmpty())
		{
			State->SetPlayerName(Name);
		}
		State->Catchphrase = CleanText(Catchphrase, MaxCatchphraseLength);
	}
}

// ---------------------------------------------------------------- Éditeur

void UTrainerAppearanceComponent::ClientOpenEditor_Implementation(ETrainerEditorMode Mode)
{
	// Ouvert par un dialogue : on attend que la boîte se ferme (elle rend la main au jeu en partant)
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UDialogueComponent* Dialogue = Controller ? Controller->FindComponentByClass<UDialogueComponent>() : nullptr;
	if (Dialogue && Dialogue->IsInConversation())
	{
		PendingMode = Mode;
		Dialogue->OnConversationEnded.AddUniqueDynamic(this, &UTrainerAppearanceComponent::HandleConversationEnded);
		return;
	}
	OpenEditor(Mode);
}

void UTrainerAppearanceComponent::HandleConversationEnded()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (UDialogueComponent* Dialogue = Controller ? Controller->FindComponentByClass<UDialogueComponent>() : nullptr)
	{
		Dialogue->OnConversationEnded.RemoveDynamic(this, &UTrainerAppearanceComponent::HandleConversationEnded);
	}
	if (PendingMode.IsSet())
	{
		const ETrainerEditorMode Mode = PendingMode.GetValue();
		PendingMode.Reset();
		OpenEditor(Mode);
	}
}

bool UTrainerAppearanceComponent::OpenEditor(ETrainerEditorMode Mode)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UTrainerAppearanceData* Data = GetAppearanceData();
	if (Editor || !PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}
	if (!Data || Data->Bases.IsEmpty())
	{
		const FString Message = TEXT("Éditeur de personnage : créer d'abord Content/Characters/Player/DA_TrainerAppearance (docs/PERSONNAGE_CREATION.md)");
		UE_LOG(LogPokemonNewAge, Warning, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange, Message);
		}
		return false;
	}

	// Studio local (jamais répliqué) : chaque joueur a le sien, au-dessus de lui
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = PlayerController;
	const FVector StudioLocation = Pawn->GetActorLocation() + FVector(0.f, 0.f, StudioHeight);
	Studio = GetWorld()->SpawnActor<ACharacterStudio>(ACharacterStudio::StaticClass(), StudioLocation, FRotator::ZeroRotator, Params);
	if (!Studio)
	{
		return false;
	}

	const APokemonPlayerState* State = Pawn->GetPlayerState<APokemonPlayerState>();
	const FTrainerAppearance Current = GetCurrentAppearance();
	Studio->Init(Data, Current);

	Editor = CreateWidget<UCharacterCreatorWidget>(PlayerController, UCharacterCreatorWidget::StaticClass());
	if (!Editor)
	{
		Studio->Destroy();
		Studio = nullptr;
		return false;
	}
	Editor->Init(this, Studio, Data, Mode, Current, State ? State->GetPlayerName() : FString(), State ? State->Catchphrase : FString());

	PlayerController->SetViewTargetWithBlend(Studio, 0.f);
	Editor->SetIsFocusable(true);
	Editor->AddToPlayerScreen(8);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Editor->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	return true;
}

void UTrainerAppearanceComponent::CloseEditor(bool bKeep, const FTrainerAppearance& Appearance, const FString& TrainerName, const FString& Catchphrase)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (bKeep)
	{
		FTrainerAppearance Final = Appearance;
		Final.bCreated = true;
		RequestAppearance(Final);
		RequestProfile(TrainerName, Catchphrase);
	}
	if (Editor)
	{
		Editor->RemoveFromParent();
		Editor = nullptr;
	}
	if (PlayerController)
	{
		PlayerController->SetViewTargetWithBlend(Pawn, 0.f);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
	if (Studio)
	{
		Studio->Destroy();
		Studio = nullptr;
	}
}
