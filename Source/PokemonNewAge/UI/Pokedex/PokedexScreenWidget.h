#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Pokemon/PokemonTypes.h"
#include "PokedexScreenWidget.generated.h"

class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class APokemonPlayerState;
class UPokemonDatabase;
class UPokedexModel;
class UPokedexScreenWidget;
class UCanvasPanel;
struct FPokemonInstance;

/** Les 10 boutons du volet, dans l'ordre de la grille (gauche à droite, haut en bas) et des icônes. */
UENUM(BlueprintType)
enum class EPokedexMenu : uint8
{
	Pokemon,
	Pokedex,
	Bag,
	Radio,
	Trainer,
	Map,
	Save,
	Online,
	Options,
	Style
};

constexpr int32 PokedexMenuCount = 10;

/** Pages de l'écran. */
UENUM(BlueprintType)
enum class EPokedexPage : uint8
{
	/** Accueil : heure, équipe en bref (le joueur choisit un bouton du volet) */
	Home,
	Party,
	PartyActions,
	Summary,
	Pokedex,
	PokedexEntry,
	Bag,
	BagTarget,
	Trainer,
	Save,
	Options,
	ConfirmDelete,
	/** Objet à faire tenir au Pokémon choisi */
	GiveItem,
	Map,
	Online,
	Controls,
	/** Radio et Style : pas encore faits */
	ComingSoon
};

/** Ce que l'appareil doit faire pour accompagner l'écran (son, voyants). */
UENUM(BlueprintType)
enum class EPokedexFeedback : uint8
{
	Navigate,
	Confirm,
	Back,
	Error,
	Saved
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPokedexFeedback, EPokedexFeedback);

/** Relie un bouton de ligne à son numéro (les délégués UMG n'ont pas de paramètre). */
UCLASS()
class UPokedexRowBinding : public UObject
{
	GENERATED_BODY()

public:

	int32 Index = 0;
	TWeakObjectPtr<UPokedexScreenWidget> Screen;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();
};

/**
 *  L'écran du Pokédex (SCRUM-49, 50, 51, 52, 64) : toutes les pages des 10 menus, construites en C++ et remplies
 *  avec les vraies données (PlayerState du joueur local + DA_PokemonDatabase).
 *  Affiché sur l'écran 3D par APokedexDevice (Widget Component), ou à plat par le menu de secours.
 *  Commandes : Navigate (croix, ZQSD, flèches), Confirm (A, Entrée, clic), Back (B, Échap, clic droit sur rien).
 */
UCLASS()
class UPokedexScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille de l'écran en pixels (le Widget Component l'affiche sur l'écran de 6,4 × 4,32 cm du modèle) */
	static constexpr float ScreenWidth = 960.f;
	static constexpr float ScreenHeight = 648.f;

	/** Ouvre la page d'un des 10 menus */
	void OpenMenu(EPokedexMenu Menu);

	static FText GetMenuName(EPokedexMenu Menu);
	static FText GetMenuDescription(EPokedexMenu Menu);

	/** Ce qu'affiche le petit écran du haut du volet pour la page en cours (fiche, objet, lieu...) */
	void GetInfo(FText& OutTitle, FText& OutText) const;

	/** Pièces, icônes et carte (DA_PokedexModel), chargé à la demande */
	const UPokedexModel* GetModel() const;

	/** Revient à l'accueil */
	void ShowHome();

	/** Déplace la sélection (X : gauche/droite, Y : haut/bas, Y positif = vers le bas) */
	void Navigate(FIntPoint Direction);

	void Confirm();

	/** Faux si on était déjà à l'accueil (l'appareil rend alors la main au volet) */
	bool Back();

	bool IsAtHome() const { return Page == EPokedexPage::Home; }

	/** Touche du clavier ou de la manette ; vrai si utilisée */
	bool HandleKey(const FKey& Key);

	/** Ligne cliquée ou survolée à la souris */
	void OnRowClicked(int32 Index);
	void OnRowHovered(int32 Index);

	/** Message en bas de l'écran pendant quelques secondes */
	void ShowStatus(const FText& Message, bool bError = false);

	/** Son et voyants : écouté par APokedexDevice */
	FOnPokedexFeedback OnFeedback;

	/** Le menu de secours (sans 3D) demande à fermer l'écran */
	FSimpleMulticastDelegate OnRequestClose;

	/** Écran à plat : il prend le focus clavier lui-même */
	bool bStandalone = false;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:

	// Construction des pages (Refresh reconstruit la page courante avec les données à jour)
	void SetPage(EPokedexPage NewPage, bool bResetSelection = true);
	void Refresh();
	void BuildHome();
	void BuildParty();
	void BuildPartyActions();
	void BuildSummary();
	void BuildPokedex();
	void BuildPokedexEntry();
	void BuildBag();
	void BuildBagTarget();
	void BuildTrainer();
	void BuildSave();
	void BuildOptions();
	void BuildConfirmDelete();
	void BuildGiveItem();
	void BuildMap();
	void BuildOnline();
	void BuildControls();
	void BuildComingSoon();

	// Briques d'interface
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold = false);
	UHorizontalBox* AddRow(int32 Index, bool bSelectable = true);
	void AddRowText(UHorizontalBox* Row, const FText& Text, int32 Size, const FLinearColor& Color, bool bFill = false, bool bBold = false);
	void AddRowHPBar(UHorizontalBox* Row, int32 HP, int32 MaxHP, float Width);
	void AddLine(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold = false);
	void AddLabelValue(const FText& Label, const FText& Value);
	void AddSpacer(float Height);
	void AddTypeBadges(UHorizontalBox* Row, EPokemonType Type1, EPokemonType Type2);

	// Données
	APokemonPlayerState* GetPlayerState() const;
	const UPokemonDatabase* GetDatabase() const;
	FText GetTypeName(EPokemonType Type) const;
	FText GetStatusShort(EMajorStatus Status) const;
	TArray<FName> GetBagPocketItems(int32 Pocket) const;
	/** Objets du sac qu'un Pokémon peut tenir (tout sauf les objets rares) */
	TArray<FName> GetHoldableItems() const;
	void BuildSpeciesList();
	/** Lieux où l'espèce vit à l'état sauvage (DT_Encounters) */
	TArray<FText> GetSpeciesLocations(FName Species) const;
	/** Position du joueur sur la carte (u, v de 0 à 1) ; faux sans Landscape */
	bool GetPlayerMapPosition(FVector2D& OutUV) const;
	/** Lieu le plus proche de la position (ville en priorité), -1 si aucun */
	int32 FindPlaceAt(const FVector2D& UV) const;
	/** Villes de la carte, pour la liste de la page Carte */
	TArray<int32> GetMapTowns() const;

	// Actions
	void UseSelectedItemOn(int32 PartyIndex);
	void DoSave();
	void ChangeOption(int32 Option, int32 Delta);
	void Feedback(EPokedexFeedback Type);

	// Arbre de widgets fixe
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Content;
	/** Boîte où AddRow / AddLine ajoutent (Content, ou la colonne de droite de la carte) */
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RowTarget;

	/** Lignes cliquables de la page courante */
	UPROPERTY(Transient) TArray<TObjectPtr<UPokedexRowBinding>> RowBindings;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> RowBackgrounds;

	EPokedexPage Page = EPokedexPage::Home;
	int32 Selection = 0;
	/** Nombre de lignes sélectionnables de la page */
	int32 RowCount = 0;
	/** Première ligne affichée des longues listes (Pokédex, sac) */
	int32 ScrollOffset = 0;

	/** Pokémon de l'équipe concerné (résumé, déplacer, objet) */
	int32 PartyIndex = 0;
	/** Déplacer : Pokémon choisi en premier, -1 sinon */
	int32 MovingFrom = -1;
	/** Résumé : 0 infos, 1 stats, 2 attaques */
	int32 SummaryTab = 0;
	/** Sac : poche 0 soins, 1 capture, 2 baies et matériaux, 3 objets clés */
	int32 BagPocket = 0;
	FName SelectedItem;
	/** Pokédex : espèces triées par numéro */
	TArray<FName> SpeciesList;
	int32 DexIndex = 0;
	/** Menu ouvert (titre des pages « bientôt ») */
	EPokedexMenu CurrentMenu = EPokedexMenu::Pokemon;

	UPROPERTY(Transient)
	mutable TObjectPtr<UPokedexModel> CachedModel;
	mutable bool bModelLoaded = false;

	float StatusTimeLeft = 0.f;
	float RefreshTimer = 0.f;
	float ClockTimer = 0.f;
};
