// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	AutoInsertComment = EASCAutoInsertComment::Always;
	bSelectNodeWhenClickingOnPin = true;
	bDisableResizing = false;
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
	bSaveCommentDataOnSavingGraph = true;
	bSaveCommentDataOnExit = true;
	bDetectNodesContainedForNewComments = true;
	ResizeChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Shift);
	ResizeCollisionMethod = ECommentCollisionMethod::Contained;
	AltCollisionMethod = ECommentCollisionMethod::Intersect;
	bSnapToGridWhileResizing = false;
	bIgnoreKnotNodes = false;
	bIgnoreKnotNodesWhenPressingAlt = false;
	bIgnoreKnotNodesWhenResizing = false;
	bIgnoreSelectedNodesOnCreation = false;
	bRefreshContainingNodesOnMove = false;
	bDisableTooltip = false;
#if !(ASC_UE_VERSION_OR_LATER(5, 0))
	IgnoredGraphs.Add("ControlRigGraph");
#endif
	bSuppressSuggestedSettings = false;
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCommentBubble = false;
	bHideCornerPoints = false;
	bDebugGraph_ASC = false;
}
