#include "Character/InZOIImportLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

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
