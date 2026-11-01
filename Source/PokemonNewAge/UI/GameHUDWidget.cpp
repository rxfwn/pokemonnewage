#include "UI/GameHUDWidget.h"
#include "Battle/BattleComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/GameClock.h"
#include "Core/PokemonSaveGame.h"
#include "Engine/Texture2D.h"
#include "PokemonNewAgePlayerController.h"
#include "UI/GameFonts.h"
#include "UI/Pokedex/PokedexModel.h"

#define LOCTEXT_NAMESPACE "GameHUD"

namespace GameHUD
{
	/** Assets créés par Tools/HUD/import_hud_unreal.py */
	const TCHAR* Folder = TEXT("/Game/UI/HUD");

	/**
	 *  Mesures de la maquette v2 de L'amie (fichiers du projet hud/v2/Town_Announcement.png, 1920 x 1080), relevées au pixel.
	 *  Tailles de police UMG = taille Figma en px × 0,75 (Slate compte en points à 96 ppp).
	 *  Futura : hauteur des capitales 0,667 em, haut des capitales à 0,315 em sous le haut de la boîte de texte.
	 */
	/** Bloc du bas à droite : coin haut gauche (1648, 728) */
	const FVector2D BlockSize(272.f, 352.f);
	/** Centre de la boussole (point du joueur en 1767,5 ; 923,5) et rayon du disque (206 px de diamètre) */
	const FVector2D CompassCenter(119.5f, 195.5f);
	constexpr float CompassRadius = 103.f;
	/** Images de L'amie : Navigation_Compass (412 × 442, disque + N, affichée à 50 %) et Navigation_Compass_Arrow (238 × 234, 100 %) */
	const FVector2D CompassImageSize(206.f, 221.f);
	const FVector2D CompassImageCenter(102.75f, 117.75f);
	const FVector2D ArrowImageSize(238.f, 234.f);
	const FVector2D ArrowDot(112.5f, 170.5f);
	/** Futura Bold resserrée pour imiter la Futura BdCn BT de la maquette (largeurs mesurées : 0,70) */
	constexpr float CondensedScale = 0.7f;

	template <typename T>
	T* LoadHUDAsset(const TCHAR* Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Panel, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
	{
		UCanvasPanelSlot* WidgetSlot = Panel->AddChildToCanvas(Widget);
		WidgetSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		WidgetSlot->SetAlignment(Alignment);
		WidgetSlot->SetPosition(Position);
		WidgetSlot->SetSize(Size);
		return WidgetSlot;
	}

	FText FormatTemperature(float Celsius)
	{
		return FText::Format(LOCTEXT("Temperature", "{0}°C"), FText::AsNumber(FMath::RoundToInt(Celsius)));
	}
}

using namespace GameHUD;

TSharedRef<SWidget> UGameHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Affichage seulement : ne prend jamais les clics
		SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Main"));
		UOverlaySlot* CanvasSlot = Root->AddChildToOverlay(Canvas);
		CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		CanvasSlot->SetVerticalAlignment(VAlign_Fill);

		// Maquette de L'amie : Futura BdCn BT (gras condensé), droite, blanche, sans ombre (UI/GameFonts.h)
		auto MakeText = [this](float Size, ETextJustify::Type Justify)
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			bool bCondensed = false;
			Label->SetFont(PokemonFonts::GetHUD(Size, &bCondensed));
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Label->SetJustification(Justify);
			if (!bCondensed)
			{
				// Pas encore la vraie condensée : on resserre la Futura Bold à l'affichage
				FWidgetTransform Narrow;
				Narrow.Scale = FVector2D(CondensedScale, 1.f);
				Label->SetRenderTransform(Narrow);
				Label->SetRenderTransformPivot(FVector2D(Justify == ETextJustify::Right ? 1.f : 0.5f, 0.5f));
			}
			return Label;
		};
		auto MakeLine = [this](const FLinearColor& Color)
		{
			UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Line->SetBrushColor(Color);
			return Line;
		};

		// ---- Icône du Pokédex en haut à droite (112 px à 40 px du bord droit, 39 px du haut)
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PokedexIcon"));
		if (UTexture2D* IconTexture = LoadHUDAsset<UTexture2D>(TEXT("T_HUD_PokedexIcon")))
		{
			Icon->SetBrushFromTexture(IconTexture, false);
		}
		else
		{
			Icon->SetColorAndOpacity(FLinearColor(0.85f, 0.15f, 0.18f, 0.9f));
		}
		PokedexIcon = Icon;
		Place(Canvas, Icon, FVector2D(1.f, 0.f), FVector2D(-40.f, 39.f), FVector2D(112.f, 112.f), FVector2D(1.f, 0.f));
		SavedText = MakeText(14.f, ETextJustify::Right);
		SavedText->SetText(LOCTEXT("Saved", "Partie sauvegardée"));
		SavedText->SetRenderOpacity(0.f);
		Place(Canvas, SavedText, FVector2D(1.f, 0.f), FVector2D(-40.f, 155.f), FVector2D(220.f, 20.f), FVector2D(1.f, 0.f));

		// ---- Grand nom du lieu : capitales de 68 px (Figma 102 px) de y 181 à 248, centré ; trait blanc 584 × 4 en (668, 264)
		UCanvasPanel* BannerPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Banner"));
		Place(Canvas, BannerPanel, FVector2D(0.5f, 0.f), FVector2D(0.f, 149.f), FVector2D(900.f, 125.f), FVector2D(0.5f, 0.f));
		BannerText = MakeText(76.5f, ETextJustify::Center);
		BannerText->SetShadowOffset(FVector2D(3.f, 3.f));  // ombre de la maquette
		BannerText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Place(BannerPanel, BannerText, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(900.f, 131.f), FVector2D::ZeroVector);
		const float LineLeft = (900.f - 584.f) * 0.5f;
		Place(BannerPanel, MakeLine(FLinearColor(0.f, 0.f, 0.f, 0.25f)), FVector2D::ZeroVector, FVector2D(LineLeft + 3.f, 118.f), FVector2D(584.f, 4.f), FVector2D::ZeroVector);
		Place(BannerPanel, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(LineLeft, 115.f), FVector2D(584.f, 4.f), FVector2D::ZeroVector);
		BannerPanel->SetRenderOpacity(0.f);
		Banner = BannerPanel;

		// ---- Bloc du bas à droite (repère local : coin haut gauche en 1648, 728 de la maquette)
		UCanvasPanel* Block = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InfoBlock"));
		Place(Canvas, Block, FVector2D(1.f, 1.f), FVector2D::ZeroVector, BlockSize, FVector2D(1.f, 1.f));
		InfoBlock = Block;

		// Nom du lieu : capitales de 16 px (Figma 24 px) de y 734 à 750, centré sur le trait
		PlaceText = MakeText(18.f, ETextJustify::Center);
		Place(Block, PlaceText, FVector2D::ZeroVector, FVector2D(17.f, -1.6f), FVector2D(206.f, 31.f), FVector2D::ZeroVector);
		// Trait blanc 206 × 2 en (1665, 759)
		Place(Block, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(17.f, 31.f), FVector2D(206.f, 2.f), FVector2D::ZeroVector);
		// Heure | température | météo aux positions de la maquette : textes centrés en x 1698 et 1773,5 (capitales y 771-787),
		// traits verticaux 2 × 24 en x 1738 et 1805 (y 768), picto 26 × 26 en (1825, 766)
		TimeText = MakeText(18.f, ETextJustify::Center);
		Place(Block, TimeText, FVector2D::ZeroVector, FVector2D(50.f, 35.4f), FVector2D(76.f, 31.f), FVector2D(0.5f, 0.f));
		Place(Block, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(90.f, 40.f), FVector2D(2.f, 24.f), FVector2D::ZeroVector);
		TemperatureText = MakeText(18.f, ETextJustify::Center);
		Place(Block, TemperatureText, FVector2D::ZeroVector, FVector2D(125.5f, 35.4f), FVector2D(60.f, 31.f), FVector2D(0.5f, 0.f));
		Place(Block, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(157.f, 40.f), FVector2D(2.f, 24.f), FVector2D::ZeroVector);
		WeatherImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Weather"));
		Place(Block, WeatherImage, FVector2D::ZeroVector, FVector2D(177.f, 38.f), FVector2D(26.f, 26.f), FVector2D::ZeroVector);
		for (const TCHAR* Name : { TEXT("T_HUD_Weather_Sun"), TEXT("T_HUD_Weather_Cloud"), TEXT("T_HUD_Weather_Rain"), TEXT("T_HUD_Weather_Wind") })
		{
			WeatherIcons.Add(LoadHUDAsset<UTexture2D>(Name));
		}

		// Boussole « verre » : décor flouté dans le disque, puis l'image du disque avec le N, puis le cône et le point du joueur
		UBackgroundBlur* Glass = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("CompassBlur"));
		Glass->SetBlurStrength(CompassBlur);
		Glass->SetCornerRadius(FVector4(CompassRadius, CompassRadius, CompassRadius, CompassRadius));
		Place(Block, Glass, FVector2D::ZeroVector, CompassCenter - FVector2D(CompassRadius), FVector2D(CompassRadius * 2.f), FVector2D::ZeroVector);
		UImage* Disc = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Compass"));
		if (UTexture2D* DiscTexture = LoadHUDAsset<UTexture2D>(TEXT("T_HUD_Compass")))
		{
			Disc->SetBrushFromTexture(DiscTexture, false);
		}
		else
		{
			Disc->SetVisibility(ESlateVisibility::Collapsed);
		}
		Place(Block, Disc, FVector2D::ZeroVector, CompassCenter - CompassImageCenter, CompassImageSize, FVector2D::ZeroVector);
		// Flèche de L'amie (cône + point) : tourne autour de son point, posé au centre du disque
		ViewCone = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ViewCone"));
		if (UTexture2D* ArrowTexture = LoadHUDAsset<UTexture2D>(TEXT("T_HUD_CompassArrow")))
		{
			ViewCone->SetBrushFromTexture(ArrowTexture, false);
		}
		else
		{
			ViewCone->SetVisibility(ESlateVisibility::Collapsed);
		}
		ViewCone->SetRenderTransformPivot(ArrowDot / ArrowImageSize);
		Place(Block, ViewCone, FVector2D::ZeroVector, CompassCenter - ArrowDot, ArrowImageSize, FVector2D::ZeroVector);
		GpsMarker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Gps"));
		GpsMarker->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.8f, 0.2f), 7.f, FLinearColor::White, 2.f));
		GpsMarker->SetVisibility(ESlateVisibility::Collapsed);
		Place(Block, GpsMarker, FVector2D::ZeroVector, CompassCenter - FVector2D(7.f), FVector2D(14.f), FVector2D::ZeroVector);
		GpsText = MakeText(14.f, ETextJustify::Center);
		Place(Block, GpsText, FVector2D::ZeroVector, FVector2D(17.f, CompassCenter.Y + 22.f), FVector2D(206.f, 40.f), FVector2D::ZeroVector);

		// ---- Appareil photo : aide en bas au centre (absente des photos)
		UCanvasPanel* PhotoLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Photo"));
		UOverlaySlot* PhotoSlot = Root->AddChildToOverlay(PhotoLayer);
		PhotoSlot->SetHorizontalAlignment(HAlign_Fill);
		PhotoSlot->SetVerticalAlignment(VAlign_Fill);
		PhotoText = MakeText(20.f, ETextJustify::Center);
		Place(PhotoLayer, PhotoText, FVector2D(0.5f, 1.f), FVector2D(0.f, -48.f), FVector2D(1000.f, 60.f), FVector2D(0.5f, 1.f));
		PhotoLayer->SetVisibility(ESlateVisibility::Collapsed);
		PhotoHint = PhotoLayer;

		SetWeather(Weather);
		RefreshInfo();
	}
	return Super::RebuildWidget();
}

void UGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions();
	const bool bPhoto = Controller && Controller->IsInPhotoMode();
	// Menus, boutique, PC... montrent la souris : le HUD se retire
	const bool bShow = Controller && !bPhoto && (!Options || Options->bShowHUD) && !Controller->IsGameMenuOpen()
		&& !Controller->IsSettingsMenuOpen() && !Controller->bShowMouseCursor
		&& !(Controller->BattleComponent && Controller->BattleComponent->IsInBattle());
	if (Canvas)
	{
		Canvas->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (PhotoHint)
	{
		PhotoHint->SetVisibility(bPhoto ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const float Now = GetWorld() ? float(GetWorld()->GetRealTimeSeconds()) : 0.f;

	if (bPhoto && PhotoText)
	{
		const bool bFlash = PhotoTime >= 0.f && Now - PhotoTime < 1.5f;
		PhotoText->SetText(bFlash ? LOCTEXT("PhotoSaved", "Photo enregistrée !")
			: LOCTEXT("PhotoHint", "APPAREIL PHOTO     Clic, Entrée ou A : photo     Échap ou B : quitter"));
	}
	if (!bShow)
	{
		return;
	}

	// Grand nom du lieu : fondu d'entrée 0,6 s, puis de sortie 0,8 s
	if (Banner && BannerTime >= 0.f)
	{
		const float Age = Now - BannerTime;
		const float Alpha = Age < 0.6f ? Age / 0.6f : FMath::Clamp((BannerDuration - Age) / 0.8f, 0.f, 1.f);
		Banner->SetRenderOpacity(Alpha);
		if (Age > BannerDuration)
		{
			BannerTime = -1.f;
		}
	}
	if (SavedText && SavedTime >= 0.f)
	{
		const float Age = Now - SavedTime;
		SavedText->SetRenderOpacity(FMath::Clamp(2.5f - Age, 0.f, 1.f));
	}

	InfoTimer -= InDeltaTime;
	if (InfoTimer <= 0.f)
	{
		InfoTimer = 0.5f;
		RefreshInfo();
	}
	UpdateCompass();
}

void UGameHUDWidget::RefreshInfo()
{
	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	const UPokedexModel* Model = UPokedexModel::Get();
	const int32 PlaceIndex = Controller ? Controller->GetCurrentPlace() : INDEX_NONE;
	if (PlaceText)
	{
		PlaceText->SetText(Model && Model->MapPlaces.IsValidIndex(PlaceIndex) ? Model->MapPlaces[PlaceIndex].Name : LOCTEXT("Kanto", "Kanto"));
	}

	// Heure du jeu (cycle jour / nuit de la map), pas celle du PC
	if (TimeText)
	{
		TimeText->SetText(PokemonClock::FormatTime(GetWorld()));
	}
	if (TemperatureText)
	{
		const float Hour = PokemonClock::GetHour(GetWorld());
		TemperatureText->SetText(FormatTemperature(BaseTemperature + TemperatureSwing * FMath::Cos((Hour - 15.f) / 24.f * UE_TWO_PI)));
	}
}

void UGameHUDWidget::UpdateCompass()
{
	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	if (!Controller)
	{
		return;
	}
	// Nord = +X du monde (plan de Kanto) : le cône tourne avec la caméra, dans le sens des aiguilles d'une montre
	if (ViewCone)
	{
		ViewCone->SetRenderTransformAngle(Controller->GetControlRotation().Yaw);
	}

	const UPokedexModel* Model = UPokedexModel::Get();
	const int32 Target = Controller->GetGpsTarget();
	const APawn* Hero = Controller->GetPawn();
	FVector TargetLocation;
	if (!Model || !Hero || !Model->MapPlaces.IsValidIndex(Target) || !Model->MapToWorld(GetWorld(), Model->MapPlaces[Target].UV, TargetLocation))
	{
		if (GpsMarker)
		{
			GpsMarker->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (GpsText)
		{
			GpsText->SetText(FText::GetEmpty());
		}
		return;
	}
	const FVector Delta = TargetLocation - Hero->GetActorLocation();
	const float Bearing = FMath::Atan2(float(Delta.Y), float(Delta.X));
	const FVector2D Direction(FMath::Sin(Bearing), -FMath::Cos(Bearing));
	if (GpsMarker)
	{
		GpsMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
		GpsMarker->SetRenderTranslation(Direction * (CompassRadius - 8.f));
	}
	if (GpsText)
	{
		const float Meters = float(Delta.Size2D()) / 100.f;
		FNumberFormattingOptions OneDecimal;
		OneDecimal.SetMaximumFractionalDigits(1);
		const FText Distance = Meters >= 1000.f
			? FText::Format(LOCTEXT("Km", "{0} km"), FText::AsNumber(Meters / 1000.f, &OneDecimal))
			: FText::Format(LOCTEXT("Meters", "{0} m"), FText::AsNumber(FMath::RoundToInt(Meters)));
		GpsText->SetText(FText::Format(LOCTEXT("GpsInfo", "{0}\n{1}"), Model->MapPlaces[Target].Name, Distance));
	}
}

void UGameHUDWidget::ShowPlaceBanner(const FText& PlaceName)
{
	if (BannerText)
	{
		BannerText->SetText(FText::FromString(PlaceName.ToString().ToUpper()));
	}
	BannerTime = GetWorld() ? float(GetWorld()->GetRealTimeSeconds()) : 0.f;
}

void UGameHUDWidget::OnAutoSaved()
{
	SavedTime = GetWorld() ? float(GetWorld()->GetRealTimeSeconds()) : 0.f;
}

void UGameHUDWidget::OnPhotoTaken()
{
	PhotoTime = GetWorld() ? float(GetWorld()->GetRealTimeSeconds()) : 0.f;
}

void UGameHUDWidget::SetWeather(EHUDWeather NewWeather)
{
	Weather = NewWeather;
	const int32 Index = int32(Weather);
	if (WeatherImage)
	{
		if (WeatherIcons.IsValidIndex(Index) && WeatherIcons[Index])
		{
			WeatherImage->SetBrushFromTexture(WeatherIcons[Index], false);
			WeatherImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			WeatherImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

#undef LOCTEXT_NAMESPACE
