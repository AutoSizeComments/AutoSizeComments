// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsModule.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphPanelNodeFactory.h"
#include "AutoSizeCommentsInputProcessor.h"
#include "AutoSizeCommentsNotifications.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsStyle.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FAutoSizeCommentsModule"

#define ASC_ENABLED (!IS_MONOLITHIC && !UE_BUILD_SHIPPING && !UE_BUILD_TEST && !UE_GAME && !UE_SERVER)

DEFINE_LOG_CATEGORY(LogAutoSizeComments)

void FAutoSizeCommentsModule::StartupModule()
{
#if ASC_ENABLED
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAutoSizeCommentsModule::OnPostEngineInit);
#endif
}

void FAutoSizeCommentsModule::OnPostEngineInit()
{
	UE_LOG(LogAutoSizeComments, Log, TEXT("Startup AutoSizeComments"));

	FAutoSizeCommentsCacheFile::Get().Init();

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

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UAutoSizeCommentsSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FASCSettingsDetails::MakeInstance));
	}

	FAutoSizeCommentGraphHandler::Get().BindDelegates();

	FAutoSizeCommentsInputProcessor::Create();

	FAutoSizeCommentsNotifications::Get().Initialize();

	FASCStyle::Initialize();
}

void FAutoSizeCommentsModule::ShutdownModule()
{
#if ASC_ENABLED
	UE_LOG(LogAutoSizeComments, Log, TEXT("Shutdown AutoSizeComments"));

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

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

	FAutoSizeCommentGraphHandler::Get().UnbindDelegates();

	FAutoSizeCommentsInputProcessor::Cleanup();

	FAutoSizeCommentsNotifications::Get().Shutdown();

	FASCStyle::Shutdown();
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAutoSizeCommentsModule, AutoSizeComments)
