// Copyright 2018 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsCacheFile.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommentNodePadding = FVector2D(30, 30);
	MinimumVerticalPadding = 24.0f;
	CommentTextAlignment = ETextJustify::Left;
	DefaultFontSize = 18;
	bUseDefaultFontSize = false;
	bUseRandomColor = true;
	RandomColorOpacity = 1.f;
	MinimumControlOpacity = 0.f;
	DefaultCommentColor = FLinearColor::White;
	HeaderStyle.Color = FLinearColor::Gray;
	bAggressivelyUseDefaultColor = false;
	bMoveEmptyCommentBoxes = false;
	EmptyCommentBoxSpeed = 10;
	bEnableGlobalSettings = true;
	bGlobalColorBubble = false;
	bGlobalShowBubbleWhenZoomed = true;
	bSaveCommentNodeDataToFile = true;
	bDetectNodesContainedForNewComments = true;
	ResizeCollisionMethod = ASC_Collision_Contained;
	AltCollisionMethod = ASC_Collision_Intersect;
	bSnapToGridWhileResizing = false;
	bIgnoreKnotNodes = false;
	bIgnoreSelectedNodesOnCreation = false;
	bRefreshContainingNodesOnMove = false;
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCommentBubble = false;
	bHideCornerPoints = false;
}
