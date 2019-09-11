// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AutoSizeComments.h"
#include "AutoSizeGraphNodeFactory.h"
#include "AutoSizeSettings.h"
#include "AutoSizeCommentsCacheFile.h"

#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FAutoSizeCommentsModule"

DEFINE_LOG_CATEGORY(LogASC)

class FAutoSizeCommentsModule : public IAutoSizeCommentsModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	FASCCacheFile& GetSizeCache() override { return Cache; }

private:
	TSharedPtr<FAutoSizeGraphNodeFactory> AutoSizeGraphNodeFactory;

	FASCCacheFile Cache;
};

IMPLEMENT_MODULE(FAutoSizeCommentsModule, AutoSizeComments)

void FAutoSizeCommentsModule::StartupModule()
{
	// Register the graph node factory
	AutoSizeGraphNodeFactory = MakeShareable(new FAutoSizeGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(AutoSizeGraphNodeFactory);

	// Register custom settings to appear in the project settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Editor", "Plugins", "Auto Size Comments",
			LOCTEXT("AutoSizeCommentsName", "Auto Size Comments"),
			LOCTEXT("AutoSizeCommentsNameDesc", "Configure options for auto resizing comment boxes"),
			GetMutableDefault<UAutoSizeSettings>()
		);
	}
}

void FAutoSizeCommentsModule::ShutdownModule()
{
	// Remove custom settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		SettingsModule->UnregisterSettings("Editor", "Plugins", "Auto Size Comments");

	// Unregister the graph node factory
	if (AutoSizeGraphNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(AutoSizeGraphNodeFactory);
		AutoSizeGraphNodeFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
