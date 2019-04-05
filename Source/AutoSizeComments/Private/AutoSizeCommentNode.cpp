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
#include "Widgets/Text/STextBlock.h"

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

	bool bIsPreset = IsPresetStyle();

	// Set comment color
	FLinearColor DefaultColor = GetMutableDefault<UAutoSizeSettings>()->DefaultCommentColor;
	if (GetMutableDefault<UAutoSizeSettings>()->bUseRandomColor)
	{
		if (CommentNode->CommentColor == DefaultColor || CommentNode->CommentColor == FLinearColor::White) // only randomize if the node has the default color
			CommentNode->CommentColor = FLinearColor::MakeRandomColor();
	}
	else if (!IsHeaderComment())
	{
		if (GetMutableDefault<UAutoSizeSettings>()->bAggressivelyUseDefaultColor)
		{
			CommentNode->CommentColor = DefaultColor;
		}
		else if (CommentNode->CommentColor == FLinearColor::White)
		{
			CommentNode->CommentColor = DefaultColor;
		}
	}

	if (GetMutableDefault<UAutoSizeSettings>()->bUseDefaultFontSize && !IsHeaderComment() && !bIsPreset)
	{
		CommentNode->FontSize = GetMutableDefault<UAutoSizeSettings>()->DefaultFontSize;
	}

	// Set comment bubble color
	CommentNode->bColorCommentBubble = GetMutableDefault<UAutoSizeSettings>()->bGlobalColorBubble;

	CommentNode->bCommentBubbleVisible_InDetailsPanel = GetMutableDefault<UAutoSizeSettings>()->bGlobalShowBubbleWhenZoomed;
}

void SAutoSizeCommentNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
{
	/** Copied from SGraphNodeComment::MoveTo */
	FVector2D PositionDelta = NewPosition - GetPosition();
	
	SnapVectorToGrid(PositionDelta);

	FVector2D NewPos = GetPosition() + PositionDelta;
	SGraphNode::MoveTo(NewPos, NodeFilter);

	FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
	
	if (!(KeysState.IsAltDown() && KeysState.IsControlDown()) && !IsHeaderComment())
	{
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

FReply SAutoSizeCommentNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsEditable.Get())
	{
		FVector2D Size = GetDesiredSize();
		float Top = AnchorSize;
		float Left = AnchorSize;
		float Right = Size.X - AnchorSize;
		float Bottom = Size.Y - AnchorSize;

		AnchorPoint = NONE;

		if (MousePositionInNode.X > Right && MousePositionInNode.Y > Bottom)
		{
			AnchorPoint = BOTTOM_RIGHT;
		}
		else if (MousePositionInNode.X < Left && MousePositionInNode.Y < Top)
		{
			AnchorPoint = TOP_LEFT;
		}
		else if (MousePositionInNode.X < Left && MousePositionInNode.Y > Bottom)
		{
			AnchorPoint = BOTTOM_LEFT;
		}
		else if (MousePositionInNode.X > Right && MousePositionInNode.Y < Top)
		{
			AnchorPoint = TOP_RIGHT;
		}
		
		if (AnchorPoint != NONE)
		{
			DragSize = UserSize;
			bUserIsDragging = true;
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}

	return SGraphNode::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentNode::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && bUserIsDragging)
	{
		bUserIsDragging = false;
		AnchorPoint = NONE;
		RefreshNodesInsideComment(true);
		return FReply::Handled().ReleaseMouseCapture();
	}

	return SGraphNode::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentNode::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bUserIsDragging)
	{
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		FVector2D GraphSpaceCoordinates = NodeCoordToGraphCoord(MouseEvent.GetScreenSpacePosition());
		FVector2D ScaledCoordinates = GraphSpaceCoordinates / (OwnerWindow.IsValid() ? OwnerWindow->GetDPIScaleFactor() : 1.0f);
		FVector2D OldGraphSpaceCoordinates = NodeCoordToGraphCoord(MouseEvent.GetLastScreenSpacePosition());
		FVector2D Delta = (GraphSpaceCoordinates - OldGraphSpaceCoordinates) / (OwnerWindow.IsValid() ? OwnerWindow->GetDPIScaleFactor() : 1.0f);

		int32 OldNodeWidth = GraphNode->NodeWidth;
		int32 OldNodeHeight = GraphNode->NodeHeight;


		if (IsHeaderComment())
		{
			Delta.Y = 0;
		}

		if (AnchorPoint == BOTTOM_RIGHT)
		{
			DragSize += Delta;
		}
		else if (AnchorPoint == TOP_LEFT)
		{
			DragSize -= Delta;
		}
		else if (AnchorPoint == BOTTOM_LEFT)
		{
			DragSize.X -= Delta.X;
			DragSize.Y += Delta.Y;
		}
		else if (AnchorPoint == TOP_RIGHT)
		{
			DragSize.X += Delta.X;
			DragSize.Y -= Delta.Y;
		}
		
		float MinY = IsHeaderComment() ? 0 : 80;
		FVector2D ClampedSize(FMath::Max(125.f, DragSize.X), FMath::Max(GetTitleBarHeight() + MinY, DragSize.Y));

		if (UserSize != ClampedSize)
		{
			UserSize = ClampedSize;
			
			GetNodeObj()->ResizeNode(UserSize);

			if (AnchorPoint == TOP_LEFT || AnchorPoint == BOTTOM_LEFT)
			{
				int32 DeltaWidth = GraphNode->NodeWidth - OldNodeWidth;
				GraphNode->NodePosX -= DeltaWidth;
			}

			if (AnchorPoint == TOP_LEFT || AnchorPoint == TOP_RIGHT)
			{
				int32 DeltaHeight = GraphNode->NodeHeight - OldNodeHeight;
				GraphNode->NodePosY -= DeltaHeight;
			}
		}
	}
		
	return SGraphNode::OnMouseMove(MyGeometry, MouseEvent);
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

	if (RefreshNodesDelay == 0 && !IsHeaderComment() && !bUserIsDragging)
	{
		FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();

		bool bIsAltDown = KeysState.IsAltDown();
		if (!bIsAltDown)
		{
			if (bPreviousAltDown) // refresh when the alt key is released
			{
				RefreshNodesInsideComment(false);
			}

			ResizeToFit();

			MoveEmptyCommentBoxes();
		}

		bPreviousAltDown = bIsAltDown;
	}

	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (IsHeaderComment())
	{
		UserSize.Y = GetTitleBarHeight();
	}

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

	// Update cached font size
	if (CachedFontSize != CommentNode->FontSize)
	{
		UpdateGraphNode();
	}

	if (CachedNumPresets != GetMutableDefault<UAutoSizeSettings>()->PresetStyles.Num())
	{
		UpdateGraphNode();
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

	CommentStyle = FEditorStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.CommentBlock.TitleInlineEditableText");
	CommentStyle.EditableTextBoxStyle.Font.Size = CommentNode->FontSize;
	CommentStyle.TextStyle.Font.Size = CommentNode->FontSize;
	CachedFontSize = CommentNode->FontSize;

	// Create the random color button
	TSharedRef<SButton> RandomColorButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleRandomizeColorButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Randomize the color of the comment box"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock).Text(FText::FromString(FString("?"))).ColorAndOpacity(FLinearColor::White)
			]
		];

	// Create the random color button
	TSharedRef<SButton> ToggleHeaderButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleHeaderButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Toggle between a header node and a resizing node"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock).Text(FText::FromString(FString("H"))).ColorAndOpacity(FLinearColor::White)
			]
		];

	// Create the replace button
	TSharedRef<SButton> ReplaceButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleRefreshButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Replace with selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock).Text(FText::FromString(FString("R"))).ColorAndOpacity(FLinearColor::White)
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
				TSharedRef<SWidget>(SNew(SImage).Image(
					FCoreStyle::Get().GetBrush("EditableComboBox.Add")
				))
			]
		];

	// Create the remove button
	TSharedRef<SButton> RemoveButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
		.OnClicked(this, &SAutoSizeCommentNode::HandleSubtractButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Remove selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
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
				TSharedRef<SWidget>(SNew(SImage).Image(
					FCoreStyle::Get().GetBrush("TrashCan_Small")
				))
			 ]
		];

	TSharedRef<SBox> AnchorBox = 
		SNew(SBox).WidthOverride(16).HeightOverride(16)
		[
			SNew(SBorder).BorderImage(FEditorStyle::GetBrush("Tutorials.Border"))
		];

	// Create the color controls
	TSharedRef<SHorizontalBox> ColorControls = SNew(SHorizontalBox);

	TSharedRef<SBorder> ColorControlsWithBorder = 
		SNew(SBorder).ColorAndOpacity(FLinearColor::White)
		[
			ColorControls
		];

	TArray<FPresetCommentStyle> Presets = GetMutableDefault<UAutoSizeSettings>()->PresetStyles;
	CachedNumPresets = Presets.Num();

	if (!IsHeaderComment()) // header comments don't need color presets
	{
		for (FPresetCommentStyle Preset : Presets)
		{
			TSharedRef<SButton> Button = SNew(SButton)
				.ButtonColorAndOpacity(Preset.Color)
				.OnClicked(this, &SAutoSizeCommentNode::HandlePresetButtonClicked, Preset)
				.ToolTipText(FText::FromString("Set preset color"))
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(10).HeightOverride(10)
				];

			ColorControls->AddSlot().AttachWidget(Button);
		}
		ColorControls->AddSlot().AttachWidget(RandomColorButton);
	}
	
	// Create the comment controls
	TSharedRef<SHorizontalBox> CommentControls = SNew(SHorizontalBox);
	CommentControls->AddSlot().AttachWidget(ReplaceButton);
	CommentControls->AddSlot().AttachWidget(AddButton);
	CommentControls->AddSlot().AttachWidget(RemoveButton);
	CommentControls->AddSlot().AttachWidget(ClearButton);
	
	// Create the bottom horizontal box containing comment controls and anchor points (header comments don't need these)
	TSharedRef<SHorizontalBox> BottomHBox = SNew(SHorizontalBox);
	if (!IsHeaderComment())
	{
		BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AttachWidget(AnchorBox);
		BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill).AttachWidget(CommentControls);
		BottomHBox->AddSlot().FillWidth(1).HAlign(HAlign_Left).VAlign(VAlign_Fill).AttachWidget(SNew(SSpacer));
		BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Bottom).AttachWidget(AnchorBox);
	}

	ContentScale.Bind(this, &SGraphNode::GetContentScale);
	GetOrAddSlot(ENodeZone::Center).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Kismet.Comment.Background"))
		.ColorAndOpacity(FLinearColor::White)
		.BorderBackgroundColor(this, &SAutoSizeCommentNode::GetCommentBodyColor)
		.AddMetaData<FGraphNodeMetaData>(TagMeta)
		[
			SNew(SVerticalBox).ToolTipText(this, &SGraphNode::GetNodeTooltip)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Top)
			[
				SAssignNew(TitleBar, SBorder)
				.BorderImage(FEditorStyle::GetBrush("Graph.Node.TitleBackground"))
				.BorderBackgroundColor(this, &SAutoSizeCommentNode::GetCommentTitleBarColor)
				.HAlign(HAlign_Fill).VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Top)
					[
						AnchorBox
					]
					+ SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Top)
					[
						SAssignNew(InlineEditableText, SInlineEditableTextBlock)
						.Style(&CommentStyle)
						.Text(this, &SAutoSizeCommentNode::GetEditableNodeTitleAsText)
						.OnVerifyTextChanged(this, &SAutoSizeCommentNode::OnVerifyNameTextChanged)
						.OnTextCommitted(this, &SAutoSizeCommentNode::OnNameTextCommited)
						.IsReadOnly(this, &SAutoSizeCommentNode::IsNameReadOnly)
						.IsSelected(this, &SAutoSizeCommentNode::IsSelectedExclusively)
						.WrapTextAt(this, &SAutoSizeCommentNode::GetWrapAt)
						.MultiLine(true)
						.ModiferKeyForNewLine(EModifierKey::Shift)
					]
					+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top)
					[
						ToggleHeaderButton
					]
					+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top)
					[
						AnchorBox
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(1.0f)
			[
				ErrorReporting->AsWidget()
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).VAlign(VAlign_Top)
			[
				ColorControlsWithBorder
			]
			+ SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[
				SNew(SBorder).BorderImage(FEditorStyle::GetBrush("NoBorder"))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
			[
				BottomHBox
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
	return (MousePositionInNode.X > 40 && MousePositionInNode.Y <= GetTitleBarHeight()) || IsLocalPositionInCorner(MousePositionInNode);
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

FReply SAutoSizeCommentNode::HandleRandomizeColorButtonClicked()
{
	CommentNode->CommentColor = FLinearColor::MakeRandomColor();
	return FReply::Handled();
}

FReply SAutoSizeCommentNode::HandleHeaderButtonClicked()
{
	FPresetCommentStyle Style = GetMutableDefault<UAutoSizeSettings>()->HeaderStyle;

	if (CommentNode->CommentColor == Style.Color)
	{
		CommentNode->CommentColor = FLinearColor::MakeRandomColor();
		CommentNode->FontSize = GetMutableDefault<UAutoSizeSettings>()->DefaultFontSize;
		RefreshNodesInsideComment(false);
	}
	else
	{
		CommentNode->CommentColor = Style.Color;
		CommentNode->FontSize = Style.FontSize;
		CommentNode->ClearNodesUnderComment();
	}

	UpdateGraphNode();

	return FReply::Handled();
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

FReply SAutoSizeCommentNode::HandlePresetButtonClicked(FPresetCommentStyle Style)
{
	CommentNode->CommentColor = Style.Color;
	CommentNode->FontSize = Style.FontSize;
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
	if (GetDesiredSize().IsZero())
		return;

	FVector2D NodePosition = GetPosition();
	FVector2D BottomRight = NodePosition + FVector2D(1, 1);
	if (!OwnerGraphPanelPtr.Pin().Get()->IsRectVisible(NodePosition, BottomRight))
		return;

	if (RefreshNodesDelay > 0)
	{
		RefreshNodesDelay -= 1;

		if (RefreshNodesDelay == 0)
		{
			RefreshNodesInsideComment(false);
		}
	}
}

void SAutoSizeCommentNode::RefreshNodesInsideComment(bool bCheckContained)
{
	CommentNode->ClearNodesUnderComment();

	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	float TitleBarHeight = GetTitleBarHeight();

	const FVector2D NodeSize(UserSize.X, UserSize.Y - TitleBarHeight);

	// Get our geometry
	FVector2D NodePosition = GetPosition();
	NodePosition.Y += TitleBarHeight;

	const FSlateRect CommentRect = FSlateRect::FromPointAndExtent(NodePosition, NodeSize).ExtendBy(1);

	FChildren* PanelChildren = OwnerPanel->GetAllChildren();
	int32 NumChildren = PanelChildren->Num();

	// Iterate across all nodes in the graph
	for (int32 NodeIndex = 0; NodeIndex < NumChildren; ++NodeIndex)
	{
		const TSharedRef<SGraphNode> SomeNodeWidget = StaticCastSharedRef<SGraphNode>(PanelChildren->GetChildAt(NodeIndex));
		UObject* GraphObject = SomeNodeWidget->GetObjectBeingDisplayed();

		if (GraphObject == nullptr) continue;

		// skip if we already contain the graph obj
		if (CommentNode->GetNodesUnderComment().Contains(GraphObject))
			continue;

		// check if the node bounds is contained in ourself
		if (GraphObject != CommentNode)
		{

			const FVector2D SomeNodePosition = SomeNodeWidget->GetPosition();
			const FVector2D SomeNodeSize = SomeNodeWidget->GetDesiredSize();
			const FSlateRect NodeGeometryGraphSpace = FSlateRect::FromPointAndExtent(SomeNodePosition, SomeNodeSize);

			bool bShouldAddNode = bCheckContained 
				? FSlateRect::IsRectangleContained(CommentRect, NodeGeometryGraphSpace)
				: CommentRect.ContainsPoint(SomeNodePosition);

			if (bShouldAddNode)
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

		float VerticalPadding = FMath::Max(30.f, Padding.Y + 16.f); // ensure we can always see the buttons

		FSlateRect Bounds = GetBoundsForNodesInside().ExtendBy(FMargin(Padding.X, VerticalPadding, Padding.X, VerticalPadding));

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
			GraphNode->NodePosX = DesiredPos.X;
			GraphNode->NodePosY = DesiredPos.Y;
		}
	}
	else
	{
		bool bEdited = false;
		if (UserSize.X < 125) // the comment has no nodes, resize to default
		{
			bEdited = true;
			UserSize.X = 225;
		}

		if (UserSize.Y < 80)
		{
			bEdited = true;
			UserSize.Y = 150;
		}

		if (bEdited)
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

		TotalMovement *= GetDefault<UAutoSizeSettings>()->EmptyCommentBoxSpeed;

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

void SAutoSizeCommentNode::SnapVectorToGrid(FVector2D& Vector)
{
	const float SnapSize = SNodePanel::GetSnapGridSize();
	Vector.X = SnapSize * FMath::RoundToFloat(Vector.X / SnapSize);
	Vector.Y = SnapSize * FMath::RoundToFloat(Vector.Y / SnapSize);
}

bool SAutoSizeCommentNode::IsLocalPositionInCorner(const FVector2D& MousePositionInNode) const
{
	FVector2D CornerBounds = GetDesiredSize() - FVector2D(40, 40);
	return MousePositionInNode.Y >= CornerBounds.Y && MousePositionInNode.X >= CornerBounds.X;
}

bool SAutoSizeCommentNode::IsHeaderComment()
{
	return CommentNode->CommentColor == GetMutableDefault<UAutoSizeSettings>()->HeaderStyle.Color;
}

bool SAutoSizeCommentNode::IsPresetStyle()
{
	for (FPresetCommentStyle Style : GetMutableDefault<UAutoSizeSettings>()->PresetStyles)
		if (CommentNode->CommentColor == Style.Color && CommentNode->FontSize == Style.FontSize)
			return true;

	return false;
}

FSlateRect SAutoSizeCommentNode::GetNodeBounds(UEdGraphNode* Node)
{
	if (!Node)
		return FSlateRect();

	FVector2D Pos(Node->NodePosX, Node->NodePosY);

	FVector2D Size(300, 150);

	TWeakPtr<SGraphNode> LocalGraphNode = Node->DEPRECATED_NodeWidget;
	if (LocalGraphNode.IsValid())
	{
		Pos = LocalGraphNode.Pin()->GetPosition();
		Size = LocalGraphNode.Pin()->GetDesiredSize();

		if (SNodePanel::SNode::FNodeSlot* CommentSlot = LocalGraphNode.Pin()->GetSlot(ENodeZone::TopCenter))
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
