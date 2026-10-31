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

		/** Inclinaison de tout le texte (0 = droit, 0,18 comme l'écran de titre) */
		constexpr float Skew = 0.18f;

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
			// Futura Bold pour tout (L'amie, 2026-10-31 : « futura bold penché, c'est la même » que Futura Md BT) ;
			// FF_Futura_Medium / FF_Futura_MediumItalic seulement si on les ajoute un jour.
			// Les accents (é, à, ç, ©, °) sont ajoutés au .ttf par Tools/HUD/add_futura_accents.py (écran de titre)
			UFontFace* Bold = LoadFace(TEXT("FF_Futura_Bold"));
			UFontFace* Medium = LoadFace(TEXT("FF_Futura_Medium"));
			UFontFace* Italic = LoadFace(TEXT("FF_Futura_MediumItalic"));
			Medium = Medium ? Medium : Bold;
			Bold = Bold ? Bold : Medium;
			Italic = Italic ? Italic : Medium;
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
		// Penché partout, comme la maquette de L'amie
		Info.SkewAmount = Skew;
		return Info;
	}
}
