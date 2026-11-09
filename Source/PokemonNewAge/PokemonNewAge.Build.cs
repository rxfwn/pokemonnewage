// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PokemonNewAge : ModuleRules
{
	public PokemonNewAge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Pas de build « unity » : sinon Unreal colle les .cpp entre eux et les noms
		// internes de deux fichiers (namespace anonyme) se masquent l'un l'autre (erreur C4459).
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"DeveloperSettings"
		});

		// ProceduralMeshComponent : fond arrondi du studio du créateur (ACharacterStudio)
		PrivateDependencyModuleNames.AddRange(new string[] { "Landscape", "ProceduralMeshComponent" });

		// Préparation des coupes de cheveux à l'import (InZOIImportLibrary::BakeHairStrands) : éditeur seulement
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription" });
		}

		// Import des animations inZOI (.ueanim) par Tools/InZOI/import_inzoi.py, sans fenêtre (InZOIImportLibrary) :
		// seulement dans l'éditeur, et seulement si le plugin UEFormat est dans Plugins/UEFormat (il n'est pas dans Git).
		bool bUEFormat = Target.bBuildEditor && Directory.Exists(Path.Combine(ModuleDirectory, "..", "..", "Plugins", "UEFormat"));
		if (bUEFormat)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UEFormat", "AssetRegistry" });
		}
		PublicDefinitions.Add("WITH_UEFORMAT=" + (bUEFormat ? "1" : "0"));

		PublicIncludePaths.AddRange(new string[] {
			"PokemonNewAge",
			"PokemonNewAge/Variant_Platforming",
			"PokemonNewAge/Variant_Platforming/Animation",
			"PokemonNewAge/Variant_Combat",
			"PokemonNewAge/Variant_Combat/AI",
			"PokemonNewAge/Variant_Combat/Animation",
			"PokemonNewAge/Variant_Combat/Gameplay",
			"PokemonNewAge/Variant_Combat/Interfaces",
			"PokemonNewAge/Variant_Combat/UI",
			"PokemonNewAge/Variant_SideScrolling",
			"PokemonNewAge/Variant_SideScrolling/AI",
			"PokemonNewAge/Variant_SideScrolling/Gameplay",
			"PokemonNewAge/Variant_SideScrolling/Interfaces",
			"PokemonNewAge/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
