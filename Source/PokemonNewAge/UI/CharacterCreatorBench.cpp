// Banc d'essai automatique du créateur de personnage : commande console « BancPerso [étiquette] ».
// Ouvre le studio et l'éditeur sans personnage joueur, enchaîne des réglages fixes et enregistre une capture par
// réglage dans Saved/BancPerso/<étiquette>/. Sert à vérifier les défauts sans faire tester L'amie à chaque étape.
// Lancement sans éditeur (voir Tools/InZOI/banc_perso.bat) :
//   UnrealEditor.exe PokemonNewAge.uproject /Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/Engine.GameModeBase
//     -game -windowed -ResX=1920 -ResY=1080 -ExecCmds="BancPerso avant" -BancQuit

#include "UI/CharacterCreatorWidget.h"
#include "Character/CharacterStudio.h"
#include "Character/TrainerAppearanceRender.h"
#include "Character/TrainerAppearanceData.h"
#include "Character/TrainerAnimInstance.h"
#include "Components/CanvasPanel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ContentStreaming.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "PokemonNewAge.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

class FCharacterCreatorBench
{
public:

	struct FStep
	{
		FString Name;
		TFunction<void()> Setup;
		float Wait = 2.5f;
		bool bUI = false;
	};

	static void Start(UWorld* World, const FString& Label, const FString& Filter)
	{
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		UTrainerAppearanceData* Data = TrainerAppearanceRender::LoadDefaultData();
		if (!PlayerController || !Data || Data->Bases.IsEmpty())
		{
			UE_LOG(LogPokemonNewAge, Error, TEXT("[BancPerso] Pas de PlayerController ou pas de catalogue d'apparence"));
			return;
		}
		TSharedPtr<FCharacterCreatorBench> Bench = MakeShared<FCharacterCreatorBench>();
		Bench->OutDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("BancPerso") / Label);
		IFileManager::Get().MakeDirectory(*Bench->OutDir, true);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = PlayerController;
		ACharacterStudio* Studio = World->SpawnActor<ACharacterStudio>(ACharacterStudio::StaticClass(), FVector(0.f, 0.f, 300000.f), FRotator::ZeroRotator, Params);
		FTrainerAppearance Start;
		Start.Base = Data->Bases[0].Id;
		Studio->Init(Data, Start);
		Studio->SetAutoPoses(false); // captures reproductibles : pas de pose au hasard du temps
		UCharacterCreatorWidget* Editor = CreateWidget<UCharacterCreatorWidget>(PlayerController, UCharacterCreatorWidget::StaticClass());
		Editor->Init(nullptr, Studio, Data, ETrainerEditorMode::Creation, Start, TEXT("Banc"), FString());
		Editor->AddToPlayerScreen(8);
		PlayerController->SetViewTargetWithBlend(Studio, 0.f);
		PlayerController->SetShowMouseCursor(true);
		Bench->Studio = Studio;
		Bench->Editor = Editor;
		Bench->Data = Data;
		Bench->BuildSteps();
		if (!Filter.IsEmpty())
		{
			// « BancPerso essai 3 » : seulement les captures dont le nom commence par 3 (le cou), pour aller vite
			Bench->Steps.RemoveAll([&Filter](const FStep& Step) { return !Step.Name.StartsWith(Filter); });
		}
		Bench->bQuit = FParse::Param(FCommandLine::Get(), TEXT("BancQuit"));
		UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] %d captures dans %s"), Bench->Steps.Num(), *Bench->OutDir);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Bench](float DeltaTime)
		{
			return Bench->Tick(DeltaTime);
		}));
	}

private:

	TWeakObjectPtr<ACharacterStudio> Studio;
	TWeakObjectPtr<UCharacterCreatorWidget> Editor;
	TWeakObjectPtr<UTrainerAppearanceData> Data;
	TArray<FStep> Steps;
	FString OutDir;
	int32 StepIndex = -1;
	float Remaining = 4.f; // laisse le studio s'installer
	bool bQuit = false;
	bool bCaptured = false;
	float Yaw = 0.f;

	// ------------------------------------------------ Réglages

	void SetView(EStudioFocus Focus, float NewYaw, float Zoom = 0.f)
	{
		ACharacterStudio* S = Studio.Get();
		if (!S)
		{
			return;
		}
		S->SetFocus(Focus);
		if (Zoom != 0.f)
		{
			S->Zoom(Zoom);
		}
		S->Turn(NewYaw - Yaw);
		Yaw = NewYaw;
	}

	void ShowUI(bool bShow)
	{
		if (UCharacterCreatorWidget* E = Editor.Get())
		{
			E->bUIHidden = !bShow;
			E->Canvas->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	/** Montre ou cache une pièce (emplacement), ou le corps seul (None) */
	void ShowPart(FName Slot, bool bShow)
	{
		ACharacterStudio* S = Studio.Get();
		if (!S)
		{
			return;
		}
		if (Slot.IsNone())
		{
			S->GetPreview()->SetVisibility(bShow, false);
			return;
		}
		TArray<USkeletalMeshComponent*> Components;
		S->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Component->ComponentTags.Num() > 1 && Component->ComponentTags[1] == Slot)
			{
				Component->SetVisibility(bShow, false);
			}
		}
	}

	/** Ombre portée d'une pièce (emplacement), ou du corps seul (None) */
	void SetShadow(FName Slot, bool bCast)
	{
		ACharacterStudio* S = Studio.Get();
		if (!S)
		{
			return;
		}
		TArray<USkeletalMeshComponent*> Components;
		S->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			const bool bMatch = Slot.IsNone() ? Component == S->GetPreview()
				: (Component->ComponentTags.Num() > 1 && Component->ComponentTags[1] == Slot);
			if (bMatch)
			{
				Component->SetCastShadow(bCast);
			}
		}
	}

	/** Journal : paramètres posés sur les matériaux des cheveux (mèches devant : Color3, FrontAxis, HeadCenter) */
	void LogHairParameters()
	{
		TArray<USkeletalMeshComponent*> Components;
		Studio->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Component->ComponentTags.Num() < 2 || Component->ComponentTags[1] != TEXT("Hair"))
			{
				continue;
			}
			const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
			for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
			{
				const UMaterialInterface* Material = Component->GetMaterial(Index);
				FLinearColor Color3, Front, Center;
				Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color3")), Color3);
				Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("FrontAxis")), Front);
				Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("HeadCenter")), Center);
				UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Cheveux %s [%d %s] %s : Color3 %s Front %s Center %s"),
					*GetNameSafe(Component->GetSkeletalMeshAsset()), Index, SlotNames.IsValidIndex(Index) ? *SlotNames[Index].ToString() : TEXT("?"),
					*GetNameSafe(Material), *Color3.ToString(), *Front.ToString(), *Center.ToString());
			}
		}
	}

	/** Composants des pièces d'un emplacement (None : tous, corps compris) */
	TArray<USkeletalMeshComponent*> PartComponents(FName Slot) const
	{
		TArray<USkeletalMeshComponent*> Components, Result;
		Studio->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Slot.IsNone() || (Component->ComponentTags.Num() > 1 && Component->ComponentTags[1] == Slot))
			{
				Result.Add(Component);
			}
		}
		return Result;
	}

	/** Essai A/B : réglage numérique posé en direct sur les matériaux d'un emplacement */
	void SetScalar(FName Slot, FName Parameter, float Value)
	{
		for (USkeletalMeshComponent* Component : PartComponents(Slot))
		{
			Component->SetScalarParameterValueOnMaterials(Parameter, Value);
		}
		UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Essai %s.%s = %.3f"), *Slot.ToString(), *Parameter.ToString(), Value);
	}

	/** Essai A/B : alpha (intensité) d'une couleur de maquillage, transformé ; garde la valeur d'origine pour la remettre */
	void MapAlpha(FName Parameter, TFunction<float(float)> Map)
	{
		for (USkeletalMeshComponent* Component : PartComponents(NAME_None))
		{
			for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
			{
				UMaterialInterface* Material = Component->GetMaterial(Index);
				FLinearColor Color;
				if (!Material || !Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Color) || Color.A <= 0.f)
				{
					continue;
				}
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
				if (!MID)
				{
					MID = Component->CreateDynamicMaterialInstance(Index);
				}
				const FString Key = FString::Printf(TEXT("%p/%d/%s"), Component, Index, *Parameter.ToString());
				const float Original = SavedAlphas.FindOrAdd(Key, Color.A);
				Color.A = Map(Original);
				MID->SetVectorParameterValue(Parameter, Color);
				UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Essai %s : %.3f -> %.3f"), *Parameter.ToString(), Original, Color.A);
			}
		}
	}

	TMap<FString, float> SavedAlphas;

	/** Modèle complet dont une pièce (tête, coupe) contient ce texte, sinon INDEX_NONE */
	int32 FindFullPreset(const FString& Text) const
	{
		const TArray<FTrainerPresetDef>& Presets = Data->FullPresets;
		for (int32 Index = 0; Index < Presets.Num(); ++Index)
		{
			// La vignette garde le nom du modèle inZOI (Icon_Female_Head_003_CT017)
			if (Presets[Index].Thumbnail.ToSoftObjectPath().ToString().Contains(Text))
			{
				return Index;
			}
			for (const FName Id : Presets[Index].Look.Outfit)
			{
				if (Id.ToString().Contains(Text))
				{
					return Index;
				}
				const FTrainerPartDef* Part = Data->FindPart(Id);
				if (Part && Part->Mesh.ToSoftObjectPath().ToString().Contains(Text))
				{
					return Index;
				}
			}
		}
		return INDEX_NONE;
	}

	/** Première silhouette « <Genre> 1 » de la base portée */
	int32 FindBodyPreset(const FString& Kind) const
	{
		const UCharacterCreatorWidget* E = Editor.Get();
		const TArray<FTrainerPresetDef>& Presets = Data->BodyPresets;
		for (int32 Index = 0; Index < Presets.Num(); ++Index)
		{
			const FName Base = Presets[Index].Look.Base;
			if ((Base.IsNone() || Base == E->Current.Base) && Presets[Index].Label.ToString().StartsWith(Kind))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void Add(const FString& Name, TFunction<void()> Setup, float Wait = 2.5f, bool bUI = false)
	{
		Steps.Add({ Name, MoveTemp(Setup), Wait, bUI });
	}

	void BuildSteps()
	{
		// 1) Onglet Modèles, interface visible : le personnage doit rester centré pendant l'attente
		// Comme à l'ouverture du jeu : apparence par défaut (aucun modèle cliqué), puis longue attente
		Add(TEXT("00_modeles_t0"), [this]() { ShowUI(true); Editor->SelectTab(UCharacterCreatorWidget::ETab::Presets); SetView(EStudioFocus::Body, 0.f); }, 3.f, true);
		Add(TEXT("01_modeles_t15"), []() {}, 15.f, true);
		Add(TEXT("02_modeles_t45"), []() {}, 30.f, true);
		Add(TEXT("03_modeles_clic_t10"), [this]() { Editor->ApplyPreset(0, true, false); }, 10.f, true);

		// 2) Six modèles complets (dont la tête W_Head03_CT017 aux cheveux roses) : corps, visage de face et de 3/4
		TArray<int32> Presets;
		const int32 Pink = FindFullPreset(TEXT("Head_003_CT017"));
		for (int32 Index = 0; Index < Data->FullPresets.Num() && Presets.Num() < (Pink == INDEX_NONE ? 6 : 5); ++Index)
		{
			if (Index != Pink)
			{
				Presets.Add(Index);
			}
		}
		if (Pink != INDEX_NONE)
		{
			Presets.Add(Pink);
		}
		UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Modèle CT017 : %d"), Pink);
		for (int32 N = 0; N < Presets.Num(); ++N)
		{
			const int32 Index = Presets[N];
			const FString Prefix = FString::Printf(TEXT("1%d_modele%02d"), N, Index);
			Add(Prefix + TEXT("_corps"), [this, Index]() { ShowUI(false); Editor->ApplyPreset(Index, true, false); SetView(EStudioFocus::Body, 0.f); }, 5.f);
			Add(Prefix + TEXT("_visage"), [this]() { SetView(EStudioFocus::Face, 0.f); });
			Add(Prefix + TEXT("_visage34"), [this]() { SetView(EStudioFocus::Face, 35.f); });
		}

		// 3) Cheveux (modèle rose) avec la couleur « Mèches devant » (zone 3) bien visible
		const int32 HairPreset = Pink != INDEX_NONE ? Pink : 0;
		Add(TEXT("20_meches_face"), [this, HairPreset]()
		{
			Editor->ApplyPreset(HairPreset, true, false);
			FTrainerAppearance New = Editor->Current;
			// 60 % : quantité par défaut du jeu (HAIR_ZONE_AMOUNTS) ; 20b à 100 %
			New.SetPartColor(TEXT("Hair"), 3, FLinearColor(0.05f, 0.45f, 1.f, 0.6f));
			Editor->ChangeAppearance(New, TEXT("Bench"), false);
			SetView(EStudioFocus::Face, 0.f);
		}, 4.f);
		Add(TEXT("20b_meches_face_100"), [this]()
		{
			FTrainerAppearance New = Editor->Current;
			New.SetPartColor(TEXT("Hair"), 3, FLinearColor(0.05f, 0.45f, 1.f, 1.f));
			Editor->ChangeAppearance(New, TEXT("Bench"), false);
		}, 2.f);
		Add(TEXT("20c_meches_retour_60"), [this]()
		{
			FTrainerAppearance New = Editor->Current;
			New.SetPartColor(TEXT("Hair"), 3, FLinearColor(0.05f, 0.45f, 1.f, 0.6f));
			Editor->ChangeAppearance(New, TEXT("Bench"), false);
		}, 2.f);
		Add(TEXT("21_meches_34"), [this]() { SetView(EStudioFocus::Face, 35.f); LogHairParameters(); });
		Add(TEXT("22_meches_profil"), [this]() { SetView(EStudioFocus::Face, 90.f); });
		// Mèches devant sur une coupe courte d'homme (modèle 0)
		Add(TEXT("22c_meches_homme"), [this]()
		{
			Editor->ApplyPreset(0, true, false);
			FTrainerAppearance New = Editor->Current;
			New.SetPartColor(TEXT("Hair"), 3, FLinearColor(0.05f, 0.45f, 1.f, 1.f));
			Editor->ChangeAppearance(New, TEXT("Bench"), false);
			SetView(EStudioFocus::Face, 35.f);
		}, 4.f);
		Add(TEXT("23_meches_dos"), [this]() { SetView(EStudioFocus::Face, 180.f); });

		// 4) Cou : un peu plus loin, pour voir le col et le cou (lignes sombres), femme puis homme
		const int32 NeckPresets[] = { HairPreset, HairPreset == 0 ? 1 : 0 };
		for (int32 B = 0; B < 2; ++B)
		{
			const int32 Full = NeckPresets[B];
			const FString P = FString::Printf(TEXT("3%d"), B);
			Add(P + TEXT("0_cou_face"), [this, Full]() { Editor->ApplyPreset(Full, true, false); SetView(EStudioFocus::Face, 0.f, -3.f); }, 4.f);
			Add(P + TEXT("1_cou_34"), [this]() { SetView(EStudioFocus::Face, 30.f, -3.f); });
			Add(P + TEXT("2_cou_profil"), [this]() { SetView(EStudioFocus::Face, 75.f, -3.f); });
			// Essais : d'où viennent les lignes du cou ? (sans le corps, sans la tête, sans le haut)
			Add(P + TEXT("3_cou_sans_corps"), [this]() { SetView(EStudioFocus::Face, 0.f, -3.f); ShowPart(NAME_None, false); }, 3.f);
			Add(P + TEXT("4_cou_sans_tete"), [this]() { ShowPart(NAME_None, true); ShowPart(TEXT("Head"), false); }, 3.f);
			Add(P + TEXT("5_cou_sans_haut"), [this]() { ShowPart(TEXT("Head"), true); ShowPart(TEXT("Top"), false); }, 3.f);
			Add(P + TEXT("6_cou_sans_maquillage"), [this]()
			{
				ShowPart(TEXT("Top"), true);
				// Pièces portées (journal) puis tout l'onglet Maquillage (maquillage, taches, tatouages) retiré
				FString Worn;
				for (const FName Id : Editor->Current.Outfit)
				{
					Worn += Id.ToString() + TEXT(" ");
				}
				for (const FTrainerPartColor& Color : Editor->Current.PartColors)
				{
					Worn += FString::Printf(TEXT("[%s/%d %s] "), *Color.Slot.ToString(), Color.Zone, *Color.Color.ToString());
				}
				UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Pièces : %s"), *Worn);
				FTrainerAppearance New = Editor->Current;
				for (const FTrainerPartSlotDef& SlotDef : Data->PartSlots)
				{
					if (SlotDef.Tab == TEXT("Makeup"))
					{
						New.Outfit.Add(FName(*(TEXT("-") + SlotDef.Slot.ToString())));
					}
				}
				Editor->ChangeAppearance(New, TEXT("Bench"), false);
			}, 3.f);
			Add(P + TEXT("7_cou_sans_cheveux"), [this]() { ShowPart(TEXT("Hair"), false); }, 2.f);
			Add(P + TEXT("8_cou_sans_poils"), [this]() { ShowPart(TEXT("Hair"), true); ShowPart(TEXT("Eyebrows"), false); ShowPart(TEXT("Eyelashes"), false); ShowPart(TEXT("Beard"), false); ShowPart(TEXT("Mustache"), false); }, 2.f);
			Add(P + TEXT("A_cou_sans_corps_ni_haut"), [this]() { ShowPart(NAME_None, false); ShowPart(TEXT("Top"), false); }, 2.f);
			Add(P + TEXT("B_cou_sans_ombre_corps_haut"), [this]() { ShowPart(NAME_None, true); ShowPart(TEXT("Top"), true); SetShadow(NAME_None, false); SetShadow(TEXT("Top"), false); }, 2.f);
			Add(P + TEXT("C_cou_sans_ombre_tete"), [this]() { SetShadow(NAME_None, true); SetShadow(TEXT("Top"), true); SetShadow(TEXT("Head"), false); }, 2.f);
			Add(P + TEXT("D_cou_ombres_remises"), [this]() { SetShadow(TEXT("Head"), true); }, 1.f);
			Add(P + TEXT("9_cou_remis"), [this]() { for (const TCHAR* S : { TEXT("Eyebrows"), TEXT("Eyelashes"), TEXT("Beard"), TEXT("Mustache") }) { ShowPart(S, true); } }, 1.f);
		}

		// 5) Silhouettes du corps, pour un modèle de chaque base : face, profil, dos
		const TCHAR* Kinds[] = { TEXT("Moyen"), TEXT("Mince"), TEXT("Rond"), TEXT("Musclé") };
		const int32 BasePresets[] = { HairPreset, HairPreset == 0 ? 1 : 0 };
		for (int32 B = 0; B < 2; ++B)
		{
			const int32 Full = BasePresets[B];
			for (int32 K = 0; K < 4; ++K)
			{
				const FString Kind = Kinds[K];
				const FString Prefix = FString::Printf(TEXT("4%d%d_%s"), B, K, *Kind);
				Add(Prefix + TEXT("_face"), [this, Full, Kind]()
				{
					Editor->ApplyPreset(Full, true, false);
					const int32 Body = FindBodyPreset(Kind);
					UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Silhouette %s : %d"), *Kind, Body);
					if (Body != INDEX_NONE)
					{
						Editor->ApplyPreset(Body, false, true);
					}
					SetView(EStudioFocus::Body, 0.f);
				}, 4.f);
				Add(Prefix + TEXT("_profil"), [this]() { SetView(EStudioFocus::Body, 90.f); });
				Add(Prefix + TEXT("_dos"), [this]() { SetView(EStudioFocus::Body, 180.f); });
			}
		}
		// 6) Poses du studio (bouton P) : candidates pour l'attente, le personnage doit rester sur place
		const FTrainerBaseDef* PoseBase = Data->FindBase(Data->Bases[0].Id);
		const int32 PoseCount = PoseBase ? PoseBase->StudioAnimations.Num() : 0;
		for (int32 Pose = 0; Pose < PoseCount; ++Pose)
		{
			const FString Label = PoseBase->StudioAnimations[Pose].Label.ToString().Replace(TEXT(" "), TEXT("_"));
			const FString Prefix = FString::Printf(TEXT("5%02d_pose_%s"), Pose, *Label);
			Add(Prefix + TEXT("_t2"), [this, Pose]()
			{
				if (Pose == 0)
				{
					Editor->ApplyPreset(0, true, false);
				}
				Studio->NextAnimation();
				SetView(EStudioFocus::Body, 0.f);
			}, 2.f);
			Add(Prefix + TEXT("_t9"), []() {}, 7.f);
		}
		// 7) Ressemblance : visage de face cadré comme les vignettes d'inZOI (Tools/InZOI/planche_ressemblance.ps1
		//    met chaque capture à côté de sa vignette, d'après la ligne « Vignette » du journal)
		TArray<int32> Faces;
		for (int32 Index = 0; Index < FMath::Min(10, Data->FullPresets.Num()); ++Index)
		{
			Faces.Add(Index);
		}
		if (Pink != INDEX_NONE && !Faces.Contains(Pink))
		{
			Faces.Add(Pink);
		}
		for (const int32 Index : Faces)
		{
			const FString Name = FString::Printf(TEXT("7%02d_ressemblance"), Index);
			Add(Name, [this, Index, Name]()
			{
				ShowUI(false);
				Editor->ApplyPreset(Index, true, false);
				UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Vignette %s = %s"), *Name, *Data->FullPresets[Index].Thumbnail.ToSoftObjectPath().ToString());
				SetView(EStudioFocus::Face, 0.f, -1.5f);
			}, 4.f);
		}
		// 7b) Modèles de contrôle (notes du 9 novembre) : visage, puis gros plan (yeux, cils, barbe, peau)
		const TCHAR* ControlKeys[] = { TEXT("Female_Head_007_CT010"), TEXT("Female_Head_007_CT022"), TEXT("Male_Head_007_CT011"),
			TEXT("Male_Head_010_CT015"), TEXT("Female_Head_006_CT004") };
		for (int32 K = 0; K < UE_ARRAY_COUNT(ControlKeys); ++K)
		{
			const int32 Index = FindFullPreset(ControlKeys[K]);
			UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Modèle de contrôle %s : %d"), ControlKeys[K], Index);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			const FString Name = FString::Printf(TEXT("75%d_controle"), K);
			Add(Name, [this, Index, Name]()
			{
				ShowUI(false);
				Editor->ApplyPreset(Index, true, false);
				UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Vignette %s = %s"), *Name, *Data->FullPresets[Index].Thumbnail.ToSoftObjectPath().ToString());
				SetView(EStudioFocus::Face, 0.f, -1.5f);
			}, 4.f);
			Add(FString::Printf(TEXT("76%d_gros_plan"), K), [this]() { SetView(EStudioFocus::Face, 20.f, 6.f); }, 2.5f);
		}
		// 8) Silhouettes à côté de leur vignette inZOI (torse nu, comme les vignettes) : homme puis femme
		const int32 BodyBases[] = { HairPreset == 0 ? 1 : 0, HairPreset };
		for (int32 B = 0; B < 2; ++B)
		{
			const int32 Full = BodyBases[B];
			for (int32 Index = 0; Index < Data->BodyPresets.Num(); ++Index)
			{
				const FString Name = FString::Printf(TEXT("8%d%02d_silhouette"), B, Index);
				Add(Name, [this, Full, Index, Name]()
				{
					ShowUI(false);
					Editor->ApplyPreset(Full, true, false);
					const FName Base = Data->BodyPresets[Index].Look.Base;
					if (!Base.IsNone() && Base != Editor->Current.Base)
					{
						return; // silhouette de l'autre base : capture sans intérêt, ignorée par la planche
					}
					Editor->ApplyPreset(Index, false, true);
					ShowPart(TEXT("Top"), false);
					UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Vignette %s = %s"), *Name, *Data->BodyPresets[Index].Thumbnail.ToSoftObjectPath().ToString());
					SetView(EStudioFocus::Body, 0.f);
				}, 3.f);
			}
		}
		// A) Essais A/B en direct (sans réimport) sur les modèles de contrôle : couleur et reflet des cheveux, teinte
		//    de peau, couverture des lèvres, sourcils. Chaque capture garde les réglages précédents remis à zéro.
		const TCHAR* AbKeys[] = { TEXT("Female_Head_007_CT022"), TEXT("Female_Head_007_CT010"), TEXT("Female_Head_006_CT004"), TEXT("Beard") };
		for (int32 K = 0; K < UE_ARRAY_COUNT(AbKeys); ++K)
		{
			const int32 Index = FindFullPreset(AbKeys[K]);
			UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Essai A/B %s : %d"), AbKeys[K], Index);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			const FString P = FString::Printf(TEXT("AB%d"), K);
			Add(P + TEXT("0_base"), [this, Index]()
			{
				ShowUI(false);
				SavedAlphas.Reset();
				Editor->ApplyPreset(Index, true, false);
				SetView(EStudioFocus::Face, 15.f);
			}, 4.f);
			Add(P + TEXT("1_cheveux_g100"), [this]() { SetScalar(TEXT("Hair"), TEXT("ColorGamma"), 1.0f); });
			Add(P + TEXT("2_cheveux_g115"), [this]() { SetScalar(TEXT("Hair"), TEXT("ColorGamma"), 1.15f); });
			Add(P + TEXT("3_reflet_doux"), [this]()
			{
				SetScalar(TEXT("Hair"), TEXT("ColorGamma"), 0.85f);
				SetScalar(TEXT("Hair"), TEXT("Specular"), 0.22f);
				SetScalar(TEXT("Beard"), TEXT("Specular"), 0.06f);
				SetScalar(TEXT("Mustache"), TEXT("Specular"), 0.06f);
			});
			Add(P + TEXT("4_peau_chaude"), [this]()
			{
				SetScalar(TEXT("Hair"), TEXT("Specular"), 0.45f);
				SetScalar(TEXT("Beard"), TEXT("Specular"), 0.12f);
				SetScalar(TEXT("Mustache"), TEXT("Specular"), 0.12f);
				// 15 % plus foncée, plus chaude (moins de bleu, un peu moins de vert), sans passer par le nuancier
				FTrainerAppearance Look = Editor->Current;
				Look.SkinColor = FLinearColor(Look.SkinColor.R * 0.87f, Look.SkinColor.G * 0.83f, Look.SkinColor.B * 0.76f, 1.f);
				UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Essai peau %s -> %s"), *Editor->Current.SkinColor.ToString(), *Look.SkinColor.ToString());
				Studio->ShowAppearance(Look);
			});
			Add(P + TEXT("5_levres_sqrt"), [this]()
			{
				Studio->ShowAppearance(Editor->Current);
				for (const TCHAR* Channel : { TEXT("MakeupLips_R"), TEXT("MakeupLips_G"), TEXT("MakeupLips_B") })
				{
					MapAlpha(Channel, [](float A) { return FMath::Sqrt(A); });
				}
			});
			Add(P + TEXT("6_levres_pleines"), [this]()
			{
				for (const TCHAR* Channel : { TEXT("MakeupLips_R"), TEXT("MakeupLips_G"), TEXT("MakeupLips_B") })
				{
					MapAlpha(Channel, [](float A) { return 1.f; });
				}
			});
			Add(P + TEXT("7_sourcils_70"), [this]()
			{
				for (const TCHAR* Channel : { TEXT("MakeupLips_R"), TEXT("MakeupLips_G"), TEXT("MakeupLips_B") })
				{
					MapAlpha(Channel, [](float A) { return A; });
				}
				SetScalar(TEXT("Eyebrows"), TEXT("Density"), 0.7f);
				for (const TCHAR* Channel : { TEXT("MakeupBrows_R"), TEXT("MakeupBrows_G"), TEXT("MakeupBrows_B") })
				{
					MapAlpha(Channel, [](float A) { return A * 0.7f; });
				}
			});
			Add(P + TEXT("8_sourcils_gros_plan"), [this]() { SetView(EStudioFocus::Face, 0.f, 6.f); });
			Add(P + TEXT("9_base_gros_plan"), [this]()
			{
				SetScalar(TEXT("Eyebrows"), TEXT("Density"), 1.f);
				for (const TCHAR* Channel : { TEXT("MakeupBrows_R"), TEXT("MakeupBrows_G"), TEXT("MakeupBrows_B") })
				{
					MapAlpha(Channel, [](float A) { return A; });
				}
			});
		}
		// B7) Vêtements à forte corpulence (0,86) : limite d'échelle des os d'aide du haut (0,35 actuelle) contre
		//     la pose exacte du corps (0)
		const TCHAR* FatKeys[] = { TEXT("Female_Head_007_CT010"), TEXT("Male_Head_007_CT011") };
		for (int32 K = 0; K < UE_ARRAY_COUNT(FatKeys); ++K)
		{
			const int32 Index = FindFullPreset(FatKeys[K]);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			const FString P = FString::Printf(TEXT("B7%d"), K);
			Add(P + TEXT("0_corpulence_limite035"), [this, Index]()
			{
				ShowUI(false);
				Editor->ApplyPreset(Index, true, false);
				Editor->Current.SetSlider(Data->MassSliderId, 0.86f);
				Studio->ShowAppearance(Editor->Current);
				SetView(EStudioFocus::Body, 0.f);
			}, 4.f);
			Add(P + TEXT("1_profil_limite035"), [this]() { SetView(EStudioFocus::Body, 90.f); });
			Add(P + TEXT("2_dos_limite035"), [this]() { SetView(EStudioFocus::Body, 180.f); });
			for (const float Limit : { 0.f, 0.6f })
			{
				const FString L = Limit == 0.f ? TEXT("exacte") : TEXT("limite060");
				Add(P + TEXT("3_face_") + L, [this, Limit]()
				{
					for (const TCHAR* Slot : { TEXT("Top"), TEXT("Outer") })
					{
						for (USkeletalMeshComponent* Component : PartComponents(Slot))
						{
							if (UTrainerAnimInstance* Anim = Cast<UTrainerAnimInstance>(Component->GetAnimInstance()))
							{
								Anim->HelperScaleLimit = Limit;
							}
						}
					}
					SetView(EStudioFocus::Body, 0.f);
				});
				Add(P + TEXT("4_profil_") + L, [this]() { SetView(EStudioFocus::Body, 90.f); });
				Add(P + TEXT("5_dos_") + L, [this]() { SetView(EStudioFocus::Body, 180.f); });
			}
		}
		// DR) Dreadlocks trop claires (W_Head07_CT022, couleur du jeu 402909) : couleur appliquée ? reflets ? diffusion ?
		const int32 DreadPreset = FindFullPreset(TEXT("Female_Head_007_CT022"));
		if (DreadPreset != INDEX_NONE)
		{
			Add(TEXT("DR0_base"), [this, DreadPreset]()
			{
				ShowUI(false);
				Editor->ApplyPreset(DreadPreset, true, false);
				SetView(EStudioFocus::Face, 0.f);
				LogHairParameters();
				for (USkeletalMeshComponent* Component : PartComponents(TEXT("Hair")))
				{
					for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
					{
						const UMaterialInterface* Material = Component->GetMaterial(Index);
						FLinearColor Tint;
						float Spec = -1.f, Rough = -1.f, Scatter = -1.f;
						Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Tint")), Tint);
						Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Specular")), Spec);
						Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Roughness")), Rough);
						Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Scatter")), Scatter);
						UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Dreads %s [%d] Tint %s Spec %.2f Rough %.2f Scatter %.2f"),
							*GetNameSafe(Material), Index, *Tint.ToString(), Spec, Rough, Scatter);
					}
				}
			}, 4.f);
			Add(TEXT("DR1_noir"), [this]()
			{
				for (USkeletalMeshComponent* Component : PartComponents(TEXT("Hair")))
				{
					Component->SetVectorParameterValueOnMaterials(TEXT("Tint"), FVector(0.f, 0.f, 0.f));
				}
			});
			Add(TEXT("DR2_reglages_jeu"), [this]()
			{
				for (USkeletalMeshComponent* Component : PartComponents(TEXT("Hair")))
				{
					Component->SetVectorParameterValueOnMaterials(TEXT("Tint"), FVector(0.0513f, 0.0176f, 0.0030f));
				}
				SetScalar(TEXT("Hair"), TEXT("Specular"), 0.25f);
				SetScalar(TEXT("Hair"), TEXT("Roughness"), 0.55f);
				SetScalar(TEXT("Hair"), TEXT("Scatter"), 0.35f);
			});
			Add(TEXT("DR3_sans_diffusion"), [this]() { SetScalar(TEXT("Hair"), TEXT("Scatter"), 0.f); });
			Add(TEXT("DR4_gamma1"), [this]() { SetScalar(TEXT("Hair"), TEXT("ColorGamma"), 1.f); });
			Add(TEXT("DR5_gros_plan"), [this]() { SetView(EStudioFocus::Face, 0.f, 6.f); });
		}
		// AD) Grain des cheveux de loin : tramage de l'opacité (DitherSpread 0,4 actuel, 0,15, 0), corps puis visage
		const int32 GrainPreset = FMath::Max(0, FindFullPreset(TEXT("Female_Head_007_CT010")));
		for (const float Spread : { 0.4f, 0.15f, 0.f })
		{
			const FString Tag = FString::Printf(TEXT("%03d"), FMath::RoundToInt(Spread * 100.f));
			Add(TEXT("AD_corps_") + Tag, [this, GrainPreset, Spread]()
			{
				ShowUI(false);
				if (Spread == 0.4f)
				{
					Editor->ApplyPreset(GrainPreset, true, false);
				}
				SetScalar(TEXT("Hair"), TEXT("DitherSpread"), Spread);
				SetView(EStudioFocus::Body, 0.f);
			}, 3.f);
			Add(TEXT("AD_visage_") + Tag, [this]() { SetView(EStudioFocus::Face, 0.f); }, 2.5f);
		}
		// 9) Fond du studio (cyclorama) : corps entier avec et sans interface, de biais, puis d'autres couleurs
		const int32 BackdropPreset = FMath::Max(0, FindFullPreset(TEXT("Female_Head_007_CT010")));
		Add(TEXT("90_fond_ui"), [this, BackdropPreset]()
		{
			Editor->ApplyPreset(BackdropPreset, true, false);
			Studio->SetBackdropColor(FLinearColor(0.55f, 0.58f, 0.64f));
			ShowUI(true);
			Editor->SelectTab(UCharacterCreatorWidget::ETab::Presets);
			SetView(EStudioFocus::Body, 0.f);
		}, 4.f, true);
		Add(TEXT("91_fond_corps"), [this]() { ShowUI(false); SetView(EStudioFocus::Body, 0.f); }, 2.f);
		Add(TEXT("92_fond_biais"), [this]() { SetView(EStudioFocus::Body, 40.f, -1.5f); }, 2.f);
		Add(TEXT("93_fond_beige"), [this]() { Studio->SetBackdropColor(FLinearColor(0.7f, 0.6f, 0.48f)); SetView(EStudioFocus::Body, 0.f); }, 2.f);
		Add(TEXT("94_fond_anthracite"), [this]() { Studio->SetBackdropColor(FLinearColor(0.06f, 0.065f, 0.075f)); }, 2.f);
		Add(TEXT("95_fond_visage"), [this]() { Studio->SetBackdropColor(FLinearColor(0.55f, 0.58f, 0.64f)); SetView(EStudioFocus::Face, 0.f); }, 2.5f);
		Add(TEXT("99_fin"), [this]() { SetView(EStudioFocus::Body, 0.f); }, 1.f);
	}

	// ------------------------------------------------ Boucle

	void Capture(const FStep& Step)
	{
#if WITH_EDITOR
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
#endif
		const FString File = OutDir / Step.Name + TEXT(".png");
		FScreenshotRequest::RequestScreenshot(File, Step.bUI, false);
		if (ACharacterStudio* S = Studio.Get())
		{
			// Dérive du bassin (l'attente le faisait sortir de l'écran) : écart horizontal avec le centre du studio
			const FVector Pelvis = S->GetPreview()->DoesSocketExist(TEXT("pelvis")) ? S->GetPreview()->GetSocketLocation(TEXT("pelvis")) : FVector::ZeroVector;
			const FVector Delta = Pelvis - S->GetActorLocation();
			UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] %s (bassin : %.1f, %.1f, %.1f)"), *Step.Name, Delta.X, Delta.Y, Delta.Z);
		}
	}

	bool Tick(float DeltaTime)
	{
		if (!Studio.IsValid() || !Editor.IsValid() || !Data.IsValid())
		{
			return false;
		}
		Remaining -= DeltaTime;
		if (Remaining > 0.f)
		{
			// Juste avant la capture : textures en pleine résolution et matériaux compilés
			if (Remaining < 1.2f && StepIndex >= 0 && !bCaptured)
			{
				IStreamingManager::Get().StreamAllResources(0.5f);
			}
			return true;
		}
		if (StepIndex >= 0 && Steps.IsValidIndex(StepIndex) && !bCaptured)
		{
			// La capture est prise au prochain rendu : on attend qu'elle soit faite avant de changer le réglage
			Capture(Steps[StepIndex]);
			bCaptured = true;
			Remaining = 0.6f;
			return true;
		}
		bCaptured = false;
		++StepIndex;
		if (!Steps.IsValidIndex(StepIndex))
		{
			UE_LOG(LogPokemonNewAge, Display, TEXT("[BancPerso] Terminé"));
			if (bQuit)
			{
				FPlatformMisc::RequestExit(false, TEXT("BancPerso"));
			}
			return false;
		}
		Steps[StepIndex].Setup();
		Remaining = Steps[StepIndex].Wait;
		return true;
	}
};

static FAutoConsoleCommandWithWorldAndArgs GBancPersoCommand(
	TEXT("BancPerso"),
	TEXT("Banc d'essai du créateur de personnage : BancPerso <étiquette> [début des noms], captures dans Saved/BancPerso/<étiquette>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		FCharacterCreatorBench::Start(World, Args.IsEmpty() ? FString(TEXT("essai")) : Args[0], Args.Num() > 1 ? Args[1] : FString());
	}));
