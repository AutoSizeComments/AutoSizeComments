// Copyright 2020 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommentNodePadding = FVector2D(30, 30);
	MinimumVerticalPadding = 24.0f;
	CommentTextPadding = FMargin(2, 0, 2, 0);
	CommentTextAlignment = ETextJustify::Left;
	DefaultFontSize = 18;
	bUseDefaultFontSize = false;
	bUseRandomColor = true;
	RandomColorOpacity = 1.f;
	bUseRandomColorFromList = false;
	PredefinedRandomColorList.Add(FLinearColor(1, 0, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 0, 1));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 1));
	PredefinedRandomColorList.Add(FLinearColor(1, 1, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 1));
	PredefinedRandomColorList.Add(FLinearColor(1, 0, 1));
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
	bDisableTooltip = false;
	IgnoredGraphs.Add("ControlRigGraph");
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCommentBubble = false;
	bHideCornerPoints = false;
	bDebugGraph_ASC = false;
}
