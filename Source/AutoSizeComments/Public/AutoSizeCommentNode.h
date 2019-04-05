// Copyright 2018 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Editor/GraphEditor/Public/SGraphNodeComment.h"
#include "Editor/GraphEditor/Public/SGraphNodeResizable.h"

struct FPresetCommentStyle;

/**
 * Auto resizing comment node
 */
enum ASC_AnchorPoint
{
	TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, NONE
};

class SAutoSizeCommentNode : public SGraphNode
{
public:
	/** This delay is to ensure that all nodes exist on the graph and have their bounds properly set */
	int RefreshNodesDelay = 0;

	bool bIsDragging = false;

	bool bPreviousAltDown = false;

	/** Variables related to resizing the comment box by dragging anchor corner points */
	FVector2D DragSize;
	bool bUserIsDragging = false;

	ASC_AnchorPoint AnchorPoint = NONE;
	float AnchorSize = 40.f;

	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;

public:
	SLATE_BEGIN_ARGS(SAutoSizeCommentNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UEdGraphNode* InNode);

	//~ Begin SWidget Interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget Interface

	//~ Begin SNodePanel::SNode Interface
	virtual bool ShouldAllowCulling() const override { return false; }
	virtual int32 GetSortDepth() const override;
	//~ End SNodePanel::SNode Interface

	//~ Begin SPanel Interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	//~ End SPanel Interface

	//~ Begin SGraphNode Interface
	virtual bool IsNameReadOnly() const override;
	virtual FSlateColor GetCommentColor() const override { return GetCommentBodyColor(); }
	//~ End SGraphNode Interface

	/** return if the node can be selected, by pointing given location */
	virtual bool CanBeSelected(const FVector2D& MousePositionInNode) const override;

	/** return size of the title bar */
	virtual FVector2D GetDesiredSizeForMarquee() const override;

	/** return rect of the title bar */
	virtual FSlateRect GetTitleRect() const override;

protected:
	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;

	//~ Begin SNodePanel::SNode Interface
	virtual void SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	//~ End SNodePanel::SNode Interface

	virtual FReply HandleRandomizeColorButtonClicked();
	virtual FReply HandleHeaderButtonClicked();
	virtual FReply HandleRefreshButtonClicked();
	virtual FReply HandlePresetButtonClicked(FPresetCommentStyle Color);
	virtual FReply HandleAddButtonClicked();
	virtual FReply HandleSubtractButtonClicked();
	virtual FReply HandleClearButtonClicked();

	virtual bool AddAllSelectedNodes();
	virtual bool RemoveAllSelectedNodes();

private:
	/** @return the color to tint the comment body */
	FSlateColor GetCommentBodyColor() const;

	/** @return the color to tint the title bar */
	FSlateColor GetCommentTitleBarColor() const;

	/** @return the color to tint the comment bubble */
	FSlateColor GetCommentBubbleColor() const;

	/** Returns the width to wrap the text of the comment at */
	float GetWrapAt() const;

	void ResizeToFit();

	void MoveEmptyCommentBoxes();
private:
	FVector2D UserSize;

	bool bHasSetNodesUnderComment;

	/** the title bar, needed to obtain it's height */
	TSharedPtr<SBorder> TitleBar;

	class UEdGraphNode_Comment* CommentNode;

	/** cached comment title */
	FString CachedCommentTitle;

	/** cached comment title */
	int32 CachedWidth;

	/** cached font size */
	int32 CachedFontSize;

	int32 CachedNumPresets;

	/** Local copy of the comment style */
	FInlineEditableTextBlockStyle CommentStyle;

public:
	/** Update the nodes */
	void UpdateRefreshDelay();

	void RefreshNodesInsideComment(bool bCheckContained);

	float GetTitleBarHeight() const;

	/** Util functions */
	FSlateRect GetBoundsForNodesInside();
	FSlateRect GetNodeBounds(UEdGraphNode* Node);
	TSet<UEdGraphNode_Comment*> GetOtherCommentNodes();
	void UpdateExistingCommentNodes();
	bool AnySelectedNodes();
	static bool RemoveNodesFromUnderComment(UEdGraphNode_Comment* InCommentNode, TSet<UObject*>& NodesToRemove);
	static FSlateRect GetCommentBounds(UEdGraphNode_Comment* InCommentNode);
	void SnapVectorToGrid(FVector2D& Vector);
	bool IsLocalPositionInCorner(const FVector2D& MousePositionInNode) const;

	bool IsHeaderComment();
	bool IsPresetStyle();
};
