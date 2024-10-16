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
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",

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
				"MaterialEditor", 
			}
		);

		if (true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"RigVMDeveloper",
					"RigVMEditor",
				}
			);
		}
	}
}