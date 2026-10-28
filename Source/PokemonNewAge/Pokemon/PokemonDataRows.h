#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Pokemon/PokemonTypes.h"
#include "PokemonDataRows.generated.h"

/**
 *  Lignes des Data Tables Pokémon (Content/Pokemon/Data/DT_*).
 *  Les fichiers sources sont Data/Pokemon/DT_*.json, générés par Tools/Data/build_pokemon_data.py
 *  et importés par Tools/Data/import_data_tables.py (voir docs/DONNEES_POKEMON.md).
 *
 *  Identifiants : le nom de ligne est l'identifiant anglais stable (Bulbasaur, Tackle, Route1_Walk).
 *  Les autres données (Pokémon d'un joueur, équipe, combat) ne stockent que ces FName :
 *  c'est léger à répliquer en multijoueur, chaque machine relit le reste dans ses tables.
 */

// ---------------------------------------------------------------- Espèces

/** Une évolution possible d'une espèce. */
USTRUCT(BlueprintType)
struct FPokemonEvolution
{
	GENERATED_BODY()

	/** Espèce obtenue (ligne de DT_Species) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evolution")
	FName Species;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evolution")
	EEvolutionTrigger Trigger = EEvolutionTrigger::Level;

	/** Niveau minimum (Trigger = Level), 0 sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evolution")
	int32 MinLevel = 0;

	/** Objet à utiliser (Trigger = Item), ex. FireStone. None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evolution")
	FName Item;
};

/** DT_Species : une ligne par espèce (151). Nom de ligne = nom anglais (Bulbasaur). */
USTRUCT(BlueprintType)
struct FSpeciesRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Numéro du Pokédex national */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	int32 NationalDex = 0;

	/** Nom affiché (français) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	FText DisplayName;

	/** Catégorie affichée dans le Pokédex (« Pokémon Graine ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokedex")
	FText Genus;

	/** Texte de la fiche Pokédex */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokedex", meta=(MultiLine=true))
	FText PokedexEntry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	EPokemonType Type1 = EPokemonType::Normal;

	/** None si l'espèce n'a qu'un type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	EPokemonType Type2 = EPokemonType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	FPokemonBaseStats BaseStats;

	/** EV gagnés en battant cette espèce */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	FPokemonBaseStats EVYield;

	/** Talents possibles (lignes de DT_Abilities), un tiré au hasard à la capture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	TArray<FName> Abilities;

	/** Taux de capture, 3 (légendaire) à 255 (très facile) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	int32 CatchRate = 255;

	/** Base de l'expérience donnée quand on le bat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 BaseExperience = 0;

	/** Bonheur à la capture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture")
	int32 BaseFriendship = 70;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	EGrowthRate GrowthRate = EGrowthRate::MediumFast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	bool bGenderless = false;

	/** Probabilité d'être une femelle (0 à 1), ignorée si bGenderless */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	float FemaleChance = 0.5f;

	/** Taille en mètres (Pokédex, et échelle du modèle 3D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokedex")
	float HeightM = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pokedex")
	float WeightKg = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	bool bIsLegendary = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	bool bIsMythical = false;

	/** Pokémon qu'on peut avoir dans la V1 (de Bourg Palette au badge de Pierre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
	bool bAvailableInV1 = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evolution")
	TArray<FPokemonEvolution> Evolutions;
};

// ---------------------------------------------------------------- Types

/**
 *  DT_TypeChart : une ligne par type ATTAQUANT (Fire), une colonne par type qui DÉFEND.
 *  Valeurs : 0 (immunité), 0.5, 1 ou 2.
 */
USTRUCT(BlueprintType)
struct FTypeChartRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Nom du type affiché (français) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Type")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Normal = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Fire = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Water = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Electric = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Grass = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Ice = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Fighting = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Poison = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Ground = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Flying = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Psychic = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Bug = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Rock = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Ghost = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Dragon = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Dark = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Steel = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Against") float Fairy = 1.f;

	/** Multiplicateur contre un type qui défend (1 pour None) */
	float Against(EPokemonType Defender) const;
};

// ---------------------------------------------------------------- Attaques

/** Changement de statistique causé par une attaque (+1, -2...). */
USTRUCT(BlueprintType)
struct FMoveStatChange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	EPokemonStat Stat = EPokemonStat::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	int32 Stages = 0;
};

/** DT_Moves : une ligne par attaque (354, générations 1 à 3). Nom de ligne = nom anglais (Tackle). */
USTRUCT(BlueprintType)
struct FMoveRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Numéro officiel de l'attaque */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	int32 Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	EPokemonType Type = EPokemonType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	EMoveCategory Category = EMoveCategory::Physical;

	/** 0 = pas de dégâts directs (ou dégâts calculés à part) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	int32 Power = 0;

	/** En %, 0 = ne rate jamais */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	int32 Accuracy = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	int32 PP = 10;

	/** Priorité : +1 (Vive-Attaque) passe avant, -6 passe en dernier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	EMoveTarget Target = EMoveTarget::SelectedPokemon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Move")
	bool bMakesContact = false;

	/** Chance (%) de l'effet secondaire, 0 = toujours ou aucun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 EffectChance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	EMoveMetaCategory MetaCategory = EMoveMetaCategory::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	EMoveAilment Ailment = EMoveAilment::None;

	/** Chance (%) d'infliger Ailment, 0 = toujours (attaque de statut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 AilmentChance = 0;

	/** Coups multiples (Furie : 2 à 5), 0 = un seul coup */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 MinHits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 MaxHits = 0;

	/** Durée de l'effet en tours (Ligotage...), 0 = sans objet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 MinTurns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 MaxTurns = 0;

	/** % des dégâts récupérés en PV (Vol-Vie : 50) ; négatif = contrecoup (Bélier : -25) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 Drain = 0;

	/** % des PV max soignés (Soin : 50) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 Healing = 0;

	/** Bonus de taux de critique (Tranche : 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 CritStage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 FlinchChance = 0;

	/** Chance (%) d'appliquer StatChanges, 0 = toujours */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	int32 StatChance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	TArray<FMoveStatChange> StatChanges;
};

/** DT_Machines : CT01 à CT50 et CS01 à CS07 de Rouge Feu / Vert Feuille. Nom de ligne = TM01, HM01. */
USTRUCT(BlueprintType)
struct FMachineRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Attaque enseignée (ligne de DT_Moves) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine")
	FName Move;

	/** CS : réutilisable et utilisable hors combat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine")
	bool bIsHM = false;
};

// ---------------------------------------------------------------- Apprentissages

/** Attaque apprise à un niveau donné (niveau 1 = connue dès la naissance). */
USTRUCT(BlueprintType)
struct FLevelUpMove
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Learnset")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Learnset")
	FName Move;
};

/** DT_Learnsets : attaques apprises par espèce (Rouge Feu / Vert Feuille). Nom de ligne = espèce. */
USTRUCT(BlueprintType)
struct FLearnsetRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Triées par niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Learnset")
	TArray<FLevelUpMove> LevelUpMoves;

	/** Attaques que les CT/CS peuvent enseigner à cette espèce */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Learnset")
	TArray<FName> MachineMoves;
};

// ---------------------------------------------------------------- Talents, natures, XP

/** DT_Abilities : talents de la 3G. Nom de ligne = nom anglais (Overgrow). */
USTRUCT(BlueprintType)
struct FAbilityRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(MultiLine=true))
	FText Description;
};

/** DT_Natures : 25 natures. Neutre quand IncreasedStat == DecreasedStat (Hardi, Docile...). */
USTRUCT(BlueprintType)
struct FNatureRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nature")
	FText DisplayName;

	/** Statistique x1,1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nature")
	EPokemonStat IncreasedStat = EPokemonStat::Attack;

	/** Statistique x0,9 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nature")
	EPokemonStat DecreasedStat = EPokemonStat::Attack;

	bool IsNeutral() const { return IncreasedStat == DecreasedStat; }
};

/** DT_Experience : expérience totale nécessaire pour atteindre un niveau. Nom de ligne = niveau (1 à 100). */
USTRUCT(BlueprintType)
struct FExperienceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience") int32 Slow = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience") int32 MediumFast = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience") int32 Fast = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience") int32 MediumSlow = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience") int32 Erratic = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience") int32 Fluctuating = 0;

	int32 For(EGrowthRate Rate) const;
};

// ---------------------------------------------------------------- Rencontres sauvages

/** Une case de la table de rencontre. Les Rate d'une table font 100 au total. */
USTRUCT(BlueprintType)
struct FEncounterSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	FName Species;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	int32 MinLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	int32 MaxLevel = 2;

	/** Chance en % */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	int32 Rate = 0;
};

/** DT_Encounters : une table par lieu et méthode (Rouge Feu). Nom de ligne = Route1_Walk, MtMoon_1F_Walk. */
USTRUCT(BlueprintType)
struct FEncounterTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Lieu (même identifiant que Content/World/<Lieu>) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	FName Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	FText LocationDisplayName;

	/** Partie du lieu (étage de grotte...), vide si le lieu est d'un seul tenant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	FName Area;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	EEncounterMethod Method = EEncounterMethod::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter")
	TArray<FEncounterSlot> Slots;
};

// ---------------------------------------------------------------- Dresseurs

/** Un Pokémon de l'équipe d'un dresseur. */
USTRUCT(BlueprintType)
struct FTrainerPokemon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FName Species;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	int32 Level = 5;

	/** Vide = les 4 dernières attaques apprises à ce niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	TArray<FName> Moves;
};

/** DT_Trainers : dresseurs à combattre. Nom de ligne = BugCatcherRick, Brock, RivalLab_Bulbasaur. */
USTRUCT(BlueprintType)
struct FTrainerRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Classe (identifiant anglais : BugCatcher, GymLeader...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FName TrainerClass;

	/** Classe affichée (« Scout ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FText ClassDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FName Location;

	/** La ligne ne vaut que si le joueur a choisi ce starter (équipes du rival), None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FName RequiredPlayerStarter;

	/** Argent gagné en le battant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	int32 PrizeMoney = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	bool bIsGymLeader = false;

	/** Badge remis (champions d'arène), None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FName Badge;

	/** Objet remis après le combat (TM39...), None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	FName RewardItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trainer")
	TArray<FTrainerPokemon> Party;
};

// ---------------------------------------------------------------- Objets

/** DT_Items : objets du sac (soins, Poké Balls, objets rares). Nom de ligne = PokeBall, Potion. */
USTRUCT(BlueprintType)
struct FItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EItemCategory Category = EItemCategory::Other;

	/** Prix d'achat en boutique (revente : moitié), 0 = ne se vend pas */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	int32 Price = 0;

	/** PV rendus (Potion : 20) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medicine")
	int32 HealHP = 0;

	/** Rend tous les PV (Potion Max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medicine")
	bool bHealFull = false;

	/** Statuts soignés (Antidote : Poison et BadlyPoisoned) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medicine")
	TArray<EMajorStatus> CuresStatus;

	/** Ranime un Pokémon K.O. avec ce % de ses PV max (Rappel : 50), 0 = ne ranime pas */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medicine")
	int32 ReviveHPPercent = 0;

	/** Multiplicateur de la formule de capture (Poké Ball 1, Super Ball 1,5, Hyper Ball 2), 0 = pas une Ball */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PokeBall")
	float CatchRateMultiplier = 0.f;

	/** Capture à coup sûr (Master Ball) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="PokeBall")
	bool bGuaranteedCatch = false;

	/** Effet quand un Pokémon le tient en combat (menu Pokémon > Donner un objet) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Held")
	EHeldItemEffect HeldEffect = EHeldItemEffect::None;

	/** Force de l'effet tenu : PV, % ou chance selon HeldEffect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Held")
	int32 HeldValue = 0;

	/** Type renforcé (HeldEffect = TypeBoost) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Held")
	EPokemonType HeldBoostType = EPokemonType::None;

	/** Utilisé hors combat pour faire progresser un Pokémon (Super Bonbon, pierres, CT). Voir docs/PROGRESSION.md */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	EFieldItemEffect FieldEffect = EFieldItemEffect::None;

	/** Niveaux gagnés (FieldEffect = LevelUp, Super Bonbon : 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	int32 LevelsGained = 0;

	/** Attaque enseignée (FieldEffect = TeachMove, ligne de DT_Moves) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	FName TeachesMove;

	/** Ne disparaît pas à l'usage (CT et CS, comme depuis la 5G) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	bool bReusable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	bool bUsableInBattle = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	bool bUsableInField = true;

	bool IsPokeBall() const { return CatchRateMultiplier > 0.f || bGuaranteedCatch; }
};
