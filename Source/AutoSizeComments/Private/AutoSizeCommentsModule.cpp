// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsModule.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphPanelNodeFactory.h"
#include "AutoSizeCommentsSettings.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FAutoSizeCommentsModule"

DEFINE_LOG_CATEGORY(LogAutoSizeComments)

class FAutoSizeCommentsModule final : public IAutoSizeCommentsModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	virtual FAutoSizeCommentsCacheFile& GetSizeCache() override { return Cache; }

private:
	TSharedPtr<FAutoSizeCommentsGraphPanelNodeFactory> ASCNodeFactory;

	FAutoSizeCommentsCacheFile Cache;

	void SuggestBlueprintAssistSettings();
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

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAutoSizeCommentsModule::SuggestBlueprintAssistSettings);
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

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

void FAutoSizeCommentsModule::SuggestBlueprintAssistSettings()
{
	if (FModuleManager::Get().IsModuleLoaded("BlueprintAssist"))
	{
		if (GetDefault<UAutoSizeCommentsSettings>()->bShowPromptForBlueprintAssist)
		{
			UAutoSizeCommentsSettings* MutableSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
			MutableSettings->bShowPromptForBlueprintAssist = false;
			MutableSettings->SaveConfig();

			const FText Message = FText::FromString("The Blueprint Assist module is loaded, would you like to apply suggested settings?");
			const FText Title = FText::FromString("Auto Size Comments");
			if (FMessageDialog::Open(EAppMsgType::OkCancel, Message, &Title) == EAppReturnType::Ok)
			{
				MutableSettings->bIgnoreKnotNodes = true;
				MutableSettings->bIgnoreKnotNodesWhenResizing = true;
				MutableSettings->bIgnoreKnotNodesWhenPressingAlt = true;
				UE_LOG(LogAutoSizeComments, Warning, TEXT("Applied suggested settings for Blueprint Assist Module"));
				UE_LOG(LogAutoSizeComments, Warning, TEXT("Ignore Knot Nodes: True"));
				UE_LOG(LogAutoSizeComments, Warning, TEXT("Ignore Knot Nodes When Resizing: True"));
				UE_LOG(LogAutoSizeComments, Warning, TEXT("Ignore Knot Nodes When Pressing Alt: True"));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
