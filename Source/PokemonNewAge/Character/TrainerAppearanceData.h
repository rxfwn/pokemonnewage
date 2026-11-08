#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/TrainerAppearance.h"
#include "TrainerAppearanceData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimSequenceBase;
class UMaterialInterface;
class UTexture;
class UTexture2D;

/** Onglet de l'éditeur où apparaît un curseur */
UENUM(BlueprintType)
enum class ETrainerSliderGroup : uint8
{
	Face UMETA(DisplayName="Visage"),
	Body UMETA(DisplayName="Corps")
};

/**
 *  Déplacement d'un os quand un curseur est au maximum (déformation « par os », comme inZOI).
 *  Ajouté à la pose de l'animation : Location en cm, Scale ajouté à l'échelle (0,3 = +30 %), Rotation composée.
 *  Appliqué par UTrainerAnimInstance ; le curseur à 0,5 donne la moitié.
 */
USTRUCT(BlueprintType)
struct FTrainerBoneOffset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FName Bone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FVector Scale = FVector::ZeroVector;
};

/**
 *  Un curseur de l'éditeur. Deux façons de déformer, cumulables :
 *  - par os (BonesPositive / BonesNegative), pour les corps importés (inZOI, voir docs/INTEGRATION_INZOI.md) ;
 *  - par morph targets exportés de Character Creator :
 *  vers la droite MorphPositive monte de 0 à 1, vers la gauche MorphNegative (s'il existe).
 *  Le même nom de morph est appliqué au corps ET aux vêtements qui l'ont (CC les exporte ensemble).
 */
USTRUCT(BlueprintType)
struct FTrainerSliderDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Id;

	/** Texte à l'écran (« Largeur du nez ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	/** Onglet Visage ou Corps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	ETrainerSliderGroup Group = ETrainerSliderGroup::Face;

	/** Titre de la section dans la liste : Visage, Yeux, Nez, Bouche, Oreilles, Silhouette... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Section;

	/** Nom exact du morph dans le Skeletal Mesh (onglet Morph Target Preview de l'éditeur de mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName MorphPositive;

	/** Optionnel : morph de l'autre sens. Vide = curseur de 0 à 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName MorphNegative;

	/** Bornes pour qu'on ne puisse pas « casser » le visage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=(ClampMin=0, ClampMax=1))
	float Limit = 1.f;

	/** Os déplacés quand le curseur va vers la droite (valeur au maximum) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	TArray<FTrainerBoneOffset> BonesPositive;

	/** Os déplacés quand le curseur va vers la gauche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	TArray<FTrainerBoneOffset> BonesNegative;

	/** Part de ce curseur poussée par le curseur global Muscles (MuscleSliderId) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	float MuscleCoef = 0.f;

	/** Part de ce curseur poussée par le curseur global Corpulence (MassSliderId) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	float MassCoef = 0.f;

	/** Curseur de -1 à 1 même sans morph ni os négatifs (Muscles, Corpulence) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bSigned = false;

	/** Vrai si le curseur descend sous 0 */
	bool HasNegative() const { return bSigned || !MorphNegative.IsNone() || !BonesNegative.IsEmpty(); }

	/** Vrai si le curseur déplace des os */
	bool HasBones() const { return !BonesPositive.IsEmpty() || !BonesNegative.IsEmpty(); }
};

/**
 *  Poignée à attraper directement sur le personnage en mode Détaillé (comme inZOI) : un point posé sur un os,
 *  glisser à gauche / à droite bouge SliderX, glisser en haut / en bas bouge SliderY.
 */
USTRUCT(BlueprintType)
struct FTrainerDragHandle
{
	GENERATED_BODY()

	/** Texte au survol (« Nez ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	ETrainerSliderGroup Group = ETrainerSliderGroup::Face;

	/** Os (ou socket) où se place le point, par exemple CC_Base_NoseTip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Bone;

	/** Décalage depuis l'os, en cm, dans le repère du personnage (X devant, Z en haut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FVector Offset = FVector::ZeroVector;

	/** Curseur bougé en glissant horizontalement (vide = aucun) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName SliderX;

	/** Curseur bougé en glissant verticalement, vers le haut = plus (vide = aucun) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName SliderY;

	/** Pixels de glissement (écran 1920 x 1080) pour passer de 0 à 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=(ClampMin=10))
	float PixelsPerUnit = 150.f;
};

/** Un modèle de la galerie : un personnage complet (onglet Modèles) ou seulement un visage (Visage > Modèles de visage) */
USTRUCT(BlueprintType)
struct FTrainerPresetDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	/** Vignette ronde (optionnelle : sinon le nom s'affiche dans le rond) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/**
	 *  L'apparence du modèle. Modèle complet : remplace tout (sauf les vêtements et le prénom).
	 *  Modèle de visage : seulement les curseurs listés ici, le reste ne bouge pas.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FTrainerAppearance Look;
};

/**
 *  Emplacement de pièce (tête, haut, bas, chaussures...). Chaque emplacement est un Skeletal Mesh qui suit le corps
 *  (Leader Pose). L'ordre de la liste est celui des sous-onglets de la Tenue.
 */
USTRUCT(BlueprintType)
struct FTrainerPartSlotDef
{
	GENERATED_BODY()

	/** Head, Top, Bottom, Outer, Full, Shoes, Acc... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Slot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	/** Onglet de l'éditeur : Visage (têtes, barbes), Tenue ou Accessoires */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Tab = TEXT("Outfit");

	/** Faux = on peut l'enlever (bouton « Aucun ») ; vrai pour la tête */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bRequired = false;

	/** Emplacements retirés quand celui-ci est porté (une robe enlève le haut et le bas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FName> Replaces;

	/** Emplacement que cette pièce suit au lieu du corps (la barbe suit la tête : Leader Pose sur la tête,
	 *  et le modèle de la variante faite pour cette tête, voir FTrainerPartDef::Variants). Vide = le corps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName FollowSlot;

	/**
	 *  Emplacement sans modèle 3D (maquillage, motif d'iris) : la pièce pose sa texture (FTrainerPartDef::MaskTexture)
	 *  dans ce paramètre des matériaux de la pièce suivie (FollowSlot, la tête). Pour le maquillage, les couleurs des
	 *  canaux R, V, B vont dans <MaskParameter>_R, _G, _B (alpha = intensité). Vide = pièce normale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName MaskParameter;

	/** Noms des zones de couleur dans le nuancier (index = zone : 0 = principale, 1 à 3) ; vide = « Principale », « Zone N » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FText> ZoneLabels;

	/** Curseur sous le nuancier qui règle l'alpha de la zone (début du dégradé, quantité de mèches, intensité) ; vide = pas de curseur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FText> ZoneAmountLabels;

	/** Alpha donné à une zone quand on lui choisit une couleur pour la première fois (1 si absent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<float> ZoneDefaultAmounts;

	/**
	 *  Vêtement qui pend loin du corps (haut, veste) : au lieu de la Leader Pose, il copie la pose du corps en bornant
	 *  l'échelle des os d'aide à 1 ± cette valeur (voir UTrainerAnimInstance::HelperScaleLimit). 0 = Leader Pose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=(ClampMin=0, ClampMax=2))
	float HelperScaleLimit = 0.f;

	FText GetZoneLabel(int32 Zone) const;
	float GetZoneDefaultAmount(int32 Zone) const { return ZoneDefaultAmounts.IsValidIndex(Zone) ? ZoneDefaultAmounts[Zone] : 1.f; }
};

/** Modèle d'une pièce ajusté à une tête précise (inZOI fait une barbe par tête) */
USTRUCT(BlueprintType)
struct FTrainerPartVariant
{
	GENERATED_BODY()

	/** Nom de l'asset de la tête portée (ex. SKM_Male_Head_002) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName HeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> Mesh;
};

/** Une pièce : une tête, un vêtement, une paire de chaussures... (catalogue rempli par Tools/InZOI/import_inzoi.py) */
USTRUCT(BlueprintType)
struct FTrainerPartDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	/** Emplacement (FTrainerPartSlotDef::Slot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Slot;

	/** Rangement dans la liste (T-shirt, Chemise, Pull...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Category;

	/** Bases qui peuvent la porter (vide = toutes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FName> Bases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Modèles ajustés à chaque tête (pièces d'un emplacement avec FollowSlot) ; Mesh sert pour les autres têtes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerPartVariant> Variants;

	/** Matériaux à mettre à la place de ceux du mesh, dans l'ordre des emplacements (vide = ceux du mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** Pièce sans modèle (emplacement avec MaskParameter) : masque de maquillage ou motif d'iris */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UTexture> MaskTexture;

	/** Maquillage : couleur de départ des canaux R, V, B du masque (alpha = intensité, 0 = canal inutilisé) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FLinearColor> MaskColors;

	/** Réglages posés avec la texture (ex. IrisTexEdge : bord de l'iris dans le motif) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TMap<FName, float> MaskScalars;

	/** Autres textures posées avec le masque (ex. SkinScarColor : couleur d'une cicatrice) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TMap<FName, TSoftObjectPtr<UTexture>> MaskExtraTextures;

	/** Faux = à acheter en boutique (pas encore branché : tout est disponible pour l'instant) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bStartOwned = true;

	bool FitsBase(FName Base) const { return Bases.IsEmpty() || Bases.Contains(Base); }
};

/** Animation proposée dans le studio de l'éditeur (bouton « Pose ») */
USTRUCT(BlueprintType)
struct FTrainerStudioAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UAnimSequenceBase> Animation;
};

/** Une base de personnage (Homme, Femme), exportée de Character Creator avec Auto Setup */
USTRUCT(BlueprintType)
struct FTrainerBaseDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Matériaux posés sur le corps, un par emplacement (vide = ceux du mesh). Le script inZOI y met la peau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/**
	 *  Animation Blueprint retargeté (vide = garder celui du personnage). Pour que les curseurs « par os » marchent,
	 *  sa classe parente doit être TrainerAnimInstance (ou TrainerAnimInstance lui-même : pose de référence).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSubclassOf<UAnimInstance> AnimClass;

	/** Animation d'attente du studio quand Anim Class n'a pas de graphe (TrainerAnimInstance seul) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimation;

	/** Poses et gestes du studio (bouton « Pose » de l'éditeur), après l'attente */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerStudioAnimation> StudioAnimations;

	/** Bouton de l'éditeur (♂ ou ♀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bMale = false;

	/** Curseurs propres à cette base (les curseurs communs sont dans CommonSliders) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerSliderDef> Sliders;

	/** Pièces portées quand le joueur n'a rien choisi pour un emplacement (la tête, une tenue de départ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FName> DefaultParts;
};

/**
 *  Catalogue de l'éditeur de personnage : bases, curseurs, réglages des matériaux.
 *  Un seul asset dans le jeu : Content/Characters/Player/DA_TrainerAppearance.
 *  Ajouter un curseur = une ligne ici, pas de code.
 */
UCLASS(BlueprintType)
class UTrainerAppearanceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerBaseDef> Bases;

	/** Curseurs valables pour toutes les bases (même noms de morphs chez l'homme et la femme) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TArray<FTrainerSliderDef> CommonSliders;

	/** Emplacements de matériau qui reçoivent la couleur de peau (Std_Skin_Head, Std_Skin_Body...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	TArray<FName> SkinMaterialSlots;

	/** Paramètre couleur des matériaux de peau à teinter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	FName SkinColorParameter = TEXT("SkinTint");

	/** Emplacements des yeux (Std_Eye_L, Std_Eye_R...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	TArray<FName> EyeMaterialSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
	FName EyeColorParameter = TEXT("IrisColor");

	/** Points à attraper sur le visage et le corps (mode Détaillé) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FTrainerDragHandle> DragHandles;

	/** Onglet Modèles : personnages complets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FTrainerPresetDef> FullPresets;

	/** Visage > Modèles de visage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FTrainerPresetDef> FacePresets;

	/** Corps > Silhouettes : seulement les curseurs du corps, pour la base de Look.Base (toutes si vide) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FTrainerPresetDef> BodyPresets;

	/** Teintes de peau proposées (Visage > Peau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FLinearColor> SkinPalette;

	/** Couleurs d'yeux proposées (Visage > Yeux) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FLinearColor> EyePalette;

	/** Emplacements de pièces, dans l'ordre d'affichage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parts")
	TArray<FTrainerPartSlotDef> PartSlots;

	/** Toutes les pièces (têtes, vêtements, chaussures, accessoires) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parts")
	TArray<FTrainerPartDef> Parts;

	/** Curseur global qui pousse tous les groupes d'os selon leur MuscleCoef */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	FName MuscleSliderId = TEXT("Muscle");

	/** Curseur global qui pousse tous les groupes d'os selon leur MassCoef */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	FName MassSliderId = TEXT("Masse");

	/** Multiplie toutes les déformations par os (pour adoucir si c'est trop fort) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones", meta=(ClampMin=0, ClampMax=2))
	float BoneDeformationScale = 1.f;

	/** Part de Muscles et Corpulence ajoutée à chaque groupe d'os du corps (coefficients delta_muscle / delta_masse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sliders", meta=(ClampMin=0, ClampMax=1))
	float GlobalSliderShare = 0.35f;

	/** Essai : échange les axes Y et Z des déplacements et échelles d'os (si l'import .glb a tourné les repères des os) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bones")
	bool bSwapBoneYZ = false;

	/** Os que la caméra vise en gros plan sur le visage (le premier trouvé dans le mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editor")
	TArray<FName> HeadBones = { TEXT("CC_Base_Head"), TEXT("head"), TEXT("FACIAL_C_FacialRoot") };

	UTrainerAppearanceData();

	const FTrainerBaseDef* FindBase(FName Id) const;

	/** Curseur de la base ou commun, nullptr si inconnu */
	const FTrainerSliderDef* FindSlider(const FTrainerBaseDef& Base, FName Slider) const;

	/** Tous les curseurs d'une base (communs puis propres) */
	TArray<FTrainerSliderDef> GetSliders(const FTrainerBaseDef& Base) const;

	const FTrainerPartDef* FindPart(FName Id) const;
	const FTrainerPartSlotDef* FindSlot(FName Slot) const;

	/** Pièce effectivement portée par emplacement : choix du joueur, sinon pièce par défaut de la base ; robes et combinaisons retirent haut et bas */
	TMap<FName, const FTrainerPartDef*> ResolveParts(const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance) const;

	/** Déplacements d'os de toute l'apparence (curseurs + Muscles + Corpulence), un par os */
	TArray<FTrainerBoneOffset> ComputeBoneOffsets(const FTrainerBaseDef& Base, const FTrainerAppearance& Appearance) const;
};
