#pragma once

#include "CoreMinimal.h"
#include "Components/BackgroundBlur.h"

class UTextBlock;
class UWidget;
class UWidgetTree;

/**
 *  Panneaux en verre de l'interface, d'après l'effet « Glass » de Figma des maquettes de L'amie (2026-11-01 :
 *  Paramètres, combat, fenêtres de dialogue). Un panneau = UBackgroundBlur (le flou du décor, « Frost » de Figma)
 *  qui contient une image au matériau /Game/UI/Materials/M_UI_Glass (remplissage en dégradé, contour intérieur
 *  en dégradé, reflet de lumière sur les bords) puis le contenu.
 *  Le matériau est fait par Tools/UI/make_glass_material.py (lancé tout seul à l'ouverture de l'éditeur).
 *  Sans lui, le panneau prend un simple fond blanc translucide arrondi.
 */
namespace PokemonGlass
{
	/** Arrondi des coins des maquettes (16 px en 1920 x 1080) */
	constexpr float DefaultCornerRadius = 16.f;

	/** Flou du décor derrière le verre (Frost 4 dans Figma) */
	constexpr float DefaultBlur = 6.f;

	/** Couleur du texte posé sur le verre (blanc des maquettes) */
	const FLinearColor TextColor(0.97f, 0.97f, 0.97f, 1.f);

	/**
	 *  Panneau en verre autour de Content (peut être nul), avec cette marge intérieure.
	 *  Le widget rendu est celui à placer dans le parent et à montrer / cacher.
	 */
	UBackgroundBlur* Make(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding,
		float CornerRadius = DefaultCornerRadius, FName Name = NAME_None);

	/** Ombre légère sous le texte, comme sur les maquettes (lisible sur un décor clair) */
	void AddTextShadow(UTextBlock* Text);
}
