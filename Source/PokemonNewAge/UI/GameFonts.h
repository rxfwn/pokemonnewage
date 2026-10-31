#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 *  Police de tout le jeu : Futura (demandé par L'amie le 2026-10-31, maquette de l'écran de titre en Futura Md BT).
 *  Font Faces importées par Tools/HUD/import_hud_unreal.py depuis Tools/HUD/Assets/fonts/ dans /Game/UI/HUD/Fonts :
 *    FF_Futura_Medium (texte), FF_Futura_MediumItalic (italique), FF_Futura_Bold (gras).
 *  Il en manque une : on prend la plus proche (Medium <-> Bold), et Roboto si aucune n'est importée.
 */
namespace PokemonFonts
{
	enum class EStyle : uint8
	{
		Regular,
		Bold,
		Italic
	};

	FSlateFontInfo Get(float Size, EStyle Style = EStyle::Regular);

	inline FSlateFontInfo Get(float Size, bool bBold)
	{
		return Get(Size, bBold ? EStyle::Bold : EStyle::Regular);
	}
}
