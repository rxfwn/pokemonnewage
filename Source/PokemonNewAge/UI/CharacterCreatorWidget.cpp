#include "UI/CharacterCreatorWidget.h"
#include "Character/CharacterStudio.h"
#include "Character/TrainerAppearanceRender.h"
#include "UI/GameFonts.h"
#include "UI/GlassPanel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

#define LOCTEXT_NAMESPACE "CharacterCreator"

// ================================================================ Petits widgets

void UCreatorButton::Setup(TFunction<void()> InOnClick)
{
	OnClick = MoveTemp(InOnClick);
	OnClicked.AddUniqueDynamic(this, &UCreatorButton::HandleClicked);
}

void UCreatorButton::HandleClicked()
{
	if (OnClick)
	{
		OnClick();
	}
}

void UCreatorSlider::Setup(TFunction<void(float)> InOnChange)
{
	OnChange = MoveTemp(InOnChange);
	OnValueChanged.AddUniqueDynamic(this, &UCreatorSlider::HandleValue);
}

void UCreatorSlider::HandleValue(float Value)
{
	if (OnChange)
	{
		OnChange(Value);
	}
}

// ================================================================ Mise en page

namespace CharacterCreator
{
	// Captures d'inZOI ramenées en 1920 x 1080 ; tailles de police = px Figma x 0,75 comme le reste du jeu
	constexpr float FontScale = 0.75f;
	const FVector2D DesignSize(1920.f, 1080.f);
	constexpr float LeftX = 70.f;
	constexpr float LeftWidth = 390.f;
	const FVector2D PanelPos(1410.f, 100.f);
	const FVector2D PanelSize(460.f, 820.f);
	constexpr float HotspotX = 1226.f;
	const float HotspotTops[] = { 342.f, 447.f, 532.f, 622.f, 712.f };
	constexpr float BottomY = 994.f;
	constexpr float PillHeight = 52.f;
	constexpr int32 GridColumns = 3;
	constexpr int32 SwatchColumns = 6;
	constexpr float HandleSize = 22.f;
	/** Rayon (unités de la maquette) dans lequel un clic attrape une poignée */
	constexpr float HandleGrabRadius = 20.f;

	const FLinearColor TextColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor DimColor(1.f, 1.f, 1.f, 0.65f);
	const FLinearColor DarkText(0.10f, 0.11f, 0.13f, 1.f);
	const FLinearColor LineColor(1.f, 1.f, 1.f, 0.45f);
	const FLinearColor FieldColor(0.f, 0.f, 0.f, 0.22f);
	const FLinearColor SelectedFill(1.f, 1.f, 1.f, 0.92f);
	const FLinearColor ButtonFill(1.f, 1.f, 1.f, 0.16f);
	const FLinearColor ButtonOutline(1.f, 1.f, 1.f, 0.55f);
	/** Bleu clair du bouton Complete d'inZOI */
	const FLinearColor FinishFill(0.55f, 0.76f, 0.96f, 0.95f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Value, float FigmaSize, PokemonFonts::EStyle Style = PokemonFonts::EStyle::Regular, const FLinearColor& Color = TextColor)
	{
		UTextBlock* Label = PokemonGlass::MakeShadowedText(Tree);
		Label->SetText(Value);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetFont(PokemonFonts::Get(FigmaSize * FontScale, Style));
		return Label;
	}

	FButtonStyle MakeStyle(const FButtonStyle& Base, const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FButtonStyle Style = Base;
		auto Brighter = [](FLinearColor Color, float Add) { Color.A = FMath::Min(1.f, Color.A + Add); return Color; };
		Style.SetNormal(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
		Style.SetHovered(FSlateRoundedBoxBrush(Brighter(Fill, 0.14f), Radius, Outline, OutlineWidth));
		Style.SetPressed(FSlateRoundedBoxBrush(Brighter(Fill, 0.28f), Radius, Outline, OutlineWidth));
		Style.SetDisabled(FSlateRoundedBoxBrush(Fill * FLinearColor(1.f, 1.f, 1.f, 0.4f), Radius));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	UCreatorButton* MakeButton(UWidgetTree* Tree, UWidget* Content, const FLinearColor& Fill, float Radius, TFunction<void()> OnClick,
		const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		UCreatorButton* Button = Tree->ConstructWidget<UCreatorButton>(UCreatorButton::StaticClass());
		Button->SetStyle(MakeStyle(Button->GetStyle(), Fill, Radius, Outline, OutlineWidth));
		Button->SetBackgroundColor(FLinearColor::White);
		if (Content)
		{
			Button->SetContent(Content);
			if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot))
			{
				ContentSlot->SetPadding(FMargin(0.f));
				ContentSlot->SetHorizontalAlignment(HAlign_Center);
				ContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		Button->Setup(MoveTemp(OnClick));
		return Button;
	}

	/** Bouton texte (barre d'outils, onglets) */
	UCreatorButton* MakeTextButton(UWidgetTree* Tree, const FText& Label, float FigmaSize, TFunction<void()> OnClick, UTextBlock** OutLabel = nullptr,
		const FLinearColor& Fill = FLinearColor(1.f, 1.f, 1.f, 0.f), float Radius = 22.f)
	{
		UTextBlock* Text = MakeText(Tree, Label, FigmaSize);
		Text->SetJustification(ETextJustify::Center);
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return MakeButton(Tree, Text, Fill, Radius, MoveTemp(OnClick));
	}

	UBorder* MakeRounded(UWidgetTree* Tree, const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
		Border->SetPadding(FMargin(0.f));
		return Border;
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

	/** Trait fin sous un titre de section (comme inZOI) */
	UBorder* MakeLine(UWidgetTree* Tree)
	{
		return MakeRounded(Tree, LineColor, 0.f);
	}

	/** Champ de texte : fond sombre arrondi, texte blanc */
	UEditableText* MakeField(UWidgetTree* Tree, UCanvasPanel* Canvas, const FVector2D& Position, const FVector2D& Size, const FText& Hint, float FigmaSize)
	{
		UBorder* Field = MakeRounded(Tree, FieldColor, Size.Y * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f);
		Field->SetPadding(FMargin(16.f, 4.f));
		Field->SetVerticalAlignment(VAlign_Center);
		UEditableText* Text = Tree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
		Text->SetFont(PokemonFonts::Get(FigmaSize * FontScale));
		Text->SetHintText(Hint);
		Field->SetContent(Text);
		Place(Canvas, Field, Position, Size);
		return Text;
	}

	FText SignedValue(float Value)
	{
		return FText::FromString(FString::Printf(TEXT("%+.2f"), Value));
	}

	bool SameColor(const FLinearColor& A, const FLinearColor& B)
	{
		return FMath::IsNearlyEqual(A.R, B.R, 0.01f) && FMath::IsNearlyEqual(A.G, B.G, 0.01f) && FMath::IsNearlyEqual(A.B, B.B, 0.01f);
	}
}

using namespace CharacterCreator;

// ================================================================ Ouverture

void UCharacterCreatorWidget::Init(UTrainerAppearanceComponent* InOwner, ACharacterStudio* InStudio, UTrainerAppearanceData* InData,
	ETrainerEditorMode InMode, const FTrainerAppearance& Appearance, const FString& TrainerName, const FString& Catchphrase)
{
	Owner = InOwner;
	Studio = InStudio;
	Data = InData;
	Mode = InMode;
	Current = TrainerAppearanceRender::Sanitize(Data, Appearance);
	Original = Current;
	Name = TrainerName;
	Catch = Catchphrase;
	// Création : on commence par les modèles complets ; armoire : directement les tenues ; Apparence : le visage
	Tab = Mode == ETrainerEditorMode::Creation ? ETab::Presets : (Mode == ETrainerEditorMode::Wardrobe ? ETab::Outfit : ETab::Face);
}

TSharedRef<SWidget> UCharacterCreatorWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Fond transparent qui reçoit la souris là où il n'y a pas de bouton : tourner le personnage, poignées
		UBorder* Catcher = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MouseCatcher"));
		Catcher->SetBrushColor(FLinearColor::Transparent);
		Catcher->SetVisibility(ESlateVisibility::Visible);
		UOverlaySlot* CatcherSlot = Root->AddChildToOverlay(Catcher);
		CatcherSlot->SetHorizontalAlignment(HAlign_Fill);
		CatcherSlot->SetVerticalAlignment(VAlign_Fill);

		// Toute la maquette en 1920 x 1080, à l'échelle de la fenêtre
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("Scale"));
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(Scale);
		ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
		ScaleSlot->SetVerticalAlignment(VAlign_Fill);
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		USizeBox* CanvasBox = MakeBox(WidgetTree, DesignSize, Canvas);
		CanvasBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Scale->SetContent(CanvasBox);

		BuildLeftColumn();
		BuildToolbar();
		BuildFaceHotspots();
		BuildRightPanel();
		BuildTabs();
		BuildBottomLeft();

		// Poignées du mode Détaillé (placées à chaque image sur les os du personnage)
		if (Data)
		{
			for (int32 Index = 0; Index < Data->DragHandles.Num(); ++Index)
			{
				UBorder* Dot = MakeRounded(WidgetTree, FLinearColor(1.f, 1.f, 1.f, 0.85f), HandleSize * 0.5f, FLinearColor(0.2f, 0.45f, 0.75f, 0.9f), 2.f);
				Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
				Dot->SetToolTipText(Data->DragHandles[Index].Label);
				Place(Canvas, Dot, FVector2D::ZeroVector, FVector2D(HandleSize, HandleSize));
				HandleDots.Add(Dot);
			}
		}
		HandlePositions.SetNum(HandleDots.Num());
	}
	return Super::RebuildWidget();
}

void UCharacterCreatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (Studio.IsValid())
	{
		StudioText->SetText(FText::Format(LOCTEXT("StudioLabel", "Studio : {0}"), Studio->GetLightingName()));
	}
	RefreshAll();
	if (Mode == ETrainerEditorMode::Creation)
	{
		SetStatus(LOCTEXT("Welcome", "Choisis un modèle de départ, puis affine chaque détail."));
	}
}

// ================================================================ Construction

void UCharacterCreatorWidget::BuildLeftColumn()
{
	// Haut gauche : ‹ Retour | titre (pas de retour à la création : on doit finir son personnage)
	float TitleX = 30.f;
	if (Mode != ETrainerEditorMode::Creation)
	{
		UCreatorButton* Back = MakeTextButton(WidgetTree, LOCTEXT("Back", "‹  Retour"), 28.f, [this]() { Cancel(); });
		Place(Canvas, Back, FVector2D(18.f, 16.f), FVector2D(150.f, 46.f));
		Place(Canvas, MakeText(WidgetTree, FText::FromString(TEXT("|")), 28.f, PokemonFonts::EStyle::Regular, DimColor), FVector2D(178.f, 20.f), FVector2D(20.f, 40.f));
		TitleX = 208.f;
	}
	const FText Title = Mode == ETrainerEditorMode::Creation ? LOCTEXT("TitleCreate", "Créer ton dresseur")
		: (Mode == ETrainerEditorMode::Wardrobe ? LOCTEXT("TitleWardrobe", "Armoire") : LOCTEXT("TitleAppearance", "Apparence"));
	Place(Canvas, MakeText(WidgetTree, Title, 28.f, PokemonFonts::EStyle::Regular, FLinearColor(0.75f, 0.88f, 1.f, 1.f)), FVector2D(TitleX, 20.f), FVector2D(500.f, 40.f));

	Place(Canvas, MakeText(WidgetTree, LOCTEXT("Question", "À quoi ressemble ton dresseur ?"), 34.f, PokemonFonts::EStyle::Bold), FVector2D(66.f, 86.f), FVector2D(700.f, 46.f));
	Place(Canvas, MakeText(WidgetTree, LOCTEXT("Subtitle", "Donne vie au dresseur que tu imagines."), 22.f, PokemonFonts::EStyle::Regular, DimColor), FVector2D(LeftX, 130.f), FVector2D(700.f, 30.f));

	// Profil : prénom et base
	Place(Canvas, MakeText(WidgetTree, LOCTEXT("Profile", "Profil"), 28.f, PokemonFonts::EStyle::Bold), FVector2D(LeftX, 200.f), FVector2D(LeftWidth, 40.f));
	Place(Canvas, MakeLine(WidgetTree), FVector2D(LeftX, 244.f), FVector2D(LeftWidth, 1.f));

	Place(Canvas, MakeText(WidgetTree, LOCTEXT("Name", "Prénom"), 24.f), FVector2D(LeftX, 268.f), FVector2D(150.f, 34.f));
	NameBox = MakeField(WidgetTree, Canvas, FVector2D(230.f, 262.f), FVector2D(230.f, 44.f), LOCTEXT("NameHint", "Ton prénom"), 24.f);
	NameBox->SetText(FText::FromString(Name));
	NameBox->OnTextChanged.AddUniqueDynamic(this, &UCharacterCreatorWidget::HandleNameChanged);

	Place(Canvas, MakeText(WidgetTree, LOCTEXT("Base", "Base"), 24.f), FVector2D(LeftX, 326.f), FVector2D(150.f, 34.f));
	Place(Canvas, MakeTextButton(WidgetTree, FText::FromString(TEXT("‹")), 34.f, [this]() { SelectBase(-1); }), FVector2D(230.f, 318.f), FVector2D(44.f, 44.f));
	BaseText = MakeText(WidgetTree, FText::GetEmpty(), 24.f);
	BaseText->SetJustification(ETextJustify::Center);
	Place(Canvas, BaseText, FVector2D(274.f, 324.f), FVector2D(142.f, 34.f));
	Place(Canvas, MakeTextButton(WidgetTree, FText::FromString(TEXT("›")), 34.f, [this]() { SelectBase(1); }), FVector2D(416.f, 318.f), FVector2D(44.f, 44.f));

	// Phrase d'accroche (à la place des Traits d'inZOI) : affichée sur la carte dresseur
	Place(Canvas, MakeText(WidgetTree, LOCTEXT("Catchphrase", "Phrase d'accroche"), 28.f, PokemonFonts::EStyle::Bold), FVector2D(LeftX, 418.f), FVector2D(LeftWidth, 40.f));
	Place(Canvas, MakeLine(WidgetTree), FVector2D(LeftX, 462.f), FVector2D(LeftWidth, 1.f));
	CatchBox = MakeField(WidgetTree, Canvas, FVector2D(LeftX, 482.f), FVector2D(LeftWidth, 48.f), LOCTEXT("CatchHint", "« Je serai le meilleur dresseur ! »"), 22.f);
	CatchBox->SetText(FText::FromString(Catch));
	CatchBox->OnTextChanged.AddUniqueDynamic(this, &UCharacterCreatorWidget::HandleCatchphraseChanged);
	Place(Canvas, MakeText(WidgetTree, LOCTEXT("CatchNote", "Elle s'affiche sur ta carte dresseur."), 20.f, PokemonFonts::EStyle::Regular, DimColor), FVector2D(LeftX + 6.f, 540.f), FVector2D(LeftWidth, 30.f));
}

void UCharacterCreatorWidget::BuildToolbar()
{
	// Barre du haut au centre, dans une pilule en verre (les icônes d'inZOI deviennent des mots en attendant les siennes)
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const struct { FText Label; TFunction<void()> Action; } Tools[] = {
		{ LOCTEXT("Random", "Aléatoire"), [this]() { Randomize(); } },
		{ LOCTEXT("Photo", "Photo"), [this]() { TakePhoto(); } },
		{ LOCTEXT("ZoomFace", "Visage"), [this]() { if (Studio.IsValid()) { Studio->SetFocus(EStudioFocus::Face); } } },
		{ LOCTEXT("ZoomBody", "Corps"), [this]() { if (Studio.IsValid()) { Studio->SetFocus(EStudioFocus::Body); } } },
		{ LOCTEXT("Undo", "Annuler"), [this]() { Undo(); } },
		{ LOCTEXT("Redo", "Refaire"), [this]() { Redo(); } },
	};
	for (const auto& Tool : Tools)
	{
		UHorizontalBoxSlot* ToolSlot = Row->AddChildToHorizontalBox(MakeTextButton(WidgetTree, Tool.Label, 22.f, Tool.Action));
		ToolSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ToolSlot->SetPadding(FMargin(2.f, 4.f));
	}
	PokemonGlass::FGlassLook Look;
	Look.CornerRadius = 26.f;
	Place(Canvas, PokemonGlass::Make(WidgetTree, Row, FMargin(8.f, 0.f), Look, TEXT("Toolbar")), FVector2D(640.f, 16.f), FVector2D(640.f, PillHeight));
}

void UCharacterCreatorWidget::BuildTabs()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FText Names[] = { LOCTEXT("TabPresets", "Modèles"), LOCTEXT("TabFace", "Visage"), LOCTEXT("TabBody", "Corps"),
		LOCTEXT("TabOutfit", "Tenue"), LOCTEXT("TabAccessories", "Accessoires") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		UTextBlock* Label = nullptr;
		UCreatorButton* Button = MakeTextButton(WidgetTree, Names[Index], 24.f, [this, Index]() { SelectTab(ETab(Index)); }, &Label);
		UHorizontalBoxSlot* TabSlot = Row->AddChildToHorizontalBox(Button);
		TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TabSlot->SetPadding(FMargin(3.f, 4.f));
		TabButtons.Add(Button);
		TabLabels.Add(Label);
	}
	PokemonGlass::FGlassLook Look;
	Look.CornerRadius = 26.f;
	Place(Canvas, PokemonGlass::Make(WidgetTree, Row, FMargin(4.f, 0.f), Look, TEXT("Tabs")), FVector2D(540.f, BottomY), FVector2D(840.f, PillHeight));

	// Basique / Détaillé au-dessus des onglets (Visage et Corps)
	UHorizontalBox* ModeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FText Modes[] = { LOCTEXT("Basic", "Basique"), LOCTEXT("Detailed", "Détaillé") };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UTextBlock* Label = nullptr;
		UCreatorButton* Button = MakeTextButton(WidgetTree, Modes[Index], 22.f, [this, Index]() { SetDetailed(Index == 1); }, &Label);
		UHorizontalBoxSlot* ModeSlot = ModeRow->AddChildToHorizontalBox(Button);
		ModeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ModeSlot->SetPadding(FMargin(3.f, 3.f));
		ModeLabels.Add(Label);
	}
	ModeBar = MakeBox(WidgetTree, FVector2D(360.f, 44.f), ModeRow);
	Place(Canvas, ModeBar, FVector2D(780.f, 936.f), FVector2D(360.f, 44.f));
}

void UCharacterCreatorWidget::BuildBottomLeft()
{
	Place(Canvas, MakeTextButton(WidgetTree, LOCTEXT("HideUI", "◯  Masquer l'interface (U)"), 20.f, [this]() { ToggleUI(); }),
		FVector2D(36.f, 944.f), FVector2D(300.f, 36.f));

	StudioText = MakeText(WidgetTree, LOCTEXT("Studio", "Studio"), 24.f);
	UCreatorButton* StudioButton = MakeButton(WidgetTree, StudioText, ButtonFill, PillHeight * 0.5f, [this]()
	{
		if (Studio.IsValid())
		{
			StudioText->SetText(FText::Format(LOCTEXT("StudioLabel", "Studio : {0}"), Studio->NextLighting()));
		}
	}, ButtonOutline, 1.5f);
	Place(Canvas, StudioButton, FVector2D(32.f, BottomY), FVector2D(260.f, PillHeight));

	// Bas droite : message et Terminer
	StatusText = MakeText(WidgetTree, FText::GetEmpty(), 20.f, PokemonFonts::EStyle::Regular, DimColor);
	StatusText->SetJustification(ETextJustify::Right);
	StatusText->SetAutoWrapText(true);
	Place(Canvas, StatusText, FVector2D(1410.f, 930.f), FVector2D(460.f, 56.f));

	UTextBlock* FinishLabel = MakeText(WidgetTree, LOCTEXT("Finish", "Terminer"), 26.f);
	FinishButton = MakeButton(WidgetTree, FinishLabel, FinishFill, PillHeight * 0.5f, [this]() { Finish(); });
	Place(Canvas, FinishButton, FVector2D(1670.f, BottomY), FVector2D(200.f, PillHeight));
}

void UCharacterCreatorWidget::BuildFaceHotspots()
{
	// Points sur le côté du visage, reliés au panneau de droite (Face Presets, Hair, Skin, Eyes, Makeup d'inZOI)
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Hotspots"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FText Parts[] = { LOCTEXT("PartPresets", "Modèles de visage"), LOCTEXT("PartHair", "Cheveux"), LOCTEXT("PartSkin", "Peau"),
		LOCTEXT("PartEyes", "Yeux"), LOCTEXT("PartMakeup", "Maquillage") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Parts); ++Index)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Dot = MakeRounded(WidgetTree, FLinearColor::Transparent, 10.f, FLinearColor::White, 2.f);
		UHorizontalBoxSlot* DotSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(20.f, 20.f), Dot));
		DotSlot->SetVerticalAlignment(VAlign_Center);
		DotSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		UTextBlock* Label = MakeText(WidgetTree, Parts[Index], 24.f);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		UCreatorButton* Button = MakeButton(WidgetTree, Row, FLinearColor(1.f, 1.f, 1.f, 0.f), 8.f, [this, Index]() { SelectFacePart(EFacePart(Index)); });
		if (UButtonSlot* RowSlot = Cast<UButtonSlot>(Row->Slot))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Left);
		}
		UCanvasPanelSlot* ButtonSlot = Group->AddChildToCanvas(Button);
		ButtonSlot->SetPosition(FVector2D(HotspotX, HotspotTops[Index]));
		ButtonSlot->SetSize(FVector2D(PanelPos.X - HotspotX - 6.f, 40.f));
		HotspotDots.Add(Dot);
		HotspotLabels.Add(Label);
	}
	Hotspots = Group;
	Place(Canvas, Group, FVector2D::ZeroVector, DesignSize);
}

void UCharacterCreatorWidget::BuildRightPanel()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelTitle = MakeText(WidgetTree, FText::GetEmpty(), 28.f, PokemonFonts::EStyle::Bold);
	Column->AddChildToVerticalBox(PanelTitle)->SetPadding(FMargin(4.f, 0.f, 0.f, 10.f));
	Column->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(0.f, 1.f), MakeLine(WidgetTree)))->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	PanelList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PanelList"));
	PanelList->SetAnimateWheelScrolling(true);
	PanelList->SetScrollbarThickness(FVector2D(4.f, 4.f));
	UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(PanelList);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	PokemonGlass::FGlassLook Look;
	Look.CornerRadius = 20.f;
	RightPanel = PokemonGlass::Make(WidgetTree, Column, FMargin(24.f, 20.f), Look, TEXT("RightPanel"));
	Place(Canvas, RightPanel, PanelPos, PanelSize);
}

// ================================================================ Panneau de droite

void UCharacterCreatorWidget::RefreshRightPanel()
{
	PanelList->ClearChildren();
	Sliders.Reset();
	if (!Data)
	{
		return;
	}
	switch (Tab)
	{
	case ETab::Presets:
		PanelTitle->SetText(LOCTEXT("PanelPresets", "Modèles"));
		AddPresetGrid(Data->FullPresets, true);
		break;

	case ETab::Face:
		if (bDetailed)
		{
			PanelTitle->SetText(LOCTEXT("PanelFaceDetailed", "Visage en détail"));
			AddNote(LOCTEXT("HandleHint", "Attrape les points sur le visage et fais-les glisser, ou règle chaque curseur."));
			AddSliders(ETrainerSliderGroup::Face);
			break;
		}
		switch (FacePart)
		{
		case EFacePart::Presets:
			PanelTitle->SetText(LOCTEXT("PanelFacePresets", "Modèles de visage"));
			// Têtes importées (inZOI) d'abord, puis les réglages de curseurs enregistrés
			if (!AddPartGrid(TEXT("Face")) || !Data->FacePresets.IsEmpty())
			{
				AddPresetGrid(Data->FacePresets, false);
			}
			break;
		case EFacePart::Hair:
			PanelTitle->SetText(LOCTEXT("PanelHair", "Cheveux"));
			AddNote(LOCTEXT("HairSoon", "Les coiffures arrivent avec la garde-robe : elles se rangeront ici."));
			break;
		case EFacePart::Skin:
			PanelTitle->SetText(LOCTEXT("PanelSkin", "Peau"));
			AddSection(LOCTEXT("SkinTone", "Teint"));
			AddSwatches(Data->SkinPalette, true);
			break;
		case EFacePart::Eyes:
			PanelTitle->SetText(LOCTEXT("PanelEyes", "Yeux"));
			AddSection(LOCTEXT("EyeColor", "Couleur des yeux"));
			AddSwatches(Data->EyePalette, false);
			break;
		case EFacePart::Makeup:
			PanelTitle->SetText(LOCTEXT("PanelMakeup", "Maquillage"));
			AddNote(LOCTEXT("MakeupSoon", "Bientôt : maquillage, taches de rousseur, grains de beauté, tatouages…"));
			break;
		}
		break;

	case ETab::Body:
		PanelTitle->SetText(LOCTEXT("PanelBody", "Corps"));
		AddSliders(ETrainerSliderGroup::Body);
		break;

	case ETab::Outfit:
		PanelTitle->SetText(LOCTEXT("PanelOutfit", "Tenue"));
		if (!AddPartGrid(TEXT("Outfit")))
		{
			AddNote(LOCTEXT("OutfitSoon", "Les vêtements arriveront avec les boutiques de vêtements des villes. Ceux que tu achètes se rangeront ici et dans l'armoire de ta chambre."));
		}
		break;

	case ETab::Accessories:
		PanelTitle->SetText(LOCTEXT("PanelAccessories", "Accessoires"));
		if (!AddPartGrid(TEXT("Accessories")))
		{
			AddNote(LOCTEXT("AccessoriesSoon", "Bientôt : casquettes, sacs, lunettes, bijoux, piercings…"));
		}
		break;
	}
}

void UCharacterCreatorWidget::AddSection(const FText& Title)
{
	UTextBlock* Text = MakeText(WidgetTree, Title, 22.f, PokemonFonts::EStyle::Bold, DimColor);
	PanelList->AddChild(Text);
	if (UScrollBoxSlot* TextSlot = Cast<UScrollBoxSlot>(Text->Slot))
	{
		TextSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f));
	}
}

void UCharacterCreatorWidget::AddNote(const FText& Text)
{
	UTextBlock* Note = MakeText(WidgetTree, Text, 22.f, PokemonFonts::EStyle::Regular, DimColor);
	Note->SetAutoWrapText(true);
	PanelList->AddChild(Note);
	if (UScrollBoxSlot* NoteSlot = Cast<UScrollBoxSlot>(Note->Slot))
	{
		NoteSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 12.f));
	}
}

void UCharacterCreatorWidget::AddSliders(ETrainerSliderGroup Group)
{
	const FTrainerBaseDef* Base = Data->FindBase(Current.Base);
	if (!Base)
	{
		return;
	}
	FString Section = TEXT("\x01");
	bool bAny = false;
	for (const FTrainerSliderDef& Def : Data->GetSliders(*Base))
	{
		if (Def.Group != Group)
		{
			continue;
		}
		bAny = true;
		if (Def.Section.ToString() != Section)
		{
			Section = Def.Section.ToString();
			if (!Section.IsEmpty())
			{
				AddSection(Def.Section);
			}
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Label = MakeText(WidgetTree, Def.Label.IsEmpty() ? FText::FromName(Def.Id) : Def.Label, 22.f);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(160.f, 0.f), Label));
		LabelSlot->SetVerticalAlignment(VAlign_Center);

		UCreatorSlider* Slider = WidgetTree->ConstructWidget<UCreatorSlider>(UCreatorSlider::StaticClass());
		Slider->SliderId = Def.Id;
		Slider->SetMinValue(Def.HasNegative() ? -Def.Limit : 0.f);
		Slider->SetMaxValue(Def.Limit);
		Slider->SetStepSize(0.01f);
		Slider->SetIndentHandle(false);
		Slider->SetSliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.45f));
		Slider->SetSliderHandleColor(FLinearColor::White);
		Slider->SetValue(Current.GetSlider(Def.Id));
		const FName Id = Def.Id;
		Slider->Setup([this, Id](float Value)
		{
			FTrainerAppearance New = Current;
			New.SetSlider(Id, Value);
			ChangeAppearance(New, Id);
			RefreshSliderValues();
		});
		UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);

		Slider->ValueText = MakeText(WidgetTree, SignedValue(Current.GetSlider(Def.Id)), 20.f, PokemonFonts::EStyle::Regular, DimColor);
		Slider->ValueText->SetJustification(ETextJustify::Right);
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(56.f, 0.f), Slider->ValueText));
		ValueSlot->SetVerticalAlignment(VAlign_Center);

		PanelList->AddChild(MakeBox(WidgetTree, FVector2D(0.f, 42.f), Row));
		Sliders.Add(Slider);
	}
	if (!bAny)
	{
		AddNote(LOCTEXT("NoSliders", "Pas encore de curseur ici : ajoute-les dans DA_TrainerAppearance (Common Sliders ou Sliders de la base)."));
	}
}

void UCharacterCreatorWidget::AddPresetGrid(const TArray<FTrainerPresetDef>& Presets, bool bFull)
{
	if (Presets.IsEmpty())
	{
		AddNote(bFull ? LOCTEXT("NoFullPresets", "Pas encore de modèle : ajoute-les dans DA_TrainerAppearance (Full Presets).")
			: LOCTEXT("NoFacePresets", "Pas encore de modèle de visage : ajoute-les dans DA_TrainerAppearance (Face Presets)."));
		return;
	}
	UHorizontalBox* Row = nullptr;
	for (int32 Index = 0; Index < Presets.Num(); ++Index)
	{
		if (Index % GridColumns == 0)
		{
			Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			PanelList->AddChild(Row);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
			}
		}
		const FTrainerPresetDef& Preset = Presets[Index];

		// Vignette ronde (image si fournie, sinon le nom dans le rond)
		UWidget* Inside = nullptr;
		if (UTexture2D* Thumbnail = Preset.Thumbnail.LoadSynchronous())
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Image->SetBrushFromTexture(Thumbnail);
			Inside = MakeBox(WidgetTree, FVector2D(100.f, 100.f), Image);
		}
		else
		{
			UTextBlock* Initial = MakeText(WidgetTree, Preset.Label.IsEmpty() ? FText::AsNumber(Index + 1) : Preset.Label, 20.f);
			Initial->SetJustification(ETextJustify::Center);
			Initial->SetAutoWrapText(true);
			Inside = Initial;
		}
		UCreatorButton* Button = MakeButton(WidgetTree, Inside, ButtonFill, 56.f, [this, Index, bFull]()
		{
			const TArray<FTrainerPresetDef>& List = bFull ? Data->FullPresets : Data->FacePresets;
			if (!List.IsValidIndex(Index))
			{
				return;
			}
			FTrainerAppearance New = Current;
			const FTrainerAppearance& Look = List[Index].Look;
			if (bFull)
			{
				// Tout le personnage, sauf les vêtements (achetés en boutique)
				New = Look;
				New.Outfit = Current.Outfit;
				New.bCreated = Current.bCreated;
				if (Look.Base.IsNone())
				{
					New.Base = Current.Base;
				}
			}
			else
			{
				for (const FTrainerSliderValue& Value : Look.Sliders)
				{
					New.SetSlider(Value.Slider, Value.Value);
				}
			}
			ChangeAppearance(New, TEXT("Preset"), false);
			RefreshAll();
		}, ButtonOutline, 1.5f);

		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UVerticalBoxSlot* ButtonSlot = Cell->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(112.f, 112.f), Button));
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		if (Preset.Thumbnail.IsNull() == false)
		{
			UTextBlock* Caption = MakeText(WidgetTree, Preset.Label, 18.f, PokemonFonts::EStyle::Regular, DimColor);
			Caption->SetJustification(ETextJustify::Center);
			Cell->AddChildToVerticalBox(Caption)->SetHorizontalAlignment(HAlign_Center);
		}
		UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Cell);
		CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

bool UCharacterCreatorWidget::AddPartGrid(FName TabName)
{
	const FTrainerBaseDef* Base = Data->FindBase(Current.Base);
	if (!Base)
	{
		return false;
	}
	// Emplacements de cet onglet qui ont au moins une pièce pour cette base
	TArray<const FTrainerPartSlotDef*> Slots;
	for (const FTrainerPartSlotDef& SlotDef : Data->PartSlots)
	{
		const FName SlotName = SlotDef.Slot;
		if (SlotDef.Tab == TabName && Data->Parts.ContainsByPredicate([&](const FTrainerPartDef& P) { return P.Slot == SlotName && P.FitsBase(Base->Id); }))
		{
			Slots.Add(&SlotDef);
		}
	}
	if (Slots.IsEmpty())
	{
		return false;
	}
	const FTrainerPartSlotDef* Shown = Slots[0];
	for (const FTrainerPartSlotDef* SlotDef : Slots)
	{
		if (SlotDef->Slot == PartSlotFilter)
		{
			Shown = SlotDef;
		}
	}

	// Puces des emplacements (Haut, Bas, Chaussures...) quand il y en a plusieurs
	if (Slots.Num() > 1)
	{
		UWrapBox* Chips = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Chips->SetInnerSlotPadding(FVector2D(8.f, 8.f));
		for (const FTrainerPartSlotDef* SlotDef : Slots)
		{
			const bool bSelected = SlotDef == Shown;
			const FName SlotName = SlotDef->Slot;
			UTextBlock* Label = nullptr;
			UCreatorButton* Chip = MakeTextButton(WidgetTree, SlotDef->Label.IsEmpty() ? FText::FromName(SlotName) : SlotDef->Label, 20.f,
				[this, SlotName]() { PartSlotFilter = SlotName; RefreshRightPanel(); }, &Label, bSelected ? SelectedFill : ButtonFill, 18.f);
			if (bSelected && Label)
			{
				Label->SetColorAndOpacity(FSlateColor(DarkText));
				Label->SetShadowColorAndOpacity(FLinearColor::Transparent);
			}
			Chips->AddChildToWrapBox(MakeBox(WidgetTree, FVector2D(0.f, 40.f), Chip));
		}
		PanelList->AddChild(Chips);
		if (UScrollBoxSlot* ChipsSlot = Cast<UScrollBoxSlot>(Chips->Slot))
		{
			ChipsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
	}

	// Pièce portée maintenant dans cet emplacement
	const TMap<FName, const FTrainerPartDef*> Worn = Data->ResolveParts(*Base, Current);
	const FTrainerPartDef* const* WornPart = Worn.Find(Shown->Slot);
	const FName WornId = WornPart && *WornPart ? (*WornPart)->Id : NAME_None;

	TArray<const FTrainerPartDef*> List;
	for (const FTrainerPartDef& Part : Data->Parts)
	{
		if (Part.Slot == Shown->Slot && Part.FitsBase(Base->Id))
		{
			List.Add(&Part);
		}
	}

	const FName SlotName = Shown->Slot;
	auto Wear = [this, SlotName](FName Id)
	{
		FTrainerAppearance New = Current;
		New.Outfit.RemoveAll([&](FName Existing)
		{
			const FString Text = Existing.ToString();
			if (Text.StartsWith(TEXT("-")))
			{
				return FName(*Text.Mid(1)) == SlotName;
			}
			const FTrainerPartDef* Part = Data->FindPart(Existing);
			return !Part || Part->Slot == SlotName;
		});
		New.Outfit.Add(Id);
		ChangeAppearance(New, SlotName, false);
		RefreshRightPanel();
	};

	FString Category = TEXT("\x01");
	int32 Column = 0;
	UHorizontalBox* Row = nullptr;
	auto AddTile = [&](UWidget* Inside, bool bSelected, TFunction<void()> OnClick)
	{
		if (!Row || Column == GridColumns)
		{
			Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			PanelList->AddChild(Row);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			}
			Column = 0;
		}
		UCreatorButton* Button = MakeButton(WidgetTree, Inside, ButtonFill, 16.f, MoveTemp(OnClick),
			bSelected ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.25f), bSelected ? 4.f : 1.f);
		UHorizontalBoxSlot* TileSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(124.f, 124.f), Button));
		TileSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		++Column;
	};

	if (!Shown->bRequired)
	{
		UTextBlock* NoneText = MakeText(WidgetTree, LOCTEXT("PartNone", "Aucun"), 20.f);
		NoneText->SetJustification(ETextJustify::Center);
		const FName NoneId(*(TEXT("-") + SlotName.ToString()));
		AddTile(NoneText, WornId.IsNone(), [Wear, NoneId]() { Wear(NoneId); });
	}
	for (const FTrainerPartDef* Part : List)
	{
		if (Part->Category.ToString() != Category)
		{
			Category = Part->Category.ToString();
			if (!Category.IsEmpty())
			{
				Row = nullptr;
				AddSection(Part->Category);
			}
		}
		UWidget* Inside = nullptr;
		if (UTexture2D* Thumbnail = Part->Thumbnail.LoadSynchronous())
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Image->SetBrushFromTexture(Thumbnail);
			Inside = MakeBox(WidgetTree, FVector2D(112.f, 112.f), Image);
		}
		else
		{
			UTextBlock* Text = MakeText(WidgetTree, Part->Label.IsEmpty() ? FText::FromName(Part->Id) : Part->Label, 16.f);
			Text->SetJustification(ETextJustify::Center);
			Text->SetAutoWrapText(true);
			Inside = Text;
		}
		const FName Id = Part->Id;
		AddTile(Inside, Id == WornId, [Wear, Id]() { Wear(Id); });
	}
	return true;
}

void UCharacterCreatorWidget::AddSwatches(const TArray<FLinearColor>& Colors, bool bSkin)
{
	UHorizontalBox* Row = nullptr;
	const FLinearColor Selected = bSkin ? Current.SkinColor : Current.EyeColor;
	for (int32 Index = 0; Index < Colors.Num(); ++Index)
	{
		if (Index % SwatchColumns == 0)
		{
			Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			PanelList->AddChild(Row);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			}
		}
		const FLinearColor Color = FLinearColor(Colors[Index].R, Colors[Index].G, Colors[Index].B, 1.f);
		const bool bSelected = SameColor(Color, Selected);
		UCreatorButton* Button = MakeButton(WidgetTree, nullptr, Color, 26.f, [this, Color, bSkin]()
		{
			FTrainerAppearance New = Current;
			(bSkin ? New.SkinColor : New.EyeColor) = Color;
			ChangeAppearance(New, bSkin ? FName(TEXT("Skin")) : FName(TEXT("Eyes")), false);
			RefreshRightPanel();
		}, bSelected ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.35f), bSelected ? 4.f : 1.f);
		UHorizontalBoxSlot* SwatchSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(52.f, 52.f), Button));
		SwatchSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}
}

// ================================================================ État

void UCharacterCreatorWidget::RefreshAll()
{
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		const bool bSelected = Index == int32(Tab);
		TabButtons[Index]->SetStyle(MakeStyle(TabButtons[Index]->GetStyle(), bSelected ? SelectedFill : FLinearColor(1.f, 1.f, 1.f, 0.f), 22.f));
		TabLabels[Index]->SetColorAndOpacity(FSlateColor(bSelected ? DarkText : TextColor));
	}
	for (int32 Index = 0; Index < ModeLabels.Num(); ++Index)
	{
		const bool bSelected = (Index == 1) == bDetailed;
		ModeLabels[Index]->SetColorAndOpacity(FSlateColor(bSelected ? TextColor : DimColor));
		ModeLabels[Index]->SetFont(PokemonFonts::Get(22.f * FontScale, bSelected ? PokemonFonts::EStyle::Bold : PokemonFonts::EStyle::Regular));
	}
	for (int32 Index = 0; Index < HotspotDots.Num(); ++Index)
	{
		const bool bSelected = Index == int32(FacePart);
		HotspotDots[Index]->SetBrush(FSlateRoundedBoxBrush(bSelected ? FLinearColor::White : FLinearColor::Transparent, 10.f, FLinearColor::White, 2.f));
		HotspotLabels[Index]->SetColorAndOpacity(FSlateColor(bSelected ? TextColor : DimColor));
	}

	const FTrainerBaseDef* Base = Data ? Data->FindBase(Current.Base) : nullptr;
	BaseText->SetText(Base ? (Base->Label.IsEmpty() ? FText::FromName(Base->Id) : Base->Label) : FText::GetEmpty());

	const bool bFaceOrBody = Tab == ETab::Face || Tab == ETab::Body;
	ModeBar->SetVisibility(bFaceOrBody ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Hotspots->SetVisibility(Tab == ETab::Face && !bDetailed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Studio.IsValid())
	{
		Studio->SetFocus(Tab == ETab::Face ? EStudioFocus::Face : EStudioFocus::Body);
	}
	RefreshRightPanel();
}

void UCharacterCreatorWidget::RefreshSliderValues()
{
	for (UCreatorSlider* Slider : Sliders)
	{
		const float Value = Current.GetSlider(Slider->SliderId);
		if (!FMath::IsNearlyEqual(Slider->GetValue(), Value, 0.001f))
		{
			Slider->SetValue(Value);
		}
		if (Slider->ValueText)
		{
			Slider->ValueText->SetText(SignedValue(Value));
		}
	}
}

void UCharacterCreatorWidget::SetStatus(const FText& Text)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
	}
}

// ================================================================ Actions

void UCharacterCreatorWidget::SelectTab(ETab NewTab)
{
	Tab = NewTab;
	RefreshAll();
}

void UCharacterCreatorWidget::SelectFacePart(EFacePart Part)
{
	FacePart = Part;
	RefreshAll();
}

void UCharacterCreatorWidget::SetDetailed(bool bNewDetailed)
{
	bDetailed = bNewDetailed;
	RefreshAll();
}

void UCharacterCreatorWidget::SelectBase(int32 Direction)
{
	if (!Data || Data->Bases.Num() < 2)
	{
		return;
	}
	const int32 Count = Data->Bases.Num();
	int32 Index = Data->Bases.IndexOfByPredicate([this](const FTrainerBaseDef& B) { return B.Id == Current.Base; });
	Index = (FMath::Max(0, Index) + Direction + Count) % Count;
	FTrainerAppearance New = Current;
	New.Base = Data->Bases[Index].Id;
	ChangeAppearance(New, TEXT("Base"), false);
	RefreshAll();
}

void UCharacterCreatorWidget::Randomize()
{
	if (!Data || Data->Bases.IsEmpty())
	{
		return;
	}
	FTrainerAppearance New = Current;
	New.Base = Data->Bases[FMath::RandRange(0, Data->Bases.Num() - 1)].Id;
	New.Sliders.Reset();
	if (const FTrainerBaseDef* Base = Data->FindBase(New.Base))
	{
		for (const FTrainerSliderDef& Def : Data->GetSliders(*Base))
		{
			// Reste dans les 60 % du milieu, deux tirages pour que les valeurs extrêmes soient rares
			const float Spread = 0.6f * Def.Limit;
			const float Value = !Def.HasNegative()
				? FMath::Abs(FMath::FRandRange(-Spread, Spread) + FMath::FRandRange(-Spread, Spread)) * 0.5f
				: (FMath::FRandRange(-Spread, Spread) + FMath::FRandRange(-Spread, Spread)) * 0.5f;
			New.SetSlider(Def.Id, Value);
		}
	}
	if (Data->SkinPalette.Num() > 0)
	{
		New.SkinColor = Data->SkinPalette[FMath::RandRange(0, Data->SkinPalette.Num() - 1)];
	}
	if (Data->EyePalette.Num() > 0)
	{
		New.EyeColor = Data->EyePalette[FMath::RandRange(0, Data->EyePalette.Num() - 1)];
	}
	ChangeAppearance(New, TEXT("Random"), false);
	RefreshAll();
}

void UCharacterCreatorWidget::TakePhoto()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		// Capture du studio sans l'interface, en double résolution
		PlayerController->ConsoleCommand(TEXT("HighResShot 2"));
		SetStatus(LOCTEXT("PhotoSaved", "Photo enregistrée dans Saved/Screenshots."));
	}
}

void UCharacterCreatorWidget::ChangeAppearance(const FTrainerAppearance& NewAppearance, FName ControlId, bool bMerge)
{
	PushUndo(ControlId, bMerge);
	Current = TrainerAppearanceRender::Sanitize(Data, NewAppearance);
	RedoStack.Reset();
	if (Studio.IsValid())
	{
		Studio->ShowAppearance(Current);
	}
}

void UCharacterCreatorWidget::PushUndo(FName ControlId, bool bMerge)
{
	// Les petits pas d'un même curseur (moins d'une seconde d'écart) ne font qu'une étape d'Annuler
	const double Now = FPlatformTime::Seconds();
	if (bMerge && ControlId == LastUndoControl && Now - LastUndoTime < 1.0)
	{
		LastUndoTime = Now;
		return;
	}
	UndoStack.Push(Current);
	if (UndoStack.Num() > 100)
	{
		UndoStack.RemoveAt(0);
	}
	LastUndoControl = ControlId;
	LastUndoTime = Now;
}

void UCharacterCreatorWidget::Undo()
{
	if (UndoStack.IsEmpty())
	{
		return;
	}
	RedoStack.Push(Current);
	Current = UndoStack.Pop();
	LastUndoControl = NAME_None;
	if (Studio.IsValid())
	{
		Studio->ShowAppearance(Current);
	}
	RefreshAll();
}

void UCharacterCreatorWidget::Redo()
{
	if (RedoStack.IsEmpty())
	{
		return;
	}
	UndoStack.Push(Current);
	Current = RedoStack.Pop();
	LastUndoControl = NAME_None;
	if (Studio.IsValid())
	{
		Studio->ShowAppearance(Current);
	}
	RefreshAll();
}

void UCharacterCreatorWidget::ToggleUI()
{
	bUIHidden = !bUIHidden;
	Canvas->SetVisibility(bUIHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UCharacterCreatorWidget::Finish()
{
	const FString CleanName = UTrainerAppearanceComponent::CleanText(Name, UTrainerAppearanceComponent::MaxNameLength);
	if (CleanName.IsEmpty())
	{
		SetStatus(LOCTEXT("NeedName", "Choisis d'abord un prénom."));
		if (NameBox)
		{
			NameBox->SetKeyboardFocus();
		}
		return;
	}
	if (Owner.IsValid())
	{
		Owner->CloseEditor(true, Current, CleanName, UTrainerAppearanceComponent::CleanText(Catch, UTrainerAppearanceComponent::MaxCatchphraseLength));
	}
}

void UCharacterCreatorWidget::Cancel()
{
	if (Mode == ETrainerEditorMode::Creation)
	{
		return; // le personnage doit être créé
	}
	if (Owner.IsValid())
	{
		Owner->CloseEditor(false, Original, FString(), FString());
	}
}

void UCharacterCreatorWidget::HandleNameChanged(const FText& Text)
{
	FString Value = Text.ToString();
	if (Value.Len() > UTrainerAppearanceComponent::MaxNameLength)
	{
		Value = Value.Left(UTrainerAppearanceComponent::MaxNameLength);
		NameBox->SetText(FText::FromString(Value));
	}
	Name = Value;
}

void UCharacterCreatorWidget::HandleCatchphraseChanged(const FText& Text)
{
	FString Value = Text.ToString();
	if (Value.Len() > UTrainerAppearanceComponent::MaxCatchphraseLength)
	{
		Value = Value.Left(UTrainerAppearanceComponent::MaxCatchphraseLength);
		CatchBox->SetText(FText::FromString(Value));
	}
	Catch = Value;
}

// ================================================================ Souris et clavier

FVector2D UCharacterCreatorWidget::ToCanvas(const FVector2D& AbsolutePosition) const
{
	return Canvas ? Canvas->GetCachedGeometry().AbsoluteToLocal(AbsolutePosition) : AbsolutePosition;
}

void UCharacterCreatorWidget::UpdateHandles()
{
	APlayerController* PlayerController = GetOwningPlayer();
	const ETrainerSliderGroup Group = Tab == ETab::Body ? ETrainerSliderGroup::Body : ETrainerSliderGroup::Face;
	const bool bShow = Data && Studio.IsValid() && PlayerController && bDetailed && !bUIHidden && (Tab == ETab::Face || Tab == ETab::Body);
	for (int32 Index = 0; Index < HandleDots.Num(); ++Index)
	{
		bool bVisible = false;
		if (bShow && Data->DragHandles.IsValidIndex(Index) && Data->DragHandles[Index].Group == Group)
		{
			const FTrainerDragHandle& Def = Data->DragHandles[Index];
			FVector World;
			FVector2D ViewportPosition;
			if (Studio->GetBoneLocation(Def.Bone, Def.Offset, World)
				&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, World, ViewportPosition, true))
			{
				// Position dans la fenêtre du joueur (= ce widget) → repère de la maquette 1920 x 1080
				const FVector2D Absolute = GetCachedGeometry().LocalToAbsolute(ViewportPosition);
				HandlePositions[Index] = ToCanvas(Absolute);
				if (UCanvasPanelSlot* DotSlot = Cast<UCanvasPanelSlot>(HandleDots[Index]->Slot))
				{
					DotSlot->SetPosition(HandlePositions[Index] - FVector2D(HandleSize * 0.5f));
				}
				bVisible = true;
			}
		}
		HandleDots[Index]->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bVisible)
		{
			HandlePositions[Index] = FVector2D(-10000.f);
		}
	}
}

int32 UCharacterCreatorWidget::FindHandleAt(const FVector2D& CanvasPosition) const
{
	int32 Best = INDEX_NONE;
	float BestDistance = HandleGrabRadius;
	for (int32 Index = 0; Index < HandlePositions.Num(); ++Index)
	{
		const float Distance = FVector2D::Distance(HandlePositions[Index], CanvasPosition);
		if (Distance < BestDistance)
		{
			Best = Index;
			BestDistance = Distance;
		}
	}
	return Best;
}

void UCharacterCreatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateHandles();
	if (FinishButton)
	{
		// Terminer reste cliquable (il explique ce qui manque) mais pâlit tant qu'il n'y a pas de prénom
		const bool bReady = !UTrainerAppearanceComponent::CleanText(Name, UTrainerAppearanceComponent::MaxNameLength).IsEmpty();
		FinishButton->SetRenderOpacity(bReady ? 1.f : 0.55f);
	}
}

FReply UCharacterCreatorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	LastMouse = ToCanvas(InMouseEvent.GetScreenSpacePosition());
	const int32 Handle = FindHandleAt(LastMouse);
	DragTarget = Handle != INDEX_NONE ? Handle : -2;
	if (Handle != INDEX_NONE)
	{
		PushUndo(FName(TEXT("Handle"), Handle + 1), false);
		SetStatus(Data->DragHandles[Handle].Label);
	}
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UCharacterCreatorWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (DragTarget == -1 || !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}
	const FVector2D Mouse = ToCanvas(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Delta = Mouse - LastMouse;
	LastMouse = Mouse;

	if (DragTarget == -2)
	{
		// Glisser dans le vide : le personnage tourne
		if (Studio.IsValid())
		{
			Studio->Turn(Delta.X * 0.5f);
		}
		return FReply::Handled();
	}

	if (Data && Data->DragHandles.IsValidIndex(DragTarget))
	{
		const FTrainerDragHandle& Def = Data->DragHandles[DragTarget];
		FTrainerAppearance New = Current;
		if (!Def.SliderX.IsNone())
		{
			New.SetSlider(Def.SliderX, Current.GetSlider(Def.SliderX) + Delta.X / Def.PixelsPerUnit);
		}
		if (!Def.SliderY.IsNone())
		{
			New.SetSlider(Def.SliderY, Current.GetSlider(Def.SliderY) - Delta.Y / Def.PixelsPerUnit);
		}
		// L'étape d'Annuler a été prise au clic : on ne la refait pas à chaque mouvement
		Current = TrainerAppearanceRender::Sanitize(Data, New);
		RedoStack.Reset();
		if (Studio.IsValid())
		{
			Studio->ShowAppearance(Current);
		}
		RefreshSliderValues();
	}
	return FReply::Handled();
}

FReply UCharacterCreatorWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (DragTarget != -1 && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		DragTarget = -1;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UCharacterCreatorWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Studio.IsValid())
	{
		Studio->Zoom(InMouseEvent.GetWheelDelta());
	}
	return FReply::Handled();
}

FReply UCharacterCreatorWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::U)
	{
		ToggleUI();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Cancel();
		return FReply::Handled();
	}
	if (InKeyEvent.IsControlDown() && Key == EKeys::Z)
	{
		Undo();
		return FReply::Handled();
	}
	if (InKeyEvent.IsControlDown() && Key == EKeys::Y)
	{
		Redo();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
