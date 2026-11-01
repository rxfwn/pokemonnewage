#include "UI/Battle/BattleHUDWidget.h"
#include "UI/GameFonts.h"
#include "UI/GlassPanel.h"
#include "Battle/BattleComponent.h"
#include "Battle/BattleText.h"
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
#include "Components/ScrollBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/PokemonSaveGame.h"
#include "Pokemon/PokemonDatabase.h"
#include "Pokemon/PokemonDataRows.h"
#include "Engine/Texture2D.h"
#include "Pokemon/PokemonDataSubsystem.h"
#include "PokemonNewAgePlayerController.h"
#include "Progression/ProgressionComponent.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "BattleHUD"

namespace BattleHUD
{
	// Maquette Combat_Hud de L'amie (1920 x 1080, CSS Figma du 2026-11-01) : positions en pixels de la maquette,
	// polices en px Figma (x 0,75 en UMG). Images faites par Tools/HUD/make_battle_assets.py.
	constexpr float FontScale = 0.75f;
	const FVector2D DesignSize(1920.f, 1080.f);
	const FVector2D InfoSize(565.86f, 162.f);
	const FVector2D EnemyBoxPos(49.14f, 21.f);
	const FVector2D PlayerBoxPos(1274.14f, 626.f);
	/** Dans un cadre : barre de PV en (88, 111), 451,72 x 26,85 ; « PV » en (23, 98) ; nom en (23, 2) ; niveau à 26 px du bord droit */
	const FVector2D HPBarPos(87.85f, 110.98f);
	const FVector2D HPBarSize(451.72f, 26.85f);
	const FVector2D MessagePos(47.f, 817.f);
	const FVector2D MessageSize(1210.f, 207.f);
	const FVector2D MessageTextPos(82.f, 836.f);
	const FVector2D MenuPos(1274.f, 817.f);
	const FVector2D MenuSize(566.f, 207.f);
	/** Cartes 265 x 85 espacées de 12 px (1286 -> 1563, 830 -> 927) */
	const FVector2D CardSize(265.f, 85.f);
	constexpr float CardGap = 12.f;
	constexpr float MainCardsTop = 830.f;
	constexpr float MoveCardsTop = 826.f;
	constexpr float CardsLeft = 1286.f;
	/** Bouton retour 54 x 54 entre les attaques (centre en 1554, 916) */
	const FVector2D BackPos(1527.f, 889.f);
	const FVector2D BackSize(54.f, 54.f);

	const FLinearColor Text(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.55f);
	/** « PV » orange #FFA412 */
	const FLinearColor HPLabel = FLinearColor::FromSRGBColor(FColor(0xFF, 0xA4, 0x12));
	const FLinearColor Good(0.3f, 0.85f, 0.4f, 1.f);
	const FLinearColor Bad(0.95f, 0.25f, 0.2f, 1.f);
	const FLinearColor Exp(0.25f, 0.6f, 1.f, 1.f);
	const FLinearColor LevelTag = FLinearColor::FromSRGBColor(FColor(0x8B, 0x8B, 0x8B));

	/** Étiquettes de type de la planche Types de L'amie ; les types qu'elle n'a pas dessinés suivent le même style (foncés) */
	FLinearColor TypeColor(EPokemonType Type)
	{
		auto C = [](uint8 R, uint8 G, uint8 B, float A = 1.f)
		{
			FLinearColor Color = FLinearColor::FromSRGBColor(FColor(R, G, B));
			Color.A = A;
			return Color;
		};
		switch (Type)
		{
		case EPokemonType::Electric: return C(150, 102, 0, 0.8f);
		case EPokemonType::Normal:   return C(0x8B, 0x8B, 0x8B);
		case EPokemonType::Water:    return C(0x00, 0x20, 0x3F);
		case EPokemonType::Fire:     return C(0x2F, 0x00, 0x00);
		case EPokemonType::Grass:    return C(0x01, 0x33, 0x04);
		case EPokemonType::Fighting: return C(0x2C, 0x18, 0x01);
		case EPokemonType::Psychic:  return C(0x15, 0x00, 0x38);
		case EPokemonType::Ice:      return C(0x00, 0x3A, 0x40);
		case EPokemonType::Poison:   return C(0x2A, 0x00, 0x33);
		case EPokemonType::Ground:   return C(0x3A, 0x2A, 0x05);
		case EPokemonType::Flying:   return C(0x1A, 0x23, 0x40);
		case EPokemonType::Bug:      return C(0x23, 0x33, 0x00);
		case EPokemonType::Rock:     return C(0x3A, 0x32, 0x20);
		case EPokemonType::Ghost:    return C(0x1A, 0x10, 0x33);
		case EPokemonType::Dragon:   return C(0x0F, 0x00, 0x50);
		case EPokemonType::Dark:     return C(0x1A, 0x1A, 0x1A);
		case EPokemonType::Steel:    return C(0x2F, 0x3A, 0x40);
		case EPokemonType::Fairy:    return C(0x4A, 0x10, 0x30);
		default:                     return C(0x8B, 0x8B, 0x8B);
		}
	}

	/** Barre de PV : verte au-dessus de 50 %, orange au-dessus de 20 %, rouge en dessous (planche Life_Bar) */
	int32 HPColorIndex(float Ratio)
	{
		return Ratio > 0.5f ? 0 : (Ratio > 0.2f ? 1 : 2);
	}

	UTexture2D* LoadHUDTexture(const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/HUD/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Image de la maquette (dessinée au double) affichée à la taille de la maquette ; bBox : étirée sans déformer les bouts ronds */
	FSlateBrush MakeBrush(UTexture2D* Texture, const FVector2D& Size, bool bBox = false)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		if (bBox)
		{
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.Margin = FMargin(13.f / 452.f, 0.f);
		}
		return Brush;
	}

	PokemonGlass::FGlassLook PanelLook(float Radius = 20.f)
	{
		// Rectangle 5 : linear-gradient(87.81deg, rgba(217,217,217,0.3834) 0.4%, rgba(255,255,255,0.0923) 64.91%)
		PokemonGlass::FGlassLook Look;
		Look.CornerRadius = Radius;
		Look.GradientAngle = 87.81f;
		Look.GradientStart = 0.004f;
		Look.GradientEnd = 0.6491f;
		Look.FillOpacityStart = 0.3834f / 0.71f;
		Look.FillOpacityEnd = 0.0923f / 0.71f;
		return Look;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		return CanvasSlot;
	}

	FText StatusText(EMajorStatus Status)
	{
		switch (Status)
		{
		case EMajorStatus::Paralysis:     return LOCTEXT("PAR", "PAR");
		case EMajorStatus::Sleep:         return LOCTEXT("SLP", "SOM");
		case EMajorStatus::Freeze:        return LOCTEXT("FRZ", "GEL");
		case EMajorStatus::Burn:          return LOCTEXT("BRN", "BRU");
		case EMajorStatus::Poison:
		case EMajorStatus::BadlyPoisoned: return LOCTEXT("PSN", "PSN");
		default:                          return FText::GetEmpty();
		}
	}

	/** Événements qui changent les PV du Pokémon concerné (HPAfter est alors valable) */
	bool ChangesHP(EBattleEventType Type)
	{
		switch (Type)
		{
		case EBattleEventType::Damage:
		case EBattleEventType::Heal:
		case EBattleEventType::Recoil:
		case EBattleEventType::StatusDamage:
		case EBattleEventType::HurtByConfusion:
		case EBattleEventType::LeechSeedDrain:
		case EBattleEventType::TrapDamage:
			return true;
		default:
			return false;
		}
	}
}

using namespace BattleHUD;

void UBattleHUDButtonBinding::HandleClicked()
{
	if (UBattleHUDWidget* Owner = HUD.Get())
	{
		Owner->OnButton(Action, Index);
	}
}

// ---------------------------------------------------------------- Construction

TSharedRef<SWidget> UBattleHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		HPFillTextures[0] = LoadHUDTexture(TEXT("T_Battle_HPFill_Green"));
		HPFillTextures[1] = LoadHUDTexture(TEXT("T_Battle_HPFill_Orange"));
		HPFillTextures[2] = LoadHUDTexture(TEXT("T_Battle_HPFill_Red"));
		HPTrackTexture = LoadHUDTexture(TEXT("T_Battle_HPTrack"));
		ButtonTextures[0] = LoadHUDTexture(TEXT("T_Battle_Button_Fight"));
		ButtonTextures[1] = LoadHUDTexture(TEXT("T_Battle_Button_Bag"));
		ButtonTextures[2] = LoadHUDTexture(TEXT("T_Battle_Button_Pokemon"));
		ButtonTextures[3] = LoadHUDTexture(TEXT("T_Battle_Button_Run"));
		BackTexture = LoadHUDTexture(TEXT("T_Battle_Back"));

		// Toute la maquette en 1920 x 1080, mise à l'échelle de la fenêtre
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("Scale"));
		Scale->SetStretch(EStretch::ScaleToFit);
		UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(Scale);
		ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
		ScaleSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* Design = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Design->SetWidthOverride(DesignSize.X);
		Design->SetHeightOverride(DesignSize.Y);
		Scale->SetContent(Design);
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		Design->SetContent(Canvas);

		// Cadre de l'adversaire en haut à gauche, celui du joueur à droite au-dessus du menu
		BuildInfoBox(Boxes[1], false);
		Place(Canvas, Boxes[1].Frame, EnemyBoxPos, InfoSize);
		BuildInfoBox(Boxes[0], true);
		Place(Canvas, Boxes[0].Frame, PlayerBoxPos, InfoSize);

		// Boîte de texte en bas à gauche
		Place(Canvas, PokemonGlass::Make(WidgetTree, nullptr, FMargin(0.f), PanelLook(), TEXT("MessageFrame")), MessagePos, MessageSize);
		MessageText = MakeText(FText::GetEmpty(), 64.f * FontScale, Text);
		MessageText->SetAutoWrapText(true);
		Place(Canvas, MessageText, MessageTextPos, FVector2D(MessagePos.X + MessageSize.X - MessageTextPos.X - 70.f, MessageSize.Y - 30.f));
		// Flèche « suite » : chevron orange tourné vers le bas, dans le coin
		MessageArrow = MakeText(FText::FromString(TEXT(">")), 40.f * FontScale, HPLabel, true);
		MessageArrow->SetJustification(ETextJustify::Center);
		MessageArrow->SetRenderTransformAngle(90.f);
		MessageArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Place(Canvas, MessageArrow, FVector2D(MessagePos.X + MessageSize.X - 64.f, MessagePos.Y + MessageSize.Y - 66.f), FVector2D(40.f, 48.f));

		// Menu en bas à droite : panneau en verre, cartes dans une liste (2 colonnes) qui défile si besoin
		MenuFrame = PokemonGlass::Make(WidgetTree, nullptr, FMargin(0.f), PanelLook(), TEXT("MenuFrame"));
		Place(Canvas, MenuFrame, MenuPos, MenuSize);
		MenuScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MenuScroll"));
		MenuScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		MenuScroll->SetAnimateWheelScrolling(true);
		MenuGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("MenuGrid"));
		MenuGrid->SetSlotPadding(FMargin(CardGap * 0.5f));
		MenuScroll->AddChild(MenuGrid);
		MenuScrollSlot = Place(Canvas, MenuScroll, FVector2D(CardsLeft - CardGap * 0.5f, MainCardsTop - CardGap * 0.5f),
			FVector2D(2.f * (CardSize.X + CardGap), MenuPos.Y + MenuSize.Y - (MoveCardsTop - CardGap * 0.5f) - 4.f));

		// Bouton retour (Rectangle 24 + picto Calque_1)
		UOverlay* BackContent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UOverlaySlot* BackGlassSlot = BackContent->AddChildToOverlay(PokemonGlass::MakeImage(WidgetTree, PanelLook(10.f)));
		BackGlassSlot->SetHorizontalAlignment(HAlign_Fill);
		BackGlassSlot->SetVerticalAlignment(VAlign_Fill);
		UImage* BackIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (BackTexture)
		{
			BackIcon->SetBrush(MakeBrush(BackTexture, FVector2D(31.f, 27.f)));
		}
		else
		{
			BackIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
		UOverlaySlot* IconSlot = BackContent->AddChildToOverlay(BackIcon);
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		BackButton = MakeCardButton(BackContent);
		UBattleHUDButtonBinding* BackBinding = NewObject<UBattleHUDButtonBinding>(this);
		BackBinding->Action = EBattleHUDAction::Back;
		BackBinding->HUD = this;
		BackButton->OnClicked.AddDynamic(BackBinding, &UBattleHUDButtonBinding::HandleClicked);
		BackBindingObject = BackBinding;
		Place(Canvas, BackButton, BackPos, BackSize);

		MenuFrame->SetVisibility(ESlateVisibility::Collapsed);
		MenuScroll->SetVisibility(ESlateVisibility::Collapsed);
		BackButton->SetVisibility(ESlateVisibility::Collapsed);

		// Stats gagnées à la montée de niveau : verre à gauche, au-dessus de la boîte de texte
		USizeBox* StatsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StatsSize->SetWidthOverride(420.f);
		StatsGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		StatsGrid->SetSlotPadding(FMargin(8.f, 2.f));
		StatsSize->SetContent(StatsGrid);
		StatsFrame = PokemonGlass::Make(WidgetTree, StatsSize, FMargin(28.f, 18.f), PanelLook(), TEXT("StatsFrame"));
		UCanvasPanelSlot* StatsSlot = Canvas->AddChildToCanvas(StatsFrame);
		StatsSlot->SetAutoSize(true);
		StatsSlot->SetAlignment(FVector2D(0.f, 1.f));
		StatsSlot->SetPosition(FVector2D(MessagePos.X, MessagePos.Y - 20.f));
		StatsFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetIsFocusable(true);
	return Super::RebuildWidget();
}

void UBattleHUDWidget::BuildInfoBox(FInfoBox& Box, bool bPlayer)
{
	// Cadre en verre 565,86 x 162 : nom (64 px), niveau « N.32 » (48 px) à droite, « PV » orange et barre en dégradé
	UCanvasPanel* Inside = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Box.Frame = PokemonGlass::Make(WidgetTree, Inside, FMargin(0.f), PanelLook());
	Box.Frame->SetVisibility(ESlateVisibility::Hidden);

	Box.Name = MakeText(FText::GetEmpty(), 64.f * FontScale, Text);
	Place(Inside, Box.Name, FVector2D(23.f, 2.f), FVector2D(360.f, 77.f));

	Box.Level = MakeText(FText::GetEmpty(), 48.f * FontScale, Text);
	Box.Level->SetJustification(ETextJustify::Right);
	Place(Inside, Box.Level, FVector2D(InfoSize.X - 26.f - 150.f, 18.f), FVector2D(150.f, 57.f));

	// Statut (PAR, SOM...) à gauche du niveau
	Box.Status = MakeText(FText::GetEmpty(), 32.f * FontScale, Bad, true);
	Box.Status->SetJustification(ETextJustify::Right);
	Place(Inside, Box.Status, FVector2D(InfoSize.X - 26.f - 150.f - 110.f, 30.f), FVector2D(100.f, 40.f));

	UTextBlock* HP = MakeText(LOCTEXT("HP", "PV"), 40.f * FontScale, HPLabel, true);
	HP->SetJustification(ETextJustify::Center);
	Place(Inside, HP, FVector2D(23.f, 98.f), FVector2D(38.74f, 51.f));

	UImage* Track = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (HPTrackTexture)
	{
		Track->SetBrush(MakeBrush(HPTrackTexture, HPBarSize, true));
	}
	else
	{
		Track->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f), 13.f));
	}
	Place(Inside, Track, HPBarPos, HPBarSize);

	Box.HPFill = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Box.HPFillSlot = Place(Inside, Box.HPFill, HPBarPos, HPBarSize);
	Box.HPColor = -1;

	if (bPlayer)
	{
		// Barre d'XP fine sous les PV (pas dans la maquette, mais utile à la montée de niveau)
		Box.ExpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle ExpStyle = Box.ExpBar->GetWidgetStyle();
		ExpStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f), 2.f));
		ExpStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 2.f));
		Box.ExpBar->SetWidgetStyle(ExpStyle);
		Box.ExpBar->SetFillColorAndOpacity(Exp);
		Place(Inside, Box.ExpBar, FVector2D(HPBarPos.X, HPBarPos.Y + HPBarSize.Y + 8.f), FVector2D(HPBarSize.X, 5.f));
	}
}

void UBattleHUDWidget::SetHPBar(FInfoBox& Box, float Ratio)
{
	if (!Box.HPFill || !Box.HPFillSlot)
	{
		return;
	}
	const int32 ColorIndex = HPColorIndex(Ratio);
	if (ColorIndex != Box.HPColor)
	{
		Box.HPColor = ColorIndex;
		if (UTexture2D* Fill = HPFillTextures[ColorIndex])
		{
			Box.HPFill->SetBrush(MakeBrush(Fill, HPBarSize, true));
		}
		else
		{
			const FLinearColor Colors[] = { Good, FLinearColor(0.95f, 0.55f, 0.15f), Bad };
			Box.HPFill->SetBrush(FSlateRoundedBoxBrush(Colors[ColorIndex], 13.f));
		}
	}
	// La barre se raccourcit par la droite ; les bouts restent ronds (image en « boîte »)
	const float Width = HPBarSize.X * FMath::Clamp(Ratio, 0.f, 1.f);
	Box.HPFill->SetVisibility(Width > 0.5f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Box.HPFillSlot->SetSize(FVector2D(FMath::Max(Width, HPBarSize.Y), HPBarSize.Y));
}

UTextBlock* UBattleHUDWidget::MakeText(const FText& InText, float Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(InText);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetFont(PokemonFonts::Get(Size, bBold));
	// Texte posé sur le verre (maquette Combat_Hud) : ombre légère
	PokemonGlass::AddTextShadow(Block);
	return Block;
}

UButton* UBattleHUDWidget::MakeCardButton(UWidget* Content)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	// Le fond est dans le contenu (image ou verre) : au survol la carte grossit un peu (NativeTick), sans bloc en plus
	Style.SetNormal(FSlateNoResource());
	Style.SetHovered(FSlateNoResource());
	Style.SetPressed(FSlateNoResource());
	Style.SetDisabled(FSlateNoResource());
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
	Button->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	return Button;
}

const UPokemonDatabase* UBattleHUDWidget::GetDatabase() const
{
	return UPokemonDataSubsystem::GetDatabase(this);
}

// ---------------------------------------------------------------- Combat

void UBattleHUDWidget::Begin(UBattleComponent* InBattle, const FBattleSnapshot& InSnapshot)
{
	Battle = InBattle;
	Snapshot = InSnapshot;
	PreviousSnapshot = InSnapshot;

	// Vitesse du texte (Options)
	const int32 Speed = UPokemonSaveLibrary::GetOptions()->TextSpeed;
	CharsPerSecond = Speed == 0 ? 30.f : (Speed == 2 ? 140.f : 60.f);
	HoldTime = Speed == 0 ? 1.3f : (Speed == 2 ? 0.45f : 0.85f);

	Battle->OnBattleEvents.AddDynamic(this, &UBattleHUDWidget::HandleEvents);
	Battle->OnBattleEnded.AddDynamic(this, &UBattleHUDWidget::HandleEnded);
	Battle->OnActionRefused.AddDynamic(this, &UBattleHUDWidget::HandleRefused);
}

void UBattleHUDWidget::HandleEvents(const TArray<FBattleEvent>& Events, const FBattleSnapshot& InSnapshot)
{
	if (Queue.Num() == 0 && !bMessageActive)
	{
		PreviousSnapshot = Snapshot;
	}
	Snapshot = InSnapshot;
	Queue.Append(Events);
	bWaitingForServer = false;
	ClearMenu();
	if (!bMessageActive)
	{
		StartNextEvent();
	}
}

void UBattleHUDWidget::HandleEnded(const FBattleResult& InResult)
{
	bEnded = true;
	Result = InResult;
	if (Queue.Num() == 0 && !bMessageActive)
	{
		ShowMenuForPhase();
	}
}

void UBattleHUDWidget::HandleRefused(EBattleActionError Error)
{
	bWaitingForServer = false;
	ShowMessage(UBattleText::GetActionErrorText(Error));
}

void UBattleHUDWidget::StartNextEvent()
{
	StatsFrame->SetVisibility(ESlateVisibility::Collapsed);
	while (Queue.Num() > 0)
	{
		const FBattleEvent Event = Queue[0];
		Queue.RemoveAt(0);
		ApplyEventVisuals(Event);
		const FText EventText = UBattleText::GetEventText(this, Snapshot, Event);
		if (!EventText.IsEmpty())
		{
			ShowMessage(EventText);
			return;
		}
	}

	// Tous les messages sont passés : l'état final du serveur fait foi
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Snapshot.Sides.IsValidIndex(Side) && ShownIndex[Side] >= 0)
		{
			ShownIndex[Side] = Snapshot.Sides[Side].ActiveIndex;
			RefreshInfoBox(Side, false);
		}
	}
	ShowMenuForPhase();
}

void UBattleHUDWidget::ApplyEventVisuals(const FBattleEvent& Event)
{
	const int32 Side = Event.Side;
	if (Side < 0 || Side > 1)
	{
		return;
	}
	switch (Event.Type)
	{
	case EBattleEventType::SentOut:
	{
		// PV au moment où il sort : ceux d'avant ce lot d'événements
		ShownIndex[Side] = Event.PartyIndex;
		const FPokemonInstance* Before = PreviousSnapshot.Sides.IsValidIndex(Side) && PreviousSnapshot.Sides[Side].Party.IsValidIndex(Event.PartyIndex)
			? &PreviousSnapshot.Sides[Side].Party[Event.PartyIndex] : nullptr;
		ShownLevel[Side] = 0;
		RefreshInfoBox(Side, true);
		if (Before && Before->Species == Event.Species)
		{
			DisplayedHP[Side] = TargetHP[Side] = Before->CurrentHP;
			if (Side == 0)
			{
				DisplayedExp = TargetExp = Before->Experience;
			}
			ShownLevel[Side] = Before->Level;
			Boxes[Side].Level->SetText(FText::Format(LOCTEXT("Level", "N.{0}"), Before->Level));
		}
		Boxes[Side].Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		break;
	}
	case EBattleEventType::Withdrew:
	case EBattleEventType::ForcedOut:
		Boxes[Side].Frame->SetVisibility(ESlateVisibility::Hidden);
		break;
	case EBattleEventType::Fainted:
		TargetHP[Side] = 0.f;
		break;
	case EBattleEventType::ExpGained:
		if (Side == 0 && Event.PartyIndex == ShownIndex[Side])
		{
			TargetExp += Event.Amount;
		}
		break;
	case EBattleEventType::LevelUp:
		if (Event.PartyIndex == ShownIndex[Side])
		{
			ShownLevel[Side] = Event.Amount;
			Boxes[Side].Level->SetText(FText::Format(LOCTEXT("Level", "N.{0}"), Event.Amount));
		}
		if (Side == Snapshot.LocalSide)
		{
			ShowLevelUpStats(Event);
		}
		break;
	case EBattleEventType::Caught:
		Boxes[1].Frame->SetVisibility(ESlateVisibility::Hidden);
		break;
	default:
		if (ChangesHP(Event.Type) && Event.PartyIndex == ShownIndex[Side])
		{
			TargetHP[Side] = Event.HPAfter;
		}
		break;
	}
}

const FPokemonInstance* UBattleHUDWidget::GetDisplayedPokemon(int32 Side) const
{
	if (!Snapshot.Sides.IsValidIndex(Side) || !Snapshot.Sides[Side].Party.IsValidIndex(ShownIndex[Side]))
	{
		return nullptr;
	}
	return &Snapshot.Sides[Side].Party[ShownIndex[Side]];
}

void UBattleHUDWidget::RefreshInfoBox(int32 Side, bool bInstantHP)
{
	const FPokemonInstance* Pokemon = GetDisplayedPokemon(Side);
	if (!Pokemon)
	{
		return;
	}
	FInfoBox& Box = Boxes[Side];
	const UPokemonDatabase* Database = GetDatabase();
	Box.Name->SetText(UPokemonLibrary::GetDisplayName(Database, *Pokemon));
	const int32 Level = ShownLevel[Side] > 0 && bInstantHP ? ShownLevel[Side] : Pokemon->Level;
	ShownLevel[Side] = Level;
	Box.Level->SetText(FText::Format(LOCTEXT("Level", "N.{0}"), Level));
	Box.Status->SetText(Pokemon->IsFainted() ? LOCTEXT("KO", "K.O.") : StatusText(Pokemon->Status));
	MaxHP[Side] = FMath::Max(1, Pokemon->GetMaxHP());
	TargetHP[Side] = Pokemon->CurrentHP;
	if (bInstantHP)
	{
		DisplayedHP[Side] = TargetHP[Side];
	}

	if (Side == 0)
	{
		// L'état final du serveur fait foi ; la barre y va en glissant (UpdateExpBar)
		TargetExp = Pokemon->Experience;
		if (bInstantHP)
		{
			DisplayedExp = TargetExp;
		}
		UpdateExpBar(0.f);
	}
}

void UBattleHUDWidget::UpdateExpBar(float DeltaTime)
{
	const FPokemonInstance* Pokemon = GetDisplayedPokemon(0);
	const UPokemonDatabase* Database = GetDatabase();
	const FSpeciesRow* Species = Pokemon && Database ? Database->FindSpecies(Pokemon->Species) : nullptr;
	if (!Boxes[0].ExpBar || !Species)
	{
		return;
	}
	const int32 Level = FMath::Clamp(ShownLevel[0] > 0 ? ShownLevel[0] : Pokemon->Level, 1, 100);
	const float ThisLevel = Database->GetExperienceForLevel(Species->GrowthRate, Level);
	const float NextLevel = Database->GetExperienceForLevel(Species->GrowthRate, FMath::Min(Level + 1, 100));
	const float Range = FMath::Max(NextLevel - ThisLevel, 1.f);
	// Une barre pleine se remplit en un peu plus d'une seconde ; elle s'arrête au bout tant que le niveau n'est pas annoncé
	const float Stop = Level < 100 ? FMath::Min(TargetExp, NextLevel) : TargetExp;
	if (DisplayedExp < Stop)
	{
		DisplayedExp = FMath::Min(DisplayedExp + Range * 0.8f * DeltaTime, Stop);
	}
	else if (DisplayedExp > TargetExp)
	{
		DisplayedExp = TargetExp;
	}
	Boxes[0].ExpBar->SetPercent(Level >= 100 ? 1.f : FMath::Clamp((DisplayedExp - ThisLevel) / Range, 0.f, 1.f));
}

bool UBattleHUDWidget::IsExpBarMoving() const
{
	const FPokemonInstance* Pokemon = GetDisplayedPokemon(0);
	const UPokemonDatabase* Database = GetDatabase();
	const FSpeciesRow* Species = Pokemon && Database ? Database->FindSpecies(Pokemon->Species) : nullptr;
	if (!Species || ShownLevel[0] >= 100)
	{
		return false;
	}
	const float NextLevel = Database->GetExperienceForLevel(Species->GrowthRate, FMath::Clamp(ShownLevel[0] + 1, 2, 100));
	return DisplayedExp + 0.5f < FMath::Min(TargetExp, NextLevel);
}

void UBattleHUDWidget::ShowLevelUpStats(const FBattleEvent& Event)
{
	UGameplayStatics::PlaySound2D(this, UProgressionComponent::LoadSound(TEXT("SW_LevelUp")));
	StatsGrid->ClearChildren();
	const FText Names[] = {
		LOCTEXT("StatHP", "PV max."), LOCTEXT("StatAtk", "Attaque"), LOCTEXT("StatDef", "Défense"),
		LOCTEXT("StatSpA", "Atq. Spé."), LOCTEXT("StatSpD", "Déf. Spé."), LOCTEXT("StatSpe", "Vitesse")
	};
	const FPokemonBaseStats& A = Event.StatsBefore;
	const FPokemonBaseStats& B = Event.StatsAfter;
	const int32 Before[] = { A.HP, A.Attack, A.Defense, A.SpecialAttack, A.SpecialDefense, A.Speed };
	const int32 After[] = { B.HP, B.Attack, B.Defense, B.SpecialAttack, B.SpecialDefense, B.Speed };
	for (int32 Row = 0; Row < 6; ++Row)
	{
		StatsGrid->AddChildToUniformGrid(MakeText(Names[Row], 20, Text), Row, 0);
		UTextBlock* Total = MakeText(FText::AsNumber(After[Row]), 20, Text, true);
		StatsGrid->AddChildToUniformGrid(Total, Row, 1)->SetHorizontalAlignment(HAlign_Right);
		UTextBlock* Gain = MakeText(FText::FromString(FString::Printf(TEXT("+%d"), After[Row] - Before[Row])), 20, After[Row] > Before[Row] ? Good : Dim, true);
		StatsGrid->AddChildToUniformGrid(Gain, Row, 2)->SetHorizontalAlignment(HAlign_Right);
	}
	StatsFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------- Boîte de texte

void UBattleHUDWidget::ShowMessage(const FText& InText)
{
	FullMessage = InText.ToString();
	MessageChars = 0.f;
	MessageHold = 0.f;
	bMessageActive = true;
	MessageText->SetText(FText::GetEmpty());
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
}

bool UBattleHUDWidget::IsMessageBusy() const
{
	return bMessageActive;
}

void UBattleHUDWidget::AdvanceMessage()
{
	if (!bMessageActive)
	{
		return;
	}
	// Premier appui : tout le texte d'un coup ; deuxième : message suivant
	if (MessageChars < FullMessage.Len())
	{
		MessageChars = FullMessage.Len();
		return;
	}
	bMessageActive = false;
	StartNextEvent();
}

void UBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Machine à écrire, puis le message suivant tout seul après un temps (ou au clic)
	if (bMessageActive)
	{
		const int32 Before = FMath::FloorToInt(MessageChars);
		MessageChars = FMath::Min(MessageChars + CharsPerSecond * InDeltaTime, float(FullMessage.Len()));
		const int32 After = FMath::FloorToInt(MessageChars);
		if (After != Before || After == 0)
		{
			MessageText->SetText(FText::FromString(FullMessage.Left(After)));
		}
		const bool bBarsMoving = FMath::Abs(DisplayedHP[0] - TargetHP[0]) > 0.5f || FMath::Abs(DisplayedHP[1] - TargetHP[1]) > 0.5f || IsExpBarMoving();
		if (After >= FullMessage.Len() && !bBarsMoving)
		{
			MessageArrow->SetVisibility(Queue.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			MessageHold += InDeltaTime;
			if (MessageHold >= HoldTime)
			{
				AdvanceMessage();
			}
		}
	}

	// Barres de PV : une barre pleine se vide en une seconde environ
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Speed = FMath::Max(MaxHP[Side] * 0.9f, 20.f);
		DisplayedHP[Side] = FMath::FInterpConstantTo(DisplayedHP[Side], TargetHP[Side], InDeltaTime, Speed);
		SetHPBar(Boxes[Side], FMath::Clamp(DisplayedHP[Side] / MaxHP[Side], 0.f, 1.f));
	}

	// Cartes du menu et bouton retour : grossissent un peu au survol (ou au clavier / à la manette)
	TArray<UButton*, TInlineAllocator<8>> Animated;
	for (UButton* Button : MenuButtons)
	{
		Animated.Add(Button);
	}
	Animated.Add(BackButton);
	for (UButton* Button : Animated)
	{
		if (!Button)
		{
			continue;
		}
		const bool bHover = Button->GetIsEnabled() && (Button->IsHovered() || Button->HasKeyboardFocus());
		const float Current = Button->GetRenderTransform().Scale.X;
		const float Scale = FMath::FInterpTo(Current, bHover ? 1.05f : 1.f, InDeltaTime, 18.f);
		if (!FMath::IsNearlyEqual(Scale, Current, 0.0005f))
		{
			Button->SetRenderScale(FVector2D(Scale, Scale));
		}
	}

	UpdateExpBar(InDeltaTime);

	if (CloseTimer > 0.f)
	{
		CloseTimer -= InDeltaTime;
		if (CloseTimer <= 0.f)
		{
			FinishBattle();
		}
	}
}

// ---------------------------------------------------------------- Choix du joueur

void UBattleHUDWidget::ShowMenuForPhase()
{
	if (bWaitingForServer)
	{
		return;
	}

	// Fin du combat : on ferme ; les évolutions suivent (séquence 3D de UProgressionComponent)
	if (bEnded)
	{
		ClearMenu();
		if (CloseTimer < 0.f)
		{
			CloseTimer = 0.6f;
		}
		return;
	}

	const int32 Local = Snapshot.LocalSide;
	switch (Snapshot.Phase)
	{
	case EBattlePhase::WaitingForActions:
		ShowMainMenu();
		break;
	case EBattlePhase::WaitingForReplacement:
		if (Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].GetActive() && Snapshot.Sides[Local].GetActive()->IsFainted())
		{
			ShowParty(EPartyMode::Replacement);
		}
		break;
	case EBattlePhase::WaitingForMoveLearn:
		ShowForgetMove(Snapshot.PendingLearnPartyIndex, Snapshot.PendingLearnMove);
		break;
	default:
		break;
	}
}

void UBattleHUDWidget::ClearMenu()
{
	Menu = EMenu::None;
	MenuGrid->ClearChildren();
	Bindings.Reset();
	MenuButtons.Reset();
	FirstButton = nullptr;
	CardCount = 0;
	MenuFrame->SetVisibility(ESlateVisibility::Collapsed);
	MenuScroll->SetVisibility(ESlateVisibility::Collapsed);
	BackButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UBattleHUDWidget::OpenMenu(EMenu NewMenu, bool bBackButton)
{
	Menu = NewMenu;
	// Menu principal en y 830, attaques et listes en y 826 (comme la maquette)
	MenuScrollSlot->SetPosition(FVector2D(CardsLeft - CardGap * 0.5f, (NewMenu == EMenu::Main ? MainCardsTop : MoveCardsTop) - CardGap * 0.5f));
	MenuScroll->ScrollToStart();
	MenuFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MenuScroll->SetVisibility(ESlateVisibility::Visible);
	BackButton->SetVisibility(bBackButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (FirstButton)
	{
		FirstButton->SetKeyboardFocus();
	}
}

UButton* UBattleHUDWidget::AddCard(const FText& Title, EBattleHUDAction Action, int32 Index, int32 ButtonImage, bool bEnabled,
	const FText& Tag, const FLinearColor& TagColor, const FText& RightText)
{
	// Carte 265 x 85 : image en dégradé (Attaquer, Sac, Pokémon, Fuite) ou verre (attaques, objets, équipe)
	UOverlay* Content = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UImage* Background = nullptr;
	if (ButtonTextures[0] && ButtonImage >= 0 && ButtonImage < 4 && ButtonTextures[ButtonImage])
	{
		Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Background->SetBrush(MakeBrush(ButtonTextures[ButtonImage], CardSize));
	}
	else
	{
		Background = PokemonGlass::MakeImage(WidgetTree, PanelLook(10.f));
	}
	UOverlaySlot* BackgroundSlot = Content->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UCanvasPanel* Layout = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	UOverlaySlot* LayoutSlot = Content->AddChildToOverlay(Layout);
	LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
	LayoutSlot->SetVerticalAlignment(VAlign_Fill);

	const bool bMainButton = ButtonImage >= 0;
	// Boutons du menu : 36 px, texte en y +17 ; cartes : 32 px en haut, PP à droite (bord droit +236, y +37), type en (+21, +44)
	UTextBlock* TitleText = MakeText(Title, (bMainButton ? 36.f : 32.f) * FontScale, Text);
	TitleText->SetJustification(ETextJustify::Center);
	Place(Layout, TitleText, FVector2D(0.f, bMainButton ? 17.f : 0.f), FVector2D(CardSize.X, bMainButton ? 43.f : 38.f));

	if (!RightText.IsEmpty())
	{
		UTextBlock* Right = MakeText(RightText, 32.f * FontScale, Text);
		Right->SetJustification(ETextJustify::Right);
		Place(Layout, Right, FVector2D(236.f - 200.f, 37.f), FVector2D(200.f, 38.f));
	}
	if (!Tag.IsEmpty())
	{
		// Étiquette 90 x 29, coins 5, texte 16 px en majuscules (planche Types)
		UBorder* TagBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TagBox->SetBrush(FSlateRoundedBoxBrush(TagColor, 5.f));
		TagBox->SetPadding(FMargin(0.f));
		TagBox->SetHorizontalAlignment(HAlign_Center);
		TagBox->SetVerticalAlignment(VAlign_Center);
		TagBox->SetContent(MakeText(Tag.ToUpper(), 16.f * FontScale, Text));
		Place(Layout, TagBox, FVector2D(21.f, 44.f), FVector2D(90.f, 29.f));
	}

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(CardSize.X);
	Size->SetHeightOverride(CardSize.Y);
	Size->SetContent(Content);
	UButton* Button = MakeCardButton(Size);
	Button->SetIsEnabled(bEnabled);
	if (!bEnabled)
	{
		Button->SetRenderOpacity(0.4f);
	}

	UBattleHUDButtonBinding* Binding = NewObject<UBattleHUDButtonBinding>(this);
	Binding->Action = Action;
	Binding->Index = Index;
	Binding->HUD = this;
	Button->OnClicked.AddDynamic(Binding, &UBattleHUDButtonBinding::HandleClicked);
	Bindings.Add(Binding);

	UUniformGridSlot* GridSlot = MenuGrid->AddChildToUniformGrid(Button, CardCount / 2, CardCount % 2);
	GridSlot->SetHorizontalAlignment(HAlign_Fill);
	GridSlot->SetVerticalAlignment(VAlign_Fill);
	++CardCount;
	MenuButtons.Add(Button);
	if (!FirstButton && bEnabled)
	{
		FirstButton = Button;
	}
	return Button;
}

FText UBattleHUDWidget::GetTypeName(EPokemonType Type) const
{
	const UPokemonDatabase* Database = GetDatabase();
	const FName RowName(*StaticEnum<EPokemonType>()->GetNameStringByValue(int64(Type)));
	if (Database && Database->TypeChart)
	{
		if (const FTypeChartRow* Row = Database->TypeChart->FindRow<FTypeChartRow>(RowName, TEXT("BattleHUD"), false))
		{
			return Row->DisplayName;
		}
	}
	return FText::FromName(RowName);
}

void UBattleHUDWidget::ShowMainMenu()
{
	ClearMenu();
	const FPokemonInstance* Active = GetDisplayedPokemon(Snapshot.LocalSide);
	MessageText->SetText(Active ? FText::Format(LOCTEXT("WhatWillDo", "Que doit faire {0} ?"), UPokemonLibrary::GetDisplayName(GetDatabase(), *Active)) : FText::GetEmpty());
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);

	// Attaquer (rouge), Sac (jaune), Pokémon (vert), Fuite (bleu)
	AddCard(LOCTEXT("Fight", "Attaquer"), EBattleHUDAction::Fight, 0, 0);
	AddCard(LOCTEXT("Bag", "Sac"), EBattleHUDAction::Bag, 1, 1);
	AddCard(LOCTEXT("Pokemon", "Pokémon"), EBattleHUDAction::Pokemon, 2, 2);
	AddCard(LOCTEXT("Run", "Fuite"), EBattleHUDAction::Run, 3, 3, Snapshot.BattleType == EBattleType::Wild);
	OpenMenu(EMenu::Main, false);
}

void UBattleHUDWidget::ShowMoves()
{
	ClearMenu();
	const FPokemonInstance* Active = GetDisplayedPokemon(Snapshot.LocalSide);
	const UPokemonDatabase* Database = GetDatabase();
	if (!Active)
	{
		ShowMainMenu();
		return;
	}

	// 4 cartes en verre : nom, type, PP ; bouton retour au milieu
	for (int32 MoveIndex = 0; MoveIndex < 4; ++MoveIndex)
	{
		if (!Active->Moves.IsValidIndex(MoveIndex) || Active->Moves[MoveIndex].Move.IsNone())
		{
			AddCard(FText::FromString(TEXT("-")), EBattleHUDAction::Move, MoveIndex, -1, false);
			continue;
		}
		const FPokemonMoveSlot& MoveSlot = Active->Moves[MoveIndex];
		const FMoveRow* Move = Database ? Database->FindMove(MoveSlot.Move) : nullptr;
		AddCard(Move ? Move->DisplayName : FText::FromName(MoveSlot.Move), EBattleHUDAction::Move, MoveIndex, -1, MoveSlot.CurrentPP > 0,
			Move ? GetTypeName(Move->Type) : FText::GetEmpty(), Move ? TypeColor(Move->Type) : LevelTag,
			FText::FromString(FString::Printf(TEXT("PP %d/%d"), MoveSlot.CurrentPP, MoveSlot.MaxPP)));
	}
	OpenMenu(EMenu::Moves, true);
}

void UBattleHUDWidget::ShowBag()
{
	ClearMenu();
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	int32 Count = 0;
	if (Snapshot.Sides.IsValidIndex(Local) && Database)
	{
		for (int32 Index = 0; Index < Snapshot.Sides[Local].Inventory.Num(); ++Index)
		{
			const FItemStack& Stack = Snapshot.Sides[Local].Inventory[Index];
			const FItemRow* Item = Database->FindItem(Stack.Item);
			if (!Item || !Item->bUsableInBattle || Stack.Count <= 0)
			{
				continue;
			}
			AddCard(Item->DisplayName, EBattleHUDAction::Item, Index, -1, true, FText::GetEmpty(), LevelTag,
				FText::Format(LOCTEXT("ItemCount", "x{0}"), Stack.Count));
			++Count;
		}
	}
	MessageText->SetText(Count == 0 ? LOCTEXT("NoItems", "Aucun objet utilisable.") : LOCTEXT("WhichItem", "Quel objet utiliser ?"));
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);
	OpenMenu(EMenu::Bag, true);
}

void UBattleHUDWidget::ShowParty(EPartyMode Mode)
{
	ClearMenu();
	PartyMode = Mode;
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	if (!Snapshot.Sides.IsValidIndex(Local))
	{
		return;
	}
	const FBattleSide& Side = Snapshot.Sides[Local];

	MessageText->SetText(Mode == EPartyMode::Replacement ? LOCTEXT("SendWho", "Quel Pokémon envoyer ?")
		: (Mode == EPartyMode::ItemTarget ? LOCTEXT("UseOnWho", "Sur quel Pokémon ?") : LOCTEXT("SwitchWho", "Quel Pokémon envoyer au combat ?")));
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);

	// Une carte par Pokémon : nom, niveau (étiquette grise), PV (ou K.O. / statut)
	for (int32 Index = 0; Index < Side.Party.Num(); ++Index)
	{
		const FPokemonInstance& Pokemon = Side.Party[Index];
		const bool bActive = Index == Side.ActiveIndex;
		const bool bEnabled = Mode == EPartyMode::ItemTarget || (!Pokemon.IsFainted() && !bActive);
		const FText Status = Pokemon.IsFainted() ? LOCTEXT("KO", "K.O.") : StatusText(Pokemon.Status);
		const FText HP = FText::FromString(FString::Printf(TEXT("PV %d/%d"), Pokemon.CurrentHP, Pokemon.GetMaxHP()));
		AddCard(UPokemonLibrary::GetDisplayName(Database, Pokemon), EBattleHUDAction::PartyPick, Index, -1, bEnabled,
			FText::Format(LOCTEXT("PartyLevel", "N.{0}"), Pokemon.Level), Status.IsEmpty() ? LevelTag : Bad,
			Status.IsEmpty() ? HP : FText::Format(LOCTEXT("PartyHPStatus", "{0}  {1}"), Status, HP));
	}
	if (Mode == EPartyMode::Replacement && Snapshot.BattleType == EBattleType::Wild)
	{
		AddCard(LOCTEXT("RunInstead", "Fuir"), EBattleHUDAction::Run, 0, 3);
	}
	OpenMenu(EMenu::Party, Mode != EPartyMode::Replacement);
}

void UBattleHUDWidget::ShowForgetMove(int32 PartyIndex, FName NewMove)
{
	ClearMenu();
	CloseTimer = -1.f;
	PartyMode = EPartyMode::Switch;
	const UPokemonDatabase* Database = GetDatabase();
	const int32 Local = Snapshot.LocalSide;
	const FPokemonInstance* Pokemon = Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].Party.IsValidIndex(PartyIndex)
		? &Snapshot.Sides[Local].Party[PartyIndex] : nullptr;
	const FMoveRow* Move = Database ? Database->FindMove(NewMove) : nullptr;
	const FText MoveName = Move ? Move->DisplayName : FText::FromName(NewMove);

	MessageText->SetText(FText::Format(LOCTEXT("ForgetWhich", "{0} veut apprendre {1}, mais connaît déjà 4 capacités. Laquelle oublier ?"),
		Pokemon ? UPokemonLibrary::GetDisplayName(Database, *Pokemon) : FText::GetEmpty(), MoveName));
	MessageArrow->SetVisibility(ESlateVisibility::Hidden);

	for (int32 MoveIndex = 0; Pokemon && MoveIndex < Pokemon->Moves.Num(); ++MoveIndex)
	{
		const FPokemonMoveSlot& MoveSlot = Pokemon->Moves[MoveIndex];
		const FMoveRow* Known = Database ? Database->FindMove(MoveSlot.Move) : nullptr;
		AddCard(Known ? Known->DisplayName : FText::FromName(MoveSlot.Move), EBattleHUDAction::ForgetMove, MoveIndex, -1, true,
			Known ? GetTypeName(Known->Type) : FText::GetEmpty(), Known ? TypeColor(Known->Type) : LevelTag,
			FText::FromString(FString::Printf(TEXT("PP %d/%d"), MoveSlot.CurrentPP, MoveSlot.MaxPP)));
	}
	AddCard(LOCTEXT("DontLearn", "Ne pas apprendre"), EBattleHUDAction::KeepMoves, 0, -1, true,
		Move ? GetTypeName(Move->Type) : FText::GetEmpty(), Move ? TypeColor(Move->Type) : LevelTag, MoveName);
	OpenMenu(EMenu::ForgetMove, false);
}

void UBattleHUDWidget::OnButton(EBattleHUDAction Action, int32 Index)
{
	if (!Battle)
	{
		return;
	}
	const int32 Local = Snapshot.LocalSide;

	switch (Action)
	{
	case EBattleHUDAction::Fight:
		ShowMoves();
		break;
	case EBattleHUDAction::Bag:
		ShowBag();
		break;
	case EBattleHUDAction::Pokemon:
		ShowParty(EPartyMode::Switch);
		break;
	case EBattleHUDAction::Run:
		if (Menu == EMenu::Party && PartyMode == EPartyMode::Replacement)
		{
			ClearMenu();
			bWaitingForServer = true;
			Battle->RunInsteadOfReplacement();
		}
		else
		{
			Submit(FBattleAction::MakeRun());
		}
		break;
	case EBattleHUDAction::Move:
		Submit(FBattleAction::MakeMove(Index));
		break;
	case EBattleHUDAction::Item:
	{
		const UPokemonDatabase* Database = GetDatabase();
		const FItemStack* Stack = Snapshot.Sides.IsValidIndex(Local) && Snapshot.Sides[Local].Inventory.IsValidIndex(Index)
			? &Snapshot.Sides[Local].Inventory[Index] : nullptr;
		const FItemRow* Item = Stack && Database ? Database->FindItem(Stack->Item) : nullptr;
		if (!Item)
		{
			break;
		}
		if (Item->IsPokeBall())
		{
			Submit(FBattleAction::MakeItem(Stack->Item));
		}
		else
		{
			PendingItem = Stack->Item;
			ShowParty(EPartyMode::ItemTarget);
		}
		break;
	}
	case EBattleHUDAction::PartyPick:
		if (PartyMode == EPartyMode::Replacement)
		{
			ClearMenu();
			bWaitingForServer = true;
			Battle->ChooseReplacement(Index);
		}
		else if (PartyMode == EPartyMode::ItemTarget)
		{
			Submit(FBattleAction::MakeItem(PendingItem, Index));
		}
		else
		{
			Submit(FBattleAction::MakeSwitch(Index));
		}
		break;
	case EBattleHUDAction::ForgetMove:
	case EBattleHUDAction::KeepMoves:
	{
		const int32 ForgetSlot = Action == EBattleHUDAction::ForgetMove ? Index : -1;
		ClearMenu();
		bWaitingForServer = true;
		Battle->ResolveMoveLearn(ForgetSlot);
		break;
	}
	case EBattleHUDAction::Back:
		if (Menu == EMenu::Party && PartyMode == EPartyMode::ItemTarget)
		{
			ShowBag();
		}
		else
		{
			ShowMainMenu();
		}
		break;
	}
}

void UBattleHUDWidget::Submit(const FBattleAction& Action)
{
	ClearMenu();
	bWaitingForServer = true;
	MessageText->SetText(LOCTEXT("Waiting", "..."));
	Battle->SubmitAction(Action);
}

FReply UBattleHUDWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bConfirm = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom;
	const bool bBack = Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;

	if (bMessageActive && (bConfirm || bBack))
	{
		AdvanceMessage();
		return FReply::Handled();
	}
	if (bBack)
	{
		if (Menu == EMenu::Moves || Menu == EMenu::Bag || (Menu == EMenu::Party && PartyMode != EPartyMode::Replacement))
		{
			OnButton(EBattleHUDAction::Back, 0);
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UBattleHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Clic n'importe où : message suivant
	if (bMessageActive)
	{
		AdvanceMessage();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBattleHUDWidget::FinishBattle()
{
	if (Battle)
	{
		Battle->OnBattleEvents.RemoveDynamic(this, &UBattleHUDWidget::HandleEvents);
		Battle->OnBattleEnded.RemoveDynamic(this, &UBattleHUDWidget::HandleEnded);
		Battle->OnActionRefused.RemoveDynamic(this, &UBattleHUDWidget::HandleRefused);
	}
	RemoveFromParent();
	if (APokemonNewAgePlayerController* Controller = Cast<APokemonNewAgePlayerController>(GetOwningPlayer()))
	{
		Controller->OnBattleHUDClosed();
	}
}

#undef LOCTEXT_NAMESPACE
