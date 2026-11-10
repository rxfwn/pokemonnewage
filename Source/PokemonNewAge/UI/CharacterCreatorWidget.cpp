#include "UI/CharacterCreatorWidget.h"
#include "Character/CharacterStudio.h"
#include "Character/TrainerAppearanceRender.h"
#include "Core/PokemonSaveGame.h"
#include "UI/CreatorColorWheel.h"
#include "Misc/Paths.h"
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
#include "Materials/MaterialInstance.h"
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

void UCreatorSlider::HandleValue(float NewValue)
{
	if (OnChange)
	{
		OnChange(NewValue);
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
	// Panneau de droite élargi (colonne des emplacements + colonne des catégories + grille, comme inZOI)
	const FVector2D PanelPos(1250.f, 90.f);
	const FVector2D PanelSize(630.f, 830.f);
	constexpr float HotspotX = 1010.f;
	const float HotspotTops[] = { 300.f, 390.f, 480.f, 570.f, 660.f, 750.f };
	constexpr float SlotColumnWidth = 140.f;
	constexpr float TileSize = 98.f;
	/** Couleurs toutes faites du nuancier (rangée « Modèles » d'inZOI) puis le nuancier complet */
	const FLinearColor ClothTemplates[] = {
		FLinearColor::FromSRGBColor(FColor(0x2B, 0x2E, 0x33)), FLinearColor::FromSRGBColor(FColor(0x3B, 0x2F, 0x28)),
		FLinearColor::FromSRGBColor(FColor(0xB8, 0xA4, 0x88)), FLinearColor::FromSRGBColor(FColor(0x3C, 0x55, 0x6E)),
		FLinearColor::FromSRGBColor(FColor(0x40, 0x3C, 0x5F)), FLinearColor::FromSRGBColor(FColor(0x8C, 0x8F, 0x94)),
		FLinearColor::FromSRGBColor(FColor(0xE6, 0xE4, 0xDF)) };
	constexpr float BottomY = 994.f;
	/** Fond du studio : gris bleuté clair (par défaut, comme inZOI), blanc, gris foncé, beige, bleu, rose, vert sauge, anthracite */
	const FLinearColor BackdropColors[] = {
		FLinearColor(0.55f, 0.58f, 0.64f), FLinearColor(0.9f, 0.9f, 0.9f), FLinearColor(0.25f, 0.26f, 0.28f),
		FLinearColor(0.7f, 0.6f, 0.48f), FLinearColor(0.42f, 0.55f, 0.7f), FLinearColor(0.75f, 0.55f, 0.58f),
		FLinearColor(0.45f, 0.55f, 0.45f), FLinearColor(0.06f, 0.065f, 0.075f) };
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

	/** Vignette découpée en rond (le pinceau « boîte arrondie » d'Unreal rogne l'image elle-même) */
	UImage* MakeRoundImage(UWidgetTree* Tree, UTexture2D* Texture)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.Width = 0.f;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Image->SetBrush(Brush);
		return Image;
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
		BuildWornColumn();
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
		if (const UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions())
		{
			Studio->SetBackdropColor(Options->StudioBackdrop);
		}
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
	float TitleX = LeftX;
	if (Mode != ETrainerEditorMode::Creation)
	{
		UCreatorButton* Back = MakeTextButton(WidgetTree, LOCTEXT("Back", "‹  Retour"), 28.f, [this]() { Cancel(); });
		Place(Canvas, Back, FVector2D(18.f, 16.f), FVector2D(150.f, 46.f));
		Place(Canvas, MakeText(WidgetTree, FText::FromString(TEXT("|")), 28.f, PokemonFonts::EStyle::Regular, DimColor), FVector2D(178.f, 20.f), FVector2D(20.f, 40.f));
		TitleX = 208.f;
	}
	const FText Title = Mode == ETrainerEditorMode::Creation ? LOCTEXT("TitleCreate", "Créer ton dresseur")
		: (Mode == ETrainerEditorMode::Wardrobe ? LOCTEXT("TitleWardrobe", "Armoire") : LOCTEXT("TitleAppearance", "Apparence"));
	// Blanc, plus grand que « À quoi ressemble ton dresseur ? », aligné à gauche avec elle
	Place(Canvas, MakeText(WidgetTree, Title, 46.f, PokemonFonts::EStyle::Bold), FVector2D(TitleX, 16.f), FVector2D(700.f, 60.f));

	// Profil (onglet Modèles seulement ; ailleurs la colonne des pièces portées prend sa place, comme inZOI)
	UCanvasPanel* Group = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Profile"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Place(Group, MakeText(WidgetTree, LOCTEXT("Question", "À quoi ressemble ton dresseur ?"), 34.f, PokemonFonts::EStyle::Bold), FVector2D(LeftX, 96.f), FVector2D(700.f, 46.f));

	// Profil : prénom et base
	Place(Group, MakeText(WidgetTree, LOCTEXT("Profile", "Profil"), 28.f, PokemonFonts::EStyle::Bold), FVector2D(LeftX, 200.f), FVector2D(LeftWidth, 40.f));
	Place(Group, MakeLine(WidgetTree), FVector2D(LeftX, 244.f), FVector2D(LeftWidth, 1.f));

	Place(Group, MakeText(WidgetTree, LOCTEXT("Name", "Prénom"), 24.f), FVector2D(LeftX, 268.f), FVector2D(150.f, 34.f));
	NameBox = MakeField(WidgetTree, Group, FVector2D(230.f, 262.f), FVector2D(230.f, 44.f), LOCTEXT("NameHint", "Ton prénom"), 24.f);
	NameBox->SetJustification(ETextJustify::Center);
	NameBox->SetText(FText::FromString(Name));
	NameBox->OnTextChanged.AddUniqueDynamic(this, &UCharacterCreatorWidget::HandleNameChanged);

	Place(Group, MakeText(WidgetTree, LOCTEXT("Base", "Base"), 24.f), FVector2D(LeftX, 326.f), FVector2D(150.f, 34.f));
	Place(Group, MakeTextButton(WidgetTree, FText::FromString(TEXT("‹")), 34.f, [this]() { SelectBase(-1); }), FVector2D(230.f, 318.f), FVector2D(44.f, 44.f));
	BaseText = MakeText(WidgetTree, FText::GetEmpty(), 24.f);
	BaseText->SetJustification(ETextJustify::Center);
	Place(Group, BaseText, FVector2D(274.f, 324.f), FVector2D(142.f, 34.f));
	Place(Group, MakeTextButton(WidgetTree, FText::FromString(TEXT("›")), 34.f, [this]() { SelectBase(1); }), FVector2D(416.f, 318.f), FVector2D(44.f, 44.f));

	// Phrase d'accroche (à la place des Traits d'inZOI) : affichée sur la carte dresseur
	Place(Group, MakeText(WidgetTree, LOCTEXT("Catchphrase", "Phrase d'accroche"), 28.f, PokemonFonts::EStyle::Bold), FVector2D(LeftX, 418.f), FVector2D(LeftWidth, 40.f));
	Place(Group, MakeLine(WidgetTree), FVector2D(LeftX, 462.f), FVector2D(LeftWidth, 1.f));
	CatchBox = MakeField(WidgetTree, Group, FVector2D(LeftX, 482.f), FVector2D(LeftWidth, 48.f), LOCTEXT("CatchHint", "« Je serai le meilleur dresseur ! »"), 22.f);
	CatchBox->SetJustification(ETextJustify::Center);
	CatchBox->SetText(FText::FromString(Catch));
	CatchBox->OnTextChanged.AddUniqueDynamic(this, &UCharacterCreatorWidget::HandleCatchphraseChanged);
	Place(Group, MakeText(WidgetTree, LOCTEXT("CatchNote", "Elle s'affiche sur ta carte dresseur."), 20.f, PokemonFonts::EStyle::Regular, DimColor), FVector2D(LeftX + 6.f, 540.f), FVector2D(LeftWidth, 30.f));
	ProfileGroup = Group;
	Place(Canvas, Group, FVector2D::ZeroVector, DesignSize);
}

void UCharacterCreatorWidget::BuildToolbar()
{
	// Barre du haut au centre, dans une pilule en verre (les icônes d'inZOI deviennent des mots en attendant les siennes)
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const struct { FText Label; TFunction<void()> Action; } Tools[] = {
		{ LOCTEXT("Random", "Aléatoire"), [this]() { Randomize(); } },
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
	Place(Canvas, PokemonGlass::Make(WidgetTree, Row, FMargin(8.f, 0.f), Look, TEXT("Toolbar")), FVector2D(780.f, 16.f), FVector2D(360.f, PillHeight));
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
	// Touches du clavier : une case avec la lettre, puis ce qu'elle fait (cliquable aussi)
	auto AddKeyHint = [this](const TCHAR* Letter, const FText& Label, float Y, TFunction<void()> Action)
	{
		UTextBlock* KeyText = MakeText(WidgetTree, FText::FromString(Letter), 20.f, PokemonFonts::EStyle::Bold);
		KeyText->SetJustification(ETextJustify::Center);
		UBorder* Cap = MakeRounded(WidgetTree, FLinearColor(1.f, 1.f, 1.f, 0.12f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.7f), 1.5f);
		Cap->SetHorizontalAlignment(HAlign_Center);
		Cap->SetVerticalAlignment(VAlign_Center);
		Cap->SetContent(KeyText);
		Place(Canvas, Cap, FVector2D(36.f, Y), FVector2D(34.f, 34.f));
		Place(Canvas, MakeTextButton(WidgetTree, Label, 20.f, MoveTemp(Action)), FVector2D(78.f, Y - 2.f), FVector2D(240.f, 38.f));
	};
	// Plus de bouton « Changer de pose » : le studio enchaîne lui-même attente et poses (ACharacterStudio::Tick)
	AddKeyHint(TEXT("U"), LOCTEXT("HideUI", "Masquer l'interface"), 892.f, [this]() { ToggleUI(); });

	StudioText = MakeText(WidgetTree, LOCTEXT("Studio", "Studio"), 24.f);
	UCreatorButton* StudioButton = MakeButton(WidgetTree, StudioText, ButtonFill, PillHeight * 0.5f, [this]()
	{
		if (Studio.IsValid())
		{
			StudioText->SetText(FText::Format(LOCTEXT("StudioLabel", "Studio : {0}"), Studio->NextLighting()));
		}
	}, ButtonOutline, 1.5f);
	Place(Canvas, StudioButton, FVector2D(32.f, BottomY), FVector2D(260.f, PillHeight));

	// Couleur du fond du studio, gardée dans les options du joueur
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BackdropColors); ++Index)
	{
		const FLinearColor Color = BackdropColors[Index];
		UCreatorButton* Swatch = MakeButton(WidgetTree, nullptr, Color, 13.f, [this, Color]()
		{
			if (Studio.IsValid())
			{
				Studio->SetBackdropColor(Color);
			}
			if (UPokemonOptionsSave* Options = UPokemonSaveLibrary::GetOptions())
			{
				Options->StudioBackdrop = Color;
				UPokemonSaveLibrary::SaveOptions(Options);
			}
		}, FLinearColor(1.f, 1.f, 1.f, 0.7f), 1.5f);
		Swatch->SetToolTipText(LOCTEXT("BackdropColor", "Couleur du fond"));
		Place(Canvas, Swatch, FVector2D(34.f + Index * 32.f, BottomY - 46.f), FVector2D(26.f, 26.f));
	}

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
		LOCTEXT("PartFeatures", "Traits du visage"), LOCTEXT("PartEyes", "Yeux"), LOCTEXT("PartMakeup", "Maquillage") };
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
	// Nuancier de la pièce portée, fixe sous la grille
	ColorBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ColorBox"));
	ColorBox->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(ColorBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

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
	ColorBox->ClearChildren();
	ColorBox->SetVisibility(ESlateVisibility::Collapsed);
	HexBox = nullptr;
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
			if (!AddPartGrid(TEXT("Hair")))
			{
				AddNote(LOCTEXT("HairSoon", "Les coiffures, barbes et moustaches se rangeront ici dès qu'elles seront importées."));
			}
			break;
		case EFacePart::Skin:
			PanelTitle->SetText(LOCTEXT("PanelSkin", "Peau"));
			AddSection(LOCTEXT("SkinTone", "Teint"));
			AddSwatches(Data->SkinPalette, true);
			break;
		case EFacePart::Features:
			PanelTitle->SetText(LOCTEXT("PanelFeatures", "Traits du visage"));
			AddNote(LOCTEXT("FeaturesHint", "Chaque curseur déplace une partie du visage. Le mode Détaillé permet aussi d'attraper les points sur le visage."));
			AddSliders(ETrainerSliderGroup::Face);
			break;
		case EFacePart::Eyes:
			PanelTitle->SetText(LOCTEXT("PanelEyes", "Yeux"));
			// Motifs d'iris d'inZOI (lentilles), puis la couleur
			AddPartGrid(TEXT("Eyes"));
			AddSection(LOCTEXT("EyeColor", "Couleur des yeux"));
			AddSwatches(Data->EyePalette, false);
			break;
		case EFacePart::Makeup:
			PanelTitle->SetText(LOCTEXT("PanelMakeup", "Maquillage"));
			if (!AddPartGrid(TEXT("Makeup")))
			{
				AddNote(LOCTEXT("MakeupSoon", "Le maquillage d'inZOI se rangera ici dès que ses masques seront importés (Tools/InZOI/import_inzoi.bat)."));
			}
			break;
		}
		break;

	case ETab::Body:
		PanelTitle->SetText(LOCTEXT("PanelBody", "Corps"));
		if (Data->BodyPresets.ContainsByPredicate([this](const FTrainerPresetDef& P) { return P.Look.Base.IsNone() || P.Look.Base == Current.Base; }))
		{
			AddNote(LOCTEXT("BodyShapes", "Silhouettes d'inZOI : choisis un point de départ, puis affine avec les curseurs."));
			AddPresetGrid(Data->BodyPresets, false, true);
		}
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

void UCharacterCreatorWidget::AddPresetGrid(const TArray<FTrainerPresetDef>& Presets, bool bFull, bool bBody)
{
	if (Presets.IsEmpty())
	{
		AddNote(bFull ? LOCTEXT("NoFullPresets", "Pas encore de modèle : ajoute-les dans DA_TrainerAppearance (Full Presets).")
			: LOCTEXT("NoFacePresets", "Pas encore de modèle de visage : ajoute-les dans DA_TrainerAppearance (Face Presets)."));
		return;
	}
	UHorizontalBox* Row = nullptr;
	int32 Shown = 0;
	for (int32 Index = 0; Index < Presets.Num(); ++Index)
	{
		// Silhouettes : seulement celles de la base portée (corps d'homme ou de femme)
		if (bBody && !Presets[Index].Look.Base.IsNone() && Presets[Index].Look.Base != Current.Base)
		{
			continue;
		}
		if (Shown++ % GridColumns == 0)
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
			UImage* Image = MakeRoundImage(WidgetTree, Thumbnail);
			Inside = MakeBox(WidgetTree, FVector2D(100.f, 100.f), Image);
		}
		else
		{
			UTextBlock* Initial = MakeText(WidgetTree, Preset.Label.IsEmpty() ? FText::AsNumber(Index + 1) : Preset.Label, 20.f);
			Initial->SetJustification(ETextJustify::Center);
			Initial->SetAutoWrapText(true);
			Inside = Initial;
		}
		UCreatorButton* Button = MakeButton(WidgetTree, Inside, ButtonFill, 56.f, [this, Index, bFull, bBody]()
		{
			ApplyPreset(Index, bFull, bBody);
		}, ButtonOutline, 1.5f);

		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UVerticalBoxSlot* ButtonSlot = Cell->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(112.f, 112.f), Button));
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		if (!Preset.Thumbnail.IsNull() && !Preset.Label.IsEmpty())
		{
			UTextBlock* Caption = MakeText(WidgetTree, Preset.Label, 18.f, PokemonFonts::EStyle::Regular, DimColor);
			Caption->SetJustification(ETextJustify::Center);
			Cell->AddChildToVerticalBox(Caption)->SetHorizontalAlignment(HAlign_Center);
		}
		UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Cell);
		CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UCharacterCreatorWidget::ApplyPreset(int32 Index, bool bFull, bool bBody)
{
	const TArray<FTrainerPresetDef>& List = bBody ? Data->BodyPresets : bFull ? Data->FullPresets : Data->FacePresets;
	if (!List.IsValidIndex(Index))
	{
		return;
	}
	FTrainerAppearance New = Current;
	const FTrainerAppearance& Look = List[Index].Look;
	if (bFull)
	{
		// Tout le personnage sauf les vêtements (achetés en boutique) : tête, cheveux, sourcils, cils, barbe et leurs
		// couleurs viennent du modèle ; un emplacement que le modèle ne précise pas garde le choix actuel
		New = Look;
		auto SlotOf = [this](FName Id)
		{
			const FString Text = Id.ToString();
			if (Text.StartsWith(TEXT("-")))
			{
				return FName(*Text.Mid(1));
			}
			const FTrainerPartDef* Part = Data->FindPart(Id);
			return Part ? Part->Slot : FName();
		};
		TSet<FName> LookSlots;
		for (const FName Id : Look.Outfit)
		{
			const FTrainerPartSlotDef* SlotDef = Data->FindSlot(SlotOf(Id));
			if (SlotDef && SlotDef->Tab != TEXT("Outfit") && SlotDef->Tab != TEXT("Accessories"))
			{
				LookSlots.Add(SlotDef->Slot);
			}
		}
		New.Outfit.Reset();
		for (const FName Id : Current.Outfit)
		{
			if (!LookSlots.Contains(SlotOf(Id)))
			{
				New.Outfit.Add(Id);
			}
		}
		for (const FName Id : Look.Outfit)
		{
			if (LookSlots.Contains(SlotOf(Id)))
			{
				New.Outfit.Add(Id);
			}
		}
		New.PartColors.Reset();
		for (const FTrainerPartColor& Color : Current.PartColors)
		{
			if (!LookSlots.Contains(Color.Slot))
			{
				New.PartColors.Add(Color);
			}
		}
		for (const FTrainerPartColor& Color : Look.PartColors)
		{
			if (LookSlots.Contains(Color.Slot))
			{
				New.PartColors.Add(Color);
			}
		}
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
	const FName SlotName = Shown->Slot;

	// Pièce portée maintenant dans cet emplacement
	const TMap<FName, const FTrainerPartDef*> Worn = Data->ResolveParts(*Base, Current);
	const FTrainerPartDef* const* WornPart = Worn.Find(SlotName);
	const FTrainerPartDef* WornDef = WornPart ? *WornPart : nullptr;
	const FName WornId = WornDef ? WornDef->Id : NAME_None;

	TArray<const FTrainerPartDef*> List;
	TArray<FString> Categories;
	for (const FTrainerPartDef& Part : Data->Parts)
	{
		if (Part.Slot == SlotName && Part.FitsBase(Base->Id))
		{
			List.Add(&Part);
			Categories.AddUnique(Part.Category.ToString());
		}
	}
	Categories.Remove(FString());
	if (!PartCategoryFilter.IsEmpty() && !Categories.Contains(PartCategoryFilter))
	{
		PartCategoryFilter.Reset();
	}

	// Trois colonnes comme inZOI : emplacements (Haut, Bas...), catégories (T-shirts, Sweats...), grille
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PanelList->AddChild(Columns);

	// Maquette de L'amie (Cheveux) : une colonne d'onglets collés (Cheveux, Barbe, Moustache...) séparés par un trait,
	// l'onglet choisi plus clair, puis directement la grille. Plus de colonne de catégories.
	PartCategoryFilter.Reset();
	if (Slots.Num() > 1)
	{
		UVerticalBox* SlotColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			const FTrainerPartSlotDef* SlotDef = Slots[SlotIndex];
			const FName EntrySlot = SlotDef->Slot;
			const bool bSelected = SlotDef == Shown;
			UTextBlock* Text = nullptr;
			UCreatorButton* Button = MakeTextButton(WidgetTree, SlotDef->Label.IsEmpty() ? FText::FromName(EntrySlot) : SlotDef->Label, 22.f,
				[this, EntrySlot]() { PartSlotFilter = EntrySlot; PartCategoryFilter.Reset(); ColorZone = 0; RefreshRightPanel(); },
				&Text, bSelected ? FLinearColor(1.f, 1.f, 1.f, 0.22f) : FLinearColor(1.f, 1.f, 1.f, 0.f), 0.f);
			if (Text)
			{
				Text->SetAutoWrapText(true);
				Text->SetJustification(ETextJustify::Center);
				Text->SetColorAndOpacity(FSlateColor(bSelected ? FLinearColor::White : DimColor));
				Text->SetShadowColorAndOpacity(FLinearColor::Transparent); // l'ombre floue faisait un 2e mot fantôme
			}
			SlotColumn->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(0.f, 84.f), Button));
			if (SlotIndex + 1 < Slots.Num())
			{
				SlotColumn->AddChildToVerticalBox(MakeLine(WidgetTree))->SetPadding(FMargin(12.f, 0.f));
			}
		}
		UBorder* ColumnBack = MakeRounded(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.16f), 14.f, FLinearColor(1.f, 1.f, 1.f, 0.25f), 1.f);
		ColumnBack->SetPadding(FMargin(0.f));
		ColumnBack->SetVerticalAlignment(VAlign_Top);
		ColumnBack->SetContent(SlotColumn);
		UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(SlotColumnWidth, 0.f), ColumnBack));
		ColumnSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Grid = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* GridSlot = Columns->AddChildToHorizontalBox(Grid);
	GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

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
		RefreshWornColumn();
	};

	int32 Column = 0;
	UHorizontalBox* Row = nullptr;
	auto AddTile = [&](UWidget* Inside, bool bSelected, TFunction<void()> OnClick)
	{
		if (!Row || Column == GridColumns)
		{
			Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Grid->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			Column = 0;
		}
		UCreatorButton* Button = MakeButton(WidgetTree, Inside, ButtonFill, TileSize * 0.5f, MoveTemp(OnClick),
			bSelected ? FLinearColor(0.45f, 0.72f, 1.f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.25f), bSelected ? 4.f : 1.f);
		UHorizontalBoxSlot* TileSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(TileSize, TileSize), Button));
		TileSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
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
		if (!PartCategoryFilter.IsEmpty() && Part->Category.ToString() != PartCategoryFilter)
		{
			continue;
		}
		UWidget* Inside = nullptr;
		if (UTexture2D* Thumbnail = Part->Thumbnail.LoadSynchronous())
		{
			UImage* Image = MakeRoundImage(WidgetTree, Thumbnail);
			Inside = MakeBox(WidgetTree, FVector2D(TileSize - 8.f, TileSize - 8.f), Image);
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

	// Nuancier de la pièce portée (pas pour les têtes : leur couleur, c'est la peau ; ni pour les motifs d'iris : couleur des yeux)
	if (WornDef && TabName != TEXT("Face") && (Shown->MaskParameter.IsNone() || !WornDef->MaskColors.IsEmpty()))
	{
		RefreshColorPanel(SlotName, WornDef);
	}
	return true;
}

void UCharacterCreatorWidget::RefreshColorPanel(FName SlotName, const FTrainerPartDef* Part)
{
	ColorBox->ClearChildren();
	ColorBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	const FTrainerPartSlotDef* SlotDef = Data->FindSlot(SlotName);

	// Zones : couleur principale, plus les zones du masque que le matériau de la pièce utilise vraiment.
	// Maquillage : une zone par canal utilisé du masque (pas de couleur principale).
	TArray<int32> Zones;
	if (Part->MaskColors.IsEmpty())
	{
		Zones.Add(0);
	}
	for (int32 Channel = 0; Channel < Part->MaskColors.Num() && Channel < 3; ++Channel)
	{
		if (Part->MaskColors[Channel].A > 0.f)
		{
			Zones.Add(Channel + 1);
		}
	}
	if (Zones.IsEmpty())
	{
		ColorBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	for (const TSoftObjectPtr<UMaterialInterface>& Soft : Part->Materials)
	{
		if (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Soft.LoadSynchronous()))
		{
			for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
			{
				for (int32 Zone = 1; Zone <= 3; ++Zone)
				{
					if (Value.ParameterInfo.Name == TrainerPartColorParameter(Zone))
					{
						Zones.AddUnique(Zone);
					}
				}
			}
		}
	}
	Zones.Sort();
	if (!Zones.Contains(ColorZone))
	{
		ColorZone = Zones[0];
	}

	auto ZoneColor = [this, SlotName, Part](int32 Zone)
	{
		const FTrainerPartColor* Found = Current.FindPartColor(SlotName, Zone);
		if (Found)
		{
			return Found->Color;
		}
		return Part->MaskColors.IsValidIndex(Zone - 1) ? Part->MaskColors[Zone - 1] : FLinearColor::White;
	};

	// En-tête : « Couleur » et Réinitialiser
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(MakeText(WidgetTree, LOCTEXT("ColorTitle", "Couleur"), 22.f, PokemonFonts::EStyle::Bold));
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	UCreatorButton* Reset = MakeTextButton(WidgetTree, LOCTEXT("ColorReset", "Réinitialiser"), 18.f, [this, SlotName]()
	{
		FTrainerAppearance New = Current;
		New.ClearPartColors(SlotName);
		ChangeAppearance(New, TEXT("Color"), false);
		RefreshRightPanel();
	}, nullptr, ButtonFill, 16.f);
	Header->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(150.f, 34.f), Reset));
	ColorBox->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Zones (Couleur principale, Couleur 1...) quand il y en a plusieurs
	if (Zones.Num() > 1)
	{
		UHorizontalBox* ZoneRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (const int32 Zone : Zones)
		{
			const bool bSelected = Zone == ColorZone;
			UHorizontalBox* Inside = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UHorizontalBoxSlot* DotSlot = Inside->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(18.f, 18.f), MakeRounded(WidgetTree, ZoneColor(Zone), 9.f, FLinearColor::White, 1.f)));
			DotSlot->SetVerticalAlignment(VAlign_Center);
			DotSlot->SetPadding(FMargin(8.f, 0.f, 6.f, 0.f));
			const FText ZoneName = SlotDef ? SlotDef->GetZoneLabel(Zone)
				: (Zone == 0 ? LOCTEXT("ZoneMain", "Principale") : FText::Format(LOCTEXT("ZoneN", "Zone {0}"), Zone));
			UTextBlock* Label = MakeText(WidgetTree, ZoneName, 18.f,
				PokemonFonts::EStyle::Regular, bSelected ? DarkText : TextColor);
			if (bSelected)
			{
				Label->SetShadowColorAndOpacity(FLinearColor::Transparent);
			}
			UHorizontalBoxSlot* LabelSlot = Inside->AddChildToHorizontalBox(Label);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f)); // le mot ne touche plus le bord de la pastille
			UCreatorButton* Button = MakeButton(WidgetTree, Inside, bSelected ? SelectedFill : ButtonFill, 16.f, [this, Zone]() { ColorZone = Zone; RefreshRightPanel(); });
			ZoneRow->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(0.f, 34.f), Button))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		ColorBox->AddChildToVerticalBox(ZoneRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	const FLinearColor Selected = ZoneColor(ColorZone);
	auto AddColorRow = [this, SlotName, Selected](const TArray<FLinearColor>& Colors, float Size)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (const FLinearColor& Color : Colors)
		{
			const bool bSelected = SameColor(Color, Selected);
			UCreatorButton* Button = MakeButton(WidgetTree, nullptr, Color, Size * 0.5f, [this, SlotName, Color]() { SetPartColor(SlotName, ColorZone, Color); },
				bSelected ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.35f), bSelected ? 3.f : 1.f);
			Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(Size, Size), Button))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		ColorBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	};

	// Rangée « Modèles » puis nuancier : 2 rangées de teintes (claires, foncées) + gris
	AddColorRow(TArray<FLinearColor>(ClothTemplates, UE_ARRAY_COUNT(ClothTemplates)), 40.f);
	TArray<FLinearColor> Light, Dark, Greys;
	for (int32 Index = 0; Index < 11; ++Index)
	{
		const float Hue = 360.f * Index / 11.f;
		Light.Add(FLinearColor::MakeFromHSV8(uint8(Hue / 360.f * 255.f), 150, 230));
		Dark.Add(FLinearColor::MakeFromHSV8(uint8(Hue / 360.f * 255.f), 200, 110));
		Greys.Add(FLinearColor::MakeFromHSV8(0, 0, uint8(255.f * Index / 10.f)));
	}
	AddColorRow(Light, 32.f);
	AddColorRow(Dark, 32.f);
	AddColorRow(Greys, 32.f);

	// Roue de couleur (teinte et saturation) + luminosité, en plus des couleurs prédéfinies, comme inZOI
	{
		const int32 Zone = ColorZone;
		const FLinearColor StartHSV = Selected.LinearRGBToHSV();
		UHorizontalBox* WheelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCreatorColorWheel* Wheel = WidgetTree->ConstructWidget<UCreatorColorWheel>(UCreatorColorWheel::StaticClass());
		Wheel->SetHSV(StartHSV);
		UCreatorSlider* Bright = WidgetTree->ConstructWidget<UCreatorSlider>(UCreatorSlider::StaticClass());
		Bright->SetMinValue(0.f);
		Bright->SetMaxValue(1.f);
		Bright->SetStepSize(0.01f);
		Bright->SetIndentHandle(false);
		Bright->SetSliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.45f));
		Bright->SetSliderHandleColor(FLinearColor::White);
		Bright->SetValue(StartHSV.B);
		TWeakObjectPtr<UCreatorColorWheel> WeakWheel(Wheel);
		TWeakObjectPtr<UCreatorSlider> WeakBright(Bright);
		// Couleur choisie appliquée en direct ; le panneau (pastilles, code) n'est refait qu'au relâchement
		auto Apply = [this, SlotName, Zone](const FLinearColor& HSV, bool bFinished)
		{
			FLinearColor Color = FLinearColor(HSV.R, HSV.G, HSV.B, 1.f).HSVToLinearRGB();
			if (const FTrainerPartColor* Previous = Current.FindPartColor(SlotName, Zone))
			{
				Color.A = Previous->Color.A;
			}
			else
			{
				Color.A = 1.f;
			}
			if (bFinished)
			{
				SetPartColor(SlotName, Zone, Color);
				return;
			}
			FTrainerAppearance New = Current;
			New.SetPartColor(SlotName, Zone, Color);
			ChangeAppearance(New, TEXT("ColorWheel"));
			if (HexBox)
			{
				HexBox->SetText(FText::FromString(Color.ToFColorSRGB().ToHex().Left(6)));
			}
		};
		Wheel->Setup([Apply, WeakBright](const FLinearColor& HSV, bool bFinished)
		{
			const float Value = WeakBright.IsValid() ? WeakBright->GetValue() : HSV.B;
			Apply(FLinearColor(HSV.R, HSV.G, FMath::Max(Value, 0.02f), 1.f), bFinished);
		});
		Bright->Setup([Apply, WeakWheel](float Value)
		{
			FLinearColor HSV = WeakWheel.IsValid() ? WeakWheel->GetHSV() : FLinearColor(0.f, 0.f, Value, 1.f);
			HSV.B = Value;
			if (WeakWheel.IsValid())
			{
				WeakWheel->SetHSV(HSV);
			}
			Apply(HSV, false);
		});
		WheelRow->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(150.f, 150.f), Wheel))->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		UVerticalBox* BrightBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		BrightBox->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Brightness", "Luminosité"), 18.f, PokemonFonts::EStyle::Regular, DimColor));
		BrightBox->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(220.f, 40.f), Bright));
		UHorizontalBoxSlot* BrightSlot = WheelRow->AddChildToHorizontalBox(BrightBox);
		BrightSlot->SetVerticalAlignment(VAlign_Center);
		ColorBox->AddChildToVerticalBox(WheelRow)->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
	}

	// Code hexadécimal, comme le champ « 403C5F » d'inZOI
	UHorizontalBox* HexRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HexRow->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(40.f, 32.f), MakeRounded(WidgetTree, Selected, 8.f, FLinearColor::White, 1.f)))->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	UBorder* Field = MakeRounded(WidgetTree, FieldColor, 16.f, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f);
	Field->SetPadding(FMargin(12.f, 2.f));
	Field->SetVerticalAlignment(VAlign_Center);
	HexBox = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
	HexBox->SetFont(PokemonFonts::Get(20.f * FontScale));
	HexBox->SetText(FText::FromString(Selected.ToFColorSRGB().ToHex().Left(6)));
	HexBox->OnTextCommitted.AddUniqueDynamic(this, &UCharacterCreatorWidget::HandleHexCommitted);
	Field->SetContent(HexBox);
	HexRow->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(140.f, 32.f), Field));
	ColorBox->AddChildToVerticalBox(HexRow);
	ColorSlot = SlotName;

	// Réglage de la zone (alpha de sa couleur) : début du dégradé, quantité de mèches, intensité du maquillage
	if (SlotDef && SlotDef->ZoneAmountLabels.IsValidIndex(ColorZone) && !SlotDef->ZoneAmountLabels[ColorZone].IsEmpty())
	{
		const int32 Zone = ColorZone;
		const float Amount = Current.FindPartColor(SlotName, Zone) ? Selected.A : DefaultZoneAmount(SlotName, Part, Zone);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Label = MakeText(WidgetTree, SlotDef->ZoneAmountLabels[Zone], 20.f);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(190.f, 0.f), Label));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		UCreatorSlider* Slider = WidgetTree->ConstructWidget<UCreatorSlider>(UCreatorSlider::StaticClass());
		Slider->SetMinValue(0.f);
		Slider->SetMaxValue(1.f);
		Slider->SetStepSize(0.01f);
		Slider->SetIndentHandle(false);
		Slider->SetSliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.45f));
		Slider->SetSliderHandleColor(FLinearColor::White);
		Slider->SetValue(Amount);
		Slider->ValueText = MakeText(WidgetTree, FText::AsPercent(Amount), 18.f, PokemonFonts::EStyle::Regular, DimColor);
		Slider->ValueText->SetJustification(ETextJustify::Right);
		TWeakObjectPtr<UTextBlock> ValueText = Slider->ValueText.Get();
		Slider->Setup([this, SlotName, Zone, ValueText, ZoneColor](float Value)
		{
			// Pas de RefreshRightPanel ici : il referait le curseur pendant qu'on le glisse
			FTrainerAppearance New = Current;
			FLinearColor Color = ZoneColor(Zone);
			Color.A = Value;
			New.SetPartColor(SlotName, Zone, Color);
			ChangeAppearance(New, TEXT("ColorAmount"));
			if (ValueText.IsValid())
			{
				ValueText->SetText(FText::AsPercent(Value));
			}
		});
		UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
		Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(64.f, 0.f), Slider->ValueText))->SetVerticalAlignment(VAlign_Center);
		ColorBox->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(0.f, 40.f), Row))->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	// Motif ou image sur les vêtements (matériau d'inZOI), comme le « Craft » d'inZOI : motifs du jeu + images du
	// joueur déposées dans Saved/Motifs
	if (SlotDef && SlotDef->Tab == TEXT("Outfit"))
	{
		TArray<FName> Choices;
		for (const FTrainerPatternDef& Def : Data->Patterns)
		{
			Choices.Add(Def.Id);
		}
		Choices.Append(TrainerAppearanceRender::ListUserPatterns());
		const int32 Channel = FMath::Max(ColorZone - 1, 0);
		const FTrainerPartPattern* Worn = Current.FindPartPattern(SlotName, Channel);
		ColorBox->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("PatternTitle", "Motif ou image"), 20.f))->SetPadding(FMargin(0.f, 12.f, 0.f, 6.f));
		UWrapBox* PatternWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		PatternWrap->SetInnerSlotPadding(FVector2D(8.f, 8.f));
		auto Choose = [this, SlotName, Channel](FName Pattern)
		{
			FTrainerAppearance New = Current;
			FTrainerPartPattern Value;
			Value.Scale = 4.f; // motif répété : à 1, une rayure couvrait la moitié du t-shirt
			if (const FTrainerPartPattern* Previous = Current.FindPartPattern(SlotName, Channel))
			{
				Value = *Previous;
			}
			Value.Slot = SlotName;
			Value.Channel = Channel;
			Value.Pattern = Pattern;
			New.SetPartPattern(Value);
			ChangeAppearance(New, TEXT("Pattern"));
			RefreshRightPanel();
		};
		UTextBlock* NoneText = MakeText(WidgetTree, LOCTEXT("PatternNone", "Aucun"), 16.f, PokemonFonts::EStyle::Regular, DarkText);
		NoneText->SetJustification(ETextJustify::Center);
		PatternWrap->AddChildToWrapBox(MakeBox(WidgetTree, FVector2D(56.f, 56.f), MakeButton(WidgetTree, NoneText, Worn ? ButtonFill : SelectedFill, 10.f,
			[Choose]() { Choose(NAME_None); })));
		for (const FName Id : Choices)
		{
			UImage* Thumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (UTexture2D* Texture = TrainerAppearanceRender::ResolvePattern(Data, Id))
			{
				Thumb->SetBrushFromTexture(Texture);
			}
			Thumb->SetToolTipText(FText::FromName(Id));
			const bool bSelected = Worn && Worn->Pattern == Id;
			PatternWrap->AddChildToWrapBox(MakeBox(WidgetTree, FVector2D(56.f, 56.f), MakeButton(WidgetTree, Thumb, ButtonFill, 10.f,
				[Choose, Id]() { Choose(Id); }, bSelected ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.3f), bSelected ? 3.f : 1.f)));
		}
		ColorBox->AddChildToVerticalBox(PatternWrap);
		ColorBox->AddChildToVerticalBox(MakeText(WidgetTree, FText::Format(LOCTEXT("PatternHint", "Vos images (.png, .jpg) : {0}"),
			FText::FromString(FPaths::ConvertRelativePathToFull(TrainerAppearanceRender::UserPatternFolder()))), 14.f, PokemonFonts::EStyle::Regular, DimColor))
			->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
		if (Worn)
		{
			// Taille et intensité du motif
			const struct { FText Label; float Min, Max, Value; int32 Field; } Rows[] = {
				{ LOCTEXT("PatternScale", "Taille"), 0.2f, 6.f, Worn->Scale, 0 },
				{ LOCTEXT("PatternIntensity", "Intensité"), 0.f, 1.f, Worn->Intensity, 1 } };
			for (const auto& RowDef : Rows)
			{
				UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				Row->AddChildToHorizontalBox(MakeBox(WidgetTree, FVector2D(120.f, 0.f), MakeText(WidgetTree, RowDef.Label, 18.f)))->SetVerticalAlignment(VAlign_Center);
				UCreatorSlider* Slider = WidgetTree->ConstructWidget<UCreatorSlider>(UCreatorSlider::StaticClass());
				Slider->SetMinValue(RowDef.Min);
				Slider->SetMaxValue(RowDef.Max);
				Slider->SetStepSize(0.01f);
				Slider->SetIndentHandle(false);
				Slider->SetSliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.45f));
				Slider->SetSliderHandleColor(FLinearColor::White);
				Slider->SetValue(RowDef.Value);
				const int32 PatternField = RowDef.Field;
				Slider->Setup([this, SlotName, Channel, PatternField](float Value)
				{
					const FTrainerPartPattern* Previous = Current.FindPartPattern(SlotName, Channel);
					if (!Previous)
					{
						return;
					}
					FTrainerPartPattern Pattern = *Previous;
					(PatternField == 0 ? Pattern.Scale : Pattern.Intensity) = Value;
					FTrainerAppearance New = Current;
					New.SetPartPattern(Pattern);
					ChangeAppearance(New, TEXT("PatternAmount"));
				});
				UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
				SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				SliderSlot->SetVerticalAlignment(VAlign_Center);
				ColorBox->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(0.f, 36.f), Row));
			}
		}
	}
}

float UCharacterCreatorWidget::DefaultZoneAmount(FName SlotName, const FTrainerPartDef* Part, int32 Zone) const
{
	if (Part && Part->MaskColors.IsValidIndex(Zone - 1))
	{
		return Part->MaskColors[Zone - 1].A;
	}
	const FTrainerPartSlotDef* SlotDef = Data ? Data->FindSlot(SlotName) : nullptr;
	return SlotDef ? SlotDef->GetZoneDefaultAmount(Zone) : 1.f;
}

void UCharacterCreatorWidget::HandleHexCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	FString Hex = Text.ToString().TrimStartAndEnd();
	Hex.RemoveFromStart(TEXT("#"));
	if (Hex.Len() != 6 || ColorSlot.IsNone())
	{
		return;
	}
	for (const TCHAR Char : Hex)
	{
		if (!FChar::IsHexDigit(Char))
		{
			return;
		}
	}
	SetPartColor(ColorSlot, ColorZone, FLinearColor::FromSRGBColor(FColor::FromHex(Hex)));
}

void UCharacterCreatorWidget::SetPartColor(FName SlotName, int32 Zone, const FLinearColor& Color)
{
	// L'alpha est le réglage de la zone (début du dégradé, quantité de mèches, intensité) : on garde celui déjà choisi
	float Amount = 1.f;
	if (const FTrainerPartColor* Existing = Current.FindPartColor(SlotName, Zone))
	{
		Amount = Existing->Color.A;
	}
	else
	{
		const FTrainerBaseDef* Base = Data ? Data->FindBase(Current.Base) : nullptr;
		const TMap<FName, const FTrainerPartDef*> Worn = Base ? Data->ResolveParts(*Base, Current) : TMap<FName, const FTrainerPartDef*>();
		const FTrainerPartDef* const* Part = Worn.Find(SlotName);
		Amount = DefaultZoneAmount(SlotName, Part ? *Part : nullptr, Zone);
	}
	FTrainerAppearance New = Current;
	New.SetPartColor(SlotName, Zone, FLinearColor(Color.R, Color.G, Color.B, Amount));
	ChangeAppearance(New, TEXT("Color"), false);
	RefreshRightPanel();
}

void UCharacterCreatorWidget::BuildWornColumn()
{
	// Colonne de gauche d'inZOI : les pièces portées en vignettes rondes, un clic ouvre leur onglet
	WornList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WornList"));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollbarThickness(FVector2D(3.f, 3.f));
	Scroll->AddChild(WornList);
	PokemonGlass::FGlassLook Look;
	Look.CornerRadius = 44.f;
	WornColumn = PokemonGlass::Make(WidgetTree, Scroll, FMargin(10.f, 16.f), Look, TEXT("WornColumn"));
	Place(Canvas, WornColumn, FVector2D(40.f, 150.f), FVector2D(108.f, 760.f));
}

void UCharacterCreatorWidget::RefreshWornColumn()
{
	if (!WornList || !Data)
	{
		return;
	}
	WornList->ClearChildren();
	const FTrainerBaseDef* Base = Data->FindBase(Current.Base);
	if (!Base)
	{
		return;
	}
	const TMap<FName, const FTrainerPartDef*> Worn = Data->ResolveParts(*Base, Current);
	for (const FTrainerPartSlotDef& SlotDef : Data->PartSlots)
	{
		const FTrainerPartDef* const* Found = Worn.Find(SlotDef.Slot);
		if (!Found || !*Found)
		{
			continue;
		}
		const FTrainerPartDef* Part = *Found;
		UWidget* Inside = nullptr;
		if (UTexture2D* Thumbnail = Part->Thumbnail.LoadSynchronous())
		{
			UImage* Image = MakeRoundImage(WidgetTree, Thumbnail);
			Inside = MakeBox(WidgetTree, FVector2D(76.f, 76.f), Image);
		}
		else
		{
			UTextBlock* Text = MakeText(WidgetTree, SlotDef.Label.IsEmpty() ? FText::FromName(SlotDef.Slot) : SlotDef.Label, 15.f);
			Text->SetJustification(ETextJustify::Center);
			Text->SetAutoWrapText(true);
			Inside = Text;
		}
		const FName SlotName = SlotDef.Slot;
		const FName SlotTab = SlotDef.Tab;
		UCreatorButton* Button = MakeButton(WidgetTree, Inside, ButtonFill, 42.f, [this, SlotName, SlotTab]()
		{
			if (SlotTab == TEXT("Face"))
			{
				FacePart = EFacePart::Presets;
				SelectTab(ETab::Face);
			}
			else if (SlotTab == TEXT("Hair") || SlotTab == TEXT("Makeup") || SlotTab == TEXT("Eyes"))
			{
				FacePart = SlotTab == TEXT("Hair") ? EFacePart::Hair : (SlotTab == TEXT("Makeup") ? EFacePart::Makeup : EFacePart::Eyes);
				SelectTab(ETab::Face);
			}
			else
			{
				SelectTab(SlotTab == TEXT("Accessories") ? ETab::Accessories : ETab::Outfit);
			}
			PartSlotFilter = SlotName;
			RefreshRightPanel();
		}, FLinearColor(1.f, 1.f, 1.f, 0.45f), 1.f);
		Button->SetToolTipText(SlotDef.Label);
		WornList->AddChildToVerticalBox(MakeBox(WidgetTree, FVector2D(84.f, 84.f), Button))->SetPadding(FMargin(2.f, 0.f, 2.f, 10.f));
	}
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

	ProfileGroup->SetVisibility(Tab == ETab::Presets ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	WornColumn->SetVisibility(Tab == ETab::Presets ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	RefreshWornColumn();

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
	if (Tab != NewTab)
	{
		PartSlotFilter = NAME_None;
		PartCategoryFilter.Reset();
		ColorZone = 0;
	}
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
		SelectTab(ETab::Presets); // le prénom est dans le Profil, onglet Modèles
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
	// Clic droit maintenu : tourner le personnage (comme glisser dans le vide au clic gauche)
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		LastMouse = ToCanvas(InMouseEvent.GetScreenSpacePosition());
		DragTarget = -2;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
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
	const bool bButton = InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
		|| (DragTarget == -2 && InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton));
	if (DragTarget == -1 || !bButton)
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
	if (DragTarget != -1 && (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton))
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
