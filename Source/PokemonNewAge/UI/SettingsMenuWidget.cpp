#include "UI/SettingsMenuWidget.h"
#include "UI/GameFonts.h"
#include "UI/GlassPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/Spacer.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
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
	// Maquette Options_Menu de L'amie (1920 x 1080, CSS exporté de Figma, 2026-11-01).
	// Positions en pixels de la maquette = unités UMG ; tailles de police UMG = px Figma x 0,75.
	constexpr float FontScale = 0.75f;
	const FVector2D DesignSize(1920.f, 1080.f);
	const FVector2D PanelPos(437.f, 209.f);
	const FVector2D PanelSize(1210.f, 627.f);
	const FVector2D TitlePos(437.f, 112.f);
	constexpr float TabLeft = 130.f;
	constexpr float TabWidth = 259.f;
	const float TabTops[] = { 275.f, 408.f, 541.f, 674.f };
	constexpr float RowsLeft = 523.f;
	constexpr float FirstRowTop = 275.f;
	/** Haut de la liste qui défile (au-dessus de la 1re ligne, pour qu'elle ne soit pas coupée net) */
	constexpr float ListTop = 230.f;
	constexpr float RowHeight = 48.f;
	constexpr float RowPitch = 59.f;
	/** Espace en plus au-dessus d'un titre de section (« QUALITÉ » : 675 au lieu de 629) */
	constexpr float HeaderGap = 46.f;
	/** Zone des valeurs : de 1119 à 1482 (chevrons, curseurs), soit 596 px après le début des noms */
	constexpr float LabelWidth = 1119.f - RowsLeft;
	constexpr float ControlWidth = 363.f;
	constexpr float SliderBarHeight = 11.f;
	constexpr float KnobSize = 26.f;
	const FVector2D ActionButtonSize(272.f, 55.f);
	/** Bas : bords droits des textes Ok, Annuler, Appliquer, Retour */
	const float BottomRights[] = { 989.f, 1200.f, 1444.f, 1630.f };
	constexpr float BottomTop = 863.f;
	/** Ascenseur (Rectangle 6) : 11 px de large en x 1612, du haut de la liste jusqu'au bas du panneau */
	constexpr float ScrollX = 1612.f;
	constexpr float ScrollTop = 263.f;
	constexpr float ScrollBottom = 812.f;

	const FLinearColor TextColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor TabDimColor(1.f, 1.f, 1.f, 0.3f);
	const FLinearColor StatusColor(1.f, 1.f, 1.f, 0.6f);
	const FLinearColor DimBackground(0.f, 0.f, 0.f, 0.82f);

	/** Rectangle 5 : linear-gradient(87.81deg, rgba(217,217,217,0.3834) 0.4%, rgba(255,255,255,0.0923) 64.91%), coins 20 */
	PokemonGlass::FGlassLook PanelLook()
	{
		PokemonGlass::FGlassLook Look;
		Look.CornerRadius = 20.f;
		Look.GradientAngle = 87.81f;
		Look.GradientStart = 0.004f;
		Look.GradientEnd = 0.6491f;
		Look.LayerOpacity = 0.71f;
		Look.FillOpacityStart = 0.3834f / 0.71f;
		Look.FillOpacityEnd = 0.0923f / 0.71f;
		return Look;
	}

	/** Rectangles 6, 7, 8 : linear-gradient(92.05deg, rgba(217,217,217,0.5751) 1.99%, rgba(255,255,255,0.142) 64.43%), arrondis */
	PokemonGlass::FGlassLook BarLook(bool bVertical)
	{
		PokemonGlass::FGlassLook Look;
		Look.CornerRadius = 20.f;
		// L'ascenseur est la même barre tournée de 90 degrés
		Look.GradientAngle = bVertical ? 182.05f : 92.05f;
		Look.GradientStart = 0.0199f;
		Look.GradientEnd = 0.6443f;
		Look.FillOpacityStart = 0.5751f;
		Look.FillOpacityEnd = 0.142f;
		Look.LayerOpacity = 1.f;
		Look.StrokeOpacityStart = 0.f;
		Look.StrokeOpacityEnd = 0.f;
		Look.LightIntensity = 0.f;
		return Look;
	}

	enum class EButtonLook : uint8
	{
		/** Texte seul (onglets, chevrons, Ok...) : léger fond blanc au survol */
		Text,
		/** Bouton gris en verre (touche d'une commande, « Maj gauche ») */
		Glass,
		/** Bouton rouge (« Effacer la partie ») */
		Danger
	};

	/** Limites d'images par seconde proposées (0 = sans limite) */
	const float FrameLimits[] = { 0.f, 30.f, 60.f, 120.f, 144.f, 240.f };

	/** Texte des maquettes : Futura MdCn BT, taille en px Figma, blanc avec ombre */
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Value, float FigmaSize, const FLinearColor& Color = TextColor)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Value);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetFont(PokemonFonts::Get(FigmaSize * FontScale));
		PokemonGlass::AddTextShadow(Label);
		return Label;
	}

	UButton* MakeButton(UWidgetTree* Tree, UWidget* Content, EButtonLook Look)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		const float Radius = Look == EButtonLook::Text ? 8.f : 6.f;
		FLinearColor Fill(1.f, 1.f, 1.f, 0.f);
		FLinearColor Outline(1.f, 1.f, 1.f, 0.f);
		if (Look == EButtonLook::Glass)
		{
			Fill = FLinearColor(1.f, 1.f, 1.f, 0.2f);
			Outline = FLinearColor(1.f, 1.f, 1.f, 0.45f);
		}
		else if (Look == EButtonLook::Danger)
		{
			Fill = FLinearColor(0.45f, 0.04f, 0.04f, 0.85f);
			Outline = FLinearColor(1.f, 0.6f, 0.6f, 0.45f);
		}
		const float OutlineWidth = Look == EButtonLook::Text ? 0.f : 1.f;
		auto Brighter = [](FLinearColor Color, float Add) { Color.A = FMath::Min(1.f, Color.A + Add); return Color; };
		if (Look == EButtonLook::Text)
		{
			// Pas de bloc (retour de L'amie) : au survol, le texte grossit et passe en blanc (AnimateTextButton)
			Style.SetNormal(FSlateNoResource());
			Style.SetHovered(FSlateNoResource());
			Style.SetPressed(FSlateNoResource());
		}
		else
		{
			Style.SetNormal(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
			Style.SetHovered(FSlateRoundedBoxBrush(Brighter(Fill, 0.12f), Radius, Outline, OutlineWidth));
			Style.SetPressed(FSlateRoundedBoxBrush(Brighter(Fill, 0.25f), Radius, Outline, OutlineWidth));
		}
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetContent(Content);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot))
		{
			ContentSlot->SetPadding(FMargin(0.f));
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
		}
		return Button;
	}

	/** Survol d'un bouton texte : le texte grossit de 12 % (en douceur) et passe en blanc */
	constexpr float HoverScale = 1.12f;

	void AnimateTextButton(UButton* Button, UTextBlock* Label, const FLinearColor& RestColor, float DeltaTime)
	{
		if (!Button || !Label)
		{
			return;
		}
		const bool bHover = Button->IsHovered() || Button->HasKeyboardFocus();
		const float Current = Label->GetRenderTransform().Scale.X;
		const float Scale = FMath::FInterpTo(Current, bHover ? HoverScale : 1.f, DeltaTime, 18.f);
		if (!FMath::IsNearlyEqual(Scale, Current, 0.0005f))
		{
			Label->SetRenderScale(FVector2D(Scale, Scale));
		}
		Label->SetColorAndOpacity(FSlateColor(bHover ? TextColor : RestColor));
	}

	USizeBox* MakeBox(UWidgetTree* Tree, const FVector2D& Size, UWidget* Content)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (Size.X > 0.f)
		{
			Box->SetWidthOverride(Size.X);
		}
		if (Size.Y > 0.f)
		{
			Box->SetHeightOverride(Size.Y);
		}
		Box->SetContent(Content);
		return Box;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		return CanvasSlot;
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

	/** Nom d'une touche sans « Manette : » devant (on est déjà dans la partie Manette) */
	FText KeyLabel(const FKey& Key)
	{
		FString Name = Key.GetDisplayName(false).ToString();
		int32 Colon = INDEX_NONE;
		if (Key.IsGamepadKey() && Name.FindChar(TEXT(':'), Colon))
		{
			Name = Name.RightChop(Colon + 1).TrimStartAndEnd();
		}
		return FText::FromString(Name);
	}
}

using namespace SettingsMenu;

// ================================================================ Ligne

TSharedRef<SWidget> USettingsRowWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Ligne de 48 px de haut tous les 59 px (Résolution 275, Mode d'affichage 334...) ; les titres de section
		// ont 46 px de plus au-dessus, les boutons (55 px) un peu plus de place
		const bool bButton = Kind == ESettingRowKind::Key || Kind == ESettingRowKind::Action;
		const float Height = bButton ? ActionButtonSize.Y : RowHeight;
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Line"));
		UVerticalBoxSlot* LineSlot = Root->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(0.f, Height), Line));
		LineSlot->SetPadding(FMargin(0.f, Kind == ESettingRowKind::Header ? HeaderGap : 0.f, 0.f,
			bButton ? 15.f : RowPitch - RowHeight));

		// Nom du réglage (les titres de section en majuscules, même taille)
		UTextBlock* Name = MakeText(WidgetTree, Kind == ESettingRowKind::Header ? Label.ToUpper() : Label, 40.f);
		UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(LabelWidth, 0.f), Name));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		if (Kind == ESettingRowKind::Header)
		{
			return Super::RebuildWidget();
		}

		UOverlay* Control = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Control"));
		UHorizontalBoxSlot* ControlSlot = Line->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(ControlWidth, Height), Control));
		ControlSlot->SetVerticalAlignment(VAlign_Center);

		if (bButton)
		{
			// Bouton de 272 x 55 (« Effacer la partie » rouge, touches en verre gris)
			ButtonLabel = MakeText(WidgetTree, ButtonText, 40.f);
			ButtonLabel->SetJustification(ETextJustify::Center);
			// Les noms longs (« Touche spéciale gauche ») rétrécissent pour rester dans le bouton
			UScaleBox* LabelFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ButtonLabelFit"));
			LabelFit->SetStretch(EStretch::ScaleToFit);
			LabelFit->SetStretchDirection(EStretchDirection::DownOnly);
			LabelFit->AddChild(ButtonLabel);
			USizeBox* ButtonBox = MakeBox(WidgetTree, ActionButtonSize, LabelFit);
			if (USizeBoxSlot* TextSlot = Cast<USizeBoxSlot>(LabelFit->Slot))
			{
				TextSlot->SetPadding(FMargin(12.f, 0.f));
				TextSlot->SetHorizontalAlignment(HAlign_Fill);
				TextSlot->SetVerticalAlignment(VAlign_Center);
			}
			const bool bDanger = Kind == ESettingRowKind::Action && Id == FName(TEXT("DeleteSave"));
			MainButton = MakeButton(WidgetTree, ButtonBox, bDanger ? EButtonLook::Danger : EButtonLook::Glass);
			MainButton->OnClicked.AddDynamic(this, &USettingsRowWidget::HandleButton);
			UOverlaySlot* ButtonSlot = Control->AddChildToOverlay(MainButton);
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
			return Super::RebuildWidget();
		}

		if (Kind == ESettingRowKind::Choice)
		{
			// <  valeur  > : chevrons aux bords (x 1119 et 1482), valeur centrée entre les deux
			UHorizontalBox* Choice = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UOverlaySlot* ChoiceSlot = Control->AddChildToOverlay(Choice);
			ChoiceSlot->SetHorizontalAlignment(HAlign_Fill);
			ChoiceSlot->SetVerticalAlignment(VAlign_Fill);

			PreviousLabel = MakeText(WidgetTree, FText::FromString(TEXT("<")), 40.f);
			PreviousLabel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			UButton* Previous = MakeButton(WidgetTree, PreviousLabel, EButtonLook::Text);
			Previous->OnClicked.AddDynamic(this, &USettingsRowWidget::HandlePrevious);
			Choice->AddChildToHorizontalBox(Previous)->SetVerticalAlignment(VAlign_Center);

			ValueText = MakeText(WidgetTree, FText::GetEmpty(), 40.f);
			ValueText->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* ValueSlot = Choice->AddChildToHorizontalBox(ValueText);
			ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ValueSlot->SetVerticalAlignment(VAlign_Center);

			NextLabel = MakeText(WidgetTree, FText::FromString(TEXT(">")), 40.f);
			NextLabel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			UButton* Next = MakeButton(WidgetTree, NextLabel, EButtonLook::Text);
			PreviousButton = Previous;
			NextButton = Next;
			Next->OnClicked.AddDynamic(this, &USettingsRowWidget::HandleNext);
			Choice->AddChildToHorizontalBox(Next)->SetVerticalAlignment(VAlign_Center);
			MainButton = Previous;
		}
		else
		{
			// Barre en dégradé de 363 x 11 et rond blanc de 26 px (pas de chiffre, comme la maquette)
			UImage* Bar = PokemonGlass::MakeImage(WidgetTree, BarLook(false));
			UOverlaySlot* BarSlot = Control->AddChildToOverlay(MakeBox(WidgetTree, FVector2D(0.f, SliderBarHeight), Bar));
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Center);

			ValueSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
			ValueSlider->SetMinValue(Min);
			ValueSlider->SetMaxValue(Max);
			ValueSlider->SetStepSize(Step);
			ValueSlider->SetIndentHandle(false);
			FSliderStyle Style = ValueSlider->GetWidgetStyle();
			FSlateRoundedBoxBrush Knob(FLinearColor::White, KnobSize * 0.5f);
			Knob.ImageSize = FVector2D(KnobSize, KnobSize);
			FSlateRoundedBoxBrush KnobHovered(FLinearColor(0.88f, 0.88f, 0.9f, 1.f), KnobSize * 0.5f);
			KnobHovered.ImageSize = Knob.ImageSize;
			Style.SetNormalBarImage(FSlateNoResource());
			Style.SetHoveredBarImage(FSlateNoResource());
			Style.SetDisabledBarImage(FSlateNoResource());
			Style.SetNormalThumbImage(Knob);
			Style.SetHoveredThumbImage(KnobHovered);
			Style.SetDisabledThumbImage(Knob);
			Style.SetBarThickness(0.f);
			ValueSlider->SetWidgetStyle(Style);
			ValueSlider->OnValueChanged.AddDynamic(this, &USettingsRowWidget::HandleSlider);
			UOverlaySlot* SliderSlot = Control->AddChildToOverlay(ValueSlider);
			SliderSlot->SetHorizontalAlignment(HAlign_Fill);
			SliderSlot->SetVerticalAlignment(VAlign_Fill);
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

void USettingsRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AnimateTextButton(PreviousButton, PreviousLabel, TextColor, InDeltaTime);
	AnimateTextButton(NextButton, NextLabel, TextColor, InDeltaTime);
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

		// Jeu flouté puis assombri (Rectangle 1 : noir à 82 %)
		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur"));
		Blur->SetBlurStrength(6.f);
		UOverlaySlot* BlurSlot = Root->AddChildToOverlay(Blur);
		BlurSlot->SetHorizontalAlignment(HAlign_Fill);
		BlurSlot->SetVerticalAlignment(VAlign_Fill);

		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(DimBackground);
		UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);

		// Toute la maquette en 1920 x 1080, mise à l'échelle de la fenêtre (centrée si l'écran n'est pas en 16:9)
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("Scale"));
		Scale->SetStretch(EStretch::ScaleToFit);
		UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(Scale);
		ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
		ScaleSlot->SetVerticalAlignment(VAlign_Fill);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		Scale->SetContent(MakeBox(WidgetTree, DesignSize, Canvas));

		// Panneau en verre (Rectangle 5)
		Place(Canvas, PokemonGlass::Make(WidgetTree, nullptr, FMargin(0.f), PanelLook(), TEXT("Panel")), PanelPos, PanelSize);

		// Titre
		UTextBlock* Title = MakeText(WidgetTree, LOCTEXT("Title", "Paramètres"), 64.f);
		Place(Canvas, Title, TitlePos, FVector2D(1355.f, 97.f));

		// Onglets à gauche, alignés à droite sur x 389 ; celui ouvert en blanc, les autres à 30 %
		const FText TabNames[] = { LOCTEXT("TabAudio", "Audio"), LOCTEXT("TabVideo", "Vidéo"), LOCTEXT("TabControls", "Commandes"), LOCTEXT("TabGame", "Jeu") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TabNames); ++Index)
		{
			UTextBlock* TabLabel = MakeText(WidgetTree, TabNames[Index], 64.f, TabDimColor);
			TabLabel->SetJustification(ETextJustify::Right);
			// Grossit vers la gauche : le bord droit reste aligné sur x 389
			TabLabel->SetRenderTransformPivot(FVector2D(1.f, 0.5f));
			UButton* TabButton = MakeButton(WidgetTree, TabLabel, EButtonLook::Text);
			switch (Index)
			{
			case 0: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnAudioTab); break;
			case 1: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnVideoTab); break;
			case 2: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnControlsTab); break;
			default: TabButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnGameTab); break;
			}
			Place(Canvas, TabButton, FVector2D(TabLeft, TabTops[Index]), FVector2D(TabWidth, 77.f));
			TabButtons.Add(TabButton);
			TabLabels.Add(TabLabel);
		}

		// Réglages : liste qui défile, coupée au bas du panneau ; ascenseur maison à droite
		RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Rows"));
		RowList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		RowList->SetAnimateWheelScrolling(true);
		Place(Canvas, RowList, FVector2D(RowsLeft, ListTop), FVector2D(ScrollX - 20.f - RowsLeft, PanelPos.Y + PanelSize.Y - ListTop));

		ScrollThumb = PokemonGlass::MakeImage(WidgetTree, BarLook(true), TEXT("ScrollThumb"));
		ScrollThumbSlot = Place(Canvas, ScrollThumb, FVector2D(ScrollX, ScrollTop), FVector2D(SliderBarHeight, ScrollBottom - ScrollTop));

		// Message (pas dans la maquette : discret, en bas à gauche du panneau)
		StatusText = MakeText(WidgetTree, FText::GetEmpty(), 30.f, StatusColor);
		StatusText->SetAutoWrapText(true);
		Place(Canvas, StatusText, FVector2D(PanelPos.X, 880.f), FVector2D(470.f, 80.f));

		// Ok, Annuler, Appliquer, Retour : texte seul, 48 px, alignés à droite
		const FText Buttons[] = { LOCTEXT("Ok", "Ok"), LOCTEXT("Cancel", "Annuler"), LOCTEXT("Apply", "Appliquer"), LOCTEXT("Back", "Retour") };
		const float Widths[] = { 58.f, 163.f, 196.f, 138.f };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
		{
			UTextBlock* Label = MakeText(WidgetTree, Buttons[Index], 48.f);
			Label->SetJustification(ETextJustify::Right);
			Label->SetRenderTransformPivot(FVector2D(1.f, 0.5f));
			UButton* Button = MakeButton(WidgetTree, Label, EButtonLook::Text);
			BottomButtons.Add(Button);
			BottomLabels.Add(Label);
			switch (Index)
			{
			case 0: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnOkClicked); break;
			case 1: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCancelClicked); break;
			case 2: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnApplyClicked); break;
			default: Button->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnBackClicked); break;
			}
			// Un peu de marge autour du mot pour le survol, le texte reste à sa place
			if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Label->Slot))
			{
				LabelSlot->SetPadding(FMargin(10.f, 0.f));
			}
			Place(Canvas, Button, FVector2D(BottomRights[Index] - Widths[Index] - 10.f, BottomTop), FVector2D(Widths[Index] + 20.f, 57.f));
		}
	}
	return Super::RebuildWidget();
}

void USettingsMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateScrollThumb();
	for (int32 Index = 0; Index < TabButtons.Num() && Index < TabLabels.Num(); ++Index)
	{
		AnimateTextButton(TabButtons[Index], TabLabels[Index], Index == int32(Tab) ? TextColor : TabDimColor, InDeltaTime);
	}
	for (int32 Index = 0; Index < BottomButtons.Num() && Index < BottomLabels.Num(); ++Index)
	{
		AnimateTextButton(BottomButtons[Index], BottomLabels[Index], TextColor, InDeltaTime);
	}
}

void USettingsMenuWidget::UpdateScrollThumb()
{
	if (!RowList || !ScrollThumbSlot)
	{
		return;
	}
	// Taille de la barre = part visible de la liste ; cachée quand tout tient
	const float Visible = RowList->GetCachedGeometry().GetLocalSize().Y;
	const float Hidden = RowList->GetScrollOffsetOfEnd();
	if (Visible <= 1.f || Hidden <= 1.f)
	{
		ScrollThumb->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	ScrollThumb->SetVisibility(ESlateVisibility::HitTestInvisible);
	const float Track = ScrollBottom - ScrollTop;
	const float Thumb = FMath::Max(40.f, Track * Visible / (Visible + Hidden));
	const float Progress = FMath::Clamp(RowList->GetScrollOffset() / Hidden, 0.f, 1.f);
	ScrollThumbSlot->SetPosition(FVector2D(ScrollX, ScrollTop + (Track - Thumb) * Progress));
	ScrollThumbSlot->SetSize(FVector2D(SliderBarHeight, Thumb));
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
		Values.FootstepsVolume = Options->FootstepsVolume;
		Values.WeatherVolume = Options->WeatherVolume;
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
		Options->FootstepsVolume = Values.FootstepsVolume;
		Options->WeatherVolume = Values.WeatherVolume;
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
	for (int32 Index = 0; Index < TabLabels.Num(); ++Index)
	{
		TabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == int32(Tab) ? TextColor : TabDimColor));
	}
	if (!RowList)
	{
		return;
	}
	RowList->ClearChildren();
	Rows.Reset();
	// La 1re ligne en y 275 (la liste commence un peu plus haut)
	USpacer* Top = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Top->SetSize(FVector2D(1.f, FirstRowTop - ListTop));
	RowList->AddChild(Top);

	auto PercentFormat = [](float Value) { return Percent(Value); };
	auto Quality = [this](FName Id, const FText& Label, int32 Value) { AddChoice(Id, Label, QualityNames(), Value); };

	switch (Tab)
	{
	case ETab::Audio:
		AddSlider(TEXT("MasterVolume"), LOCTEXT("MasterVolume", "Volume général"), 0.f, 1.f, 0.05f, Values.MasterVolume, PercentFormat);
		AddSlider(TEXT("MusicVolume"), LOCTEXT("MusicVolume", "Musique"), 0.f, 1.f, 0.05f, Values.MusicVolume, PercentFormat);
		// Comme la maquette Options_Menu : Bruits de pas et Météo à part, les autres bruitages dans Vfx
		AddSlider(TEXT("FootstepsVolume"), LOCTEXT("FootstepsVolume", "Bruits de pas"), 0.f, 1.f, 0.05f, Values.FootstepsVolume, PercentFormat);
		AddSlider(TEXT("WeatherVolume"), LOCTEXT("WeatherVolume", "Météo"), 0.f, 1.f, 0.05f, Values.WeatherVolume, PercentFormat);
		AddSlider(TEXT("EffectsVolume"), LOCTEXT("EffectsVolume", "Vfx"), 0.f, 1.f, 0.05f, Values.EffectsVolume, PercentFormat);
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
			{ LOCTEXT("Fullscreen", "Plein écran non fenêtré"), LOCTEXT("Borderless", "Plein écran fenêtré"), LOCTEXT("Windowed", "Fenêtré") }, Values.WindowMode);
		AddToggle(TEXT("VSync"), LOCTEXT("VSync", "Synchro verticale"), Values.bVSync);
		TArray<FText> LimitNames;
		int32 LimitIndex = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameLimits); ++Index)
		{
			LimitNames.Add(FrameLimits[Index] <= 0.f ? LOCTEXT("NoLimit", "Sans limites") : FText::AsNumber(FMath::RoundToInt(FrameLimits[Index])));
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
			Row->ButtonText = SettingsMenu::KeyLabel(Entry.Key);
			RowList->AddChild(Row);
			Rows.Add(Row);
		}
		USettingsRowWidget* Reset = AddRow(TEXT("ResetKeys"), ESettingRowKind::Action, FText::GetEmpty());
		Reset->ButtonText = LOCTEXT("ResetKeys", "Touches d'origine");
		RowList->AddChild(Reset);
		Rows.Add(Reset);
		break;
	}

	case ETab::Game:
	{
		AddChoice(TEXT("TextSpeed"), LOCTEXT("TextSpeed", "Vitesse du texte"),
			{ LOCTEXT("Slow", "Lente"), LOCTEXT("Normal", "Normale"), LOCTEXT("Fast", "Rapide") }, Values.TextSpeed);
		AddToggle(TEXT("ShowHUD"), LOCTEXT("ShowHUD", "Afficher le HUD"), Values.bShowHUD);
		AddToggle(TEXT("AutoSave"), LOCTEXT("AutoSave", "Sauvegarde automatique"), Values.bAutoSave);
		AddHeader(LOCTEXT("SaveHeader", "PARTIE"));
		USettingsRowWidget* Delete = AddRow(TEXT("DeleteSave"), ESettingRowKind::Action, LOCTEXT("DeleteLabel", "Recommencer de zéro"));
		Delete->ButtonText = LOCTEXT("DeleteSave", "Effacer la partie");
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
	else if (Is(TEXT("FootstepsVolume"))) { Values.FootstepsVolume = Row->Value; }
	else if (Is(TEXT("WeatherVolume"))) { Values.WeatherVolume = Row->Value; }
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
		Row->ButtonText = LOCTEXT("PressKey", "Appuie...");
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
			Row->ButtonText = LOCTEXT("ConfirmDelete", "Sûr ? Encore");
			Row->RefreshValue();
			return;
		}
		bConfirmDelete = false;
		const bool bDeleted = UPokemonSaveLibrary::DeleteSaveGame();
		Row->ButtonText = LOCTEXT("DeleteSave", "Effacer la partie");
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
			SetStatus(FText::Format(LOCTEXT("KeySet", "Touche changée : {0}."), SettingsMenu::KeyLabel(Key)));
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
