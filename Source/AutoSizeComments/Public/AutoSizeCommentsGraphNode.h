// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutoSizeCommentsMacros.h"
#include "SGraphNode.h"

enum class ECommentCollisionMethod : uint8;
class SCommentBubble;
class UAutoSizeCommentsSettings;
struct FASCCommentData;
struct FPresetCommentStyle;
class UEdGraphNode_Comment;

DECLARE_STATS_GROUP(TEXT("AutoSizeComments"), STATGROUP_AutoSizeComments, STATCAT_Advanced);

/**
 * Auto resizing comment node
 */
enum class EASCAnchorPoint : uint8
{
	Left,
	Right,
	Top,
	Bottom,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	None
};

class SAutoSizeCommentsGraphNode final : public SGraphNode
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

	EASCAnchorPoint CachedAnchorPoint = EASCAnchorPoint::None;
	float AnchorSize = 40.f;

	bool bWasCopyPasted = false;

	bool bRequireUpdate = false;

#if ASC_UE_VERSION_OR_LATER(4, 27)
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
#else
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;
#endif

public:
	// @formatter:off
	SLATE_BEGIN_ARGS(SAutoSizeCommentsGraphNode) {}
	SLATE_END_ARGS()
	// @formatter:on

	void Construct(const FArguments& InArgs, class UEdGraphNode* InNode);
	virtual ~SAutoSizeCommentsGraphNode() override;
	void OnDeleted();

	//~ Begin SWidget Interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override { return FReply::Unhandled(); }
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

	class UEdGraphNode_Comment* GetCommentNodeObj() const { return CommentNode; }

	FASCCommentData& GetCommentData() const;

	void ResizeToFit();

	void ApplyHeaderStyle();

protected:
	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;

	//~ Begin SNodePanel::SNode Interface
	virtual void SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	//~ End SNodePanel::SNode Interface

	FReply HandleRandomizeColorButtonClicked();
	FReply HandleResizeButtonClicked();
	FReply HandleHeaderButtonClicked();
	FReply HandleRefreshButtonClicked();
	FReply HandlePresetButtonClicked(const FPresetCommentStyle Style);
	FReply HandleAddButtonClicked();
	FReply HandleSubtractButtonClicked();
	FReply HandleClearButtonClicked();

	void InitializeASCNode(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes);
	void InitializeNodesUnderComment(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes);

	bool AddInitialNodes();
	bool AddAllSelectedNodes();
	bool RemoveAllSelectedNodes();

	void UpdateColors(const float InDeltaTime);

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

	FSlateColor GetPresetColor(const FLinearColor Color) const;

	/** Returns the width to wrap the text of the comment at */
	float GetWrapAt() const;

	void MoveEmptyCommentBoxes();

	void CreateCommentControls();
	void CreateColorControls();

	void InitializeColor(const UAutoSizeCommentsSettings* ASCSettings, bool bIsPresetStyle, bool bIsHeaderComment);
	void InitializeCommentBubbleSettings();

	bool AreControlsEnabled() const;

	FVector2D UserSize;

	bool bHasSetNodesUnderComment = false;

	/** the title bar, needed to obtain it's height */
	TSharedPtr<SBorder> TitleBar;

	UEdGraphNode_Comment* CommentNode = nullptr;

	TSharedPtr<SCommentBubble> CommentBubble;

	/** cached comment title */
	FString CachedCommentTitle;

	/** cached comment title */
	int32 CachedWidth = 0;

	/** cached font size */
	int32 CachedFontSize = 0;

	int32 CachedNumPresets = 0;

	bool bCachedBubbleVisibility = false;
	bool bCachedColorCommentBubble = false;

	float OpacityValue = 0;

	bool bIsHeader = false;

	bool bInitialized = false;

	// TODO: Look into resize transaction perhaps requires the EdGraphNode_Comment to have UPROPERTY() for NodesUnderComment
	// TSharedPtr<FScopedTransaction> ResizeTransaction;

	/** Local copy of the comment style */
	FInlineEditableTextBlockStyle CommentStyle;
	FSlateColor GetCommentTextColor() const;

	TSharedPtr<SButton> ResizeButton;
	TSharedPtr<SButton> ToggleHeaderButton;
	TSharedPtr<SHorizontalBox> ColorControls;
	TSharedPtr<SHorizontalBox> CommentControls;

	bool bAreControlsEnabled = false;

public:
	/** Update the nodes */
	void UpdateRefreshDelay();

	void RefreshNodesInsideComment(const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots = false, const bool bUpdateExistingComments = true);

	float GetTitleBarHeight() const;

	/** Util functions */
	FSlateRect GetBoundsForNodesInside();
	FSlateRect GetNodeBounds(UEdGraphNode* Node);
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> GetOtherCommentNodes();
	TArray<UEdGraphNode_Comment*> GetParentComments() const;
	void UpdateExistingCommentNodes(const TArray<UEdGraphNode_Comment*>* OldParentComments, const TArray<UObject*>* OldCommentContains);
	void UpdateExistingCommentNodes();
	bool AnySelectedNodes();
	static FSlateRect GetCommentBounds(UEdGraphNode_Comment* InCommentNode);
	void SnapVectorToGrid(FVector2D& Vector);
	void SnapBoundsToGrid(FSlateRect& Bounds, int GridMultiplier);
	bool IsLocalPositionInCorner(const FVector2D& MousePositionInNode) const;
	TArray<UEdGraphNode*> GetEdGraphNodesUnderComment(UEdGraphNode_Comment* InCommentNode) const;
	bool AddAllNodesUnderComment(const TArray<UObject*>& Nodes, const bool bUpdateExistingComments = true);
	bool IsValidGraphPanel(TSharedPtr<SGraphPanel> GraphPanel);
	void RemoveInvalidNodes();

	EASCAnchorPoint GetAnchorPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

	void SetIsHeader(bool bNewValue);
	bool IsHeaderComment() const;
	bool IsPresetStyle();

	bool LoadCache();
	void UpdateCache();

	void QueryNodesUnderComment(TArray<UEdGraphNode*>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots = false);
	void QueryNodesUnderComment(TArray<TSharedPtr<SGraphNode>>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots = false);

	void RandomizeColor();

	void AdjustMinSize(FVector2D& InSize);

	bool HasNodeBeenDeleted(UEdGraphNode* Node);

	bool CanAddNode(const TSharedPtr<SGraphNode> OtherGraphNode, const bool bIgnoreKnots = false) const;
	bool CanAddNode(const UObject* Node, const bool bIgnoreKnots = false) const;
	void OnAltReleased();

	static bool IsCommentNode(UObject* Object);
	static bool IsNotCommentNode(UObject* Object) { return !IsCommentNode(Object); }
	static bool IsMajorNode(UObject* Object);
	static bool IsHeaderComment(UEdGraphNode_Comment* OtherComment);

	FKey GetResizeKey() const;
	bool AreResizeModifiersDown() const;

	bool IsSingleSelectedNode() const;

	bool IsNodeUnrelated() const;
	void SetNodesRelated(const TArray<UEdGraphNode*>& Nodes, bool bIncludeSelf = true);
	void ResetNodesUnrelated();
};
