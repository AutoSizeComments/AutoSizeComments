# AutoSizeComments
Plugin for Unreal Engine that adds auto resizing comment boxes

Settings for the plugin are under Edit > Editor Preferences > Plugins > Auto Size Comments

Hold alt to quickly add / remove nodes from the comment boxes

Unreal forum page: https://forums.unrealengine.com/t/free-autosizecomments/117614/123

Marketplace link: https://www.unrealengine.com/marketplace/auto-size-comments

This was originally a feature from the [Blueprint Assist Plugin](https://www.unrealengine.com/marketplace/en-US/product/blueprint-assist), a plugin that provides automatic formatting and mouse-free node editing when working with blueprints.

# Building the plugin

There are two methods of building the plugin. I suggest using the first method if you have a C++ project setup.

## From a C++ project

1. In your project folder, create a plugin folder `C:/UnrealProjects/MyProject/Plugins`

1. Clone the repo into the plugin folder

1. Open your project in your IDE and build the project

1. If everything built successfully, you can keep the plugin local to your project. Or you can move it into the engine plugin folder. 
    * `C:/Epic Games/UE_5.0/Engine/Plugins/Marketplace`

## Without a C++ project (Using RunUAT.bat)

1. Clone the repo
1. Open the command prompt and run the UnrealEngine batch file to build the plugin, with your download location and out location.

> "`%UNREAL_DESIRED%`/Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin -Plugin="`%DOWNLOAD_LOCATION%`/AutoSizeComments/AutoSizeComments.uplugin" -Package="`%OUT_LOCATION%`" -CreateSubFolder

Example: Building for desired version `UE_5.0`:

* `%UNREAL_DESIRED%`: C:/Program Files/Epic Games/UE_5.0
* `%DOWNLOAD_LOCATION%`: C:/Temp
* `%OUT_LOCATION%`: C:Temp/AutoSizeComments

> "**C:/Program Files/Epic Games/UE_5.0/Engine/Build/BatchFiles/RunUAT.bat**" BuildPlugin -Plugin="**C:Temp/AutoSizeComments/AutoSizeComments.uplugin**" -Package="**C:/TempAutoSizeComments**" -CreateSubFolder

4. Copy the built plugin folder from your package location (`C:/TempAutoSizeComments/AutoSizeComments`) into either your:
    * Project plugin location `%PROJECT_DIR%/Plugins`
    * Engine plugin marketplace folder `C:/Epic Games/UE_5.0/Engine/Plugins/Marketplace`
