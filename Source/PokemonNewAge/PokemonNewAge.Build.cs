// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PokemonNewAge : ModuleRules
{
	public PokemonNewAge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

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
