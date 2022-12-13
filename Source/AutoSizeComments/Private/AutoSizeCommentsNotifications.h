// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/LazySingleton.h"

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
