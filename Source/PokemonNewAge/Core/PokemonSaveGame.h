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

/** Le fichier de sauvegarde (Saved/SaveGames/Partie.sav). */
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
};

/** Sauvegarder, charger, et lire les réglages (C++ et Blueprint). */
UCLASS()
class UPokemonSaveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static const FString SaveSlot;
	static const FString OptionsSlot;

	/** Écrit la partie du joueur local sur son PC. Faux si pas de PlayerState ou si l'écriture échoue */
	UFUNCTION(BlueprintCallable, Category="Save")
	static bool SaveGame(APlayerController* Player);

	UFUNCTION(BlueprintPure, Category="Save")
	static bool HasSaveGame();

	/** Lit la sauvegarde, nullptr s'il n'y en a pas */
	UFUNCTION(BlueprintCallable, Category="Save")
	static UPokemonSaveGame* LoadSaveGame();

	UFUNCTION(BlueprintCallable, Category="Save")
	static bool DeleteSaveGame();

	/** Réglages (créés avec les valeurs par défaut s'ils n'existent pas) */
	UFUNCTION(BlueprintCallable, Category="Save")
	static UPokemonOptionsSave* GetOptions();

	UFUNCTION(BlueprintCallable, Category="Save")
	static void SaveOptions(UPokemonOptionsSave* Options);

	/** Applique le volume général tout de suite */
	UFUNCTION(BlueprintCallable, Category="Save", meta=(WorldContext="WorldContextObject"))
	static void ApplyOptions(const UObject* WorldContextObject, const UPokemonOptionsSave* Options);
};
