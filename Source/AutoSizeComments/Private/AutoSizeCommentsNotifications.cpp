// Fill out your copyright notice in the Description page of Project Settings.

#include "AutoSizeCommentsNotifications.h"

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
	if (ISourceControlModule::Get().IsEnabled())
	{
		ShowSourceControlNotification();
	}
	else
	{
		ISourceControlModule::Get().RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateRaw(this, &FAutoSizeCommentsNotifications::HandleSourceControlProviderChanged));
	}
}

void FAutoSizeCommentsNotifications::ShowSourceControlNotification()
{
	if (!ShouldShowSourceControlNotification())
	{
		return;
	}

	FNotificationInfo Info(FText::FromString("Auto Size Comments"));
	Info.SubText = FText::FromString("Source control is enabled: Apply Reactive resizing mode?");
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

	const auto OnApply = FSimpleDelegate::CreateLambda([&SourceControlNotification = SourceControlNotification]
	{
		UAutoSizeCommentsSettings* ASCSettings = GetMutableDefault<UAutoSizeCommentsSettings>();
		ASCSettings->Modify();
		ASCSettings->ResizingMode = EASCResizingMode::Reactive;
		ASCSettings->SaveConfig();
		UE_LOG(LogAutoSizeComments, Log, TEXT("Set 'Resizing Mode' to 'EASCResizingMode::Reactive'"));

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
	if (ShouldShowSourceControlNotification() && NewControlProvider.IsEnabled())
	{
		ShowSourceControlNotification();
	}
}

bool FAutoSizeCommentsNotifications::ShouldShowSourceControlNotification()
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
	return !SourceControlNotification.IsValid() &&
		ASCSettings->ResizingMode != EASCResizingMode::Reactive &&
		!ASCSettings->bSuppressSourceControlNotification;
}
