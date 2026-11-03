#pragma once

#include "CoreMinimal.h"
#include "Components/BackgroundBlur.h"

class UImage;
class UTextBlock;
class UWidget;
class UWidgetTree;

/**
 *  Panneaux en verre de l'interface, d'après l'effet « Glass » de Figma des maquettes de L'amie (2026-11-01 :
 *  Paramètres, combat, fenêtres de dialogue). Un panneau = UBackgroundBlur (le flou du décor, « Frost » de Figma)
 *  qui contient une image au matériau /Game/UI/Materials/M_UI_Glass (remplissage en dégradé, contour intérieur
 *  en dégradé, reflet de lumière sur les bords) puis le contenu.
 *  Le matériau est fait par Tools/UI/make_glass_material.py (lancé tout seul à l'ouverture de l'éditeur).
 *  Sans lui, le panneau prend un simple fond blanc translucide arrondi. Voir docs/VERRE_UI.md.
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
	 *  Réglages d'un panneau, comme le CSS exporté de Figma. Les valeurs par défaut sont celles du matériau
	 *  (seules celles qui changent sont passées au matériau : une instance MI_UI_Glass garde la main sur les autres).
	 *  Les opacités du remplissage sont avant LayerOpacity (CSS 0,3834 avec un calque à 71 % = 0,54).
	 */
	struct FGlassLook
	{
		float CornerRadius = DefaultCornerRadius;
		float Blur = DefaultBlur;
		/** Degrés comme en CSS : 180 = de haut en bas, 90 = de gauche à droite */
		float GradientAngle = 180.f;
		float GradientStart = 0.f;
		float GradientEnd = 1.f;
		float FillOpacityStart = 0.39f;
		float FillOpacityEnd = 0.13f;
		float StrokeOpacityStart = 0.01f;
		float StrokeOpacityEnd = 0.17f;
		float LayerOpacity = 0.71f;
		float LightIntensity = 0.45f;
	};

	/**
	 *  Panneau en verre autour de Content (peut être nul), avec cette marge intérieure.
	 *  Le widget rendu est celui à placer dans le parent et à montrer / cacher.
	 */
	UBackgroundBlur* Make(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding, const FGlassLook& Look, FName Name = NAME_None);

	inline UBackgroundBlur* Make(UWidgetTree* Tree, UWidget* Content, const FMargin& Padding,
		float CornerRadius = DefaultCornerRadius, FName Name = NAME_None)
	{
		FGlassLook Look;
		Look.CornerRadius = CornerRadius;
		return Make(Tree, Content, Padding, Look, Name);
	}

	/** Seulement l'image au matériau, sans flou du décor (barres des curseurs, ascenseur...) */
	UImage* MakeImage(UWidgetTree* Tree, const FGlassLook& Look, FName Name = NAME_None);

	/** Ombre nette sous un texte déjà créé (CSS text-shadow 3px 4px, noir à 25 %, sans le flou) */
	void AddTextShadow(UTextBlock* Text);

	/** Texte avec l'ombre floue des maquettes (text-shadow 3px 4px 4px noir à 25 %, USoftShadowTextBlock) */
	UTextBlock* MakeShadowedText(UWidgetTree* Tree, FName Name = NAME_None);
}
