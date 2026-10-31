#include "UI/Title/TitleScreenSettings.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "TitleScreen"

UTitleScreenSettings::UTitleScreenSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("TitleScreen");
	Maps.Add(TEXT("L_Kanto"));
	Backgrounds.Add(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Title/T_Title_Background.T_Title_Background"))));
	Logo = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Title/T_Title_Logo.T_Title_Logo")));
	Title = LOCTEXT("Title", "POKÉMON");
	Subtitle = LOCTEXT("Subtitle", "ÉCLAIR");
	VersionText = LOCTEXT("Version", "v.1.0.0");
	CopyrightText = LOCTEXT("Copyright", "© 2026");
}

#undef LOCTEXT_NAMESPACE
