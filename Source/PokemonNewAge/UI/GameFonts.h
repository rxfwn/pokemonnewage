#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 *  Police de tout le jeu : Futura penchée (demandé par L'amie le 2026-10-31, maquette de l'écran de titre).
 *  Font Faces de /Game/UI/HUD/Fonts (Tools/HUD/import_hud_unreal.py) : FF_Futura_Medium pour le texte et l'italique,
 *  FF_Futura_Bold pour le gras, inclinées par SkewAmount. L'une remplace l'autre si elle manque ; Roboto penché sinon.
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

	/**
	 *  Police droite du HUD en jeu (maquette de L'amie : Futura BdCn BT, gras condensé, pas penché) :
	 *  FF_Futura_BoldCondensed si elle est importée, sinon FF_Futura_Bold. bOutCondensed dit laquelle on a eue
	 *  (le HUD resserre lui-même le texte quand ce n'est pas la vraie condensée).
	 */
	FSlateFontInfo GetHUD(float Size, bool* bOutCondensed = nullptr);

	inline FSlateFontInfo Get(float Size, bool bBold)
	{
		return Get(Size, bBold ? EStyle::Bold : EStyle::Regular);
	}
}
