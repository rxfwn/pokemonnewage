#include "Core/SaveServerSubsystem.h"
#include "Core/PokemonSaveGame.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "PokemonNewAge.h"

#define LOCTEXT_NAMESPACE "SaveServer"

namespace PokemonSaveServer
{
	/** Le lancement n'attend jamais plus que ça le serveur */
	constexpr float SyncTimeout = 4.f;
	constexpr float RequestTimeout = 10.f;
	constexpr float RetryInterval = 60.f;

	FString ToIso(const FDateTime& Utc)
	{
		return Utc.ToString(TEXT("%Y-%m-%dT%H:%M:%SZ"));
	}
}

USaveServerSubsystem* USaveServerSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USaveServerSubsystem>() : nullptr;
}

void USaveServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSync();
	RetryHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USaveServerSubsystem::RetryTick), PokemonSaveServer::RetryInterval);
}

void USaveServerSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RetryHandle);
	Super::Deinitialize();
}

bool USaveServerSubsystem::IsConfigured() const
{
	// Les fenêtres de test en plus dans l'éditeur ont leurs propres saves locales : elles ne touchent pas au serveur
	const UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance && GameInstance->GetWorldContext() && GameInstance->GetWorldContext()->PIEInstance > 0)
	{
		return false;
	}
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	return Options && !Options->SaveServerUrl.IsEmpty() && !Options->SaveServerPlayer.IsEmpty() && !Options->SaveServerKey.IsEmpty();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> USaveServerSubsystem::MakeRequest(const FString& Verb, const FString& Path) const
{
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	FString Url = Options ? Options->SaveServerUrl : FString();
	Url.RemoveFromEnd(TEXT("/"));
	if (!Url.StartsWith(TEXT("http")))
	{
		Url = TEXT("http://") + Url;
	}
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(Verb);
	Request->SetURL(Url + Path);
	Request->SetHeader(TEXT("X-Pokemon-Key"), Options ? Options->SaveServerKey : FString());
	Request->SetTimeout(PokemonSaveServer::RequestTimeout);
	return Request;
}

// ---------------------------------------------------------------- Lancement

void USaveServerSubsystem::StartSync()
{
	if (!IsConfigured() || bSyncing)
	{
		return;
	}
	bSyncing = true;
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), TEXT("/saves/") + Options->SaveServerPlayer);
	Request->SetTimeout(PokemonSaveServer::SyncTimeout);
	TWeakObjectPtr<USaveServerSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda([WeakThis](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		USaveServerSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		const int32 Code = bConnected && Response.IsValid() ? Response->GetResponseCode() : 0;
		Self->bOnline = Code == 200 || Code == 404;
		UPokemonSaveGame* Local = UPokemonSaveLibrary::LoadSaveGame(Self->GetGameInstance());

		if (Code == 200)
		{
			FDateTime ServerUtc;
			FDateTime::ParseIso8601(*Response->GetHeader(TEXT("X-Saved-At")), ServerUtc);
			const FDateTime LocalUtc = Local ? Local->SavedAtUtc : FDateTime::MinValue();
			if (!Local || ServerUtc > LocalUtc)
			{
				// Partie plus récente faite sur un autre PC : elle devient la copie locale
				USaveGame* Downloaded = UGameplayStatics::LoadGameFromMemory(Response->GetContent());
				if (Cast<UPokemonSaveGame>(Downloaded))
				{
					UGameplayStatics::SaveGameToSlot(Downloaded, UPokemonSaveLibrary::GetSaveSlot(Self->GetGameInstance()), 0);
					UE_LOG(LogPokemonNewAge, Log, TEXT("Serveur de saves : save du serveur recuperee (%s)"), *ServerUtc.ToString());
				}
			}
			else if (LocalUtc > ServerUtc)
			{
				// Partie faite hors ligne : le serveur la reçoit
				Self->Upload(Local);
			}
		}
		else if (Code == 404 && Local)
		{
			Self->Upload(Local);
		}
		else if (Code != 404)
		{
			UE_LOG(LogPokemonNewAge, Warning, TEXT("Serveur de saves injoignable (code %d) : on joue avec la save du PC"), Code);
		}
		Self->FinishSync();
		Self->RefreshFriendCards();
	});
	Request->ProcessRequest();
}

void USaveServerSubsystem::FinishSync()
{
	bSyncing = false;
	TArray<TFunction<void()>> Callbacks = MoveTemp(SyncCallbacks);
	for (TFunction<void()>& Callback : Callbacks)
	{
		Callback();
	}
}

void USaveServerSubsystem::WhenSynced(TFunction<void()> Callback)
{
	if (bSyncing)
	{
		SyncCallbacks.Add(MoveTemp(Callback));
	}
	else
	{
		Callback();
	}
}

void USaveServerSubsystem::Resync()
{
	bHasFriendCards = false;
	FriendCards.Reset();
	StartSync();
}

// ---------------------------------------------------------------- Envoi

void USaveServerSubsystem::Upload(const UPokemonSaveGame* Save)
{
	if (!Save || !IsConfigured())
	{
		return;
	}
	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(const_cast<UPokemonSaveGame*>(Save), Bytes))
	{
		return;
	}
	PendingSave = MoveTemp(Bytes);
	PendingSavedAt = PokemonSaveServer::ToIso(Save->SavedAtUtc);
	FJsonObjectConverter::UStructToJsonObjectString(UPokemonSaveLibrary::MakeTrainerCard(Save->Player), PendingCard);
	SendPending();
}

void USaveServerSubsystem::SendPending()
{
	if (PendingSave.Num() == 0 || !IsConfigured())
	{
		return;
	}
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("PUT"), TEXT("/saves/") + Options->SaveServerPlayer);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	Request->SetHeader(TEXT("X-Saved-At"), PendingSavedAt);
	Request->SetContent(PendingSave);
	const int32 SentSize = PendingSave.Num();
	const FString SentAt = PendingSavedAt;
	TWeakObjectPtr<USaveServerSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda([WeakThis, SentSize, SentAt](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		USaveServerSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		const int32 Code = bConnected && Response.IsValid() ? Response->GetResponseCode() : 0;
		Self->bOnline = Code > 0;
		if (Code == 200)
		{
			Self->LastUploadUtc = FDateTime::UtcNow();
			// Une save plus récente a pu être faite pendant l'envoi : on ne retire que celle qui est partie
			if (Self->PendingSavedAt == SentAt && Self->PendingSave.Num() == SentSize)
			{
				Self->PendingSave.Reset();
			}
		}
		else
		{
			UE_LOG(LogPokemonNewAge, Warning, TEXT("Save non envoyee au serveur (code %d), nouvel essai dans une minute"), Code);
		}
	});
	Request->ProcessRequest();

	// La carte dresseur part avec (sans réessai : la prochaine sauvegarde la renverra)
	if (!PendingCard.IsEmpty())
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CardRequest = MakeRequest(TEXT("PUT"), TEXT("/cards/") + Options->SaveServerPlayer);
		CardRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		CardRequest->SetContentAsString(PendingCard);
		CardRequest->ProcessRequest();
	}
}

bool USaveServerSubsystem::RetryTick(float DeltaTime)
{
	SendPending();
	return true;
}

// ---------------------------------------------------------------- Cartes des amis

void USaveServerSubsystem::RefreshFriendCards()
{
	if (!IsConfigured())
	{
		return;
	}
	TWeakObjectPtr<USaveServerSubsystem> WeakThis(this);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = MakeRequest(TEXT("GET"), TEXT("/cards"));
	Request->OnProcessRequestComplete().BindLambda([WeakThis](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		USaveServerSubsystem* Self = WeakThis.Get();
		if (!Self || !bConnected || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			return;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("cards"), List))
		{
			return;
		}
		const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
		const FString Me = Options ? Options->SaveServerPlayer : FString();
		Self->FriendCards.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			FTrainerCard Card;
			if (!Value.IsValid() || !Value->TryGetObject(Object) || !FJsonObjectConverter::JsonObjectToUStruct((*Object).ToSharedRef(), &Card))
			{
				continue;
			}
			FString Player;
			(*Object)->TryGetStringField(TEXT("Player"), Player);
			if (Player != Me && !Card.TrainerName.IsEmpty())
			{
				Self->FriendCards.Add(MoveTemp(Card));
			}
		}
		Self->FriendCards.Sort([](const FTrainerCard& A, const FTrainerCard& B) { return A.SavedAt > B.SavedAt; });
		Self->bHasFriendCards = true;
	});
	Request->ProcessRequest();
}

FText USaveServerSubsystem::GetStatusText() const
{
	if (!IsConfigured())
	{
		return LOCTEXT("NotSet", "non réglé (console : ServeurSaves)");
	}
	if (bSyncing)
	{
		return LOCTEXT("Syncing", "connexion...");
	}
	if (!bOnline)
	{
		return LOCTEXT("Offline", "hors ligne (save gardée sur le PC)");
	}
	return PendingSave.Num() > 0 ? LOCTEXT("Pending", "envoi en attente") : LOCTEXT("UpToDate", "à jour");
}

#undef LOCTEXT_NAMESPACE
