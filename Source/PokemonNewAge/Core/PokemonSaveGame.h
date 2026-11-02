#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pokemon/PokemonInstance.h"
#include "PokemonSaveGame.generated.h"

class APlayerController;

/**
 *  Ce qu'on sauvegarde d'un joueur (SCRUM-52) : tout son PlayerState et l'endroit où il se tient.
 *  En multijoueur chacun garde sa sauvegarde sur son PC (choix de SCRUM-68) : le client écrit sa copie répliquée,
 *  et au lancement il l'envoie au serveur qui la remet dans son PlayerState.
 */
USTRUCT(BlueprintType)
struct FPlayerSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FString TrainerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 TrainerNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FPokemonInstance> Party;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FPokemonInstance> StoragePC;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FItemStack> Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 Money = 3000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FName> Badges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FName> StoryFlags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FName> PokedexSeen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FName> PokedexCaught;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FName CurrentLocation;

	/** Dernier Centre Pokémon où l'équipe a été soignée (APokemonCenterPoint), None = jamais */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FName LastHealPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FVector LastHealLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FRotator LastHealRotation = FRotator::ZeroRotator;

	/** Position du personnage. bHasTransform faux = point de départ de la map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	bool bHasTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FRotator PlayerRotation = FRotator::ZeroRotator;

	/** Temps de jeu total en secondes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	float PlayTimeSeconds = 0.f;

	/** Compteur de pas du Pokédex */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 StepCount = 0;
};

/** Un Pokémon de l'équipe tel qu'on le montre sur la carte dresseur d'un ami */
USTRUCT(BlueprintType)
struct FTrainerCardPokemon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FName Species;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FString Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 Level = 1;
};

/**
 *  Carte dresseur partagée avec les amis (choix du 2026-11-02) : un petit fichier JSON écrit à chaque sauvegarde dans le
 *  dossier partagé (OneDrive, Google Drive...), lu par les autres pour voir où on en est, même hors ligne.
 *  En lecture seule : ni échange ni combat avec un joueur absent. Voir docs/MULTIJOUEUR.md.
 */
USTRUCT(BlueprintType)
struct FTrainerCard
{
	GENERATED_BODY()

	/** Pour lire les vieilles cartes si le format change */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 Version = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FString TrainerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 TrainerNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FTrainerCardPokemon> Party;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FName> Badges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 PokedexSeen = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	int32 PokedexCaught = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FName CurrentLocation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	float PlayTimeSeconds = 0.f;

	/** Heure de la sauvegarde (UTC) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save")
	FDateTime SavedAt;
};

/** Le fichier de sauvegarde (Saved/SaveGames/Partie.sav ; Partie_PIE1.sav... pour les fenêtres de test en plus dans l'éditeur). */
UCLASS()
class UPokemonSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** Pour lire les vieilles sauvegardes si le format change */
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FDateTime SavedAt;

	/** Heure de la sauvegarde en UTC : le serveur de saves garde la plus récente (docs/SERVEUR_SAVES.md) */
	UPROPERTY()
	FDateTime SavedAtUtc;

	/** Nom de la map (L_Kanto) */
	UPROPERTY()
	FString MapName;

	UPROPERTY()
	FPlayerSaveData Player;
};

/** Réglages du joueur, indépendants de la partie (Saved/SaveGames/Options.sav). */
UCLASS()
class UPokemonOptionsSave : public USaveGame
{
	GENERATED_BODY()

public:

	/** 0 lente, 1 normale, 2 rapide (boîte de dialogue, combat) */
	UPROPERTY()
	int32 TextSpeed = 1;

	/** Volume général, 0 à 1 */
	UPROPERTY()
	float MasterVolume = 1.f;

	/** Musique, 0 à 1 (classe de son SC_Music de Content/Audio) */
	UPROPERTY()
	float MusicVolume = 1.f;

	/** Vfx : bruitages des effets, des attaques et de l'interface, 0 à 1 (SC_Effects) */
	UPROPERTY()
	float EffectsVolume = 1.f;

	/** Bruits de pas, 0 à 1 (SC_Footsteps) ; séparés à la demande de L'amie le 2026-11-01 */
	UPROPERTY()
	float FootstepsVolume = 1.f;

	/** Météo (pluie, vent...), 0 à 1 (SC_Weather) */
	UPROPERTY()
	float WeatherVolume = 1.f;

	/** Sensibilité de la caméra à la souris, 0,5 à 2 */
	UPROPERTY()
	float CameraSensitivity = 1.f;

	UPROPERTY()
	bool bInvertCameraY = false;

	/** Champ de vision de la caméra, en degrés (menu Échap > Vidéo) */
	UPROPERTY()
	float FieldOfView = 90.f;

	/** Boussole, lieu, heure et météo en bas à droite (menu Échap > Jeu) */
	UPROPERTY()
	bool bShowHUD = true;

	/** Sauvegarde automatique (menu Échap > Jeu) */
	UPROPERTY()
	bool bAutoSave = true;

	/** Touches changées dans Options > Commandes : commande (Jump_Key, Forward_Key, Interact_Pad...) -> touche */
	UPROPERTY()
	TMap<FName, FKey> KeyBindings;

	/** Dossier partagé où vont les cartes dresseur (OneDrive, Google Drive...). Vide = pas de cartes. Commande DossierAmis */
	UPROPERTY()
	FString FriendsFolder;

	/** Adresse du dernier monde rejoint (IP du réseau local ou de Tailscale), pour « Rejoindre » du menu En ligne */
	UPROPERTY()
	FString LastHostAddress;

	/** Serveur de saves (Raspberry Pi, docs/SERVEUR_SAVES.md) : adresse (http://100.x.x.x:8787), nom du joueur et sa clé.
	 *  Vide = pas de serveur, la save reste seulement sur le PC. Commande ServeurSaves */
	UPROPERTY()
	FString SaveServerUrl;

	UPROPERTY()
	FString SaveServerPlayer;

	UPROPERTY()
	FString SaveServerKey;
};

/** Sauvegarder, charger, et lire les réglages (C++ et Blueprint). */
UCLASS()
class UPokemonSaveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static const FString SaveSlot;
	static const FString OptionsSlot;

	/**
	 *  Nom du fichier de partie : « Partie », ou « Partie_PIE1 », « Partie_PIE2 »... pour les fenêtres de test en plus
	 *  dans l'éditeur (4 joueurs sur un même PC ne s'écrasent pas). Chaque joueur a son fichier sur son PC.
	 */
	static FString GetSaveSlot(const UObject* WorldContextObject);

	/**
	 *  Écrit la partie du joueur local sur son PC, puis sa carte dresseur dans le dossier des amis.
	 *  Faux si pas de PlayerState, si sa sauvegarde n'est pas encore revenue du serveur, ou si l'écriture échoue
	 */
	UFUNCTION(BlueprintCallable, Category="Save")
	static bool SaveGame(APlayerController* Player);

	UFUNCTION(BlueprintPure, Category="Save", meta=(WorldContext="WorldContextObject"))
	static bool HasSaveGame(const UObject* WorldContextObject);

	/** Lit la sauvegarde, nullptr s'il n'y en a pas */
	UFUNCTION(BlueprintCallable, Category="Save", meta=(WorldContext="WorldContextObject"))
	static UPokemonSaveGame* LoadSaveGame(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category="Save", meta=(WorldContext="WorldContextObject"))
	static bool DeleteSaveGame(const UObject* WorldContextObject);

	// ------------------------------------------------ Cartes dresseur des amis

	/** Carte dresseur faite d'une sauvegarde */
	static FTrainerCard MakeTrainerCard(const FPlayerSaveData& Data);

	/** Écrit la carte dans le dossier des amis (rien si le dossier n'est pas choisi). Faux si l'écriture échoue */
	static bool WriteTrainerCard(const FPlayerSaveData& Data);

	/** Cartes des amis (serveur de saves, sinon dossier partagé), sauf la sienne, la plus récente d'abord */
	UFUNCTION(BlueprintCallable, Category="Save", meta=(WorldContext="WorldContextObject"))
	static TArray<FTrainerCard> LoadFriendCards(const UObject* WorldContextObject, const FString& OwnName, int32 OwnNumber);

	/** Réglages (créés avec les valeurs par défaut s'ils n'existent pas) */
	UFUNCTION(BlueprintCallable, Category="Save")
	static UPokemonOptionsSave* GetOptions();

	UFUNCTION(BlueprintCallable, Category="Save")
	static void SaveOptions(UPokemonOptionsSave* Options);

	/** Applique le volume général tout de suite */
	UFUNCTION(BlueprintCallable, Category="Save", meta=(WorldContext="WorldContextObject"))
	static void ApplyOptions(const UObject* WorldContextObject, const UPokemonOptionsSave* Options);
};
