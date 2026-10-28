#pragma once

#include "CoreMinimal.h"
#include "Battle/BattleTypes.h"

class UBattleSession;

/**
 *  IA simple des adversaires (SCRUM-37).
 *  - Pokémon sauvage : une attaque au hasard parmi celles qui ont des PP.
 *  - Dresseur : chaque attaque reçoit une note (dégâts estimés x efficacité x précision, bonus si elle met K.O.,
 *    attaques de statut utiles en début de combat), puis on tire au sort en favorisant les meilleures notes.
 *    Un dresseur ne gaspille pas une attaque sans effet (Électrik contre Sol) ni un statut déjà infligé.
 */
struct FBattleAI
{
	static FBattleAction ChooseAction(const UBattleSession& Session, int32 Side, FRandomStream& Stream);

	/** Note d'une attaque pour le côté Side (0 = inutile). Exposé pour les tests */
	static float ScoreMove(const UBattleSession& Session, int32 Side, int32 MoveSlot);
};
