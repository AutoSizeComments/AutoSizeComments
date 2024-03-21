// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/LazySingleton.h"

class SNotificationItem;
class ISourceControlModule;
class ISourceControlProvider;

class AUTOSIZECOMMENTS_API FAutoSizeCommentsNotifications
{
public:
	static FAutoSizeCommentsNotifications& Get();
	static void TearDown();

	void Initialize();
	void Shutdown();

protected:
	// ~~ Source control related
	TWeakPtr<SNotificationItem> SourceControlNotification;
	FDelegateHandle SourceControlNotificationDelegate;
	void ShowSourceControlNotification();
	void HandleSourceControlProviderChanged(ISourceControlProvider& OldSourceControlProvider, ISourceControlProvider& NewControlProvider);
	bool ShouldShowSourceControlNotification();
	// ~~ Source control related

	// ~~ Blueprint Assist related
	TWeakPtr<SNotificationItem> BlueprintAssistNotification;
	void ShowBlueprintAssistNotification();
	// ~~ Blueprint Assist related

	ISourceControlModule* GetSourceControlModule(); 
};
