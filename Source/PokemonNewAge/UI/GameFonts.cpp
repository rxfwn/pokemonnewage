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
			UFontFace* Medium = LoadFace(TEXT("FF_Futura_Medium"));
			UFontFace* Bold = LoadFace(TEXT("FF_Futura_Bold"));
			UFontFace* Italic = LoadFace(TEXT("FF_Futura_MediumItalic"));
			Medium = Medium ? Medium : Bold;
			Bold = Bold ? Bold : Medium;
			Italic = Italic ? Italic : Medium;
			if (!Medium)
			{
				return nullptr;
			}
			TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
			auto AddTypeface = [&Composite](const TCHAR* Name, UFontFace* Face)
			{
				FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
				Entry.Name = Name;
				Entry.Font = FFontData(Face);
			};
			AddTypeface(TEXT("Regular"), Medium);
			AddTypeface(TEXT("Bold"), Bold);
			AddTypeface(TEXT("Italic"), Italic);
			CachedFont = Composite;
			return CachedFont;
		}
	}

	FSlateFontInfo Get(float Size, EStyle Style)
	{
		const TCHAR* Typeface = Style == EStyle::Bold ? TEXT("Bold") : Style == EStyle::Italic ? TEXT("Italic") : TEXT("Regular");
		if (TSharedPtr<const FCompositeFont> Font = GetFont())
		{
			return FSlateFontInfo(Font, Size, FName(Typeface));
		}
		return FCoreStyle::GetDefaultFontStyle(Style == EStyle::Bold ? TEXT("Bold") : Style == EStyle::Italic ? TEXT("Italic") : TEXT("Regular"), Size);
	}
}
