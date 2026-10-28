#include "Dialogue/DialogueTriggerVolume.h"
#include "Components/BoxComponent.h"
#include "Core/PokemonPlayerState.h"
#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueLibrary.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ADialogueTriggerVolume::ADialogueTriggerVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(200.f, 200.f, 150.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(255, 170, 40);
	RootComponent = Box;
}

void ADialogueTriggerVolume::BeginPlay()
{
	Super::BeginPlay();

	// Les scènes sont décidées par le serveur, pour le joueur qui entre
	if (HasAuthority())
	{
		Box->OnComponentBeginOverlap.AddDynamic(this, &ADialogueTriggerVolume::OnBoxBegin);
	}
}

void ADialogueTriggerVolume::OnBoxBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* PlayerController = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	UDialogueComponent* Dialogue = PlayerController ? PlayerController->FindComponentByClass<UDialogueComponent>() : nullptr;
	const APokemonPlayerState* State = PlayerController ? PlayerController->GetPlayerState<APokemonPlayerState>() : nullptr;
	if (!Dialogue || !State || Dialogue->IsInConversation())
	{
		return;
	}

	const UDataTable* Dialogues = Dialogue->GetDialoguesTable();
	const FDialogueRow* Row = Dialogues ? Dialogues->FindRow<FDialogueRow>(ConversationId, TEXT("DialogueTrigger"), false) : nullptr;
	if (Row && UDialogueLibrary::AreConditionsMet(*Row, State->StoryFlags))
	{
		Dialogue->PlayConversation(ConversationId);
	}
}

TArray<FString> ADialogueTriggerVolume::GetConversationOptions() const
{
	TArray<FString> Options;
	if (const UDataTable* Dialogues = LoadObject<UDataTable>(nullptr, TEXT("/Game/World/Dialogues/DT_Dialogues.DT_Dialogues")))
	{
		Dialogues->ForeachRow<FDialogueRow>(TEXT("GetConversationOptions"), [&Options](const FName& Key, const FDialogueRow& Row)
		{
			if (Row.Trigger == EDialogueTrigger::Script)
			{
				Options.Add(Key.ToString());
			}
		});
	}
	return Options;
}
