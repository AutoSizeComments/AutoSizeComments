// Copyright 2021 fpwong. All Rights Reserved.

using UnrealBuildTool;

public class AutoSizeComments : ModuleRules
{
	public AutoSizeComments(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				// ... add other public dependencies that you statically link with here ...
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",

				// ... add private dependencies that you statically link with here ...
				"GraphEditor",
				"BlueprintGraph",
				"EditorStyle",
				"UnrealEd",
				"InputCore",
				"Projects",
				"Json",
				"JsonUtilities",
				"EngineSettings",
				"AssetRegistry",
				"EditorSubsystem"
			}
		);
	}
}