// Copyright 2018 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsCacheFile.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommentNodePadding = FVector2D(30, 30);
	DefaultFontSize = 18;
	bUseDefaultFontSize = false;
	bUseRandomColor = true;
	RandomColorOpacity = 0.5f;
	DefaultCommentColor = FLinearColor::White;
	HeaderStyle.Color = FLinearColor::Gray;
	bAggressivelyUseDefaultColor = false;
	bMoveEmptyCommentBoxes = true;
	EmptyCommentBoxSpeed = 10;
	bGlobalColorBubble = false;
	bGlobalShowBubbleWhenZoomed = true;
	bSaveCommentNodeDataToFile = true;
	bDetectNodesContainedForNewComments = true;
	ResizeCollisionMethod = ASC_Collision_Contained;
	AltCollisionMethod = ASC_Collision_Intersect;
	bIgnoreKnotNodes = false;
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCommentBubble = false;
}
