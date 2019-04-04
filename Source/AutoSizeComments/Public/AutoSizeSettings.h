// Copyright 2018 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AutoSizeSettings.generated.h"

USTRUCT()
struct FPresetCommentStyle 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, config, Category = Default)
	int FontSize = 18;
};


UCLASS(config = EditorSettings)
class AUTOSIZECOMMENTS_API UAutoSizeSettings : public UObject
{
	GENERATED_BODY()
		
public:
	UAutoSizeSettings(const FObjectInitializer& ObjectInitializer);

	/** The default font size for comment boxes */
	UPROPERTY(EditAnywhere, config, Category = FontSize)
	int DefaultFontSize;

	/** If enabled, all nodes will be changed to the default font size (unless they are a preset or floating node) */
	UPROPERTY(EditAnywhere, config, Category = FontSize)
	bool bUseDefaultFontSize;

	/** If Use Random Color is not enabled, comment boxes will spawn with this default color */
	UPROPERTY(EditAnywhere, config, Category = Color)
	FLinearColor DefaultCommentColor;

	/** If enabled, comment boxes will spawn with a random color. If disabled, use default color */
	UPROPERTY(EditAnywhere, config, Category = Color)
	bool bUseRandomColor;

	/** Set all nodes in the graph to the default color */
	UPROPERTY(EditAnywhere, config, Category = Color)
	bool bAggressivelyUseDefaultColor;

	/** Style for header comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Styles)
	FPresetCommentStyle HeaderStyle;

	/** Preset styles (each style will have its own button on the comment box) */
	UPROPERTY(EditAnywhere, config, Category = Styles)
	TArray<FPresetCommentStyle> PresetStyles;

	/** Amount of padding for around the contents of a comment node */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	FVector2D CommentNodePadding;

	/** If enabled, empty comment boxes will move out of the way of other comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bMoveEmptyCommentBoxes;

	/** The speed at which empty comment boxes move */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	float EmptyCommentBoxSpeed;

	/** Globally set "Color Bubble" for every comment box that is created or loaded */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bGlobalColorBubble;

	/** Globally set "Show Bubble When Zoomed" for every comment box that is created or loaded */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bGlobalShowBubbleWhenZoomed;
};
