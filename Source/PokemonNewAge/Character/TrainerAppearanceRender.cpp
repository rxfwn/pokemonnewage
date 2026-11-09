#include "Character/TrainerAppearanceRender.h"
#include "Character/TrainerAppearanceData.h"
#include "Character/TrainerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "AnimationRuntime.h"

namespace
{
	const TCHAR* DefaultAppearanceDataPath = TEXT("/Game/Characters/Player/DA_TrainerAppearance.DA_TrainerAppearance");

	/** Catalogue créé par Tools/InZOI/import_inzoi.py dans le dossier local (hors Git) : prioritaire s'il existe */
	const TCHAR* ImportedAppearanceDataPath = TEXT("/Game/ImportedAssets/InZOI/DA_TrainerAppearance_InZOI.DA_TrainerAppearance_InZOI");

	/** Étiquette des Skeletal Mesh de pièces créés par Apply (un par emplacement) */
	const FName PartTag(TEXT("TrainerPart"));

	/**
	 *  Pose le matériau, sauf si l'emplacement porte déjà un MID fait à partir de lui : recréer les MID à chaque
	 *  clic du créateur relançait les paramètres et le chargement des textures (menu lent). Les paramètres du MID
	 *  gardé sont tous reposés juste après (couleurs, maquillage).
	 */
	void SetMaterialKeepingMID(USkeletalMeshComponent* Component, int32 Index, UMaterialInterface* Material)
	{
		UMaterialInterface* Current = Component->GetMaterial(Index);
		if (Current == Material)
		{
			return;
		}
		if (const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Current))
		{
			if (MID->Parent == Material)
			{
				return;
			}
		}
		Component->SetMaterial(Index, Material);
	}

	/** Met à jour les pièces (tête, vêtements...) : un Skeletal Mesh par emplacement, en Leader Pose sur le corps */
	void ApplyParts(const UTrainerAppearanceData* Data, const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, AActor* Owner, USkeletalMeshComponent* Mesh)
	{
		if (!Owner || (Data->Parts.IsEmpty() && Data->PartSlots.IsEmpty()))
		{
			return;
		}
		const TMap<FName, const FTrainerPartDef*> Wanted = Data->ResolveParts(Base, Appearance);

		TMap<FName, USkeletalMeshComponent*> Existing;
		TArray<USkeletalMeshComponent*> Components;
		Owner->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Component->ComponentHasTag(PartTag) && Component->ComponentTags.Num() > 1)
			{
				Existing.Add(Component->ComponentTags[1], Component);
			}
		}

		// Emplacements vidés : on retire le mesh
		for (const TPair<FName, USkeletalMeshComponent*>& Pair : Existing)
		{
			if (!Wanted.Contains(Pair.Key))
			{
				Pair.Value->SetSkeletalMesh(nullptr);
				Pair.Value->SetVisibility(false);
			}
		}

		// Les pièces qui suivent une autre pièce (barbe -> tête) passent en dernier, pour trouver le composant suivi
		auto FollowOf = [Data](FName Slot)
		{
			const FTrainerPartSlotDef* Def = Data->FindSlot(Slot);
			return Def ? Def->FollowSlot : NAME_None;
		};
		TArray<TPair<FName, const FTrainerPartDef*>> Ordered = Wanted.Array();
		Ordered.StableSort([&FollowOf](const TPair<FName, const FTrainerPartDef*>& A, const TPair<FName, const FTrainerPartDef*>& B)
		{
			return FollowOf(A.Key).IsNone() && !FollowOf(B.Key).IsNone();
		});
		TMap<FName, USkeletalMeshComponent*> Placed;

		for (const TPair<FName, const FTrainerPartDef*>& Pair : Ordered)
		{
			// Composant suivi : le corps, ou la pièce d'un autre emplacement (la tête pour la barbe)
			USkeletalMeshComponent* Leader = Mesh;
			const FName Follow = FollowOf(Pair.Key);
			if (!Follow.IsNone())
			{
				USkeletalMeshComponent* Followed = Placed.FindRef(Follow);
				if (Followed && Followed->GetSkeletalMeshAsset())
				{
					Leader = Followed;
				}
			}
			// Variante faite pour la tête portée, sinon le modèle par défaut
			TSoftObjectPtr<USkeletalMesh> MeshRef = Pair.Value->Mesh;
			if (Leader != Mesh)
			{
				const FName LeaderName = Leader->GetSkeletalMeshAsset()->GetFName();
				for (const FTrainerPartVariant& Variant : Pair.Value->Variants)
				{
					if (Variant.HeadMesh == LeaderName && !Variant.Mesh.IsNull())
					{
						MeshRef = Variant.Mesh;
						break;
					}
				}
			}
			USkeletalMesh* PartMesh = MeshRef.LoadSynchronous();
			if (!PartMesh)
			{
				continue;
			}
			USkeletalMeshComponent* Component = Existing.FindRef(Pair.Key);
			if (!Component)
			{
				const FName Name = MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), *FString::Printf(TEXT("Part_%s"), *Pair.Key.ToString()));
				Component = NewObject<USkeletalMeshComponent>(Owner, Name, RF_Transient);
				Component->ComponentTags = { PartTag, Pair.Key };
				Component->SetupAttachment(Mesh);
				Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Component->bUseBoundsFromLeaderPoseComponent = true;
				Component->SetCastShadow(Mesh->CastShadow);
				Component->RegisterComponent();
				Owner->AddInstanceComponent(Component);
			}
			if (Component->GetSkeletalMeshAsset() != PartMesh)
			{
				Component->SetSkeletalMesh(PartMesh);
				Component->EmptyOverrideMaterials();
			}
			for (int32 Index = 0; Index < Pair.Value->Materials.Num(); ++Index)
			{
				if (UMaterialInterface* Material = Pair.Value->Materials[Index].LoadSynchronous())
				{
					SetMaterialKeepingMID(Component, Index, Material);
				}
			}
			Component->SetVisibility(true);

			// Tête (onglet Visage) : elle a ses propres os du visage, que la Leader Pose ne laisse pas bouger.
			// Elle suit donc le corps par copie de pose et reçoit les curseurs du visage (UTrainerAnimInstance).
			// Vêtements du haut (HelperScaleLimit) : même copie de pose, avec l'échelle des os d'aide bornée
			const FTrainerPartSlotDef* SlotDef = Data->FindSlot(Pair.Key);
			const bool bFace = SlotDef && SlotDef->Tab == TEXT("Face");
			const float ScaleLimit = (SlotDef && !bFace && Leader == Mesh) ? SlotDef->HelperScaleLimit : 0.f;
			if (bFace || ScaleLimit > 0.f)
			{
				Component->SetLeaderPoseComponent(nullptr);
				Component->bUseBoundsFromLeaderPoseComponent = false;
				if (!Cast<UTrainerAnimInstance>(Component->GetAnimInstance()))
				{
					Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);
					Component->SetAnimInstanceClass(UTrainerAnimInstance::StaticClass());
				}
				if (UTrainerAnimInstance* Anim = Cast<UTrainerAnimInstance>(Component->GetAnimInstance()))
				{
					Anim->SetPoseSource(Mesh);
					Anim->HelperScaleLimit = ScaleLimit;
				}
				Component->AddTickPrerequisiteComponent(Mesh);
				// Tête inZOI : sa géométrie interne (couches cachées autour du cou) projetait des traits d'ombre sombres
				// sur le bas du cou, au-dessus du col (vérifié par le banc BancPerso : ils disparaissent sans son ombre).
				// Les cheveux, le corps et les vêtements gardent la leur.
				Component->SetCastShadow(!bFace && Mesh->CastShadow);
			}
			else
			{
				Component->SetLeaderPoseComponent(Leader);
			}
			Placed.Add(Pair.Key, Component);
		}
	}

	void ApplyMorphs(const UTrainerAppearanceData* Data, const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, USkeletalMeshComponent* Mesh)
	{
		for (const FTrainerSliderDef& Def : Data->GetSliders(Base))
		{
			const float Value = Appearance.GetSlider(Def.Id);
			// Un morph absent du mesh est ignoré par Unreal : pas d'erreur si un vêtement n'a pas ce morph
			if (!Def.MorphPositive.IsNone())
			{
				Mesh->SetMorphTarget(Def.MorphPositive, FMath::Max(0.f, Value));
			}
			if (!Def.MorphNegative.IsNone())
			{
				Mesh->SetMorphTarget(Def.MorphNegative, FMath::Max(0.f, -Value));
			}
		}
	}

	void ApplyColor(USkeletalMeshComponent* Mesh, const TArray<FName>& Slots, FName Parameter, const FLinearColor& Color)
	{
		if (Parameter.IsNone())
		{
			return;
		}
		for (const FName Slot : Slots)
		{
			const int32 Index = Mesh->GetMaterialIndex(Slot);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Index));
			if (!MID)
			{
				MID = Mesh->CreateDynamicMaterialInstance(Index);
			}
			if (MID)
			{
				MID->SetVectorParameterValue(Parameter, Color);
			}
		}
	}

	/** Réglage numérique des matériaux de peau (ex. MuscleDefinition : relief et couleur des muscles du corps) */
	void ApplySkinScalar(USkeletalMeshComponent* Mesh, const TArray<FName>& Slots, FName Parameter, float Value)
	{
		for (const FName Slot : Slots)
		{
			const int32 Index = Mesh->GetMaterialIndex(Slot);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Index));
			if (!MID)
			{
				MID = Mesh->CreateDynamicMaterialInstance(Index);
			}
			if (MID)
			{
				MID->SetScalarParameterValue(Parameter, Value);
			}
		}
	}

	/** Couleurs choisies dans le nuancier pour la pièce de cet emplacement (Tint = couleur principale, Color1-3 = zones du masque) */
	/**
	 *  « Mèches devant » des cheveux : le matériau a besoin du centre de la tête et de l'axe avant dans le repère du
	 *  mesh. Avant, il les devinait (axe +Y, origine du mesh) et presque toute la coupe prenait la couleur de devant.
	 *  On les lit dans la pose de référence : os « head » et bout du nez du visage (même repère pour toutes les pièces).
	 */
	void ApplyHeadFrame(USkeletalMeshComponent* Component, UMaterialInstanceDynamic* MID)
	{
		static const FName HeadBone(TEXT("head"));
		static const FName NoseBones[] = { FName(TEXT("FACIAL_C_NoseTip")), FName(TEXT("FACIAL_C_Nose")), FName(TEXT("FACIAL_C_Forehead")) };
		TArray<USkeletalMeshComponent*> Candidates = { Component };
		if (AActor* Owner = Component->GetOwner())
		{
			Owner->GetComponents<USkeletalMeshComponent>(Candidates);
		}
		for (USkeletalMeshComponent* Candidate : Candidates)
		{
			const USkeletalMesh* Asset = Candidate ? Cast<USkeletalMesh>(Candidate->GetSkeletalMeshAsset()) : nullptr;
			if (!Asset)
			{
				continue;
			}
			const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
			const int32 Head = Ref.FindBoneIndex(HeadBone);
			int32 Nose = INDEX_NONE;
			for (const FName& Name : NoseBones)
			{
				Nose = Ref.FindBoneIndex(Name);
				if (Nose != INDEX_NONE)
				{
					break;
				}
			}
			if (Head == INDEX_NONE || Nose == INDEX_NONE)
			{
				continue;
			}
			const FVector HeadPos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Head).GetLocation();
			const FVector NosePos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Nose).GetLocation();
			FVector Forward = NosePos - HeadPos;
			Forward.Z = 0.0; // horizontal : le nez est plus haut que l'os de la tête
			if (!Forward.Normalize())
			{
				continue;
			}
			// Centre de la tête : à la hauteur du nez, au-dessus de l'os « head » (qui est à la base du crâne)
			const FVector Center(HeadPos.X, HeadPos.Y, NosePos.Z);
			MID->SetVectorParameterValue(TEXT("FrontAxis"), FLinearColor(Forward.X, Forward.Y, Forward.Z, 0.f));
			MID->SetVectorParameterValue(TEXT("HeadCenter"), FLinearColor(Center.X, Center.Y, Center.Z, 0.f));
			return;
		}
	}

	/**
	 *  Crâne d'une coupe (matériau « Scalp » d'inZOI) : il garde ses racines naturelles. Teint comme les cheveux, la raie
	 *  et le dessus de la tête devenaient roses (capture de L'amie du 2026-11-08) alors que seuls les cheveux le sont.
	 */
	bool IsScalpMaterial(const USkeletalMeshComponent* Component, int32 Index)
	{
		const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
		if (SlotNames.IsValidIndex(Index) && SlotNames[Index].ToString().Contains(TEXT("Scalp")))
		{
			return true;
		}
		const UMaterialInterface* Material = Component->GetMaterial(Index);
		const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
		const UMaterialInterface* Source = MID && MID->Parent ? MID->Parent.Get() : Material;
		return Source && Source->GetName().Contains(TEXT("Scalp"));
	}

	void ApplyPartColors(USkeletalMeshComponent* Component, const FTrainerAppearance& Appearance)
	{
		if (!Component->ComponentHasTag(PartTag) || Component->ComponentTags.Num() < 2)
		{
			return;
		}
		const FName Slot = Component->ComponentTags[1];
		// MID gardé d'un clic à l'autre : les zones sans choix reprennent d'abord la couleur du matériau
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(Index));
			if (!MID || !MID->Parent)
			{
				continue;
			}
			for (int32 Zone = 0; Zone < 4; ++Zone)
			{
				if (Appearance.FindPartColor(Slot, Zone) && !IsScalpMaterial(Component, Index))
				{
					continue; // crâne : toujours remis à sa couleur d'origine
				}
				const FName Parameter = TrainerPartColorParameter(Zone);
				FLinearColor Original;
				if (MID->Parent->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Original))
				{
					MID->SetVectorParameterValue(Parameter, Original);
				}
			}
		}
		for (const FTrainerPartColor& Choice : Appearance.PartColors)
		{
			if (Choice.Slot != Slot)
			{
				continue;
			}
			const FName Parameter = TrainerPartColorParameter(Choice.Zone);
			for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
			{
				if (IsScalpMaterial(Component, Index))
				{
					continue;
				}
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(Index));
				if (!MID)
				{
					MID = Component->CreateDynamicMaterialInstance(Index);
				}
				if (MID)
				{
					MID->SetVectorParameterValue(Parameter, Choice.Color);
					if (Choice.Zone == 3)
					{
						ApplyHeadFrame(Component, MID);
					}
				}
			}
		}
	}

	/**
	 *  Pièces sans modèle (maquillage, motif d'iris) : leur texture et leurs couleurs vont dans les matériaux de la
	 *  pièce suivie (la tête). Un emplacement vide remet la texture du matériau et coupe les couleurs (alpha 0).
	 */
	void ApplyMasks(const UTrainerAppearanceData* Data, const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance, AActor* Owner)
	{
		TArray<const FTrainerPartSlotDef*> MaskSlots;
		TSet<FName> Followed;
		for (const FTrainerPartSlotDef& SlotDef : Data->PartSlots)
		{
			if (!SlotDef.MaskParameter.IsNone() && !SlotDef.FollowSlot.IsNone())
			{
				MaskSlots.Add(&SlotDef);
				Followed.Add(SlotDef.FollowSlot);
			}
		}
		if (MaskSlots.IsEmpty() || !Owner)
		{
			return;
		}
		const TMap<FName, const FTrainerPartDef*> Worn = Data->ResolveParts(Base, Appearance);
		const TCHAR* Channels[] = { TEXT("_R"), TEXT("_G"), TEXT("_B") };

		TArray<USkeletalMeshComponent*> Components;
		Owner->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (!Component->ComponentHasTag(PartTag) || Component->ComponentTags.Num() < 2 || !Component->GetSkeletalMeshAsset()
				|| !Followed.Contains(Component->ComponentTags[1]))
			{
				continue;
			}
			const FName FollowedSlot = Component->ComponentTags[1];
			for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(Index));
				if (!MID)
				{
					MID = Component->CreateDynamicMaterialInstance(Index);
				}
				if (!MID)
				{
					continue;
				}
				for (const FTrainerPartSlotDef* SlotDef : MaskSlots)
				{
					if (SlotDef->FollowSlot != FollowedSlot)
					{
						continue;
					}
					const FTrainerPartDef* const* Found = Worn.Find(SlotDef->Slot);
					const FTrainerPartDef* Part = Found ? *Found : nullptr;
					const FString Prefix = SlotDef->MaskParameter.ToString();

					if (!Part)
					{
						// Rien dans cet emplacement : texture d'origine (iris naturel), couleurs du maquillage coupées
						UTexture* Original = nullptr;
						if (MID->Parent && MID->Parent->GetTextureParameterValue(FHashedMaterialParameterInfo(SlotDef->MaskParameter), Original) && Original)
						{
							MID->SetTextureParameterValue(SlotDef->MaskParameter, Original);
						}
						for (int32 Channel = 0; Channel < 3; ++Channel)
						{
							MID->SetVectorParameterValue(FName(*(Prefix + Channels[Channel])), FLinearColor(0.f, 0.f, 0.f, 0.f));
						}
						// Réglages des pièces de cet emplacement remis à la valeur du matériau
						TSet<FName> Scalars;
						TSet<FName> Extras;
						for (const FTrainerPartDef& Other : Data->Parts)
						{
							if (Other.Slot == SlotDef->Slot)
							{
								for (const TPair<FName, float>& Scalar : Other.MaskScalars)
								{
									Scalars.Add(Scalar.Key);
								}
								for (const TPair<FName, TSoftObjectPtr<UTexture>>& Extra : Other.MaskExtraTextures)
								{
									Extras.Add(Extra.Key);
								}
							}
						}
						for (const FName Extra : Extras)
						{
							UTexture* ExtraOriginal = nullptr;
							if (MID->Parent && MID->Parent->GetTextureParameterValue(FHashedMaterialParameterInfo(Extra), ExtraOriginal) && ExtraOriginal)
							{
								MID->SetTextureParameterValue(Extra, ExtraOriginal);
							}
						}
						for (const FName Scalar : Scalars)
						{
							float Value = 0.f;
							if (MID->Parent && MID->Parent->GetScalarParameterValue(FHashedMaterialParameterInfo(Scalar), Value))
							{
								MID->SetScalarParameterValue(Scalar, Value);
							}
						}
						continue;
					}
					if (UTexture* Texture = Part->MaskTexture.LoadSynchronous())
					{
						MID->SetTextureParameterValue(SlotDef->MaskParameter, Texture);
					}
					for (const TPair<FName, float>& Scalar : Part->MaskScalars)
					{
						MID->SetScalarParameterValue(Scalar.Key, Scalar.Value);
					}
					for (const TPair<FName, TSoftObjectPtr<UTexture>>& Extra : Part->MaskExtraTextures)
					{
						if (UTexture* ExtraTexture = Extra.Value.LoadSynchronous())
						{
							MID->SetTextureParameterValue(Extra.Key, ExtraTexture);
						}
					}
					// Couleurs des canaux R, V, B : choix du nuancier (zones 1 à 3), sinon celles de l'export inZOI
					for (int32 Channel = 0; Channel < 3 && Channel < Part->MaskColors.Num(); ++Channel)
					{
						const FTrainerPartColor* Choice = Appearance.FindPartColor(SlotDef->Slot, Channel + 1);
						FLinearColor Color = Choice ? Choice->Color : Part->MaskColors[Channel];
						if (Part->MaskColors[Channel].A <= 0.f)
						{
							Color.A = 0.f; // canal désactivé dans inZOI
						}
						MID->SetVectorParameterValue(FName(*(Prefix + Channels[Channel])), Color);
					}
				}
			}
		}
	}
}

UTrainerAppearanceData* TrainerAppearanceRender::LoadDefaultData()
{
	if (UTrainerAppearanceData* Imported = LoadObject<UTrainerAppearanceData>(nullptr, ImportedAppearanceDataPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		return Imported;
	}
	return LoadObject<UTrainerAppearanceData>(nullptr, DefaultAppearanceDataPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FTrainerAppearance TrainerAppearanceRender::Sanitize(const UTrainerAppearanceData* Data, const FTrainerAppearance& Appearance)
{
	FTrainerAppearance Clean = Appearance;
	const FTrainerBaseDef* Base = Data ? Data->FindBase(Appearance.Base) : nullptr;
	if (!Base)
	{
		return Clean; // pas de catalogue : on garde tel quel (rien ne s'affichera de toute façon)
	}
	Clean.Base = Base->Id;
	Clean.Sliders.Reset();
	for (const FTrainerSliderValue& Value : Appearance.Sliders)
	{
		if (const FTrainerSliderDef* Def = Data->FindSlider(*Base, Value.Slider))
		{
			const float Min = Def->HasNegative() ? -Def->Limit : 0.f;
			Clean.SetSlider(Value.Slider, FMath::Clamp(Value.Value, Min, Def->Limit));
		}
	}
	auto ClampColor = [](const FLinearColor& C) { return FLinearColor(FMath::Clamp(C.R, 0.f, 1.f), FMath::Clamp(C.G, 0.f, 1.f), FMath::Clamp(C.B, 0.f, 1.f), 1.f); };
	Clean.SkinColor = ClampColor(Appearance.SkinColor);
	Clean.EyeColor = ClampColor(Appearance.EyeColor);
	// Pièces : on garde celles du catalogue qui vont à cette base, une par emplacement (la dernière gagne), et les « -Emplacement » (Aucun)
	// Plus tard : vérifier que le joueur possède le vêtement (boutiques)
	Clean.Outfit.Reset();
	TMap<FName, FName> BySlot;
	for (const FName Id : Appearance.Outfit)
	{
		const FString Text = Id.ToString();
		if (Text.StartsWith(TEXT("-")))
		{
			const FName Slot(*Text.Mid(1));
			if (const FTrainerPartSlotDef* SlotDef = Data->FindSlot(Slot); SlotDef && !SlotDef->bRequired)
			{
				BySlot.Add(Slot, Id);
			}
		}
		else if (const FTrainerPartDef* Part = Data->FindPart(Id); Part && Part->FitsBase(Base->Id))
		{
			BySlot.Add(Part->Slot, Id);
		}
	}
	BySlot.GenerateValueArray(Clean.Outfit);
	// Couleurs des pièces : une par emplacement et par zone (la dernière gagne), zones 0 à 3
	Clean.PartColors.Reset();
	for (const FTrainerPartColor& Choice : Appearance.PartColors)
	{
		if (Choice.Slot.IsNone() || Choice.Zone < 0 || Choice.Zone > 3)
		{
			continue;
		}
		// L'alpha sert de réglage (début du dégradé, quantité de mèches, intensité du maquillage) : gardé entre 0 et 1
		const FLinearColor C = Choice.Color;
		Clean.SetPartColor(Choice.Slot, Choice.Zone, FLinearColor(FMath::Clamp(C.R, 0.f, 1.f), FMath::Clamp(C.G, 0.f, 1.f), FMath::Clamp(C.B, 0.f, 1.f), FMath::Clamp(C.A, 0.f, 1.f)));
	}
	return Clean;
}

bool TrainerAppearanceRender::Apply(const UTrainerAppearanceData* Data, AActor* Owner, USkeletalMeshComponent* Mesh, const FTrainerAppearance& InAppearance, FName& InOutAppliedBase)
{
	const FTrainerBaseDef* Base = Data ? Data->FindBase(InAppearance.Base) : nullptr;
	if (!Mesh || !Base)
	{
		return false;
	}
	const FTrainerAppearance Appearance = Sanitize(Data, InAppearance);

	if (InOutAppliedBase != Base->Id)
	{
		USkeletalMesh* NewMesh = Base->Mesh.LoadSynchronous();
		if (!NewMesh)
		{
			return false;
		}
		Mesh->SetSkeletalMesh(NewMesh);
		if (Base->AnimClass)
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->SetAnimInstanceClass(Base->AnimClass);
		}
		else if (!Mesh->GetAnimInstance())
		{
			// Aperçu sans animation (studio) : pose de référence + curseurs par os
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->SetAnimInstanceClass(UTrainerAnimInstance::StaticClass());
		}
		Mesh->EmptyOverrideMaterials(); // les MID de l'ancienne base ne vont pas sur la nouvelle
		InOutAppliedBase = Base->Id;
	}

	// Matériaux du corps (peau) : le MID déjà posé est gardé s'il vient du même matériau (plus rapide)
	for (int32 Index = 0; Index < Base->Materials.Num(); ++Index)
	{
		if (UMaterialInterface* Material = Base->Materials[Index].LoadSynchronous())
		{
			SetMaterialKeepingMID(Mesh, Index, Material);
		}
	}

	// Pièces (tête, vêtements) avant les couleurs : la peau de la tête doit aussi être teintée
	ApplyParts(Data, *Base, Appearance, Owner, Mesh);

	// Curseurs « par os » : seulement si l'animation sait les appliquer (TrainerAnimInstance ou un enfant)
	const TArray<FTrainerBoneOffset> Offsets = Data->ComputeBoneOffsets(*Base, Appearance);
	if (UTrainerAnimInstance* Anim = Cast<UTrainerAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->SetBoneOffsets(Offsets);
		Anim->SetIdleAnimation(Base->IdleAnimation.LoadSynchronous());
	}

	// Maquillage et motif d'iris sur la tête (avant les couleurs : les MID sont créés ici s'il le faut)
	ApplyMasks(Data, *Base, Appearance, Owner);

	// Le corps et ses pièces (vêtements en Leader Pose, tête qui copie la pose) reçoivent les mêmes morphs et couleurs
	TArray<USkeletalMeshComponent*> Targets = { Mesh };
	if (Owner)
	{
		TArray<USkeletalMeshComponent*> Children;
		Owner->GetComponents<USkeletalMeshComponent>(Children);
		for (USkeletalMeshComponent* Child : Children)
		{
			const bool bPart = Child->ComponentHasTag(PartTag) && Child->GetSkeletalMeshAsset();
			if (Child != Mesh && (Child->LeaderPoseComponent.Get() == Mesh || bPart))
			{
				Targets.Add(Child);
			}
		}
	}
	for (USkeletalMeshComponent* Target : Targets)
	{
		if (Target != Mesh)
		{
			if (UTrainerAnimInstance* PartAnim = Cast<UTrainerAnimInstance>(Target->GetAnimInstance()))
			{
				PartAnim->SetBoneOffsets(Offsets); // curseurs du visage sur les os de la tête
			}
		}
		ApplyMorphs(Data, *Base, Appearance, Target);
		ApplyColor(Target, Data->SkinMaterialSlots, Data->SkinColorParameter, Appearance.SkinColor);
		// Muscles (0 à 1) : relief des abdominaux, pectoraux... et couleur « musclée » du corps d'inZOI. La corpulence
		// les cache (M_Overweight03 a Muscles à 1 : sans ça, un ventre rond montrait des abdominaux dessinés)
		const float Muscle = FMath::Clamp(Appearance.GetSlider(Data->MuscleSliderId), 0.f, 1.f);
		const float Fat = FMath::Clamp(Appearance.GetSlider(Data->MassSliderId), 0.f, 1.f);
		ApplySkinScalar(Target, Data->SkinMaterialSlots, TEXT("MuscleDefinition"), Muscle * (1.f - 0.6f * Fat));
		// Corpulence (0 à 1) : plis et poids du ventre du corps « fort » d'inZOI (_Body_Fat_D)
		ApplySkinScalar(Target, Data->SkinMaterialSlots, TEXT("BodyFat"), Fat);
		ApplyColor(Target, Data->EyeMaterialSlots, Data->EyeColorParameter, Appearance.EyeColor);
		ApplyPartColors(Target, Appearance);
	}
	return true;
}
