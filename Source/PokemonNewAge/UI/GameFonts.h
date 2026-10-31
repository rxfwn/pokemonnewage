#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 *  Police de tout le jeu : Futura Bold penchée (demandé par L'amie le 2026-10-31, maquette de l'écran de titre).
 *  Font Face FF_Futura_Bold importée par Tools/HUD/import_hud_unreal.py dans /Game/UI/HUD/Fonts, inclinée par SkewAmount.
 *  FF_Futura_Medium / FF_Futura_MediumItalic servent seulement si on les ajoute ; Roboto penché si rien n'est importé.
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
