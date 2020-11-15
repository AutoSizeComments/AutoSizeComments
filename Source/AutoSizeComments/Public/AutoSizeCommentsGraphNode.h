// Copyright 2020 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Editor/GraphEditor/Public/SGraphNodeComment.h"
#include "Editor/GraphEditor/Public/SGraphNodeResizable.h"
#include "AutoSizeCommentsSettings.h"

struct FPresetCommentStyle;

/**
 * Auto resizing comment node
 */
enum ASC_AnchorPoint
{
	LEFT, RIGHT, TOP, BOTTOM, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, NONE
};

class SAutoSizeCommentsGraphNode : public SGraphNode
{
public:
	/** This delay is to ensure that all nodes exist on the graph and have their bounds properly set */
	int RefreshNodesDelay = 0;

	bool bIsDragging = false;

	bool bIsMoving = false;

	bool bPreviousAltDown = false;

	/** Variables related to resizing the comment box by dragging anchor corner points */
	FVector2D DragSize;
	bool bUserIsDragging = false;

	ASC_AnchorPoint CachedAnchorPoint = NONE;
	float AnchorSize = 40.f;

	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;

public:
	SLATE_BEGIN_ARGS(SAutoSizeCommentsGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UEdGraphNode* InNode);
	~SAutoSizeCommentsGraphNode();

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

	class UEdGraphNode_Comment* GetCommentNodeObj() { return CommentNode; }
	
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

	virtual bool AddInitialNodes();
	virtual bool AddAllSelectedNodes();
	virtual bool RemoveAllSelectedNodes();

	void UpdateColors(float InDeltaTime);

private:
	/** @return the color to tint the comment body */
	FSlateColor GetCommentBodyColor() const;

	/** @return the color to tint the title bar */
	FSlateColor GetCommentTitleBarColor() const;

	/** @return the color to tint the comment bubble */
	FSlateColor GetCommentBubbleColor() const;

	FLinearColor CommentControlsColor;
	FSlateColor GetCommentControlsColor() const;

	FLinearColor CommentControlsTextColor;
	FSlateColor GetCommentControlsTextColor() const;

	FSlateColor GetPresetColor(FLinearColor Color) const;

	/** Returns the width to wrap the text of the comment at */
	float GetWrapAt() const;

	void ResizeToFit();

	void MoveEmptyCommentBoxes();

	void CreateCommentControls();
	void CreateColorControls();

	void InitializeColor(const UAutoSizeCommentsSettings* ASCSettings, bool bIsPresetStyle, bool bIsHeaderComment);

	FVector2D UserSize;

	bool bHasSetNodesUnderComment;

	/** the title bar, needed to obtain it's height */
	TSharedPtr<SBorder> TitleBar;

	class UEdGraphNode_Comment* CommentNode;

	TSharedPtr<SCommentBubble> CommentBubble;

	/** cached comment title */
	FString CachedCommentTitle;

	/** cached comment title */
	int32 CachedWidth;

	/** cached font size */
	int32 CachedFontSize;

	int32 CachedNumPresets;

	bool bCachedBubbleVisibility;
	bool bCachedColorCommentBubble;

	float OpacityValue;

	// TODO: Look into resize transaction perhaps requires the EdGraphNode_Comment to have UPROPERTY() for NodesUnderComment
	// TSharedPtr<FScopedTransaction> ResizeTransaction;

	/** Local copy of the comment style */
	FInlineEditableTextBlockStyle CommentStyle;

	TSharedPtr<class SButton> ToggleHeaderButton;
	TSharedPtr<class SHorizontalBox> ColorControls;
	TSharedPtr<class SHorizontalBox> CommentControls;

public:
	/** Update the nodes */
	void UpdateRefreshDelay();

	void RefreshNodesInsideComment(ECommentCollisionMethod OverrideCollisionMethod = ASC_Collision_Default);

	float GetTitleBarHeight() const;

	/** Util functions */
	FSlateRect GetBoundsForNodesInside();
	FSlateRect GetNodeBounds(UEdGraphNode* Node);
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> GetOtherCommentNodes();
	void UpdateExistingCommentNodes();
	bool AnySelectedNodes();
	static bool RemoveNodesFromUnderComment(UEdGraphNode_Comment* InCommentNode, TSet<UObject*>& NodesToRemove);
	static FSlateRect GetCommentBounds(UEdGraphNode_Comment* InCommentNode);
	void SnapVectorToGrid(FVector2D& Vector);
	bool IsLocalPositionInCorner(const FVector2D& MousePositionInNode) const;
	TArray<UEdGraphNode*> GetEdGraphNodesUnderComment(UEdGraphNode_Comment* InCommentNode) const;

	ASC_AnchorPoint GetAnchorPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

	bool IsHeaderComment() const;
	bool IsPresetStyle();

	bool LoadCache();
	void SaveToCache();

	void QueryNodesUnderComment(TArray<TSharedPtr<SGraphNode>>& OutNodesUnderComment, ECommentCollisionMethod OverrideCollisionMethod = ASC_Collision_Default);

	void RandomizeColor();

	void AdjustMinSize(FVector2D& InSize);

	bool HasNodeBeenDeleted(UEdGraphNode* Node);

	bool CanAddNode(TSharedPtr<SGraphNode> OtherGraphNode) const;
	bool CanAddNode(UObject* Node) const;
};
