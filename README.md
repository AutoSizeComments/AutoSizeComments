# AutoSizeComments
Plugin for Unreal Engine 4 that adds auto resizing comment boxes

UE4 forum page: https://forums.unrealengine.com/community/released-projects/1540542

Marketplace link: [Waiting for approval]

This was originally a feature from the [Blueprint Assist Plugin](https://forums.unrealengine.com/unreal-engine/marketplace/120671), a plugin that provides automatic formatting and mouse-free node editing when working with blueprints (coming soon!).

---

## Building for your version of UE4 (tested with >4.18)

Prerequisites: Setup your environment so that you can build a C++ UE4 project

1. Create a new **C++** project
2. Create a folder named `Plugins` in the root of your new project (see below)
3. Clone the repo into the plugins folder, so that you get `Plugins/AutoSizeComments`

Your project folder should look like:

```
ProjectName
    \.vs
    \Binaries
    \Config
    \Content
    \Intermediate
    \Source
    \Plugins
        \AutoSizeComments
    \ProjectName.sln
    \ProjectName.uproject
```
  
4. Open the .uproject and it should prompt you to rebuild the plugin
5. Hopefully the build succeeded!
6. (Extra) Install the plugin to the engine by moving the `AutoSizeComments` folder to the plugins folder under your installation of UE4 `Epic Games\UE_4.20\Engine\Plugins`.

---

## Build failed

1. Open the project in Visual Studio
2. Try to build the project and there should be an error
3. Tell me the version of your UE4 and what the error is over at the forum page
