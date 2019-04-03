// Copyright 2018 fpwong, Inc. All Rights Reserved.

#include "AutoSizeSettings.h"


UAutoSizeSettings::UAutoSizeSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CommentNodePadding = FVector2D(30, 30);
	bUseRandomColor = true;
	DefaultCommentColor = FLinearColor::White;
	FloatingColor = FLinearColor::Gray;
	bAggressivelyUseDefaultColor = false;
	bMoveEmptyCommentBoxes = true;
	EmptyCommentBoxSpeed = 10;
	bForceColorCommentBubbles = false;
}
