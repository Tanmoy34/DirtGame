// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DartGame : ModuleRules
{
	public DartGame(ReadOnlyTargetRules Target) : base(Target)
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
			"DartGame",
			"DartGame/Variant_Platforming",
			"DartGame/Variant_Platforming/Animation",
			"DartGame/Variant_Combat",
			"DartGame/Variant_Combat/AI",
			"DartGame/Variant_Combat/Animation",
			"DartGame/Variant_Combat/Gameplay",
			"DartGame/Variant_Combat/Interfaces",
			"DartGame/Variant_Combat/UI",
			"DartGame/Variant_SideScrolling",
			"DartGame/Variant_SideScrolling/AI",
			"DartGame/Variant_SideScrolling/Gameplay",
			"DartGame/Variant_SideScrolling/Interfaces",
			"DartGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
