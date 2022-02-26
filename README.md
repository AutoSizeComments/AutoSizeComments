# AutoSizeComments
Plugin for Unreal Engine 4 that adds auto resizing comment boxes

Settings for the plugin are under Edit > Editor Preferences > Plugins > Auto Size Comments

Hold alt to quickly add / remove nodes from the comment boxes

UE4 forum page: https://forums.unrealengine.com/community/released-projects/1540542

Marketplace link: https://www.unrealengine.com/marketplace/auto-size-comments

This was originally a feature from the [Blueprint Assist Plugin](https://forums.unrealengine.com/unreal-engine/marketplace/120671), a plugin that provides automatic formatting and mouse-free node editing when working with blueprints.

---

## Building for your version of UE4

1. Download the plugin
2. Open `AutoSizeComments.uplugin` in a text editor and set `EngineVersion` to your desired version, by default it is set to (4.27.0). For example if you wanted to build for 5.0.0 it would be `"EngineVersion": "5.0.0",`
3. Open the command prompt and run the UnrealEngine batch file to build the plugin, with your download location and out location.

> "`%UNREAL_DESIRED%`/Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin -Plugin="`%DOWNLOAD_LOCATION%`/AutoSizeComments/AutoSizeComments.uplugin" -Package="`%OUT_LOCATION%`" -CreateSubFolder

Example: Building for desired version `UE_5.0`:

* `%UNREAL_DESIRED%`: C:/Program Files/Epic Games/UE_5.0
* `%DOWNLOAD_LOCATION%`: C:/Temp
* `%OUT_LOCATION%`: C:Temp/AutoSizeComments

> "**C:/Program Files/Epic Games/UE_5.0/Engine/Build/BatchFiles/RunUAT.bat**" BuildPlugin -Plugin="**C:Temp/AutoSizeComments/AutoSizeComments.uplugin**" -Package="**C:/TempAutoSizeComments**" -CreateSubFolder

4. Copy the built plugin folder from your package location (`C:/TempAutoSizeComments/AutoSizeComments`) into either your:
    * Project plugin location `%PROJECT_DIR%/Plugins`
    * Engine plugin marketplace folder `C:/Epic Games/UE_5.0/Engine/Plugins/Marketplace`