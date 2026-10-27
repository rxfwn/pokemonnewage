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
class UPokedexScreenWidget;
struct FPokemonInstance;

/** Les 6 boutons du volet, dans l'ordre de la grille (gauche à droite, haut en bas). */
UENUM(BlueprintType)
enum class EPokedexMenu : uint8
{
	Pokemon,
	Pokedex,
	Bag,
	Trainer,
	Save,
	Options
};

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
	ConfirmDelete
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
 *  L'écran du Pokédex (SCRUM-49, 50, 51, 52) : toutes les pages des 6 menus, construites en C++ et remplies
 *  avec les vraies données (PlayerState du joueur local + DA_PokemonDatabase).
 *  Affiché sur l'écran 3D par APokedexDevice (Widget Component), ou à plat par le menu de secours.
 *  Commandes : Navigate (croix, ZQSD, flèches), Confirm (A, Entrée, clic), Back (B, Échap, clic droit sur rien).
 */
UCLASS()
class UPokedexScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille de l'écran en pixels (le Widget Component l'affiche sur 6,8 × 6,4 cm) */
	static constexpr float ScreenWidth = 680.f;
	static constexpr float ScreenHeight = 640.f;

	/** Ouvre la page d'un des 6 menus */
	void OpenMenu(EPokedexMenu Menu);

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
	void BuildSpeciesList();

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
	/** Sac : poche 0 soins, 1 Poké Balls, 2 objets rares, 3 autres */
	int32 BagPocket = 0;
	FName SelectedItem;
	/** Pokédex : espèces triées par numéro */
	TArray<FName> SpeciesList;
	int32 DexIndex = 0;

	float StatusTimeLeft = 0.f;
	float RefreshTimer = 0.f;
	float ClockTimer = 0.f;
};
