// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Commands/InputChord.h"

class FUICommandList;

class AUTOSIZECOMMENTS_API FAutoSizeCommentsInputProcessor
	: public TSharedFromThis<FAutoSizeCommentsInputProcessor>
	, public IInputProcessor
{
public:
	static FAutoSizeCommentsInputProcessor& Get();

	static void Create();
	static void Cleanup();

	void Init();

	//~ Begin IInputProcessor Interface
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {};
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	//~ End IInputProcessor Interface

	bool IsKeyDown(const FKey& Key);

	bool IsInputChordDown(const FInputChord& Chord);

private:
	TSet<FKey> KeysDown;

	TSharedPtr<FUICommandList> ASCCommandList;
};
