// Fill out your copyright notice in the Description page of Project Settings.

#include "AutoSizeCommentsNotifications.h"

#include "AutoSizeCommentsMacros.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

FAutoSizeCommentsNotifications& FAutoSizeCommentsNotifications::Get()
{
	return TLazySingleton<FAutoSizeCommentsNotifications>::Get();
}

void FAutoSizeCommentsNotifications::TearDown()
{
	TLazySingleton<FAutoSizeCommentsNotifications>::TearDown();
}

void FAutoSizeCommentsNotifications::Initialize()
{
	ISourceControlModule* SourceControlModule = GetSourceControlModule();
	if (!SourceControlModule)
	{
		return;
	}

	 if (SourceControlModule->IsEnabled())
	{
		ShowSourceControlNotification();
	}
	else
	{
		SourceControlNotificationDelegate = SourceControlModule->RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateRaw(this, &FAutoSizeCommentsNotifications::HandleSourceControlProviderChanged));
	}

	// We don't need this right now but leaving this here in case we do in the future
	// ShowBlueprintAssistNotification();
}

void FAutoSizeCommentsNotifications::Shutdown()
{
	if (SourceControlNotificationDelegate.IsValid())
	{
		if (ISourceControlModule* SourceControlModule = GetSourceControlModule())
		{
			SourceControlModule->UnregisterProviderChanged(SourceControlNotificationDelegate);
		}
	}
}

void FAutoSizeCommentsNotifications::ShowSourceControlNotification()
{
	if (!ShouldShowSourceControlNotification())
	{
		return;
	}

	// TODO: suggest to disable resizing when BA is enabled (need to test BA behaviour more)
	bool bIsBlueprintAssistEnabled = false;// FModuleManager::Get().IsModuleLoaded("BlueprintAssist");

	EASCResizingMode SuggestedResizingMode = bIsBlueprintAssistEnabled ? EASCResizingMode::Disabled : EASCResizingMode::Reactive;

#if ASC_UE_VERSION_OR_LATER(5, 0)
	FNotificationInfo Info(FText::FromString("Auto Size Comments"));
	if (bIsBlueprintAssistEnabled)
	{
		Info.SubText = INVTEXT("Source control detected: Disable resizing and use Blueprint Assist formatting");
	}
	else
	{
		Info.SubText = INVTEXT("Source control detected: Enable 'Reactive' resizing");
	}
#else
	FNotificationInfo Info(INVTEXT(""));
	if (bIsBlueprintAssistEnabled)
	{
		Info.Text = INVTEXT("[AutoSizeComments] Source control detected: Disable resizing and use Blueprint Assist formatting");
	}
	else
	{
		Info.Text = INVTEXT("[AutoSizeComments] Source control detected: Enable 'Reactive' resizing");
	}
#endif
	Info.bUseSuccessFailIcons = false;
	Info.ExpireDuration = 0.0f;
	Info.FadeInDuration = 0.0f;
	Info.FadeOutDuration = 0.5f;
	Info.bUseThrobber = false;
	Info.bFireAndForget = false;

	const auto OnClose = FSimpleDelegate::CreateLambda([&SourceControlNotification = SourceControlNotification]
	{
		if (SourceControlNotification.IsValid())
		{
			SourceControlNotification.Pin()->ExpireAndFadeout();
		}
	});

	const auto OnApply = FSimpleDelegate::CreateLambda([&SourceControlNotification = SourceControlNotification, SuggestedResizingMode]
	{
		UAutoSizeCommentsSettings* ASCSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
		ASCSettings->Modify();
		ASCSettings->ResizingMode = SuggestedResizingMode;
		ASCSettings->SaveConfig();
		FString ResizeModeStr = SuggestedResizingMode == EASCResizingMode::Disabled ? "EASCResizingMode::Disabled" : "EASCResizingMode::Reactive";
		UE_LOG(LogAutoSizeComments, Log, TEXT("Set 'Resizing Mode' to '%s'"), *ResizeModeStr);

		if (SourceControlNotification.IsValid())
		{
			SourceControlNotification.Pin()->ExpireAndFadeout();
		}
	});

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		FText::FromString(TEXT("Close")),
		FText(),
		OnClose,
		SNotificationItem::CS_Pending
	));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		FText::FromString(TEXT("Apply")),
		FText(),
		OnApply,
		SNotificationItem::CS_Pending
	));

	Info.CheckBoxState = ECheckBoxState::Unchecked;

	Info.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic([](ECheckBoxState NewState)
	{
		UAutoSizeCommentsSettings* MutableSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
		MutableSettings->Modify();
		MutableSettings->bSuppressSourceControlNotification = (NewState == ECheckBoxState::Checked);
		MutableSettings->SaveConfig();
	});

	Info.CheckBoxText = FText::FromString(TEXT("Do not show again"));

	SourceControlNotification = FSlateNotificationManager::Get().AddNotification(Info);
	SourceControlNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
}

void FAutoSizeCommentsNotifications::HandleSourceControlProviderChanged(ISourceControlProvider& OldSourceControlProvider, ISourceControlProvider& NewControlProvider)
{
	if (ShouldShowSourceControlNotification() && NewControlProvider.GetName() != TEXT("None"))
	{
		ShowSourceControlNotification();
	}
}

bool FAutoSizeCommentsNotifications::ShouldShowSourceControlNotification()
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
	if (!ASCSettings)
	{
		return false;
	}

	return !SourceControlNotification.IsValid() &&
		ASCSettings->ResizingMode == EASCResizingMode::Always &&
		!ASCSettings->bSuppressSourceControlNotification;
}

void FAutoSizeCommentsNotifications::ShowBlueprintAssistNotification()
{
	if (!FModuleManager::Get().IsModuleLoaded("BlueprintAssist"))
	{
		return;
	}

	UAutoSizeCommentsSettings* MutableSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
	if (MutableSettings->bSuppressSuggestedSettings)
	{
		return;
	}

#if ASC_UE_VERSION_OR_LATER(5, 0)
	FNotificationInfo Info(FText::FromString("Auto Size Comments"));
	Info.SubText = FText::FromString("Blueprint Assist plugin is loaded: Apply suggested settings?");
#else
	FNotificationInfo Info(FText::FromString("[AutoSizeComments] Blueprint Assist plugin is loaded: Apply suggested settings?"));
#endif
	Info.bUseSuccessFailIcons = false;
	Info.ExpireDuration = 0.0f;
	Info.FadeInDuration = 0.0f;
	Info.FadeOutDuration = 0.5f;
	Info.bUseThrobber = false;
	Info.bFireAndForget = false;

	// Close button
	{
		const auto OnClose = FSimpleDelegate::CreateLambda([&BlueprintAssistNotification = BlueprintAssistNotification]
		{
			if (BlueprintAssistNotification.IsValid())
			{
				BlueprintAssistNotification.Pin()->ExpireAndFadeout();
			}
		});

		Info.ButtonDetails.Add(FNotificationButtonInfo(
			FText::FromString(TEXT("Close")),
			FText(),
			OnClose,
			SNotificationItem::CS_Pending
		));
	}

	// Apply button
	{
		const auto ApplySettings = FSimpleDelegate::CreateLambda([&BlueprintAssistNotification = BlueprintAssistNotification]
		{
			UAutoSizeCommentsSettings* MutableSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
			MutableSettings->Modify();
			MutableSettings->bIgnoreKnotNodesWhenPressingAlt = true;
			MutableSettings->SaveConfig();

			UE_LOG(LogAutoSizeComments, Log, TEXT("Applied suggested settings for Blueprint Assist Module"));

			if (BlueprintAssistNotification.IsValid())
			{
				BlueprintAssistNotification.Pin()->ExpireAndFadeout();
			}
		});

		Info.ButtonDetails.Add(FNotificationButtonInfo(
			FText::FromString(TEXT("Apply")),
			FText(),
			ApplySettings,
			SNotificationItem::CS_Pending
		));
	}

	Info.CheckBoxState = ECheckBoxState::Checked;
	MutableSettings->Modify();
	MutableSettings->bSuppressSuggestedSettings = true;
	MutableSettings->SaveConfig();

	Info.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic([](ECheckBoxState NewState)
	{
		UAutoSizeCommentsSettings* MutableSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
		MutableSettings->Modify();
		MutableSettings->bSuppressSuggestedSettings = (NewState == ECheckBoxState::Checked);
		MutableSettings->SaveConfig();
	});

	Info.CheckBoxText = FText::FromString(TEXT("Do not show again"));

	BlueprintAssistNotification = FSlateNotificationManager::Get().AddNotification(Info);
	BlueprintAssistNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
}

ISourceControlModule* FAutoSizeCommentsNotifications::GetSourceControlModule()
{
	return FModuleManager::GetModulePtr<ISourceControlModule>("SourceControl");
}
