#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DialogueRows.generated.h"

/**
 *  Lignes des Data Tables de dialogues (Content/World/Dialogues/DT_Npcs et DT_Dialogues).
 *  Les fichiers sources sont Data/Dialogues/*.json, générés par Tools/Data/build_dialogues.py
 *  depuis Tools/Data/dialogues_v1.py (voir docs/DIALOGUES.md).
 *
 *  Multijoueur : les tables sont identiques partout. Ce qui change d'un joueur à l'autre, ce sont
 *  ses drapeaux d'histoire (TArray<FName>, dans son PlayerState) : chaque joueur voit donc la
 *  conversation qui correspond à SON avancement, et les événements (objets, combats) s'appliquent à lui seul.
 */

/** Ce qu'est un PNJ de DT_Npcs */
UENUM(BlueprintType)
enum class ENpcKind : uint8
{
	/** Personnage : son nom s'affiche au-dessus du texte */
	Character,
	/** Panneau : texte sans nom */
	Sign,
	/** Objet du décor (PC, télé, Poké Ball, fossile) */
	Object,
	/** Narration du jeu (Narrator) */
	System
};

/** Comment une conversation démarre */
UENUM(BlueprintType)
enum class EDialogueTrigger : uint8
{
	/** Quand le joueur parle au PNJ : la conversation valide de plus haute priorité est choisie */
	Talk,
	/** Seulement lancée par son nom : volume au sol, fin de combat, choix, autre conversation */
	Script
};

/** Ce qui se passe quand une réplique s'affiche (appliqué par le serveur, au joueur qui parle) */
UENUM(BlueprintType)
enum class EDialogueEvent : uint8
{
	None,
	/** Donne EventAmount fois l'objet EventParam */
	GiveItem,
	/** Reprend EventAmount fois l'objet EventParam */
	TakeItem,
	/** Retire EventAmount ₽ ; si le joueur n'a pas assez, on passe à FailConversation */
	TakeMoney,
	/** Donne l'espèce EventParam (DT_Species) au niveau EventAmount */
	GivePokemon,
	/** Propose de donner un surnom au dernier Pokémon reçu */
	OfferNickname,
	/** Donne le badge EventParam */
	GiveBadge,
	/** Soigne toute l'équipe */
	HealParty,
	/** Combat contre EventParam (DT_Trainers ; RivalLab = ligne RivalLab_<starter du joueur>) */
	StartTrainerBattle,
	/** Ouvre la boutique EventParam (Acheter / Vendre / Quitter) */
	OpenShop,
	/** Ouvre le PC (stockage) */
	OpenPC,
	/** Joue la cinématique EventParam (Level Sequence), la conversation reprend après */
	PlayCutscene,
	/** Ramène le joueur au dernier Centre Pokémon visité (équipe K.O.) */
	ReturnToLastHeal
};

/** DT_Npcs : PNJ, panneaux et objets avec lesquels on parle. Nom de ligne = ProfOak, PalletTown_Sign... */
USTRUCT(BlueprintType)
struct FNpcRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Nom affiché au-dessus du texte (vide pour un panneau ou un objet) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	ENpcKind Kind = ENpcKind::Character;

	/** Lieu (PalletTown, Route1...), None pour un PNJ présent partout (infirmière) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	FName Location;

	/** Ligne de DT_Trainers si ce PNJ est un dresseur, None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	FName Trainer;
};

/** Un choix proposé au joueur à la fin d'une réplique (Oui / Non) */
USTRUCT(BlueprintType)
struct FDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FText Text;

	/** Conversation qui suit ce choix, None = fin de la conversation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName NextConversation;
};

/** Une réplique : une boîte de texte */
USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()

	/** Qui parle (ligne de DT_Npcs). Narrator = narration sans nom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName Speaker;

	/** Texte, avec des variables {PlayerName}, {StarterName}... (voir UDialogueLibrary::FormatDialogueText) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(MultiLine=true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	EDialogueEvent Event = EDialogueEvent::None;

	/** Objet, espèce, dresseur, boutique ou cinématique selon Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName EventParam;

	/** Quantité, niveau ou somme selon Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	int32 EventAmount = 0;

	/** Si l'événement échoue (pas assez d'argent), on passe à cette conversation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName FailConversation;

	/** Vide = on passe à la réplique suivante */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TArray<FDialogueChoice> Choices;
};

/** DT_Dialogues : une conversation. Nom de ligne = Lieu_Pnj_Situation (PlayerHouse_Mom_AfterStarter) */
USTRUCT(BlueprintType)
struct FDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	/** PNJ à qui appartient la conversation (DT_Npcs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName Npc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	EDialogueTrigger Trigger = EDialogueTrigger::Talk;

	/** Entre plusieurs conversations Talk valides du même PNJ, la plus haute gagne */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	int32 Priority = 0;

	/** Le joueur doit avoir tous ces drapeaux */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TArray<FName> RequiredFlags;

	/** Le joueur ne doit avoir aucun de ces drapeaux */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TArray<FName> BlockedByFlags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TArray<FDialogueLine> Lines;

	/** Drapeaux donnés au joueur à la fin de la conversation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TArray<FName> SetFlags;

	/** Conversation enchaînée à la fin (après un combat : quand on revient sur la carte), None sinon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	FName NextConversation;
};
