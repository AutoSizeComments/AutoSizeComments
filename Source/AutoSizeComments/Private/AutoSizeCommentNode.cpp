// Copyright 2018 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentNode.h"

#include "EdGraphNode_Comment.h"
#include "SGraphPanel.h"
#include "SCommentBubble.h"
#include "TutorialMetaData.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#include "Editor/BlueprintGraph/Classes/K2Node_Knot.h"
#include "Editor/GraphEditor/Public/SGraphPanel.h"

#include "Runtime/Engine/Classes/EdGraph/EdGraph.h"

#include "Framework/Application/SlateApplication.h"

#include "Modules/ModuleManager.h"

#include "AutoSizeSettings.h"
#include "GraphEditorSettings.h"


void SAutoSizeCommentNode::Construct(const FArguments& InArgs, class UEdGraphNode* InNode)
{
	GraphNode = InNode;

	CommentNode = Cast<UEdGraphNode_Comment>(InNode);

	UpdateGraphNode();

	// Pull out sizes
	UserSize.X = InNode->NodeWidth;
	UserSize.Y = InNode->NodeHeight;

	// Set comment color
	FLinearColor DefaultColor = GetMutableDefault<UAutoSizeSettings>()->DefaultCommentColor;
	if (GetMutableDefault<UAutoSizeSettings>()->bUseRandomColor)
	{
		if (CommentNode->CommentColor == DefaultColor || CommentNode->CommentColor == FLinearColor::White) // only randomize if the node has the default color
			CommentNode->CommentColor = FLinearColor::MakeRandomColor();
	}
	else // set to the default color
	{
		CommentNode->CommentColor = DefaultColor;
	}
}

void SAutoSizeCommentNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
{
	/** Copied from SGraphNodeComment::MoveTo */
	FVector2D PositionDelta = NewPosition - GetPosition();
	SGraphNode::MoveTo(NewPosition, NodeFilter);
	// Don't drag note content if either of the shift keys are down.
	FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
	if (!KeysState.IsShiftDown())
	{
		UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(GraphNode);
		if (CommentNode && CommentNode->MoveMode == ECommentBoxMode::GroupMovement)
		{
			// Now update any nodes which are touching the comment but *not* selected
			// Selected nodes will be moved as part of the normal selection code
			TSharedPtr< SGraphPanel > Panel = GetOwnerPanel();

			for (FCommentNodeSet::TConstIterator NodeIt(CommentNode->GetNodesUnderComment()); NodeIt; ++NodeIt)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
				{
					if (!Panel->SelectionManager.IsNodeSelected(Node) && !NodeFilter.Find(Node->DEPRECATED_NodeWidget.Pin()))
					{
						NodeFilter.Add(Node->DEPRECATED_NodeWidget.Pin());
						Node->Modify();
						Node->NodePosX += PositionDelta.X;
						Node->NodePosY += PositionDelta.Y;
					}
				}
			}
		}
	}
}

FReply SAutoSizeCommentNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	/** Copied from SGraphNodeComment::OnMouseButtonDoubleClick */
	FVector2D LocalMouseCoordinates = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	
	// If user double-clicked in the title bar area
	if (LocalMouseCoordinates.Y < GetTitleBarHeight())
	{
		// Request a rename
		RequestRename();

		// Set the keyboard focus
		if (!HasKeyboardFocus())
		{
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		}

		return FReply::Handled();
	}
	else
	{
		// Otherwise let the graph handle it, to allow spline interactions to work when they overlap with a comment node
		return FReply::Unhandled();
	}
}

void SAutoSizeCommentNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UpdateRefreshDelay();

	if (RefreshNodesDelay == 0)
	{
		ResizeToFit();
		MoveEmptyCommentBoxes();
	}

	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Update cached title
	const FString CurrentCommentTitle = GetNodeComment();
	if (CurrentCommentTitle != CachedCommentTitle)
	{
		CachedCommentTitle = CurrentCommentTitle;
	}

	// Update cached width
	const int32 CurrentWidth = static_cast<int32>(UserSize.X);
	if (CurrentWidth != CachedWidth)
	{
		CachedWidth = CurrentWidth;
	}
}

void SAutoSizeCommentNode::UpdateGraphNode()
{
	// No pins in a comment box
	InputPins.Empty();
	OutputPins.Empty();

	// Avoid standard box model too
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	// Setup a tag for this node
	FString TagName;
	if (GraphNode != nullptr)
	{
		// We want the name of the blueprint as our name - we can find the node from the GUID
		UObject* Package = GraphNode->GetOutermost();
		UObject* LastOuter = GraphNode->GetOuter();
		while (LastOuter->GetOuter() != Package)
		{
			LastOuter = LastOuter->GetOuter();
		}
		TagName = FString::Printf(TEXT("GraphNode,%s,%s"), *LastOuter->GetFullName(), *GraphNode->NodeGuid.ToString());
	}

	SetupErrorReporting();

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);

	// Create the refresh button
	TSharedRef<SButton> RefreshButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleRefreshButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Replace with selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				//Button Content Image
				TSharedRef<SWidget>(SNew(SImage).Image(
					FCoreStyle::Get().GetBrush("GenericCommands.Redo")
				))
			]
		];

	// Create the add button
	TSharedRef<SButton> AddButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleAddButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Add selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				//Button Content Image
				TSharedRef<SWidget>(SNew(SImage).Image(
					FCoreStyle::Get().GetBrush("EditableComboBox.Add")
				))
			]
		];

	// Create the subtract button
	TSharedRef<SButton> SubtractButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleSubtractButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Remove selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				//Button Content Image
				TSharedRef<SWidget>(SNew(SImage).Image(
					FCoreStyle::Get().GetBrush("EditableComboBox.Delete")
				))
			]
		];

	// Create the clear button
	TSharedRef<SButton> ClearButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleClearButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Clear all nodes"))
		[
			 SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			 [
				//Button Content Image
				TSharedRef<SWidget>(SNew(SImage).Image(
					FCoreStyle::Get().GetBrush("TrashCan_Small")
				))
			 ]
		];

	ContentScale.Bind(this, &SGraphNode::GetContentScale);
	GetOrAddSlot(ENodeZone::Center).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Kismet.Comment.Background"))
		.ColorAndOpacity(FLinearColor::White)
		.BorderBackgroundColor(this, &SAutoSizeCommentNode::GetCommentBodyColor)
		.Padding(FMargin(3.0f))
		.AddMetaData<FGraphNodeMetaData>(TagMeta)
		[
			SNew(SVerticalBox).ToolTipText(this, &SGraphNode::GetNodeTooltip)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Top)
			[
				SAssignNew(TitleBar, SBorder)
				.BorderImage(FEditorStyle::GetBrush("Graph.Node.TitleBackground"))
				.BorderBackgroundColor(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
				.Padding(FMargin(10, 5, 5, 3))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SAssignNew(InlineEditableText, SInlineEditableTextBlock)
					.Style(FEditorStyle::Get(), "Graph.CommentBlock.TitleInlineEditableText")
					.Text(this, &SAutoSizeCommentNode::GetEditableNodeTitleAsText)
					.OnVerifyTextChanged(this, &SAutoSizeCommentNode::OnVerifyNameTextChanged)
					.OnTextCommitted(this, &SAutoSizeCommentNode::OnNameTextCommited)
					.IsReadOnly(this, &SAutoSizeCommentNode::IsNameReadOnly)
					.IsSelected(this, &SAutoSizeCommentNode::IsSelectedExclusively)
					.WrapTextAt(this, &SAutoSizeCommentNode::GetWrapAt)
					.MultiLine(true)
					.ModiferKeyForNewLine(EModifierKey::Shift)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(1.0f)
			[
				ErrorReporting->AsWidget()
			]
			+ SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[
				SNew(SBorder).BorderImage(FEditorStyle::GetBrush("NoBorder"))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill)
				[
					RefreshButton
				]
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill)
				[
					AddButton
				]
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill)
				[
					SubtractButton
				]
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill)
				[
					ClearButton
				]
				+ SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Left).VAlign(VAlign_Fill)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
				[
					SNew(SBox).WidthOverride(20).HeightOverride(20)
					[
						SNew(SBorder).BorderImage(FEditorStyle::GetBrush("Tutorials.Border"))
					]
				]
			]
		]
	];

	// Create comment bubble
	TSharedPtr<SCommentBubble> CommentBubble;

	SAssignNew(CommentBubble, SCommentBubble)
		.GraphNode(GraphNode)
		.Text(this, &SAutoSizeCommentNode::GetNodeComment)
		.OnTextCommitted(this, &SAutoSizeCommentNode::OnNameTextCommited)
		.ColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentBubbleColor)
		.AllowPinning(true)
		.EnableTitleBarBubble(false)
		.EnableBubbleCtrls(false)
		.GraphLOD(this, &SGraphNode::GetCurrentLOD)
		.InvertLODCulling(true)
		.IsGraphNodeHovered(this, &SGraphNode::IsHovered);

	GetOrAddSlot(ENodeZone::TopCenter)
		.SlotOffset(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetOffset))
		.SlotSize(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetSize))
		.AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
		.VAlign(VAlign_Top)
		[
			CommentBubble.ToSharedRef()
		];
}

FVector2D SAutoSizeCommentNode::ComputeDesiredSize(float) const
{
	return UserSize;
}

bool SAutoSizeCommentNode::IsNameReadOnly() const
{
	return !IsEditable.Get() || SGraphNode::IsNameReadOnly();
}

void SAutoSizeCommentNode::SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel)
{
	SGraphNode::SetOwner(OwnerPanel);

	// Refresh the nodes under the comment
	RefreshNodesDelay = 2;
}

bool SAutoSizeCommentNode::CanBeSelected(const FVector2D& MousePositionInNode) const
{
	FVector2D CornerBounds = GetDesiredSize() - FVector2D(40, 40);

	return MousePositionInNode.Y <= GetTitleBarHeight()
		|| (MousePositionInNode.Y >= CornerBounds.Y && MousePositionInNode.X >= CornerBounds.X);
}

FVector2D SAutoSizeCommentNode::GetDesiredSizeForMarquee() const
{
	return FVector2D(UserSize.X, GetTitleBarHeight());
}

FCursorReply SAutoSizeCommentNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	FVector2D LocalMouseCoordinates = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	if (LocalMouseCoordinates.Y <= SGraphNode::GetTitleRect().GetSize().Y)
	{
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}

	return FCursorReply::Unhandled();
}


int32 SAutoSizeCommentNode::GetSortDepth() const
{
	return CommentNode ? CommentNode->CommentDepth : -1;
}

FReply SAutoSizeCommentNode::HandleRefreshButtonClicked()
{
	if (AnySelectedNodes())
	{
		CommentNode->ClearNodesUnderComment();
		AddAllSelectedNodes();
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentNode::HandleAddButtonClicked()
{
	AddAllSelectedNodes();
	return FReply::Handled();
}

FReply SAutoSizeCommentNode::HandleSubtractButtonClicked()
{
	RemoveAllSelectedNodes();
	return FReply::Handled();
}

FReply SAutoSizeCommentNode::HandleClearButtonClicked()
{
	CommentNode->ClearNodesUnderComment();
	UpdateExistingCommentNodes();
	return FReply::Handled();
}

bool SAutoSizeCommentNode::AddAllSelectedNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	bool bDidAddAnything = false;

	if (OwnerPanel.IsValid() && OwnerPanel->SelectionManager.GetSelectedNodes().Num() > 0)
	{
		for (UObject* SelectedObj : OwnerPanel->SelectionManager.GetSelectedNodes())
		{
			if (SelectedObj != CommentNode)
			{
				CommentNode->AddNodeUnderComment(SelectedObj);
				bDidAddAnything = true;
			}
		}
	}

	if (bDidAddAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentNode::RemoveAllSelectedNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	bool bDidRemoveAnything = false;

	const FGraphPanelSelectionSet SelectedNodes = OwnerPanel->SelectionManager.GetSelectedNodes();
	const FCommentNodeSet NodesUnderComment = CommentNode->GetNodesUnderComment();

	// Clear all nodes under comment
	CommentNode->ClearNodesUnderComment();

	// Add back the nodes under comment while filtering out any which are selected
	for (UObject* NodeUnderComment : NodesUnderComment)
	{
		if (!SelectedNodes.Contains(NodeUnderComment))
		{
			CommentNode->AddNodeUnderComment(NodeUnderComment);
		}
		else
		{
			bDidRemoveAnything = true;
		}
	}

	if (bDidRemoveAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidRemoveAnything;
}

FSlateRect SAutoSizeCommentNode::GetTitleRect() const
{
	const FVector2D NodePosition = GetPosition();
	FVector2D NodeSize = TitleBar.IsValid() ? TitleBar->GetDesiredSize() : GetDesiredSize();
	const FSlateRect TitleBarOffset(13, 8, -3, 0);

	return FSlateRect(NodePosition.X, NodePosition.Y, NodePosition.X + NodeSize.X, NodePosition.Y + NodeSize.Y) + TitleBarOffset;
}

void SAutoSizeCommentNode::UpdateRefreshDelay()
{
	if (RefreshNodesDelay > 0)
	{
		RefreshNodesDelay -= 1;

		if (RefreshNodesDelay == 0)
		{
			RefreshNodesInsideComment();
		}
	}
}

void SAutoSizeCommentNode::RefreshNodesInsideComment()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	float TitleBarHeight = GetTitleBarHeight();

	const FVector2D NodeSize(UserSize.X, UserSize.Y - TitleBarHeight);

	// Get our geometry
	FVector2D NodePosition = GetPosition();
	NodePosition.Y += TitleBarHeight;

	const FSlateRect CommentRect = FSlateRect::FromPointAndExtent(NodePosition, NodeSize);

	FChildren* PanelChildren = OwnerPanel->GetAllChildren();
	int32 NumChildren = PanelChildren->Num();

	// Iterate across all nodes in the graph,
	for (int32 NodeIndex = 0; NodeIndex < NumChildren; ++NodeIndex)
	{
		const TSharedRef<SGraphNode> SomeNodeWidget = StaticCastSharedRef<SGraphNode>(PanelChildren->GetChildAt(NodeIndex));
		UObject* GraphObject = SomeNodeWidget->GetObjectBeingDisplayed();

		if (GraphObject == nullptr) continue;

		// skip knot nodes
		if (Cast<UK2Node_Knot>(GraphObject)) continue;

		// skip if we already contain the graph obj
		if (CommentNode->GetNodesUnderComment().Contains(GraphObject))
			continue;

		// check if the node bounds is contained in ourself
		if (GraphObject != CommentNode)
		{
			const FVector2D SomeNodePosition = SomeNodeWidget->GetPosition();
			const FVector2D SomeNodeSize = SomeNodeWidget->GetDesiredSize();
			const FSlateRect NodeGeometryGraphSpace = FSlateRect::FromPointAndExtent(SomeNodePosition, SomeNodeSize);

			if (FSlateRect::IsRectangleContained(CommentRect, NodeGeometryGraphSpace))
			{
				CommentNode->AddNodeUnderComment(GraphObject);
			}
		}
	}

	UpdateExistingCommentNodes();
}

float SAutoSizeCommentNode::GetTitleBarHeight() const
{
	return TitleBar.IsValid() ? TitleBar->GetDesiredSize().Y : 0.0f;
}

void SAutoSizeCommentNode::UpdateExistingCommentNodes()
{
	// Get list of all other comment nodes
	TSet<UEdGraphNode_Comment*> OtherCommentNodes = GetOtherCommentNodes();

	// Do nothing if we have no nodes under ourselves
	if (CommentNode->GetNodesUnderComment().Num() == 0)
	{
		if (GetDefault<UAutoSizeSettings>()->bMoveEmptyCommentBoxes)
		{
			for (UEdGraphNode_Comment* OtherComment : OtherCommentNodes)
			{
				// if the other comment node contains us, remove ourselves
				if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
				{
					TSet<UObject*> NodesToRemove;
					NodesToRemove.Emplace(CommentNode);
					RemoveNodesFromUnderComment(OtherComment, NodesToRemove);
				}
			}
		}

		return;
	}

	for (UEdGraphNode_Comment* OtherComment : OtherCommentNodes)
	{
		// if the other comment node contains us, remove ourselves
		if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
		{
			TSet<UObject*> NodesToRemove;
			NodesToRemove.Emplace(CommentNode);
			RemoveNodesFromUnderComment(OtherComment, NodesToRemove);
		}
	}

	for (UEdGraphNode_Comment* OtherComment : OtherCommentNodes)
	{
		if (OtherComment == CommentNode)
			continue;

		if (OtherComment->GetNodesUnderComment().Num() == 0)
			continue;

		// check if all nodes in the other comment box are within our comment box
		bool bAllNodesContainedUnderOther = !CommentNode->GetNodesUnderComment().ContainsByPredicate([OtherComment](UObject* NodeUnderSelf)
		{
			return !OtherComment->GetNodesUnderComment().Contains(NodeUnderSelf);
		});

		if (bAllNodesContainedUnderOther) // all nodes under the other comment box is also in this comment box, so add the other comment box
		{
			OtherComment->AddNodeUnderComment(CommentNode);
		}
		else
		{
			// check if all nodes in the other comment box are within our comment box
			bool bAllNodesContainedUnderSelf = !OtherComment->GetNodesUnderComment().ContainsByPredicate([this](UObject* NodeUnderOther)
			{
				return !CommentNode->GetNodesUnderComment().Contains(NodeUnderOther);
			});

			if (bAllNodesContainedUnderSelf) // all nodes under the other comment box is also in this comment box, so add the other comment box
			{
				CommentNode->AddNodeUnderComment(OtherComment);
			}
		}
	}
}

FSlateColor SAutoSizeCommentNode::GetCommentBodyColor() const
{
	return CommentNode ? CommentNode->CommentColor : FLinearColor::White;
}

FSlateColor SAutoSizeCommentNode::GetCommentTitleBarColor() const
{
	if (CommentNode)
	{
		const FLinearColor Color = CommentNode->CommentColor * 0.6f;
		return FLinearColor(Color.R, Color.G, Color.B);
	}
	else
	{
		const FLinearColor Color = FLinearColor::White * 0.6f;
		return FLinearColor(Color.R, Color.G, Color.B);
	}
}

FSlateColor SAutoSizeCommentNode::GetCommentBubbleColor() const
{
	if (CommentNode)
	{
		const FLinearColor Color = CommentNode->bColorCommentBubble ? (CommentNode->CommentColor * 0.6f) :
			GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;
		return FLinearColor(Color.R, Color.G, Color.B);
	}
	else
	{
		const FLinearColor Color = FLinearColor::White * 0.6f;
		return FLinearColor(Color.R, Color.G, Color.B);
	}
}

float SAutoSizeCommentNode::GetWrapAt() const
{
	return (float) FMath::Max(0.f, CachedWidth - 32.0f);
}

void SAutoSizeCommentNode::ResizeToFit()
{
	// Resize the node
	TArray<UObject*> UnderComment = CommentNode->GetNodesUnderComment();

	// If a node gets deleted, it still stays inside the comment box?
	// This filter checks if the node is still on the graph and filters if it is not
	SGraphPanel* Panel = GetOwnerPanel().Get();
	UEdGraph* Graph = Panel->GetGraphObj();
	TArray<UObject*> Nodes;
	Graph->GetNodesOfClass(Nodes);
	UnderComment = UnderComment.FilterByPredicate([Nodes](UObject* Obj)
	{
		return Obj != nullptr && !Obj->IsPendingKillOrUnreachable() && Nodes.Contains(Obj);
	});

	// Here we clear and add as there is no remove function
	CommentNode->ClearNodesUnderComment();
	for (UObject* Obj : UnderComment)
	{
		CommentNode->AddNodeUnderComment(Obj);
	}

	// get our currently selected nodes and resize to fit their bounds
	if (UnderComment.Num() > 0)
	{
		// get bounds and apply padding
		FVector2D Padding = GetMutableDefault<UAutoSizeSettings>()->CommentNodePadding;
		Padding.Y += 10;
		FSlateRect Bounds = GetBoundsForNodesInside().ExtendBy(FMargin(Padding.X, Padding.Y));

		const float TitleBarHeight = GetTitleBarHeight();

		// check if size has changed
		FVector2D CurrSize = Bounds.GetSize();
		CurrSize.Y += TitleBarHeight;

		if (!UserSize.Equals(CurrSize, .1f))
		{
			UserSize = CurrSize;
			GetNodeObj()->ResizeNode(CurrSize);
		}

		// check if location has changed
		FVector2D DesiredPos = Bounds.GetTopLeft();
		DesiredPos.Y -= TitleBarHeight;

		// move to desired pos
		if (!GetPosition().Equals(DesiredPos, .1f))
		{
			GraphNode->Modify();
			GraphNode->NodePosX = DesiredPos.X;
			GraphNode->NodePosY = DesiredPos.Y;
		}
	}
	else if (UserSize.X != 175 && UserSize.Y != 100) // the comment has no nodes, resize to default
	{
		UserSize.X = 175;
		UserSize.Y = 100;
		GetNodeObj()->ResizeNode(UserSize);
	}
}

void SAutoSizeCommentNode::MoveEmptyCommentBoxes()
{
	TArray<UObject*> UnderComment = CommentNode->GetNodesUnderComment();

	TSharedPtr<SGraphPanel> OwnerPanel = OwnerGraphPanelPtr.Pin();

	bool bIsSelected = OwnerPanel->SelectionManager.IsNodeSelected(GraphNode);

	bool bIsContained = false;
	for (UEdGraphNode_Comment* OtherComment : GetOtherCommentNodes())
	{
		if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
		{
			bIsContained = true;
			break;
		}
	}

	// if the comment node is empty, move away from other comment nodes
	if (UnderComment.Num() == 0 && GetDefault<UAutoSizeSettings>()->bMoveEmptyCommentBoxes && !bIsSelected && !bIsContained)
	{
		FVector2D TotalMovement(0, 0);

		bool bAnyCollision = false;

		for (UEdGraphNode_Comment* OtherComment : GetOtherCommentNodes())
		{
			if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
			{
				continue;
			}

			FSlateRect OtherBounds = GetCommentBounds(OtherComment);
			FSlateRect MyBounds = GetCommentBounds(CommentNode);

			if (FSlateRect::DoRectanglesIntersect(OtherBounds, MyBounds))
			{
				FVector2D MyCenter = MyBounds.GetCenter();

				float DeltaLeft = OtherBounds.Left - MyBounds.Right;
				float DeltaRight = OtherBounds.Right - MyBounds.Left;

				float DeltaTop = OtherBounds.Top - MyBounds.Bottom;
				float DeltaBottom = OtherBounds.Bottom - MyBounds.Top;

				float SelectedX = FMath::Abs(DeltaLeft) < FMath::Abs(DeltaRight) ? DeltaLeft : DeltaRight;
				float SelectedY = FMath::Abs(DeltaTop) < FMath::Abs(DeltaBottom) ? DeltaTop : DeltaBottom;

				if (FMath::Abs(SelectedX) < FMath::Abs(SelectedY))
				{
					TotalMovement.X += FMath::Sign(SelectedX);
				}
				else
				{
					TotalMovement.Y += FMath::Sign(SelectedY);
				}

				bAnyCollision = true;
			}
		}

		if (TotalMovement.SizeSquared() == 0 && bAnyCollision)
		{
			TotalMovement.X = (FMath::Rand() % 2) * 2 - 1;
			TotalMovement.Y = (FMath::Rand() % 2) * 2 - 1;
		}

		TotalMovement *= GetDefault<UAutoSizeSettings>()->CollisionMovementSpeed;

		GraphNode->Modify();
		GraphNode->NodePosX += TotalMovement.X;
		GraphNode->NodePosY += TotalMovement.Y;
	}
}

/***************************
****** Util functions ******
****************************/

bool SAutoSizeCommentNode::RemoveNodesFromUnderComment(UEdGraphNode_Comment* InCommentNode, TSet<UObject*>& NodesToRemove)
{
	bool bDidRemoveAnything = false;

	const FCommentNodeSet NodesUnderComment = InCommentNode->GetNodesUnderComment();

	// Clear all nodes under comment
	InCommentNode->ClearNodesUnderComment();

	// Add back the nodes under comment while filtering out any which are to be removed
	for (UObject* NodeUnderComment : NodesUnderComment)
	{
		if (!NodesToRemove.Contains(NodeUnderComment))
		{
			InCommentNode->AddNodeUnderComment(NodeUnderComment);
		}
		else
		{
			bDidRemoveAnything = true;
		}
	}

	return bDidRemoveAnything;
}

TSet<UEdGraphNode_Comment*> SAutoSizeCommentNode::GetOtherCommentNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	FChildren* PanelChildren = OwnerPanel->GetAllChildren();
	int32 NumChildren = PanelChildren->Num();

	// Get list of all other comment nodes
	TSet<UEdGraphNode_Comment*> OtherCommentNodes;
	for (int32 NodeIndex = 0; NodeIndex < NumChildren; ++NodeIndex)
	{
		const TSharedRef<SGraphNode> SomeNodeWidget = StaticCastSharedRef<SGraphNode>(PanelChildren->GetChildAt(NodeIndex));
		UObject* GraphObject = SomeNodeWidget->GetObjectBeingDisplayed();
		if (UEdGraphNode_Comment* GraphCommentNode = Cast<UEdGraphNode_Comment>(GraphObject))
		{
			if (GraphCommentNode != CommentNode)
				OtherCommentNodes.Add(GraphCommentNode);
		}
	}

	return OtherCommentNodes;
}

FSlateRect SAutoSizeCommentNode::GetCommentBounds(UEdGraphNode_Comment* InCommentNode)
{
	FVector2D Point(InCommentNode->NodePosX, InCommentNode->NodePosY);
	FVector2D Extent(InCommentNode->NodeWidth, InCommentNode->NodeHeight);
	return FSlateRect::FromPointAndExtent(Point, Extent);
}

FSlateRect SAutoSizeCommentNode::GetNodeBounds(UEdGraphNode* Node)
{
	if (!Node)
		return FSlateRect();

	FVector2D Pos(Node->NodePosX, Node->NodePosY);

	FVector2D Size(300, 150);

	TWeakPtr<SGraphNode> GraphNode = Node->DEPRECATED_NodeWidget;
	if (GraphNode.IsValid())
	{
		Pos = GraphNode.Pin()->GetPosition();
		Size = GraphNode.Pin()->GetDesiredSize();

		if (SNodePanel::SNode::FNodeSlot* CommentSlot = GraphNode.Pin()->GetSlot(ENodeZone::TopCenter))
		{
			TSharedPtr<SCommentBubble> CommentBubble = StaticCastSharedRef<SCommentBubble>(CommentSlot->GetWidget());
			if (CommentBubble.IsValid() && CommentBubble->IsBubbleVisible())
			{
				FVector2D CommentBubbleSize = CommentBubble->GetSize();

				Pos.Y -= CommentBubbleSize.Y;
				Size.Y += CommentBubbleSize.Y;
				Size.X = FMath::Max(Size.X, CommentBubbleSize.X);
			}
		}
	}

	return FSlateRect::FromPointAndExtent(Pos, Size);
}

bool SAutoSizeCommentNode::AnySelectedNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	return OwnerPanel.IsValid() && OwnerPanel->SelectionManager.GetSelectedNodes().Num() > 0;
}

FSlateRect SAutoSizeCommentNode::GetBoundsForNodesInside()
{
	TArray<UEdGraphNode*> Nodes;
	for (UObject* Obj : CommentNode->GetNodesUnderComment())
	{
		if (UEdGraphNode_Comment* OtherCommentNode = Cast<UEdGraphNode_Comment>(Obj))
			// if the node contains us and is higher depth, do not resize
			if (OtherCommentNode->GetNodesUnderComment().Contains(GraphNode) &&
				OtherCommentNode->CommentDepth > CommentNode->CommentDepth)
				continue;

		// skip knot nodes - they are buggy?
		if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(Obj))
			continue;

		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
			Nodes.Add(Node);
	}

	bool bBoundsInit = false;
	FSlateRect Bounds;
	for (int i = 0; i < Nodes.Num(); i++)
	{
		UEdGraphNode* Node = Nodes[i];

		if (!Node) continue;

		// initialize bounds from first valid node
		if (!bBoundsInit)
		{
			Bounds = GetNodeBounds(Node);
			bBoundsInit = true;
		}
		else
		{
			Bounds = Bounds.Expand(GetNodeBounds(Node));
		}
	}

	return Bounds;
}
