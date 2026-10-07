// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RaidIsComing : ModuleRules
{
	public RaidIsComing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("ProceduralMeshComponent");

        PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"RaidIsComing",
			"RaidIsComing/Variant_Strategy",
			"RaidIsComing/Variant_Strategy/UI",
			"RaidIsComing/Variant_TwinStick",
			"RaidIsComing/Variant_TwinStick/AI",
			"RaidIsComing/Variant_TwinStick/Gameplay",
			"RaidIsComing/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
