// Copyright 2021 fpwong. All Rights Reserved.

using UnrealBuildTool;

public class AutoSizeComments : ModuleRules
{
	public AutoSizeComments(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
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
                "AssetRegistry"
            }
            );
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
