#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 *  Heure du jeu (cycle jour / nuit de la map), pour le HUD et le Pokédex.
 *  1. Un acteur dont la classe contient « CycleJourNuit » (BP de L'amie) avec une variable décimale
 *     Heure / Hour / TimeOfDay / Time entre 0 et 24 : on la lit telle quelle.
 *  2. Sinon on la déduit de la hauteur du soleil (plus haut = midi), le matin quand il monte, l'après-midi quand il descend.
 *  3. Sans soleil : l'heure du PC.
 */
namespace PokemonClock
{
	/** Heure du jeu en heures décimales (0 à 24) */
	float GetHour(const UWorld* World);

	/** « 14:05 » */
	FText FormatTime(const UWorld* World);
}
