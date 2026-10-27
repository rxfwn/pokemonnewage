#pragma once

#include "CoreMinimal.h"
#include "PokemonTypes.generated.h"

/**
 *  Énumérations partagées par toutes les données Pokémon.
 *  Les noms des valeurs sont ceux écrits dans Data/Pokemon/*.json :
 *  ne jamais renommer une valeur (les Data Tables et les sauvegardes s'y réfèrent).
 */

/** Les 18 types. None = pas de second type. */
UENUM(BlueprintType)
enum class EPokemonType : uint8
{
	None,
	Normal,
	Fire,
	Water,
	Electric,
	Grass,
	Ice,
	Fighting,
	Poison,
	Ground,
	Flying,
	Psychic,
	Bug,
	Rock,
	Ghost,
	Dragon,
	Dark,
	Steel,
	Fairy
};

/** Statistiques. Accuracy et Evasion n'existent qu'en combat (modificateurs). */
UENUM(BlueprintType)
enum class EPokemonStat : uint8
{
	HP,
	Attack,
	Defense,
	SpecialAttack,
	SpecialDefense,
	Speed,
	Accuracy,
	Evasion
};

/** Courbe d'expérience d'une espèce (voir DT_Experience). */
UENUM(BlueprintType)
enum class EGrowthRate : uint8
{
	Slow,
	MediumFast,
	Fast,
	MediumSlow,
	Erratic,
	Fluctuating
};

/** Catégorie d'une attaque (physique / spéciale propre à chaque attaque, comme depuis la 4G). */
UENUM(BlueprintType)
enum class EMoveCategory : uint8
{
	Physical,
	Special,
	Status
};

/** Cible d'une attaque (reprend les cibles de PokeAPI, utiles pour le futur combat à plusieurs). */
UENUM(BlueprintType)
enum class EMoveTarget : uint8
{
	SpecificMove,
	SelectedPokemonMeFirst,
	Ally,
	UsersField,
	UserOrAlly,
	OpponentsField,
	User,
	RandomOpponent,
	AllOtherPokemon,
	SelectedPokemon,
	AllOpponents,
	EntireField,
	UserAndAllies,
	AllPokemon,
	AllAllies,
	FaintingPokemon
};

/** Famille d'effet d'une attaque : sert au combat et à l'IA pour savoir quoi appliquer. */
UENUM(BlueprintType)
enum class EMoveMetaCategory : uint8
{
	Damage,
	Ailment,
	NetGoodStats,
	Heal,
	DamageAilment,
	Swagger,
	DamageLower,
	DamageRaise,
	DamageHeal,
	OHKO,
	WholeFieldEffect,
	FieldEffect,
	ForceSwitch,
	Unique
};

/** Altération infligée par une attaque (statut majeur ou effet volatil). */
UENUM(BlueprintType)
enum class EMoveAilment : uint8
{
	None,
	Unknown,
	Paralysis,
	Sleep,
	Freeze,
	Burn,
	Poison,
	Confusion,
	Infatuation,
	Trap,
	Nightmare,
	Torment,
	Disable,
	Yawn,
	HealBlock,
	NoTypeImmunity,
	LeechSeed,
	Embargo,
	PerishSong,
	Ingrain,
	Silence,
	TarShot,
	Protect
};

/** Façon de déclencher une évolution. */
UENUM(BlueprintType)
enum class EEvolutionTrigger : uint8
{
	Level,
	Item,
	Trade
};

/** Façon de rencontrer un Pokémon sauvage. */
UENUM(BlueprintType)
enum class EEncounterMethod : uint8
{
	Walk,		// hautes herbes, grottes
	OldRod,
	GoodRod,
	SuperRod,
	Surf,
	RockSmash
};

/** Six statistiques de base. Sert aussi pour les EV donnés en battant l'espèce. */
USTRUCT(BlueprintType)
struct FPokemonBaseStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 HP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 Attack = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 Defense = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 SpecialAttack = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 SpecialDefense = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 Speed = 0;

	int32 Total() const { return HP + Attack + Defense + SpecialAttack + SpecialDefense + Speed; }
};
