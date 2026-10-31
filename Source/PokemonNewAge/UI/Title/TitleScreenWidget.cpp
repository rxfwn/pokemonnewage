#include "UI/Title/TitleScreenWidget.h"
#include "UI/Title/TitleScreenSettings.h"
#include "UI/Title/TitleScreenSubsystem.h"
#include "Core/PokemonSaveGame.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "TitleScreen"

namespace TitleScreenUI
{
	/** Couleurs de la maquette : texte blanc, ombre noire à 60 % décalée de (3, 4) */
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.72f);
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.6f);
	const FVector2D ShadowOffset(3.f, 4.f);
	const FLinearColor PanelColor(0.03f, 0.03f, 0.04f, 0.82f);

	/**
	 *  Les tailles de la maquette sont en pixels ; Unreal compte les polices en points (1 pt = 4/3 px) :
	 *  32 px dans Figma = 24 dans Unreal.
	 */
	float Px(float Pixels)
	{
		return Pixels * 0.75f;
	}

	/** Italique « fait main » quand la police n'a pas de version italique */
	constexpr float SyntheticItalic = 0.18f;

	const UFontFace* LoadFace(const TCHAR* Name)
	{
		return LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/UI/HUD/Fonts/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FSlateFontInfo FromFace(const UFontFace* Face, float Size)
	{
		TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
		FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TEXT("Regular");
		Entry.Font = FFontData(Face);
		return FSlateFontInfo(Composite, Size);
	}

	/**
	 *  Tout l'écran de titre est en Futura Bold penchée (choix de L'amie du 2026-10-31, à la place de
	 *  Futura Md BT italique). On lit d'abord le .ttf du dossier Tools (il a les accents ajoutés par
	 *  Tools/HUD/add_futura_accents.py, même si l'asset importé est plus ancien), puis l'asset, puis Roboto.
	 *  bBold et bItalic sont gardés pour pouvoir varier plus tard.
	 */
	FSlateFontInfo Futura(float Size, bool /*bBold*/, bool /*bItalic*/, const FSlateFontInfo& Fallback)
	{
		FSlateFontInfo Font;
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/HUD/Assets/fonts/FF_Futura_Bold.ttf"));
		if (FPaths::FileExists(File))
		{
			Font = FSlateFontInfo(File, Size);
		}
		else if (const UFontFace* Face = LoadFace(TEXT("FF_Futura_Bold")))
		{
			Font = FromFace(Face, Size);
		}
		else
		{
			Font = Fallback;
			Font.Size = Size;
			Font.TypefaceFontName = FName(TEXT("Bold"));
		}
		Font.SkewAmount = SyntheticItalic;
		return Font;
	}

	/**
	 *  Charge une image : l'asset s'il a été importé (Tools/Title/import_title_unreal.py), sinon directement le PNG
	 *  du même nom dans Tools/Title/Assets (pas besoin de lancer le script pour la voir).
	 */
	UTexture2D* TryLoad(const TSoftObjectPtr<UTexture2D>& Soft)
	{
		const FSoftObjectPath& Path = Soft.ToSoftObjectPath();
		if (Path.IsNull())
		{
			return nullptr;
		}
		if (FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			if (UTexture2D* Texture = Soft.LoadSynchronous())
			{
				return Texture;
			}
		}
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Title/Assets") / (Path.GetAssetName() + TEXT(".png")));
		if (FPaths::FileExists(File))
		{
			return FImageUtils::ImportFileAsTexture2D(File);
		}
		return nullptr;
	}

	bool IsBackKey(const FKey& Key)
	{
		return Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Position, const FVector2D& Alignment)
	{
		UCanvasPanelSlot* WidgetSlot = Canvas->AddChildToCanvas(Widget);
		WidgetSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		WidgetSlot->SetAlignment(Alignment);
		WidgetSlot->SetPosition(Position);
		WidgetSlot->SetAutoSize(true);
		return WidgetSlot;
	}

	void FillCanvas(UCanvasPanel* Canvas, UWidget* Widget)
	{
		UCanvasPanelSlot* WidgetSlot = Canvas->AddChildToCanvas(Widget);
		WidgetSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WidgetSlot->SetOffsets(FMargin(0.f));
	}
}

using namespace TitleScreenUI;

void UTitleScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

UTexture2D* UTitleScreenWidget::MakeVignette()
{
	constexpr int32 Size = 128;
	UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Size * Size);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const float U = (X + 0.5f) / Size;
			const float V = (Y + 0.5f) / Size * 2.f - 1.f;
			// Côté gauche (menu) et coins assombris
			const float Left = FMath::SmoothStep(0.55f, 0.f, U) * 0.55f;
			const float Edge = FMath::SmoothStep(0.6f, 1.5f, FMath::Sqrt(FMath::Square(U * 2.f - 1.f) + V * V)) * 0.6f;
			const float Alpha = FMath::Clamp(FMath::Max(Left, Edge), 0.f, 0.75f);
			Pixels[Y * Size + X] = FLinearColor(0.f, 0.f, 0.f, Alpha).ToFColor(true);
		}
	}
	Texture->SRGB = true;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

UTitleScreenWidget::FEntry UTitleScreenWidget::MakeEntry(const FText& Label, float FontSize)
{
	FEntry Entry;
	UTextBlock* Probe = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());

	Entry.Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style;
	Style.SetNormal(FSlateNoResource());
	Style.SetHovered(FSlateNoResource());
	Style.SetPressed(FSlateNoResource());
	Style.SetDisabled(FSlateNoResource());
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f));
	Entry.Button->SetStyle(Style);

	Entry.Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Entry.Label->SetText(Label);
	Entry.Label->SetFont(Futura(FontSize, false, true, Probe->GetFont()));
	Entry.Label->SetColorAndOpacity(FSlateColor(White));
	Entry.Label->SetShadowOffset(ShadowOffset);
	Entry.Label->SetShadowColorAndOpacity(Shadow);
	Entry.Button->SetContent(Entry.Label);

	EntryWidgets.Append({ Entry.Button.Get(), Entry.Label.Get() });
	return Entry;
}

TSharedRef<SWidget> UTitleScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Canvas->SetVisibility(ESlateVisibility::Visible);
		WidgetTree->RootWidget = Canvas;
		UTextBlock* Probe = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const FSlateFontInfo Default = Probe->GetFont();
		auto MakeText = [this, &Default](const FText& Value, float Size, bool bBold, bool bItalic, const FLinearColor& Color)
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Text->SetText(Value);
			Text->SetFont(Futura(Size, bBold, bItalic, Default));
			Text->SetColorAndOpacity(FSlateColor(Color));
			Text->SetShadowOffset(ShadowOffset);
			Text->SetShadowColorAndOpacity(Shadow);
			return Text;
		};

		// ---- Fond : les images de la maquette (recadrées pour remplir l'écran), sinon vignette sur la vue 3D
		for (const TSoftObjectPtr<UTexture2D>& Soft : Settings->Backgrounds)
		{
			if (UTexture2D* Texture = TryLoad(Soft))
			{
				BackgroundTextures.Add(Texture);
			}
		}
		if (BackgroundTextures.Num() > 0)
		{
			UScaleBox* Cover = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
			Cover->SetStretch(EStretch::ScaleToFill);
			Cover->SetClipping(EWidgetClipping::ClipToBounds);
			Cover->SetVisibility(ESlateVisibility::HitTestInvisible);
			UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			Cover->SetContent(Layers);
			// Deux calques : l'image en cours, et la suivante qui apparaît en fondu par-dessus
			for (int32 Layer = 0; Layer < 2; ++Layer)
			{
				UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Image->SetBrushFromTexture(BackgroundTextures[Layer % BackgroundTextures.Num()], true);
				Image->SetRenderOpacity(Layer == 0 ? 1.f : 0.f);
				UOverlaySlot* ImageSlot = Layers->AddChildToOverlay(Image);
				ImageSlot->SetHorizontalAlignment(HAlign_Fill);
				ImageSlot->SetVerticalAlignment(VAlign_Fill);
				BackgroundImages.Add(Image);
			}
			BackgroundBlock = Layers;
			FillCanvas(Canvas, Cover);
		}
		else if ((VignetteTexture = MakeVignette()) != nullptr)
		{
			UImage* Vignette = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Vignette->SetBrushFromTexture(VignetteTexture, false);
			Vignette->SetVisibility(ESlateVisibility::HitTestInvisible);
			FillCanvas(Canvas, Vignette);
		}

		// ---- Logo en haut à gauche : (34, 34), 638 × 323, ombre portée noire à 60 %
		if (UTexture2D* Logo = TryLoad(Settings->Logo))
		{
			UOverlay* LogoLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			UImage* LogoShadow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			LogoShadow->SetBrushFromTexture(Logo, false);
			LogoShadow->SetColorAndOpacity(Shadow);
			LogoShadow->SetRenderTranslation(FVector2D(3.f, 3.f));
			LogoLayers->AddChildToOverlay(LogoShadow);
			UImage* LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			LogoImage->SetBrushFromTexture(Logo, false);
			LogoLayers->AddChildToOverlay(LogoImage);
			USizeBox* LogoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LogoBox->SetWidthOverride(638.f);
			LogoBox->SetHeightOverride(323.f);
			LogoBox->SetContent(LogoLayers);
			LogoBlock = LogoBox;
		}
		else
		{
			// En attendant le PNG du logo : le nom écrit en Futura, à la même place
			UVerticalBox* Name = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Name->AddChildToVerticalBox(MakeText(Settings->Title, Px(150.f), true, true, White));
			Name->AddChildToVerticalBox(MakeText(Settings->Subtitle, Px(62.f), true, true, White))
				->SetHorizontalAlignment(HAlign_Right);
			LogoBlock = Name;
		}
		LogoBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Canvas, LogoBlock, FVector2D(0.f, 0.f), FVector2D(34.f, 34.f), FVector2D(0.f, 0.f));

		// ---- Menu à gauche : x 160, une entrée tous les 73 px à partir de y 621 ; Futura Md BT italique 32 px
		UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ContinueEntry = MakeEntry(LOCTEXT("Continue", "Continuer"), Px(32.f));
		NewGameEntry = MakeEntry(LOCTEXT("NewGame", "Nouvelle partie"), Px(32.f));
		OptionsEntry = MakeEntry(LOCTEXT("Options", "Options"), Px(32.f));
		for (const FEntry* Entry : { &ContinueEntry, &NewGameEntry, &OptionsEntry })
		{
			USizeBox* Row = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Row->SetHeightOverride(73.f);
			if (USizeBoxSlot* RowSlot = Cast<USizeBoxSlot>(Row->SetContent(Entry->Button)))
			{
				RowSlot->SetVerticalAlignment(VAlign_Top);
				RowSlot->SetHorizontalAlignment(HAlign_Left);
			}
			Menu->AddChildToVerticalBox(Row);
			EntryWidgets.Add(Row);
		}
		ContinueEntry.Button->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnContinueClicked);
		NewGameEntry.Button->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnNewGameClicked);
		OptionsEntry.Button->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnOptionsClicked);
		MenuBlock = Menu;
		Place(Canvas, Menu, FVector2D(0.f, 0.f), FVector2D(160.f, 621.f), FVector2D(0.f, 0.f));

		// ---- Bas à droite : version et ©, alignés à droite à 88 px du bord (1832 sur 1920)
		UVerticalBox* Footer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Footer->AddChildToVerticalBox(MakeText(Settings->VersionText, Px(32.f), false, true, White))->SetHorizontalAlignment(HAlign_Right);
		// Les deux lignes se suivent à 31 px dans la maquette (hauteur de ligne 38 px)
		Footer->AddChildToVerticalBox(MakeText(Settings->CopyrightText, Px(32.f), false, true, White))
			->SetPadding(FMargin(0.f, -7.f, 0.f, 0.f));
		Cast<UVerticalBoxSlot>(Footer->GetSlots().Last())->SetHorizontalAlignment(HAlign_Right);
		Footer->SetVisibility(ESlateVisibility::HitTestInvisible);
		FooterBlock = Footer;
		Place(Canvas, Footer, FVector2D(1.f, 1.f), FVector2D(-88.f, -51.f), FVector2D(1.f, 1.f));

		// ---- Question avant d'effacer la partie (au centre)
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(FSlateRoundedBoxBrush(PanelColor, 12.f));
		Panel->SetPadding(FMargin(48.f, 36.f));
		UVerticalBox* Question = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Question);
		Question->AddChildToVerticalBox(MakeText(LOCTEXT("ConfirmTitle", "Commencer une nouvelle partie ?"), Px(36.f), true, true, White))
			->SetHorizontalAlignment(HAlign_Center);
		Question->AddChildToVerticalBox(MakeText(LOCTEXT("ConfirmBody", "La partie sauvegardée sur ce PC sera effacée pour de bon."), Px(24.f), false, false, Dim))
			->SetPadding(FMargin(0.f, 10.f, 0.f, 28.f));
		UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		NoEntry = MakeEntry(LOCTEXT("No", "Non, revenir"), Px(32.f));
		YesEntry = MakeEntry(LOCTEXT("Yes", "Oui, effacer"), Px(32.f));
		Answers->AddChildToHorizontalBox(NoEntry.Button)->SetPadding(FMargin(0.f, 0.f, 64.f, 0.f));
		Answers->AddChildToHorizontalBox(YesEntry.Button);
		NoEntry.Button->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnConfirmNo);
		YesEntry.Button->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnConfirmYes);
		Question->AddChildToVerticalBox(Answers)->SetHorizontalAlignment(HAlign_Center);
		ConfirmPanel = Panel;
		Place(Canvas, Panel, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));

		RefreshSave();
		// Le focus est donné une fois le widget à l'écran (UTitleScreenSubsystem::FocusTitle)
		SetStage(EStage::Menu, false);
	}
	return Super::RebuildWidget();
}

void UTitleScreenWidget::RefreshSave()
{
	if (ContinueEntry.Button)
	{
		// Pas de partie : Continuer disparaît et Nouvelle partie remonte à sa place
		const bool bHasSave = UPokemonSaveLibrary::HasSaveGame();
		if (UWidget* Row = ContinueEntry.Button->GetParent())
		{
			Row->SetVisibility(bHasSave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	Selected = 0;
	LastFocused = INDEX_NONE;
}

TArray<const UTitleScreenWidget::FEntry*> UTitleScreenWidget::VisibleEntries() const
{
	TArray<const FEntry*> Entries;
	if (Stage == EStage::Confirm)
	{
		Entries = { &NoEntry, &YesEntry };
	}
	else if (Stage == EStage::Menu)
	{
		for (const FEntry* Entry : { &ContinueEntry, &NewGameEntry, &OptionsEntry })
		{
			const UWidget* Row = Entry->Button ? Entry->Button->GetParent() : nullptr;
			if (Row && Row->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Entries.Add(Entry);
			}
		}
	}
	return Entries;
}

void UTitleScreenWidget::SetStage(EStage NewStage, bool bFocus)
{
	Stage = NewStage;
	if (MenuBlock)
	{
		MenuBlock->SetIsEnabled(Stage == EStage::Menu);
		MenuBlock->SetRenderOpacity(Stage == EStage::Confirm ? 0.35f : 1.f);
	}
	if (ConfirmPanel)
	{
		ConfirmPanel->SetVisibility(Stage == EStage::Confirm ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	Selected = 0;
	LastFocused = INDEX_NONE;
	if (bFocus)
	{
		FocusCurrent();
	}
}

void UTitleScreenWidget::FocusCurrent()
{
	const TArray<const FEntry*> Entries = VisibleEntries();
	if (Entries.IsValidIndex(Selected) && Entries[Selected]->Button)
	{
		Entries[Selected]->Button->SetFocus();
	}
	else
	{
		SetFocus();
	}
}

void UTitleScreenWidget::PlayOutro(float Duration)
{
	SetStage(EStage::Outro, false);
	OutroAge = 0.f;
	OutroDuration = FMath::Max(Duration, 0.1f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTitleScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;

	// Arrivée : le logo puis le menu apparaissent en fondu
	if (Stage != EStage::Outro)
	{
		if (LogoBlock)
		{
			LogoBlock->SetRenderOpacity(FMath::Clamp((Age - 0.3f) / 1.2f, 0.f, 1.f));
		}
		if (MenuBlock && Stage == EStage::Menu)
		{
			MenuBlock->SetRenderOpacity(FMath::Clamp((Age - 0.9f) / 0.8f, 0.f, 1.f));
		}
		if (FooterBlock)
		{
			FooterBlock->SetRenderOpacity(FMath::Clamp((Age - 0.9f) / 0.8f, 0.f, 1.f));
		}
	}

	// Fond : zoom très lent qui va et vient, et fondu vers l'image suivante s'il y en a plusieurs
	if (BackgroundBlock)
	{
		const float Zoom = 1.03f + 0.03f * FMath::Sin(Age * 0.05f);
		BackgroundBlock->SetRenderScale(FVector2D(Zoom, Zoom));
	}
	if (BackgroundImages.Num() == 2 && BackgroundTextures.Num() > 1)
	{
		const UTitleScreenSettings* Settings = GetDefault<UTitleScreenSettings>();
		constexpr float FadeTime = 2.f;
		BackgroundAge += InDeltaTime;
		const float Fade = FMath::Clamp((BackgroundAge - Settings->BackgroundDuration) / FadeTime, 0.f, 1.f);
		BackgroundImages[1]->SetRenderOpacity(Fade);
		if (Fade >= 1.f)
		{
			// L'image suivante devient l'image en cours ; on prépare celle d'après dessous
			BackgroundIndex = (BackgroundIndex + 1) % BackgroundTextures.Num();
			BackgroundImages[0]->SetBrushFromTexture(BackgroundTextures[BackgroundIndex], true);
			BackgroundImages[1]->SetRenderOpacity(0.f);
			BackgroundImages[1]->SetBrushFromTexture(BackgroundTextures[(BackgroundIndex + 1) % BackgroundTextures.Num()], true);
			BackgroundAge = 0.f;
		}
	}

	if (Stage == EStage::Outro)
	{
		OutroAge += InDeltaTime;
		SetRenderOpacity(FMath::Clamp(1.f - OutroAge / OutroDuration, 0.f, 1.f));
		return;
	}

	// L'entrée choisie suit le focus (flèches, manette) et la souris
	const TArray<const FEntry*> Entries = VisibleEntries();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		UButton* Button = Entries[Index]->Button;
		if (Button->IsHovered() && !Button->HasKeyboardFocus())
		{
			Button->SetFocus();
		}
		if (Button->HasKeyboardFocus() || Button->HasUserFocus(GetOwningPlayer()))
		{
			Selected = Index;
		}
	}
	if (Entries.Num() > 0 && !HasFocusedDescendants())
	{
		// Clic dans le vide : on garde une entrée choisie pour le clavier et la manette
		FocusCurrent();
	}
	if (Entries.Num() > 0 && Selected != LastFocused)
	{
		if (LastFocused != INDEX_NONE)
		{
			if (UTitleScreenSubsystem* Subsystem = Owner.Get())
			{
				Subsystem->PlaySound(false);
			}
		}
		LastFocused = Selected;
	}
	// Entrée choisie : blanc plein, décalée de 14 px vers la droite ; les autres un peu plus pâles
	for (const FEntry* Entry : { &ContinueEntry, &NewGameEntry, &OptionsEntry, &NoEntry, &YesEntry })
	{
		if (!Entry->Button)
		{
			continue;
		}
		const int32 Index = Entries.IndexOfByKey(Entry);
		const bool bSelected = Index != INDEX_NONE && Index == Selected;
		Entry->Label->SetColorAndOpacity(FSlateColor(bSelected ? White : Dim));
		const float Target = bSelected ? 14.f : 0.f;
		const float Current = Entry->Label->GetRenderTransform().Translation.X;
		Entry->Label->SetRenderTranslation(FVector2D(FMath::FInterpTo(Current, Target, InDeltaTime, 14.f), 0.f));
	}
}

FReply UTitleScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Stage == EStage::Confirm && IsBackKey(InKeyEvent.GetKey()))
	{
		OnConfirmNo();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UTitleScreenWidget::OnContinueClicked()
{
	if (UTitleScreenSubsystem* Subsystem = Owner.Get(); Subsystem && Stage == EStage::Menu)
	{
		Subsystem->PlaySound(true);
		Subsystem->Continue();
	}
}

void UTitleScreenWidget::OnNewGameClicked()
{
	if (Stage != EStage::Menu)
	{
		return;
	}
	UTitleScreenSubsystem* Subsystem = Owner.Get();
	if (Subsystem)
	{
		Subsystem->PlaySound(true);
	}
	if (UPokemonSaveLibrary::HasSaveGame())
	{
		// Une partie existe : on demande avant de l'effacer
		SetStage(EStage::Confirm);
	}
	else if (Subsystem)
	{
		Subsystem->StartNewGame();
	}
}

void UTitleScreenWidget::OnOptionsClicked()
{
	if (UTitleScreenSubsystem* Subsystem = Owner.Get(); Subsystem && Stage == EStage::Menu)
	{
		Subsystem->PlaySound(true);
		Subsystem->OpenOptions();
	}
}

void UTitleScreenWidget::OnConfirmYes()
{
	if (UTitleScreenSubsystem* Subsystem = Owner.Get(); Subsystem && Stage == EStage::Confirm)
	{
		Subsystem->PlaySound(true);
		Subsystem->StartNewGame();
	}
}

void UTitleScreenWidget::OnConfirmNo()
{
	if (Stage != EStage::Confirm)
	{
		return;
	}
	SetStage(EStage::Menu, false);
	// Retour sur « Nouvelle partie »
	const TArray<const FEntry*> Entries = VisibleEntries();
	Selected = FMath::Max(0, Entries.IndexOfByKey(&NewGameEntry));
	LastFocused = Selected;
	FocusCurrent();
}

#undef LOCTEXT_NAMESPACE
