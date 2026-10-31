// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/PokemonSaveGame.h"
#include "Battle/BattleTypes.h"
#include "PokemonNewAgePlayerController.generated.h"

class UCaptureComponent;
class UInputMappingContext;
class UUserWidget;
class UInputAction;
class UGameMenuWidget;
class UBattleComponent;
class UBattleHUDWidget;
class UProgressionComponent;
class APokedexDevice;
class UGameHUDWidget;
class USettingsMenuWidget;
class UPokedexModel;
struct FInputActionInstance;
enum class EPokedexMenu : uint8;

/** Une commande dont on peut changer la touche (menu Échap > Commandes). */
struct FPokemonBindableKey
{
	/** Forward_Key, Jump_Pad... (clé de UPokemonOptionsSave::KeyBindings) */
	FName Id;
	FText Name;
	bool bGamepad = false;
	FKey DefaultKey;
	FKey Key;
};
class UDialogueComponent;
class UPokemonCenterComponent;
class UShopComponent;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class APokemonNewAgePlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Lowest camera pitch, looking down at the character (degrees) */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = -89, ClampMax = 0, Units = "Degrees"))
	float CameraPitchMin = -60.f;

	/** Highest camera pitch, looking up from behind the character (degrees) */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 89, Units = "Degrees"))
	float CameraPitchMax = 30.f;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Touche du menu en jeu (Tab, Start). Créée par le code si elle n'est pas réglée dans le Blueprint */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MenuAction;

	/** Contexte créé par le code pour la touche du menu par défaut */
	UPROPERTY(Transient)
	UInputMappingContext* DefaultMenuContext;

	/** Menu à afficher (un Widget Blueprint enfant de GameMenuWidget pour l'habiller) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UGameMenuWidget> GameMenuClass;

	/** Menu ouvert, sinon nul */
	UPROPERTY(Transient)
	TObjectPtr<UGameMenuWidget> GameMenu;

	/** Raccourcis (P Pokédex, B Sac...) et menu Échap, créés par le code, modifiables dans Échap > Commandes */
	UPROPERTY(Transient)
	UInputMappingContext* ShortcutContext;

	/** Une action par raccourci, dans l'ordre de PokemonShortcuts::Shortcuts */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ShortcutActions;

	/** Échap ou Select : menu Paramètres */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SettingsAction;

	/** Appareil photo : prendre la photo, quitter (contexte ajouté par-dessus les autres pendant le mode photo) */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PhotoContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PhotoShootAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PhotoExitAction;

	/** HUD en jeu (maquette de L'amie : icône Pokédex, nom du lieu, heure, météo, boussole). Un Widget Blueprint enfant pour l'habiller */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UGameHUDWidget> GameHUDClass;

	UPROPERTY(Transient)
	TObjectPtr<UGameHUDWidget> GameHUD;

	/** Menu Échap (Paramètres). Un Widget Blueprint enfant de SettingsMenuWidget pour l'habiller */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<USettingsMenuWidget> SettingsMenuClass;

	UPROPERTY(Transient)
	TObjectPtr<USettingsMenuWidget> SettingsMenu;

	/** Gardé ici pour que la carte, les lieux et les icônes ne soient pas déchargés */
	UPROPERTY(Transient)
	TObjectPtr<UPokedexModel> PokedexModel;

	/** Sort le Pokédex 3D, sur l'accueil (Menu nul) ou sur une fonction. Faux sans DA_PokedexModel ou si bUsePokedex3D est décoché */
	bool SpawnPokedexDevice(const EPokedexMenu* Menu);

	/** Un raccourci du menu est pressé en jeu */
	void HandleShortcut(const FInputActionInstance& Instance);

	void HandleSettingsAction();
	void TakePhoto();

	/** Le Pokédex est en 3D (SCRUM-64). Décoché, ou sans DA_PokedexModel : Pokédex à plat (UGameMenuWidget) */
	UPROPERTY(EditAnywhere, Category="UI")
	bool bUsePokedex3D = true;

	/** Sauvegarde automatique : toutes les N secondes, et en arrivant dans un nouveau lieu (si Échap > Jeu le permet) */
	UPROPERTY(EditAnywhere, Category="Save", meta=(ClampMin=30, Units="Seconds"))
	float AutoSaveInterval = 300.f;

	/** Pas de deux sauvegardes automatiques à moins de N secondes d'écart */
	UPROPERTY(EditAnywhere, Category="Save", meta=(ClampMin=5, Units="Seconds"))
	float AutoSaveMinGap = 30.f;

	float AutoSaveTimer = 0.f;
	float TimeSinceAutoSave = 0.f;

	/** Lieu actuel (index dans DA_PokedexModel > MapPlaces), relu toutes les 0,5 s */
	int32 CurrentPlace = INDEX_NONE;
	float PlaceTimer = 0.f;
	/** Faux jusqu'au premier lieu trouvé (pas de sauvegarde automatique à l'arrivée en jeu) */
	bool bPlaceKnown = false;

	/** Destination du GPS (index dans MapPlaces) */
	int32 GpsTarget = INDEX_NONE;

	bool bPhotoMode = false;

	/** Serveur : compteur de pas (position au tick précédent et distance pas encore comptée) */
	FVector LastStepLocation = FVector::ZeroVector;
	bool bHasStepLocation = false;
	float StepDistance = 0.f;

	/** Serveur : ajoute les pas du personnage au PlayerState */
	void CountSteps();

	/** Lieu du joueur, sauvegarde automatique */
	void UpdatePlace(float DeltaSeconds);
	void UpdateAutoSave(float DeltaSeconds);

	/** Sauvegarde automatique si rien ne l'empêche (combat, menu, dialogue) */
	void TryAutoSave();

	/** Classe du Pokédex 3D (un Blueprint enfant de PokedexDevice pour régler lumières, flou...) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<APokedexDevice> PokedexDeviceClass;

	/** Pokédex 3D ouvert, sinon nul */
	UPROPERTY(Transient)
	TObjectPtr<APokedexDevice> PokedexDevice;

	/** Écran de combat (SCRUM-36), un Widget Blueprint enfant de BattleHUDWidget pour l'habiller */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UBattleHUDWidget> BattleHUDClass;

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDWidget> BattleHUD;

	/** Charge la sauvegarde au lancement s'il y en a une (SCRUM-52) */
	UPROPERTY(EditAnywhere, Category="Save")
	bool bLoadSaveOnStart = true;

	/** Serveur : position chargée à donner au personnage dès qu'il existe */
	bool bHasPendingTransform = false;
	FVector PendingLocation = FVector::ZeroVector;
	FRotator PendingRotation = FRotator::ZeroRotator;

	/** Serveur : place le personnage à la position sauvegardée */
	void ApplyPendingTransform();

	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void HandleBattleStarted(const FBattleSnapshot& Snapshot);

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Compteur de pas (serveur), lieu, sauvegarde automatique (joueur local) */
	virtual void Tick(float DeltaSeconds) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

public:

	APokemonNewAgePlayerController();

	/** Combats de ce joueur : envoie ses choix au serveur et reçoit les événements (docs/COMBAT.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Battle")
	TObjectPtr<UBattleComponent> BattleComponent;

	/** Niveaux, attaques et évolutions hors combat : objets du sac, séquence d'évolution 3D (docs/PROGRESSION.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Progression")
	TObjectPtr<UProgressionComponent> ProgressionComponent;

	/** Conversations de ce joueur et boîte de dialogue (docs/DIALOGUES.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dialogue")
	TObjectPtr<UDialogueComponent> DialogueComponent;

	/** Centre Pokémon : point de retour après K.O., PC de Léo (docs/CENTRE_ET_BOUTIQUE.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Town")
	TObjectPtr<UPokemonCenterComponent> PokemonCenterComponent;

	/** Boutique Pokémon : achat et vente (docs/CENTRE_ET_BOUTIQUE.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Town")
	TObjectPtr<UShopComponent> ShopComponent;

	/** Capture, surnom, PC de Léo, lancer de Ball en 3D (docs/CAPTURE.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Capture")
	TObjectPtr<UCaptureComponent> CaptureComponent;

	/** Ouvre le menu en jeu s'il est fermé, sinon le ferme */
	UFUNCTION(BlueprintCallable, Category="UI")
	void ToggleGameMenu();

	/** Ouvre le menu en jeu et met le jeu en pause */
	UFUNCTION(BlueprintCallable, Category="UI")
	void OpenGameMenu();

	/** Ouvre le Pokédex directement sur une fonction (raccourcis) */
	void OpenGameMenuAt(EPokedexMenu Menu);

	/** Ferme le menu en jeu et relance le jeu */
	UFUNCTION(BlueprintCallable, Category="UI")
	void CloseGameMenu();

	/** Vrai si le menu en jeu est ouvert */
	UFUNCTION(BlueprintPure, Category="UI")
	bool IsGameMenuOpen() const { return GameMenu != nullptr || PokedexDevice != nullptr; }

	// ------------------------------------------------ Menu Échap (Paramètres)

	UFUNCTION(BlueprintCallable, Category="UI")
	void OpenSettingsMenu();

	UFUNCTION(BlueprintCallable, Category="UI")
	void CloseSettingsMenu();

	bool IsSettingsMenuOpen() const { return SettingsMenu != nullptr; }

	// ------------------------------------------------ Pokédex : appareil photo, GPS, lieu

	/** Range le Pokédex et passe en mode photo (HUD caché ; clic, Entrée ou A : photo ; Échap ou B : quitter) */
	void StartPhotoMode();
	void StopPhotoMode();
	bool IsInPhotoMode() const { return bPhotoMode; }

	/** Destination du GPS (index dans DA_PokedexModel > MapPlaces), INDEX_NONE = pas de GPS */
	int32 GetGpsTarget() const { return GpsTarget; }
	void SetGpsTarget(int32 PlaceIndex);

	/** Lieu où se trouve le joueur (index dans MapPlaces), INDEX_NONE en pleine nature ou hors de Kanto */
	int32 GetCurrentPlace() const { return CurrentPlace; }

	/** Appelé par le Pokédex 3D à la fin de son animation de fermeture */
	void OnPokedexClosed(APokedexDevice* Device);

	/** Vrai tant que l'écran de combat est affiché (messages de fin compris) */
	bool IsBattleHUDOpen() const;

	/** Appelé par l'écran de combat quand il se retire */
	void OnBattleHUDClosed();

	/** Réglages du joueur (Options) : volume, sensibilité et sens de la caméra */
	void ApplyGameOptions();

	// ------------------------------------------------ Touches (Options > Commandes)

	/** Commandes qu'on peut changer, clavier puis manette, avec leur touche actuelle */
	TArray<FPokemonBindableKey> GetBindableKeys() const;

	/** Change la touche d'une commande (si une autre commande l'avait, elles échangent) et l'enregistre */
	void SetKeyBinding(FName Id, const FKey& Key);

	/** Remet toutes les touches d'origine */
	void ResetKeyBindings();

	/** Touche clavier actuelle du raccourci d'une fonction du Pokédex (invalide s'il n'en a pas) */
	FKey GetShortcutKey(EPokedexMenu Menu) const;

	/** Fonction du Pokédex dont le raccourci est cette touche */
	bool FindShortcut(const FKey& Key, EPokedexMenu& OutMenu) const;

	/** Touche qui ouvre et ferme le Pokédex (Tab, Start ou celle choisie) */
	bool IsMenuKey(const FKey& Key) const;

	/** Touche du menu Échap (Échap, Select ou celle choisie) */
	bool IsSettingsKey(const FKey& Key) const;

	/** Touche clavier actuelle du Pokédex (affichée sous l'icône du HUD) */
	FKey GetMenuKey() const;

	/** Remplace les contextes d'entrée par des copies avec les touches choisies (au lancement et après un changement) */
	void ApplyKeyBindings();

	/** Lus par le personnage pour la caméra */
	float CameraSensitivity = 1.f;
	bool bInvertCameraY = false;

	// ------------------------------------------------ Demandes au serveur depuis les menus (SCRUM-68)

	/** Menu Pokémon > Déplacer */
	UFUNCTION(Server, Reliable)
	void ServerSwapPartyPokemon(int32 IndexA, int32 IndexB);

	/** Sac > objet de soin hors combat */
	UFUNCTION(Server, Reliable)
	void ServerUseFieldItem(FName Item, int32 PartyIndex);

	/** Menu Pokémon > Donner un objet */
	UFUNCTION(Server, Reliable)
	void ServerGiveHeldItem(int32 PartyIndex, FName Item);

	/** Menu Pokémon > Reprendre l'objet */
	UFUNCTION(Server, Reliable)
	void ServerTakeHeldItem(int32 PartyIndex);

	/** Au lancement : la sauvegarde du joueur (lue sur son PC) est remise dans son PlayerState */
	UFUNCTION(Server, Reliable)
	void ServerLoadSave(const FPlayerSaveData& Data);

	/** Sauvegarde tout de suite (console) */
	UFUNCTION(Exec)
	void Sauvegarder();

	/** Efface la sauvegarde : le prochain lancement repart de zéro (console) */
	UFUNCTION(Exec)
	void NouvellePartie();

	// Commandes de test du combat, à taper dans la console (touche ²) tant que l'UI n'existe pas.
	// Voir docs/COMBAT.md. Les numéros commencent à 1 comme à l'écran.

	/** CombatSauvage Pidgey 3 */
	UFUNCTION(Exec)
	void CombatSauvage(FName Species, int32 Level);

	/** CombatRencontre Route1_Walk */
	UFUNCTION(Exec)
	void CombatRencontre(FName EncounterTable);

	/** CombatDresseur Brock (ou RivalLab) */
	UFUNCTION(Exec)
	void CombatDresseur(FName TrainerGroup);

	/** Attaque 1 à 4 */
	UFUNCTION(Exec)
	void Attaque(int32 Slot);

	/** Changer 1 à 6 (aussi pour envoyer un Pokémon après un K.O.) */
	UFUNCTION(Exec)
	void Changer(int32 PartySlot);

	/** Objet PokeBall, Objet Potion 1 */
	UFUNCTION(Exec)
	void Objet(FName Item, int32 PartySlot = 1);

	UFUNCTION(Exec)
	void Fuite();

	/** 5e attaque : Oublier 1 à 4, Oublier 0 pour ne pas l'apprendre */
	UFUNCTION(Exec)
	void Oublier(int32 Slot);

	/** Évolution proposée, sans la séquence : Evoluer 1 (Evoluer 1 0 pour l'annuler) */
	UFUNCTION(Exec)
	void Evoluer(int32 PartySlot, int32 bAccept = 1);

	// Progression (docs/PROGRESSION.md)

	/** Objet de progression du sac sur un Pokémon : UtiliserObjet ThunderStone 2, UtiliserObjet TM39 1 */
	UFUNCTION(Exec)
	void UtiliserObjet(FName Item, int32 PartySlot = 1);

	/** Donne et utilise des Super Bonbons : SuperBonbon 1 5 */
	UFUNCTION(Exec)
	void SuperBonbon(int32 PartySlot = 1, int32 Count = 1);

	/** XP hors combat (hôte seulement) : DonnerXP 1 500 */
	UFUNCTION(Exec)
	void DonnerXP(int32 PartySlot, int32 Amount);

	/** Rejoue la séquence d'évolution sans rien changer : VoirEvolution Bulbasaur Ivysaur */
	UFUNCTION(Exec)
	void VoirEvolution(FName FromSpecies, FName ToSpecies);

	// Commandes de test de la capture et du PC (docs/CAPTURE.md)

	/** Surnom Bulbi : surnom du Pokémon qui vient d'être capturé (sans cette commande, il garde son nom) */
	UFUNCTION(Exec)
	void Surnom(const FString& Nickname);

	/** Affiche le contenu du PC de Léo */
	UFUNCTION(Exec)
	void VoirPC();

	/** Deposer 2 : dépose le 2e Pokémon de l'équipe au PC */
	UFUNCTION(Exec)
	void Deposer(int32 PartySlot);

	/** Retirer 1 : prend le 1er Pokémon du PC dans l'équipe */
	UFUNCTION(Exec)
	void Retirer(int32 StorageSlot);

	/** Echanger 2 1 : échange le 2e de l'équipe et le 1er du PC */
	UFUNCTION(Exec)
	void Echanger(int32 PartySlot, int32 StorageSlot);

	/** Relacher 1 : relâche le 1er Pokémon du PC (définitif) */
	UFUNCTION(Exec)
	void Relacher(int32 StorageSlot);

	/** Rencontres 0 : plus de Pokémon sauvages dans les herbes (Rencontres 1 pour les remettre) */
	UFUNCTION(Exec)
	void Rencontres(int32 bEnabled);

	/** DonnerPokemon Bulbasaur 5 */
	UFUNCTION(Exec)
	void DonnerPokemon(FName Species, int32 Level);

	/** DonnerObjet PokeBall 10 */
	UFUNCTION(Exec)
	void DonnerObjet(FName Item, int32 Count = 1);

	/** Soigne l'équipe */
	UFUNCTION(Exec)
	void Soigner();

	/** Affiche l'équipe dans le journal et à l'écran */
	UFUNCTION(Exec)
	void Equipe();

	// Commandes de test des dialogues (docs/DIALOGUES.md)

	/** Parler ProfOak : comme si le joueur parlait à ce PNJ */
	UFUNCTION(Exec)
	void Parler(FName Npc);

	/** Dialogue OaksLab_ChooseBulbasaur : joue une conversation par son nom, sans vérifier les drapeaux */
	UFUNCTION(Exec)
	void Dialogue(FName ConversationId);

	/** Drapeau GotStarter : donne un drapeau d'histoire */
	UFUNCTION(Exec)
	void Drapeau(FName Flag);

	/** Affiche les drapeaux d'histoire du joueur */
	UFUNCTION(Exec)
	void Drapeaux();

	// Commandes de test du Centre et de la Boutique (docs/CENTRE_ET_BOUTIQUE.md)

	/** Boutique ViridianPokeMart : ouvre une boutique de DT_Shops sans parler au vendeur */
	UFUNCTION(Exec)
	void Boutique(FName ShopId);

	/** Acheter PokeBall 10 : achat dans la boutique ouverte */
	UFUNCTION(Exec)
	void Acheter(FName Item, int32 Count = 1);

	/** Vendre Potion 2 : vente dans la boutique ouverte */
	UFUNCTION(Exec)
	void Vendre(FName Item, int32 Count = 1);

	/** Argent 5000 : fixe l'argent du joueur */
	UFUNCTION(Exec)
	void Argent(int32 Amount);

	/** Badge BoulderBadge : donne un badge (le stock des boutiques grandit avec les badges) */
	UFUNCTION(Exec)
	void Badge(FName BadgeId);

	/** Allume le PC de Léo sans aller au Centre */
	UFUNCTION(Exec)
	void AllumerPC();

	/** Retient le Centre le plus proche comme point de retour (comme un soin de Joëlle) */
	UFUNCTION(Exec)
	void NoterCentre();

	/** Renvoie au dernier Centre visité, comme quand toute l'équipe est K.O. */
	UFUNCTION(Exec)
	void RetourCentre();

	/** Liste les points de téléportation de test (TeleportPoint) posés dans la map */
	UFUNCTION(Exec)
	void Teleports();

	/** Va au point de téléportation Name (nom, début du nom ou numéro de la liste Teleports) */
	UFUNCTION(Exec)
	void Teleport(const FString& Name);


private:

	/** Commande d'une association touche -> action (Forward_Key...), None si on ne la change pas */
	FName GetBindingId(const struct FEnhancedActionKeyMapping& Mapping) const;

	/** Contextes d'entrée d'origine du joueur et leur priorité */
	void GetSourceContexts(TArray<TPair<UInputMappingContext*, int32>>& OutContexts) const;

	/** Copie avec les touches du joueur, pour chaque contexte d'origine */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UInputMappingContext>, TObjectPtr<UInputMappingContext>> BoundContexts;
};
