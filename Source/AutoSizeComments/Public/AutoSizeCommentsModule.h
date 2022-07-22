// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAutoSizeCommentsGraphPanelNodeFactory;
class UEdGraphNode_Comment;
class SAutoSizeCommentsGraphNode;
struct FASCState;
class FAutoSizeCommentsCacheFile;

DECLARE_LOG_CATEGORY_EXTERN(LogAutoSizeComments, Log, All)

class FAutoSizeCommentsModule final : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	/** IModuleInterface implementation */

	static FAutoSizeCommentsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAutoSizeCommentsModule>("AutoSizeComments");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AutoSizeComments");
	}

private:
	TSharedPtr<FAutoSizeCommentsGraphPanelNodeFactory> ASCNodeFactory;

	void OnPostEngineInit();
};
