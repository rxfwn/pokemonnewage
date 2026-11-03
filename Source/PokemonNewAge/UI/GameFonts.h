#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 *  Police de tout le jeu (polices fournies par L'amie le 2026-11-01, maquettes Paramètres / combat / HUD) :
 *  Font Faces de /Game/UI/HUD/Fonts (Tools/HUD/import_hud_unreal.py) : FF_Futura_MediumCondensed (Futura MdCn BT)
 *  pour le texte, FF_Futura_BoldCondensed (Futura BdCn BT) pour le gras, droites ; Italic = MdCn penchée.
 *  Avant leur import, Futura Medium / Bold ; Roboto sinon. Taille UMG = taille Figma x 0,75.
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
