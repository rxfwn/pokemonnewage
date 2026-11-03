#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameHUDWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

/** Temps affiché à côté de la température (pas encore de vrai système météo). */
UENUM(BlueprintType)
enum class EHUDWeather : uint8
{
	Sun,
	Cloud,
	Rain,
	Wind
};

/**
 *  HUD en jeu, d'après la maquette v2 de L'amie (fichiers du projet hud/v2/Town_Announcement.png, 1920 x 1080) :
 *  - icône du Pokédex en haut à droite (Tab ouvre le Pokédex 3D, sans texte de touche) ;
 *  - nom du lieu en grand en haut au centre quand on arrive quelque part (« BOURG PALETTE ») ;
 *  - en bas à droite : nom du lieu, heure, température et météo, boussole avec le cône de vue et le GPS.
 *  Caché pendant les combats, les menus et l'appareil photo, ou par Échap > Jeu > Afficher le HUD.
 *  Images et polices importées par Tools/HUD/import_hud_unreal.py dans /Game/UI/HUD.
 */
UCLASS()
class UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Grand nom du lieu en haut de l'écran, quelques secondes */
	void ShowPlaceBanner(const FText& PlaceName);

	/** Petit message sous l'icône du Pokédex */
	void OnAutoSaved();

	/** Flash « Photo enregistrée » en mode photo */
	void OnPhotoTaken();

	/** Temps affiché (en attendant un système météo) */
	UFUNCTION(BlueprintCallable, Category="HUD")
	void SetWeather(EHUDWeather NewWeather);

	/** Température moyenne et écart jour / nuit, en °C (pic à 15 h, minimum à 3 h) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	float BaseTemperature = 17.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	float TemperatureSwing = 5.f;

	/** Flou du décor dans le disque de la boussole (effet verre de la maquette ; 0 = pas de flou) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	float CompassBlur = 6.f;

	/** Durée du grand nom du lieu à l'écran, en secondes (avec les fondus) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	float BannerDuration = 4.f;

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	/** Lieu, heure, température, GPS (toutes les 0,5 s) */
	void RefreshInfo();

	/** Boussole : cône de vue et repère du GPS (chaque image) */
	void UpdateCompass();

	EHUDWeather Weather = EHUDWeather::Sun;
	float InfoTimer = 0.f;
	float BannerTime = -1.f;
	float SavedTime = -1.f;
	float PhotoTime = -1.f;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UWidget> PokedexIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SavedText;
	UPROPERTY(Transient) TObjectPtr<UWidget> Banner;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerText;
	UPROPERTY(Transient) TObjectPtr<UWidget> InfoBlock;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlaceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TemperatureText;
	UPROPERTY(Transient) TObjectPtr<UImage> WeatherImage;
	UPROPERTY(Transient) TObjectPtr<UImage> ViewCone;
	UPROPERTY(Transient) TObjectPtr<UBorder> GpsMarker;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GpsText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PhotoHint;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhotoText;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> WeatherIcons;
};
