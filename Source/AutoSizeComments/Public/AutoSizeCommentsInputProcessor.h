// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

class AUTOSIZECOMMENTS_API FAutoSizeCommentsInputProcessor
	: public TSharedFromThis<FAutoSizeCommentsInputProcessor>
	, public IInputProcessor
{
public:
	static void Create();
	static void Cleanup();

	//~ Begin IInputProcessor Interface
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {};
	//~ End IInputProcessor Interface
};
