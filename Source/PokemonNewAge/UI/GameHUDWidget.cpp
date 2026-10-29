#include "UI/GameHUDWidget.h"
#include "Battle/BattleComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Core/PokemonSaveGame.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "PokemonNewAgePlayerController.h"
#include "UI/Pokedex/PokedexModel.h"

#define LOCTEXT_NAMESPACE "GameHUD"

namespace GameHUD
{
	/** Assets créés par Tools/HUD/import_hud_unreal.py */
	const TCHAR* Folder = TEXT("/Game/UI/HUD");

	/** Bloc du bas à droite de la maquette : coin haut gauche (1648, 728) sur 1920 x 1080 */
	const FVector2D BlockSize(272.f, 352.f);
	const FVector2D CompassCenter(120.f, 196.f);
	constexpr float CompassRadius = 103.f;

	template <typename T>
	T* LoadHUDAsset(const TCHAR* Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Police de la maquette (Inter, Futura) depuis sa Font Face ; Roboto si elle n'est pas importée */
	FSlateFontInfo MakeFont(const TCHAR* FaceName, float Size, const FSlateFontInfo& Fallback)
	{
		if (const UFontFace* Face = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("%s/Fonts/%s.%s"), Folder, FaceName, FaceName), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
			Composite->DefaultTypeface.Fonts.Add(FTypefaceEntry(TEXT("Regular"), Face));
			return FSlateFontInfo(Composite, Size);
		}
		FSlateFontInfo Font = Fallback;
		Font.Size = Size;
		return Font;
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

		UTextBlock* Probe = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const FSlateFontInfo Default = Probe->GetFont();
		auto MakeText = [this, &Default](const TCHAR* Face, float Size, ETextJustify::Type Justify)
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Label->SetFont(MakeFont(Face, Size, Default));
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Label->SetJustification(Justify);
			Label->SetShadowOffset(FVector2D(1.f, 1.f));
			Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f));
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
		PokedexKey = MakeText(TEXT("FF_Inter_Bold"), 14.f, ETextJustify::Center);
		Place(Canvas, PokedexKey, FVector2D(1.f, 0.f), FVector2D(-96.f, 151.f), FVector2D(112.f, 20.f), FVector2D(0.5f, 0.f));
		SavedText = MakeText(TEXT("FF_Inter_Regular"), 14.f, ETextJustify::Right);
		SavedText->SetText(LOCTEXT("Saved", "Partie sauvegardée"));
		SavedText->SetRenderOpacity(0.f);
		Place(Canvas, SavedText, FVector2D(1.f, 0.f), FVector2D(-40.f, 174.f), FVector2D(220.f, 20.f), FVector2D(1.f, 0.f));

		// ---- Grand nom du lieu (Futura 95, centré à 157 px du haut) et trait de 584 x 4 à 268 px
		UCanvasPanel* BannerPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Banner"));
		Place(Canvas, BannerPanel, FVector2D(0.5f, 0.f), FVector2D(0.f, 157.f), FVector2D(846.f, 120.f), FVector2D(0.5f, 0.f));
		BannerText = MakeText(TEXT("FF_Futura_Bold"), 95.f, ETextJustify::Center);
		BannerText->SetShadowOffset(FVector2D(3.f, 3.f));
		BannerText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Place(BannerPanel, BannerText, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(846.f, 117.f), FVector2D::ZeroVector);
		const float LineLeft = (846.f - 584.f) * 0.5f;
		Place(BannerPanel, MakeLine(FLinearColor(0.f, 0.f, 0.f, 0.25f)), FVector2D::ZeroVector, FVector2D(LineLeft + 3.f, 114.f), FVector2D(584.f, 4.f), FVector2D::ZeroVector);
		Place(BannerPanel, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(LineLeft, 111.f), FVector2D(584.f, 4.f), FVector2D::ZeroVector);
		BannerPanel->SetRenderOpacity(0.f);
		Banner = BannerPanel;

		// ---- Bloc du bas à droite (repère local : coin haut gauche en 1648, 728 de la maquette)
		UCanvasPanel* Block = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InfoBlock"));
		Place(Canvas, Block, FVector2D(1.f, 1.f), FVector2D::ZeroVector, BlockSize, FVector2D(1.f, 1.f));
		InfoBlock = Block;

		PlaceText = MakeText(TEXT("FF_Inter_Regular"), 24.f, ETextJustify::Center);
		Place(Block, PlaceText, FVector2D::ZeroVector, FVector2D(17.f, 0.f), FVector2D(206.f, 29.f), FVector2D::ZeroVector);
		Place(Block, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(17.f, 32.f), FVector2D(206.f, 2.f), FVector2D::ZeroVector);
		TimeText = MakeText(TEXT("FF_Inter_Regular"), 24.f, ETextJustify::Center);
		Place(Block, TimeText, FVector2D::ZeroVector, FVector2D(17.f, 37.f), FVector2D(65.f, 29.f), FVector2D::ZeroVector);
		Place(Block, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(88.f, 39.f), FVector2D(2.f, 25.f), FVector2D::ZeroVector);
		TemperatureText = MakeText(TEXT("FF_Inter_Regular"), 24.f, ETextJustify::Center);
		Place(Block, TemperatureText, FVector2D::ZeroVector, FVector2D(93.f, 37.f), FVector2D(65.f, 29.f), FVector2D::ZeroVector);
		Place(Block, MakeLine(FLinearColor::White), FVector2D::ZeroVector, FVector2D(159.f, 39.f), FVector2D(2.f, 25.f), FVector2D::ZeroVector);
		WeatherImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Weather"));
		Place(Block, WeatherImage, FVector2D::ZeroVector, FVector2D(176.f, 37.f), FVector2D(29.f, 29.f), FVector2D::ZeroVector);
		for (const TCHAR* Name : { TEXT("T_HUD_Weather_Sun"), TEXT("T_HUD_Weather_Cloud"), TEXT("T_HUD_Weather_Rain") })
		{
			WeatherIcons.Add(LoadHUDAsset<UTexture2D>(Name));
		}

		// Boussole : disque gris 40 %, bord blanc 45 %, cône de vue, point du joueur, N en haut
		UBorder* Disc = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Compass"));
		Disc->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.85f, 0.85f, 0.85f, 0.4f), CompassRadius, FLinearColor(1.f, 1.f, 1.f, 0.45f), 2.f));
		Place(Block, Disc, FVector2D::ZeroVector, CompassCenter - FVector2D(CompassRadius), FVector2D(CompassRadius * 2.f), FVector2D::ZeroVector);
		ViewCone = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ViewCone"));
		if (UTexture2D* ConeTexture = LoadHUDAsset<UTexture2D>(TEXT("T_HUD_ViewCone")))
		{
			ViewCone->SetBrushFromTexture(ConeTexture, false);
		}
		else
		{
			ViewCone->SetVisibility(ESlateVisibility::Collapsed);
		}
		ViewCone->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
		Place(Block, ViewCone, FVector2D::ZeroVector, CompassCenter - FVector2D(126.f, 119.f), FVector2D(252.f, 119.f), FVector2D::ZeroVector);
		UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Player"));
		Dot->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 6.f));
		Place(Block, Dot, FVector2D::ZeroVector, CompassCenter - FVector2D(6.f), FVector2D(12.f), FVector2D::ZeroVector);
		UTextBlock* North = MakeText(TEXT("FF_Inter_Bold"), 25.f, ETextJustify::Center);
		North->SetText(LOCTEXT("North", "N"));
		Place(Block, North, FVector2D::ZeroVector, FVector2D(82.f, 78.f), FVector2D(75.f, 30.f), FVector2D::ZeroVector);
		GpsMarker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Gps"));
		GpsMarker->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.8f, 0.2f), 7.f, FLinearColor::White, 2.f));
		GpsMarker->SetVisibility(ESlateVisibility::Collapsed);
		Place(Block, GpsMarker, FVector2D::ZeroVector, CompassCenter - FVector2D(7.f), FVector2D(14.f), FVector2D::ZeroVector);
		GpsText = MakeText(TEXT("FF_Inter_Regular"), 14.f, ETextJustify::Center);
		Place(Block, GpsText, FVector2D::ZeroVector, FVector2D(17.f, CompassCenter.Y + 22.f), FVector2D(206.f, 40.f), FVector2D::ZeroVector);

		// ---- Appareil photo : aide en bas au centre (absente des photos)
		UCanvasPanel* PhotoLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Photo"));
		UOverlaySlot* PhotoSlot = Root->AddChildToOverlay(PhotoLayer);
		PhotoSlot->SetHorizontalAlignment(HAlign_Fill);
		PhotoSlot->SetVerticalAlignment(VAlign_Fill);
		PhotoText = MakeText(TEXT("FF_Inter_Regular"), 20.f, ETextJustify::Center);
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

	const FDateTime Now = FDateTime::Now();
	if (TimeText)
	{
		TimeText->SetText(FText::FromString(Now.ToString(TEXT("%H:%M"))));
	}
	if (TemperatureText)
	{
		const float Hour = Now.GetHour() + Now.GetMinute() / 60.f;
		TemperatureText->SetText(FormatTemperature(BaseTemperature + TemperatureSwing * FMath::Cos((Hour - 15.f) / 24.f * UE_TWO_PI)));
	}
	if (PokedexKey && Controller)
	{
		PokedexKey->SetText(Controller->GetMenuKey().GetDisplayName(false));
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
