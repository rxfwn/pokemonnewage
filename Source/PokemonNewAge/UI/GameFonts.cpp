#include "UI/GameFonts.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"

namespace PokemonFonts
{
	namespace
	{
		const TCHAR* FontFolder = TEXT("/Game/UI/HUD/Fonts");

		/** Inclinaison du style Italic seulement (les maquettes du 2026-11-01 sont droites) */
		constexpr float ItalicSkew = 0.18f;

		/** Gardées en mémoire pour toute la partie : la police composite ne les référence pas pour le ramasse-miettes */
		TArray<TStrongObjectPtr<UFontFace>> LoadedFaces;
		TSharedPtr<const FCompositeFont> CachedFont;

		UFontFace* LoadFace(const TCHAR* Name)
		{
			UFontFace* Face = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), FontFolder, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Face)
			{
				LoadedFaces.Emplace(Face);
			}
			return Face;
		}

		TSharedPtr<const FCompositeFont> GetFont()
		{
			if (CachedFont.IsValid())
			{
				return CachedFont;
			}
			// Polices des maquettes de L'amie (2026-11-01) : Futura MdCn BT pour le texte, Futura BdCn BT pour le gras,
			// droites ; l'italique est la MdCn penchée. Avant leur import : Futura Medium / Bold (accents ajoutés par
			// Tools/HUD/add_futura_accents.py). Chacune remplace l'autre si elle manque.
			UFontFace* Medium = LoadFace(TEXT("FF_Futura_MediumCondensed"));
			UFontFace* Bold = LoadFace(TEXT("FF_Futura_BoldCondensed"));
			Medium = Medium ? Medium : LoadFace(TEXT("FF_Futura_Medium"));
			Bold = Bold ? Bold : LoadFace(TEXT("FF_Futura_Bold"));
			Medium = Medium ? Medium : Bold;
			Bold = Bold ? Bold : Medium;
			UFontFace* Italic = Medium;
			if (!Medium)
			{
				return nullptr;
			}
			TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
			auto AddTypeface = [](FTypeface& Typeface, const TCHAR* Name, UFontFace* Face)
			{
				FTypefaceEntry& Entry = Typeface.Fonts.AddDefaulted_GetRef();
				Entry.Name = Name;
				Entry.Font = FFontData(Face);
			};
			AddTypeface(Composite->DefaultTypeface, TEXT("Regular"), Medium);
			AddTypeface(Composite->DefaultTypeface, TEXT("Bold"), Bold);
			AddTypeface(Composite->DefaultTypeface, TEXT("Italic"), Italic);

			CachedFont = Composite;
			return CachedFont;
		}
	}

	FSlateFontInfo Get(float Size, EStyle Style)
	{
		const TCHAR* Typeface = Style == EStyle::Bold ? TEXT("Bold") : Style == EStyle::Italic ? TEXT("Italic") : TEXT("Regular");
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(FName(Typeface), Size);
		if (TSharedPtr<const FCompositeFont> Font = GetFont())
		{
			Info = FSlateFontInfo(Font, Size, FName(Typeface));
		}
		Info.SkewAmount = Style == EStyle::Italic ? ItalicSkew : 0.f;
		return Info;
	}

	FSlateFontInfo GetHUD(float Size, bool* bOutCondensed)
	{
		static TSharedPtr<const FCompositeFont> HUDFont;
		static bool bCondensed = false;
		if (!HUDFont.IsValid())
		{
			UFontFace* Face = LoadFace(TEXT("FF_Futura_BoldCondensed"));
			bCondensed = Face != nullptr;
			Face = Face ? Face : LoadFace(TEXT("FF_Futura_Bold"));
			if (Face)
			{
				TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
				FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
				Entry.Name = TEXT("Regular");
				Entry.Font = FFontData(Face);
				HUDFont = Composite;
			}
		}
		if (bOutCondensed)
		{
			*bOutCondensed = bCondensed;
		}
		return HUDFont.IsValid() ? FSlateFontInfo(HUDFont, Size, FName(TEXT("Regular"))) : FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
	}
}
