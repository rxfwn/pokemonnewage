#include "UI/SettingsMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/PokemonSaveGame.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PokemonNewAgePlayerController.h"

#define LOCTEXT_NAMESPACE "SettingsMenu"

namespace SettingsMenu
{
	const FLinearColor PanelColor(0.07f, 0.08f, 0.1f, 0.93f);
	const FLinearColor TabColor(0.16f, 0.17f, 0.2f, 1.f);
	const FLinearColor TabSelectedColor(0.85f, 0.87f, 0.9f, 1.f);
	const FLinearColor TextColor(0.94f, 0.95f, 0.96f);
	const FLinearColor DimColor(0.62f, 0.65f, 0.7f);
	const FLinearColor BoxColor(0.97f, 0.97f, 0.97f);
	const FLinearColor BoxTextColor(0.05f, 0.05f, 0.07f);
	const FLinearColor ButtonColor(0.22f, 0.24f, 0.28f, 1.f);
	const FLinearColor DangerColor(0.75f, 0.2f, 0.2f, 1.f);

	/** Limites d'images par seconde proposées (0 = sans limite) */
	const float FrameLimits[] = { 0.f, 30.f, 60.f, 120.f, 144.f, 240.f };

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Value, int32 Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Value);
		Label->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = Size;
		if (bBold)
		{
			Font.TypefaceFontName = FName(TEXT("Bold"));
		}
		Label->SetFont(Font);
		return Label;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, const FLinearColor& Color, float Width)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(Color);
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetMinDesiredWidth(Width);
		UTextBlock* Text = MakeText(Tree, Label, 15, TextColor);
		Text->SetJustification(ETextJustify::Center);
		Box->SetContent(Text);
		Button->SetContent(Box);
		return Button;
	}

	TArray<FText> QualityNames()
	{
		return { LOCTEXT("QLow", "Basse"), LOCTEXT("QMedium", "Moyenne"), LOCTEXT("QHigh", "Haute"),
			LOCTEXT("QEpic", "Épique"), LOCTEXT("QCinematic", "Cinéma") };
	}

	TArray<FText> YesNo()
	{
		return { LOCTEXT("No", "Non"), LOCTEXT("Yes", "Oui") };
	}

	FText Percent(float Value)
	{
		return FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(Value * 100.f)));
	}
}

using namespace SettingsMenu;

// ================================================================ Ligne

TSharedRef<SWidget> USettingsRowWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Line"));
		WidgetTree->RootWidget = Line;

		if (Kind == ESettingRowKind::Header)
		{
			UHorizontalBoxSlot* HeaderSlot = Line->AddChildToHorizontalBox(MakeText(WidgetTree, Label, 15, DimColor, true));
			HeaderSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f));
			return Super::RebuildWidget();
		}

		// Nom du réglage
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(250.f);
		LabelBox->SetContent(MakeText(WidgetTree, Label, 16, TextColor));
		UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(LabelBox);
		LabelSlot->SetVerticalAlignment(VAlign_Center);

		if (Kind == ESettingRowKind::Key || Kind == ESettingRowKind::Action)
		{
			MainButton = MakeButton(WidgetTree, ButtonText, Kind == ESettingRowKind::Action && Id == FName(TEXT("DeleteSave")) ? DangerColor : ButtonColor, 220.f);
			ButtonLabel = Cast<UTextBlock>(Cast<USizeBox>(MainButton->GetContent())->GetContent());
			MainButton->OnClicked.AddDynamic(this, &USettingsRowWidget::HandleButton);
			Line->AddChildToHorizontalBox(MainButton)->SetVerticalAlignment(VAlign_Center);
			return Super::RebuildWidget();
		}

		// Case blanche avec la valeur, comme dans la référence de L'amie
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrush(FSlateRoundedBoxBrush(BoxColor, 3.f));
		Box->SetPadding(FMargin(8.f, 2.f));
		USizeBox* BoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BoxSize->SetMinDesiredWidth(Kind == ESettingRowKind::Choice ? 190.f : 70.f);
		ValueText = MakeText(WidgetTree, FText::GetEmpty(), 15, BoxTextColor, true);
		ValueText->SetJustification(ETextJustify::Center);
		BoxSize->SetContent(ValueText);
		Box->SetContent(BoxSize);

		if (Kind == ESettingRowKind::Choice)
		{
			UButton* Previous = MakeButton(WidgetTree, FText::FromString(TEXT("<")), ButtonColor, 34.f);
			Previous->OnClicked.AddDynamic(this, &USettingsRowWidget::HandlePrevious);
			Line->AddChildToHorizontalBox(Previous)->SetVerticalAlignment(VAlign_Center);
			UHorizontalBoxSlot* BoxSlot = Line->AddChildToHorizontalBox(Box);
			BoxSlot->SetPadding(FMargin(6.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			UButton* Next = MakeButton(WidgetTree, FText::FromString(TEXT(">")), ButtonColor, 34.f);
			Next->OnClicked.AddDynamic(this, &USettingsRowWidget::HandleNext);
			Line->AddChildToHorizontalBox(Next)->SetVerticalAlignment(VAlign_Center);
			MainButton = Previous;
		}
		else
		{
			UHorizontalBoxSlot* BoxSlot = Line->AddChildToHorizontalBox(Box);
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			ValueSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
			ValueSlider->SetMinValue(Min);
			ValueSlider->SetMaxValue(Max);
			ValueSlider->SetStepSize(Step);
			ValueSlider->SetSliderBarColor(FLinearColor(0.45f, 0.47f, 0.5f));
			ValueSlider->SetSliderHandleColor(FLinearColor::White);
			ValueSlider->OnValueChanged.AddDynamic(this, &USettingsRowWidget::HandleSlider);
			UHorizontalBoxSlot* SliderSlot = Line->AddChildToHorizontalBox(ValueSlider);
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetPadding(FMargin(16.f, 0.f, 8.f, 0.f));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		}
		RefreshValue();
	}
	return Super::RebuildWidget();
}

void USettingsRowWidget::RefreshValue()
{
	if (ValueText)
	{
		if (Kind == ESettingRowKind::Choice)
		{
			ValueText->SetText(Choices.IsValidIndex(Index) ? Choices[Index] : FText::GetEmpty());
		}
		else if (Kind == ESettingRowKind::Slider)
		{
			ValueText->SetText(Format ? Format(Value) : FText::AsNumber(Value));
		}
	}
	if (ValueSlider && !FMath::IsNearlyEqual(ValueSlider->GetValue(), Value))
	{
		ValueSlider->SetValue(Value);
	}
	if (ButtonLabel)
	{
		ButtonLabel->SetText(ButtonText);
	}
}

UWidget* USettingsRowWidget::GetFocusTarget() const
{
	if (ValueSlider)
	{
		return ValueSlider;
	}
	return MainButton;
}

void USettingsRowWidget::HandleSlider(float NewValue)
{
	const float Snapped = Step > 0.f ? FMath::GridSnap(NewValue, Step) : NewValue;
	Value = FMath::Clamp(Snapped, Min, Max);
	RefreshValue();
	if (USettingsMenuWidget* Owner = Menu.Get())
	{
		Owner->OnRowChanged(this);
	}
}

void USettingsRowWidget::HandlePrevious()
{
	if (Choices.Num() > 0)
	{
		Index = (Index - 1 + Choices.Num()) % Choices.Num();
		RefreshValue();
		if (USettingsMenuWidget* Owner = Menu.Get())
		{
			Owner->OnRowChanged(this);
		}
	}
}

void USettingsRowWidget::HandleNext()
{
	if (Choices.Num() > 0)
	{
		Index = (Index + 1) % Choices.Num();
		RefreshValue();
		if (USettingsMenuWidget* Owner = Menu.Get())
		{
			Owner->OnRowChanged(this);
		}
	}
}

void USettingsRowWidget::HandleButton()
{
	if (USettingsMenuWidget* Owner = Menu.Get())
	{
		Owner->OnRowAction(this);
	}
}

// ================================================================ Menu

TSharedRef<SWidget> USettingsMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur"));
		Blur->SetBlurStrength(6.f);
		UOverlaySlot* BlurSlot = Root->AddChildToOverlay(Blur);
		BlurSlot->SetHorizontalAlignment(HAlign_Fill);
		BlurSlot->SetVerticalAlignment(VAlign_Fill);

		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);

		// Panneau central
		USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
		PanelSize->SetWidthOverride(1080.f);
		PanelSize->SetHeightOverride(660.f);
		UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize);
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrush(FSlateRoundedBoxBrush(PanelColor, 8.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), 1.f));
		Panel->SetPadding(FMargin(22.f, 16.f));
		PanelSize->SetContent(Panel);

		UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Main"));
		Panel->SetContent(Main);
		UVerticalBoxSlot* TitleSlot = Main->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Title", "Paramètres"), 22, TextColor, true));
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

		// Onglets à gauche, réglages à droite
		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
		Main->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBox* Tabs = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Tabs"));
		Body->AddChildToHorizontalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		const FText TabNames[] = { LOCTEXT("TabAudio", "Audio"), LOCTEXT("TabVideo", "Vidéo"), LOCTEXT("TabControls", "Commandes"), LOCTEXT("TabGame", "Jeu") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TabNames); ++Index)
		{
			UButton* TabButton = MakeButton(WidgetTree, TabNames[Index], TabColor, 150.f);
			switch (Index)
			{
			case 0: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnAudioTab); break;
			case 1: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnVideoTab); break;
			case 2: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnControlsTab); break;
			default: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnGameTab); break;
			}
			Tabs->AddChildToVerticalBox(TabButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			TabButtons.Add(TabButton);
		}

		RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Rows"));
		Body->AddChildToHorizontalBox(RowList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Message et boutons du bas
		UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Bottom"));
		Main->AddChildToVerticalBox(Bottom)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		StatusText = MakeText(WidgetTree, FText::GetEmpty(), 14, DimColor);
		UHorizontalBoxSlot* StatusSlot = Bottom->AddChildToHorizontalBox(StatusText);
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		const TPair<FText, int32> Buttons[] = {
			{ LOCTEXT("Ok", "OK"), 0 }, { LOCTEXT("Cancel", "Annuler"), 1 }, { LOCTEXT("Apply", "Appliquer"), 2 }, { LOCTEXT("Back", "Retour"), 3 } };
		for (const TPair<FText, int32>& Entry : Buttons)
		{
			UButton* Button = MakeButton(WidgetTree, Entry.Key, ButtonColor, 110.f);
			switch (Entry.Value)
			{
			case 0: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnOkClicked); break;
			case 1: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCancelClicked); break;
			case 2: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnApplyClicked); break;
			default: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnBackClicked); break;
			}
			UHorizontalBoxSlot* ButtonSlot = Bottom->AddChildToHorizontalBox(Button);
			ButtonSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
	}
	return Super::RebuildWidget();
}

void USettingsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	LoadValues();
	ShowTab(ETab::Audio);
}

void USettingsMenuWidget::FocusFirst()
{
	if (TabButtons.Num() > 0)
	{
		TabButtons[0]->SetKeyboardFocus();
	}
	else
	{
		SetKeyboardFocus();
	}
}

// ---------------------------------------------------------------- Valeurs

void USettingsMenuWidget::LoadValues()
{
	Values = FValues();
	if (const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions())
	{
		Values.MasterVolume = Options->MasterVolume;
		Values.MusicVolume = Options->MusicVolume;
		Values.EffectsVolume = Options->EffectsVolume;
		Values.TextSpeed = Options->TextSpeed;
		Values.CameraSensitivity = Options->CameraSensitivity;
		Values.bInvertCameraY = Options->bInvertCameraY;
		Values.FieldOfView = Options->FieldOfView;
		Values.bShowHUD = Options->bShowHUD;
		Values.bAutoSave = Options->bAutoSave;
	}
	if (const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Values.Resolution = Settings->GetScreenResolution();
		Values.WindowMode = int32(Settings->GetFullscreenMode());
		Values.bVSync = Settings->IsVSyncEnabled();
		Values.FrameRateLimit = Settings->GetFrameRateLimit();
		float Normalized = 1.f, Current = 100.f, MinScale = 50.f, MaxScale = 100.f;
		Settings->GetResolutionScaleInformationEx(Normalized, Current, MinScale, MaxScale);
		Values.ResolutionScale = Current;
		Values.ViewDistance = Settings->GetViewDistanceQuality();
		Values.AntiAliasing = Settings->GetAntiAliasingQuality();
		Values.Shadows = Settings->GetShadowQuality();
		Values.Textures = Settings->GetTextureQuality();
		Values.Effects = Settings->GetVisualEffectQuality();
		Values.PostProcess = Settings->GetPostProcessingQuality();
		Values.Foliage = Settings->GetFoliageQuality();
	}
}

void USettingsMenuWidget::ApplyValues()
{
	if (UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions())
	{
		Options->MasterVolume = Values.MasterVolume;
		Options->MusicVolume = Values.MusicVolume;
		Options->EffectsVolume = Values.EffectsVolume;
		Options->TextSpeed = Values.TextSpeed;
		Options->CameraSensitivity = Values.CameraSensitivity;
		Options->bInvertCameraY = Values.bInvertCameraY;
		Options->FieldOfView = Values.FieldOfView;
		Options->bShowHUD = Values.bShowHUD;
		Options->bAutoSave = Values.bAutoSave;
		UPokemonSaveLibrary::SaveOptions(Options);
	}
	if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		Controller->ApplyGameOptions();
	}
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetScreenResolution(Values.Resolution);
		Settings->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(Values.WindowMode));
		Settings->SetVSyncEnabled(Values.bVSync);
		Settings->SetFrameRateLimit(Values.FrameRateLimit);
		Settings->SetResolutionScaleValueEx(Values.ResolutionScale);
		Settings->SetViewDistanceQuality(Values.ViewDistance);
		Settings->SetAntiAliasingQuality(Values.AntiAliasing);
		Settings->SetShadowQuality(Values.Shadows);
		Settings->SetTextureQuality(Values.Textures);
		Settings->SetVisualEffectQuality(Values.Effects);
		Settings->SetPostProcessingQuality(Values.PostProcess);
		Settings->SetFoliageQuality(Values.Foliage);
		// Enregistre aussi GameUserSettings.ini
		Settings->ApplySettings(false);
	}
	SetStatus(LOCTEXT("Applied", "Réglages appliqués."));
}

// ---------------------------------------------------------------- Onglets

USettingsRowWidget* USettingsMenuWidget::AddRow(FName Id, ESettingRowKind Kind, const FText& Label)
{
	USettingsRowWidget* Row = CreateWidget<USettingsRowWidget>(this, USettingsRowWidget::StaticClass());
	Row->Id = Id;
	Row->Kind = Kind;
	Row->Label = Label;
	Row->Menu = this;
	return Row;
}

void USettingsMenuWidget::AddHeader(const FText& Label)
{
	USettingsRowWidget* Row = AddRow(NAME_None, ESettingRowKind::Header, Label);
	RowList->AddChild(Row);
}

void USettingsMenuWidget::AddSlider(FName Id, const FText& Label, float Min, float Max, float Step, float Value, TFunction<FText(float)> Format)
{
	USettingsRowWidget* Row = AddRow(Id, ESettingRowKind::Slider, Label);
	Row->Min = Min;
	Row->Max = Max;
	Row->Step = Step;
	Row->Value = FMath::Clamp(Value, Min, Max);
	Row->Format = MoveTemp(Format);
	RowList->AddChild(Row);
	Rows.Add(Row);
}

void USettingsMenuWidget::AddChoice(FName Id, const FText& Label, const TArray<FText>& Choices, int32 Index)
{
	USettingsRowWidget* Row = AddRow(Id, ESettingRowKind::Choice, Label);
	Row->Choices = Choices;
	Row->Index = FMath::Clamp(Index, 0, FMath::Max(0, Choices.Num() - 1));
	RowList->AddChild(Row);
	Rows.Add(Row);
}

void USettingsMenuWidget::AddToggle(FName Id, const FText& Label, bool bValue)
{
	AddChoice(Id, Label, YesNo(), bValue ? 1 : 0);
}

void USettingsMenuWidget::ShowTab(ETab NewTab)
{
	Tab = NewTab;
	CapturingKey = NAME_None;
	bConfirmDelete = false;
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		TabButtons[Index]->SetBackgroundColor(Index == int32(Tab) ? TabSelectedColor : TabColor);
	}
	if (!RowList)
	{
		return;
	}
	RowList->ClearChildren();
	Rows.Reset();

	auto PercentFormat = [](float Value) { return Percent(Value); };
	auto Quality = [this](FName Id, const FText& Label, int32 Value) { AddChoice(Id, Label, QualityNames(), Value); };

	switch (Tab)
	{
	case ETab::Audio:
		AddSlider(TEXT("MasterVolume"), LOCTEXT("MasterVolume", "Volume général"), 0.f, 1.f, 0.05f, Values.MasterVolume, PercentFormat);
		AddSlider(TEXT("MusicVolume"), LOCTEXT("MusicVolume", "Musique"), 0.f, 1.f, 0.05f, Values.MusicVolume, PercentFormat);
		AddSlider(TEXT("EffectsVolume"), LOCTEXT("EffectsVolume", "Bruitages"), 0.f, 1.f, 0.05f, Values.EffectsVolume, PercentFormat);
		break;

	case ETab::Video:
	{
		TArray<FText> ResolutionNames;
		int32 ResolutionIndex = Resolutions.IndexOfByKey(Values.Resolution);
		if (ResolutionIndex == INDEX_NONE)
		{
			Resolutions.Add(Values.Resolution);
			ResolutionIndex = Resolutions.Num() - 1;
		}
		for (const FIntPoint& Resolution : Resolutions)
		{
			ResolutionNames.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y)));
		}
		AddChoice(TEXT("Resolution"), LOCTEXT("Resolution", "Résolution"), ResolutionNames, ResolutionIndex);
		AddChoice(TEXT("WindowMode"), LOCTEXT("WindowMode", "Mode d'affichage"),
			{ LOCTEXT("Fullscreen", "Plein écran"), LOCTEXT("Borderless", "Plein écran fenêtré"), LOCTEXT("Windowed", "Fenêtré") }, Values.WindowMode);
		AddToggle(TEXT("VSync"), LOCTEXT("VSync", "Synchro verticale"), Values.bVSync);
		TArray<FText> LimitNames;
		int32 LimitIndex = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameLimits); ++Index)
		{
			LimitNames.Add(FrameLimits[Index] <= 0.f ? LOCTEXT("NoLimit", "Sans limite") : FText::AsNumber(FMath::RoundToInt(FrameLimits[Index])));
			if (FMath::IsNearlyEqual(FrameLimits[Index], Values.FrameRateLimit))
			{
				LimitIndex = Index;
			}
		}
		AddChoice(TEXT("FrameLimit"), LOCTEXT("FrameLimit", "Images par seconde max."), LimitNames, LimitIndex);
		AddSlider(TEXT("FieldOfView"), LOCTEXT("FieldOfView", "Champ de vision"), 60.f, 120.f, 1.f, Values.FieldOfView,
			[](float Value) { return FText::AsNumber(FMath::RoundToInt(Value)); });
		AddSlider(TEXT("ResolutionScale"), LOCTEXT("ResolutionScale", "Échelle de résolution"), 50.f, 100.f, 1.f, Values.ResolutionScale,
			[](float Value) { return FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Value))); });
		AddHeader(LOCTEXT("QualityHeader", "QUALITÉ"));
		AddChoice(TEXT("Preset"), LOCTEXT("Preset", "Qualité globale"), QualityNames(), Values.Shadows);
		Quality(TEXT("ViewDistance"), LOCTEXT("ViewDistance", "Distance d'affichage"), Values.ViewDistance);
		Quality(TEXT("AntiAliasing"), LOCTEXT("AntiAliasing", "Anticrénelage"), Values.AntiAliasing);
		Quality(TEXT("Shadows"), LOCTEXT("Shadows", "Ombres"), Values.Shadows);
		Quality(TEXT("Textures"), LOCTEXT("Textures", "Textures"), Values.Textures);
		Quality(TEXT("Effects"), LOCTEXT("Effects", "Effets"), Values.Effects);
		Quality(TEXT("PostProcess"), LOCTEXT("PostProcess", "Post-traitement"), Values.PostProcess);
		Quality(TEXT("Foliage"), LOCTEXT("Foliage", "Végétation"), Values.Foliage);
		break;
	}

	case ETab::Controls:
	{
		AddSlider(TEXT("Sensitivity"), LOCTEXT("Sensitivity", "Sensibilité de la caméra"), 0.5f, 2.f, 0.1f, Values.CameraSensitivity,
			[](float Value) { return FText::FromString(FString::Printf(TEXT("x %.1f"), Value)); });
		AddToggle(TEXT("InvertY"), LOCTEXT("InvertY", "Inverser haut/bas"), Values.bInvertCameraY);

		const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
		const TArray<FPokemonBindableKey> Keys = Controller ? Controller->GetBindableKeys() : TArray<FPokemonBindableKey>();
		bool bPadHeader = false;
		AddHeader(LOCTEXT("Keyboard", "CLAVIER"));
		for (const FPokemonBindableKey& Entry : Keys)
		{
			if (Entry.bGamepad && !bPadHeader)
			{
				bPadHeader = true;
				AddHeader(LOCTEXT("Gamepad", "MANETTE"));
			}
			USettingsRowWidget* Row = AddRow(Entry.Id, ESettingRowKind::Key, Entry.Name);
			Row->ButtonText = Entry.Key.GetDisplayName(false);
			RowList->AddChild(Row);
			Rows.Add(Row);
		}
		USettingsRowWidget* Reset = AddRow(TEXT("ResetKeys"), ESettingRowKind::Action, FText::GetEmpty());
		Reset->ButtonText = LOCTEXT("ResetKeys", "Remettre les touches d'origine");
		RowList->AddChild(Reset);
		Rows.Add(Reset);
		break;
	}

	case ETab::Game:
	{
		AddChoice(TEXT("TextSpeed"), LOCTEXT("TextSpeed", "Vitesse du texte"),
			{ LOCTEXT("Slow", "Lente"), LOCTEXT("Normal", "Normale"), LOCTEXT("Fast", "Rapide") }, Values.TextSpeed);
		AddToggle(TEXT("ShowHUD"), LOCTEXT("ShowHUD", "Afficher le HUD (lieu, heure, boussole)"), Values.bShowHUD);
		AddToggle(TEXT("AutoSave"), LOCTEXT("AutoSave", "Sauvegarde automatique"), Values.bAutoSave);
		AddHeader(LOCTEXT("SaveHeader", "PARTIE"));
		USettingsRowWidget* Delete = AddRow(TEXT("DeleteSave"), ESettingRowKind::Action, LOCTEXT("DeleteLabel", "Recommencer de zéro"));
		Delete->ButtonText = LOCTEXT("DeleteSave", "Effacer la sauvegarde");
		RowList->AddChild(Delete);
		Rows.Add(Delete);
		USettingsRowWidget* Quit = AddRow(TEXT("Quit"), ESettingRowKind::Action, FText::GetEmpty());
		Quit->ButtonText = LOCTEXT("Quit", "Quitter le jeu");
		RowList->AddChild(Quit);
		Rows.Add(Quit);
		break;
	}
	}
	RowList->ScrollToStart();
}

void USettingsMenuWidget::OnAudioTab() { ShowTab(ETab::Audio); }
void USettingsMenuWidget::OnVideoTab() { ShowTab(ETab::Video); }
void USettingsMenuWidget::OnControlsTab() { ShowTab(ETab::Controls); }
void USettingsMenuWidget::OnGameTab() { ShowTab(ETab::Game); }

// ---------------------------------------------------------------- Changements

void USettingsMenuWidget::OnRowChanged(USettingsRowWidget* Row)
{
	const FName Id = Row->Id;
	const bool bOn = Row->Index == 1;
	auto Is = [&Id](const TCHAR* Name) { return Id == FName(Name); };

	if (Is(TEXT("MasterVolume"))) { Values.MasterVolume = Row->Value; }
	else if (Is(TEXT("MusicVolume"))) { Values.MusicVolume = Row->Value; }
	else if (Is(TEXT("EffectsVolume"))) { Values.EffectsVolume = Row->Value; }
	else if (Is(TEXT("Resolution")) && Resolutions.IsValidIndex(Row->Index)) { Values.Resolution = Resolutions[Row->Index]; }
	else if (Is(TEXT("WindowMode"))) { Values.WindowMode = Row->Index; }
	else if (Is(TEXT("VSync"))) { Values.bVSync = bOn; }
	else if (Is(TEXT("FrameLimit")) && Row->Index < UE_ARRAY_COUNT(FrameLimits)) { Values.FrameRateLimit = FrameLimits[Row->Index]; }
	else if (Is(TEXT("FieldOfView"))) { Values.FieldOfView = Row->Value; }
	else if (Is(TEXT("ResolutionScale"))) { Values.ResolutionScale = Row->Value; }
	else if (Is(TEXT("ViewDistance"))) { Values.ViewDistance = Row->Index; }
	else if (Is(TEXT("AntiAliasing"))) { Values.AntiAliasing = Row->Index; }
	else if (Is(TEXT("Shadows"))) { Values.Shadows = Row->Index; }
	else if (Is(TEXT("Textures"))) { Values.Textures = Row->Index; }
	else if (Is(TEXT("Effects"))) { Values.Effects = Row->Index; }
	else if (Is(TEXT("PostProcess"))) { Values.PostProcess = Row->Index; }
	else if (Is(TEXT("Foliage"))) { Values.Foliage = Row->Index; }
	else if (Is(TEXT("Sensitivity"))) { Values.CameraSensitivity = Row->Value; }
	else if (Is(TEXT("InvertY"))) { Values.bInvertCameraY = bOn; }
	else if (Is(TEXT("TextSpeed"))) { Values.TextSpeed = Row->Index; }
	else if (Is(TEXT("ShowHUD"))) { Values.bShowHUD = bOn; }
	else if (Is(TEXT("AutoSave"))) { Values.bAutoSave = bOn; }
	else if (Is(TEXT("Preset")))
	{
		// Qualité globale : toutes les qualités d'un coup
		Values.ViewDistance = Values.AntiAliasing = Values.Shadows = Values.Textures = Values.Effects = Values.PostProcess = Values.Foliage = Row->Index;
		for (USettingsRowWidget* Other : Rows)
		{
			if (Other != Row && Other->Kind == ESettingRowKind::Choice && Other->Choices.Num() == 5)
			{
				Other->Index = Row->Index;
				Other->RefreshValue();
			}
		}
	}
	SetStatus(LOCTEXT("Pending", "Modifié : Appliquer ou OK pour garder."));
}

void USettingsMenuWidget::OnRowAction(USettingsRowWidget* Row)
{
	APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	if (Row->Kind == ESettingRowKind::Key)
	{
		CapturingKey = Row->Id;
		Row->ButtonText = LOCTEXT("PressKey", "Appuie sur une touche...");
		Row->RefreshValue();
		SetStatus(LOCTEXT("CaptureHint", "Appuie sur la nouvelle touche (Échap : annuler)."));
		return;
	}
	if (Row->Id == FName(TEXT("ResetKeys")) && Controller)
	{
		Controller->ResetKeyBindings();
		ShowTab(ETab::Controls);
		SetStatus(LOCTEXT("KeysReset", "Touches d'origine remises."));
	}
	else if (Row->Id == FName(TEXT("DeleteSave")))
	{
		if (!bConfirmDelete)
		{
			bConfirmDelete = true;
			Row->ButtonText = LOCTEXT("ConfirmDelete", "Sûr ? Cliquer encore");
			Row->RefreshValue();
			return;
		}
		bConfirmDelete = false;
		const bool bDeleted = UPokemonSaveLibrary::DeleteSaveGame();
		Row->ButtonText = LOCTEXT("DeleteSave", "Effacer la sauvegarde");
		Row->RefreshValue();
		SetStatus(bDeleted ? LOCTEXT("Deleted", "Sauvegarde effacée : le prochain lancement repartira de zéro.")
			: LOCTEXT("NothingToDelete", "Il n'y a pas de sauvegarde."));
	}
	else if (Row->Id == FName(TEXT("Quit")))
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
	}
}

// ---------------------------------------------------------------- Touches et boutons du bas

FReply USettingsMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CapturingKey.IsNone())
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}
	const FKey Key = InKeyEvent.GetKey();
	const FName Id = CapturingKey;
	CapturingKey = NAME_None;
	if (Key == EKeys::Escape)
	{
		SetStatus(LOCTEXT("CaptureCancelled", "Touche inchangée."));
	}
	else if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		// Une touche de manette pour une commande de manette, une touche du clavier pour le clavier
		const bool bWantsPad = Id.ToString().EndsWith(TEXT("_Pad"));
		if (Key.IsGamepadKey() != bWantsPad || Key.IsMouseButton() || Key.IsAnalog())
		{
			SetStatus(bWantsPad ? LOCTEXT("NeedPad", "Il faut un bouton de la manette.") : LOCTEXT("NeedKey", "Il faut une touche du clavier."));
		}
		else
		{
			Controller->SetKeyBinding(Id, Key);
			SetStatus(FText::Format(LOCTEXT("KeySet", "Touche changée : {0}."), Key.GetDisplayName(false)));
		}
	}
	ShowTab(ETab::Controls);
	return FReply::Handled();
}

FReply USettingsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer());
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || (Controller && Controller->IsSettingsKey(Key)))
	{
		Close();
		return FReply::Handled();
	}
	// LB / RB : onglet précédent / suivant
	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
	{
		const int32 Count = 4;
		ShowTab(ETab((int32(Tab) + (Key == EKeys::Gamepad_RightShoulder ? 1 : Count - 1)) % Count));
		if (Rows.Num() > 0 && Rows[0]->GetFocusTarget())
		{
			Rows[0]->GetFocusTarget()->SetKeyboardFocus();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USettingsMenuWidget::OnOkClicked()
{
	ApplyValues();
	Close();
}

void USettingsMenuWidget::OnCancelClicked()
{
	LoadValues();
	ShowTab(Tab);
	SetStatus(LOCTEXT("Reverted", "Changements annulés."));
}

void USettingsMenuWidget::OnApplyClicked()
{
	ApplyValues();
	LoadValues();
	ShowTab(Tab);
	SetStatus(LOCTEXT("Applied", "Réglages appliqués."));
}

void USettingsMenuWidget::OnBackClicked()
{
	Close();
}

void USettingsMenuWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

void USettingsMenuWidget::Close()
{
	if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		Controller->CloseSettingsMenu();
	}
}

#undef LOCTEXT_NAMESPACE
