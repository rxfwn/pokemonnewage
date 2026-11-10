#include "Character/InZOIImportLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

#if WITH_EDITOR
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#endif

#if WITH_UEFORMAT
#include "Readers/UEFAnimReader.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#endif

bool UInZOIImportLibrary::CanImportUEAnim()
{
	return WITH_UEFORMAT != 0;
}

UAnimSequence* UInZOIImportLibrary::ImportUEAnim(const FString& Filename, USkeleton* Skeleton, const FString& AssetPath)
{
#if WITH_UEFORMAT
	if (!Skeleton || !FPackageName::IsValidLongPackageName(AssetPath))
	{
		return nullptr;
	}
	UEFAnimReader Data(Filename);
	if (!Data.Read() || Data.NumFrames <= 0)
	{
		return nullptr;
	}
	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		return nullptr;
	}
	const FName Name(*FPackageName::GetLongPackageAssetName(AssetPath));

	// Même remplissage que UEFAnimFactory::FactoryCreateFile du plugin, sans la fenêtre de choix du squelette
	UAnimSequence* AnimSequence = NewObject<UAnimSequence>(Package, Name, RF_Public | RF_Standalone);
	AnimSequence->SetSkeleton(Skeleton);
	IAnimationDataController& Controller = AnimSequence->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Import UEAnim inZOI")));
	Controller.InitializeModel();
	AnimSequence->ResetAnimation();
	Controller.SetFrameRate(FFrameRate(FMath::RoundToInt(Data.FramesPerSecond), 1));
	Controller.SetNumberOfFrames(FFrameNumber(Data.NumFrames));

	for (const FTrack& Track : Data.Tracks)
	{
		const FName BoneName(Track.TrackName.c_str());
		if (Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
		{
			continue; // os absent de ce squelette (ex. accessoire d'inZOI)
		}
		TArray<FVector3f> PosKeys;
		TArray<FQuat4f> RotKeys;
		TArray<FVector3f> ScaleKeys;
		PosKeys.SetNum(Data.NumFrames);
		RotKeys.SetNum(Data.NumFrames);
		ScaleKeys.SetNum(Data.NumFrames);
		FVector3f PrevPos = FVector3f::ZeroVector;
		FQuat4f PrevRot = FQuat4f::Identity;
		FVector3f PrevScale = FVector3f::OneVector;
		int32 PosIndex = 0, RotIndex = 0, ScaleIndex = 0;
		for (int32 Frame = 0; Frame < Data.NumFrames; ++Frame)
		{
			if (PosIndex < Track.TrackPosKeys.Num() && Track.TrackPosKeys[PosIndex].Frame == Frame)
			{
				PrevPos = Track.TrackPosKeys[PosIndex++].VectorValue;
			}
			if (RotIndex < Track.TrackRotKeys.Num() && Track.TrackRotKeys[RotIndex].Frame == Frame)
			{
				PrevRot = Track.TrackRotKeys[RotIndex++].QuatValue;
			}
			if (ScaleIndex < Track.TrackScaleKeys.Num() && Track.TrackScaleKeys[ScaleIndex].Frame == Frame)
			{
				PrevScale = Track.TrackScaleKeys[ScaleIndex++].VectorValue;
			}
			PosKeys[Frame] = PrevPos;
			RotKeys[Frame] = PrevRot;
			ScaleKeys[Frame] = PrevScale;
		}
		Controller.AddBoneCurve(BoneName);
		Controller.SetBoneTrackKeys(BoneName, PosKeys, RotKeys, ScaleKeys);
	}

	for (const FCurve& Curve : Data.Curves)
	{
		TArray<FRichCurveKey> Keys;
		for (const FFloatKey& Key : Curve.CurveKeys)
		{
			FRichCurveKey RichKey;
			RichKey.Time = Data.FramesPerSecond > 0.f ? Key.Frame / Data.FramesPerSecond : 0.f;
			RichKey.Value = Key.FloatValue;
			Keys.Add(RichKey);
		}
		const FAnimationCurveIdentifier CurveId(FName(Curve.CurveName.c_str()), ERawCurveTrackTypes::RCT_Float);
		Controller.AddCurve(CurveId);
		Controller.SetCurveKeys(CurveId, Keys, false);
	}

	Controller.NotifyPopulated();
	Controller.CloseBracket();
	AnimSequence->PostEditChange();
	FAssetRegistryModule::AssetCreated(AnimSequence);
	Package->MarkPackageDirty();
	return AnimSequence;
#else
	return nullptr;
#endif
}

bool UInZOIImportLibrary::SetFunctionIOId(UMaterialExpression* Expression, const FString& GuidText)
{
#if WITH_EDITOR
	FGuid Id;
	if (!FGuid::Parse(GuidText, Id))
	{
		return false;
	}
	if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression))
	{
		Input->Id = Id;
		return true;
	}
	if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expression))
	{
		Output->Id = Id;
		return true;
	}
#endif
	return false;
}

int32 UInZOIImportLibrary::CountUVChannels(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh || !Mesh->HasMeshDescription(0))
	{
		return -1;
	}
	if (FMeshDescription* Description = Mesh->GetMeshDescription(0))
	{
		FSkeletalMeshAttributes Attributes(*Description);
		return Attributes.GetVertexInstanceUVs().GetNumChannels();
	}
#endif
	return -1;
}

FString UInZOIImportLibrary::BakeHairStrands(USkeletalMesh* Mesh, FVector FrontAxis, float FrontAngleDegrees, float MinHeightAboveCenter, float MinForward)
{
#if WITH_EDITOR
	if (!Mesh || !Mesh->HasMeshDescription(0))
	{
		return FString();
	}
	FMeshDescription* Description = Mesh->GetMeshDescription(0);
	if (!Description)
	{
		return FString();
	}
	FSkeletalMeshAttributes Attributes(*Description);
	Attributes.Register();
	TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();

	// Îlots : sommets reliés par des triangles (union-find)
	const int32 NumVertices = Description->Vertices().GetArraySize();
	TArray<int32> Parent;
	Parent.SetNumUninitialized(NumVertices);
	for (int32 Index = 0; Index < NumVertices; ++Index)
	{
		Parent[Index] = Index;
	}
	auto Find = [&Parent](int32 X)
	{
		while (Parent[X] != X)
		{
			Parent[X] = Parent[Parent[X]];
			X = Parent[X];
		}
		return X;
	};
	for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
	{
		TArrayView<const FVertexID> Corners = Description->GetTriangleVertices(Triangle);
		for (int32 Corner = 1; Corner < Corners.Num(); ++Corner)
		{
			const int32 A = Find(Corners[0].GetValue());
			const int32 B = Find(Corners[Corner].GetValue());
			if (A != B)
			{
				Parent[B] = A;
			}
		}
	}

	// Centre du crâne : milieu de la boîte de la coupe à l'horizontale, os « head » + 9 cm en hauteur
	FBox Bounds(ForceInit);
	for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
	{
		Bounds += FVector(Positions[Vertex]);
	}
	FVector Center = Bounds.GetCenter();
	const int32 HeadBone = Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head"));
	if (HeadBone != INDEX_NONE)
	{
		FTransform HeadTransform = Mesh->GetRefSkeleton().GetRefBonePose()[HeadBone];
		for (int32 Bone = Mesh->GetRefSkeleton().GetParentIndex(HeadBone); Bone != INDEX_NONE; Bone = Mesh->GetRefSkeleton().GetParentIndex(Bone))
		{
			HeadTransform = HeadTransform * Mesh->GetRefSkeleton().GetRefBonePose()[Bone];
		}
		Center.Z = HeadTransform.GetLocation().Z + 9.f;
	}
	FVector Front = FrontAxis;
	Front.Z = 0.f;
	if (!Front.Normalize())
	{
		Front = FVector(0.f, 1.f, 0.f);
	}

	// Racine de chaque îlot : son point le plus haut (les mèches tombent du crâne). Le plus proche du centre du crâne
	// tombait souvent au niveau de l'oreille pour les coupes longues : les mèches qui encadrent le visage étaient ratées.
	TMap<int32, FVertexID> Roots;
	TMap<int32, float> RootHeights;
	for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
	{
		const int32 Island = Find(Vertex.GetValue());
		const float Height = Positions[Vertex].Z;
		float* Best = RootHeights.Find(Island);
		if (!Best || Height > *Best)
		{
			RootHeights.Add(Island, Height);
			Roots.Add(Island, Vertex);
		}
	}
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(FrontAngleDegrees));
	TMap<int32, FVector4f> IslandColors;
	int32 FrontCount = 0;
	for (const TPair<int32, FVertexID>& Pair : Roots)
	{
		const FVector Root(Positions[Pair.Value]);
		FVector Flat = Root - Center;
		const float Height = Flat.Z;
		Flat.Z = 0.f;
		// Devant : dans l'angle, pas trop bas, et assez en avant (racine à la lisière du front ou aux tempes) ; sans la
		// distance, toutes les mèches parties de la raie au sommet du crâne passaient (422 cartes sur 642, coupe longue)
		const float Forward = FVector::DotProduct(Root - Center, Front);
		const bool bFront = Flat.Normalize() && FVector::DotProduct(Flat, Front) >= CosLimit && Height >= MinHeightAboveCenter
			&& Forward >= MinForward;
		FrontCount += bFront ? 1 : 0;
		// Numéro de mèche : hachage de l'îlot (stable d'un import à l'autre tant que le mesh ne change pas)
		const float Random = float(FCrc::MemCrc32(&Pair.Key, sizeof(int32)) & 0xFFFF) / 65535.f;
		IslandColors.Add(Pair.Key, FVector4f(bFront ? 1.f : 0.f, Random, 0.f, 1.f));
	}
	for (const FVertexInstanceID Instance : Description->VertexInstances().GetElementIDs())
	{
		const int32 Island = Find(Description->GetVertexInstanceVertex(Instance).GetValue());
		if (const FVector4f* Color = IslandColors.Find(Island))
		{
			Colors[Instance] = *Color;
		}
	}
	USkeletalMesh::FCommitMeshDescriptionParams Params;
	Params.bMarkPackageDirty = true;
	Mesh->CommitMeshDescription(0, Params);
	Mesh->SetHasVertexColors(true);
	Mesh->Build();
	Mesh->PostEditChange();
	return FString::Printf(TEXT("%d/%d"), FrontCount, Roots.Num());
#else
	return FString();
#endif
}
