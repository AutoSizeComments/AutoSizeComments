// Copyright 2020 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsGraphPanelNodeFactory.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsCacheFile.h"

#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FAutoSizeCommentsModule"

DEFINE_LOG_CATEGORY(LogAutoSizeComments)

class FAutoSizeCommentsModule : public IAutoSizeCommentsModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	FAutoSizeCommentsCacheFile& GetSizeCache() override { return Cache; }

private:
	TSharedPtr<FAutoSizeCommentsGraphPanelNodeFactory> ASCNodeFactory;

	FAutoSizeCommentsCacheFile Cache;
};

IMPLEMENT_MODULE(FAutoSizeCommentsModule, AutoSizeComments)

void FAutoSizeCommentsModule::StartupModule()
{
	// Register the graph node factory
	ASCNodeFactory = MakeShareable(new FAutoSizeCommentsGraphPanelNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(ASCNodeFactory);

	// Register custom settings to appear in the project settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Editor", "Plugins", "AutoSizeComments",
			LOCTEXT("AutoSizeCommentsName", "Auto Size Comments"),
			LOCTEXT("AutoSizeCommentsNameDesc", "Configure the Auto Size Comments plugin"),
			GetMutableDefault<UAutoSizeCommentsSettings>()
		);
	}
}

void FAutoSizeCommentsModule::ShutdownModule()
{
	// Remove custom settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "AutoSizeComments");
	}

	// Unregister the graph node factory
	if (ASCNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(ASCNodeFactory);
		ASCNodeFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
