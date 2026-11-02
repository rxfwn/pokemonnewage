#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Core/PokemonSaveGame.h"
#include "SaveServerSubsystem.generated.h"

class UPokemonSaveGame;

/**
 *  Serveur de saves (Raspberry Pi à la maison, docs/SERVEUR_SAVES.md) : la save de chaque joueur y est gardée
 *  en plus de la copie sur son PC.
 *  - Au lancement : si la save du serveur est plus récente que celle du PC (partie faite sur un autre PC),
 *    elle remplace la copie locale avant le chargement ; si celle du PC est plus récente (partie hors ligne), elle part.
 *  - À chaque sauvegarde : la save et la carte dresseur partent en arrière-plan. Si le serveur ne répond pas,
 *    le jeu continue et réessaie toutes les minutes.
 *  - Cartes des amis : lues sur le serveur (sinon dans le dossier partagé).
 *  Rien ne se passe tant que l'adresse, le joueur et la clé ne sont pas réglés (commande ServeurSaves),
 *  ni dans les fenêtres de test en plus de l'éditeur.
 */
UCLASS()
class USaveServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static USaveServerSubsystem* Get(const UObject* WorldContextObject);

	/** Adresse, joueur et clé réglés */
	bool IsConfigured() const;

	/** Vrai tant que la save du serveur est en train d'arriver au lancement */
	bool IsSyncing() const { return bSyncing; }

	/** Appelle Callback dès que la save du lancement est prête (tout de suite si rien n'est en cours) */
	void WhenSynced(TFunction<void()> Callback);

	/** Envoie cette save et la carte dresseur, en arrière-plan */
	void Upload(const UPokemonSaveGame* Save);

	/** Redemande les cartes des amis au serveur ; elles arrivent dans GetFriendCards */
	void RefreshFriendCards();

	/** Dernières cartes reçues du serveur (sans la sienne) */
	const TArray<FTrainerCard>& GetFriendCards() const { return FriendCards; }
	bool HasFriendCards() const { return bHasFriendCards; }

	/** Texte d'état pour le menu En ligne : « Serveur de saves : à jour », « hors ligne »... */
	FText GetStatusText() const;

	/** Recontacte le serveur tout de suite (après un changement de réglages) */
	void Resync();

private:

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Verb, const FString& Path) const;

	/** Au lancement : compare la save du serveur et celle du PC */
	void StartSync();
	void FinishSync();

	void SendPending();
	bool RetryTick(float DeltaTime);

	bool bSyncing = false;
	bool bOnline = false;
	bool bHasFriendCards = false;
	FDateTime LastUploadUtc;
	TArray<TFunction<void()>> SyncCallbacks;

	/** Save qui n'a pas pu partir (renvoyée toutes les minutes) */
	TArray<uint8> PendingSave;
	FString PendingSavedAt;
	FString PendingCard;

	TArray<FTrainerCard> FriendCards;
	FTSTicker::FDelegateHandle RetryHandle;
};
