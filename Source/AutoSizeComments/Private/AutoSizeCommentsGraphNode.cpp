// Copyright 2020 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentsGraphNode.h"

#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsCacheFile.h"

#include "K2Node_Knot.h"
#include "EdGraphNode_Comment.h"
#include "SGraphPanel.h"
#include "SCommentBubble.h"
#include "TutorialMetaData.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#include "Runtime/Engine/Classes/EdGraph/EdGraph.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditorSettings.h"
//#include "ScopedTransaction.h"

void SAutoSizeCommentsGraphNode::Construct(const FArguments& InArgs, class UEdGraphNode* InNode)
{
	GraphNode = InNode;

	CommentNode = Cast<UEdGraphNode_Comment>(InNode);

	UpdateGraphNode();

	// Pull out sizes
	UserSize.X = InNode->NodeWidth;
	UserSize.Y = InNode->NodeHeight;

	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	const bool bIsPresetStyle = IsPresetStyle();
	const bool bIsHeaderComment = IsHeaderComment();

	// init color
	InitializeColor(ASCSettings, bIsPresetStyle, bIsHeaderComment);

	// use default font
	if (ASCSettings->bUseDefaultFontSize && !bIsHeaderComment && !bIsPresetStyle)
	{
		CommentNode->FontSize = ASCSettings->DefaultFontSize;
	}

	if (ASCSettings->bEnableGlobalSettings)
	{
		CommentNode->bColorCommentBubble = ASCSettings->bGlobalColorBubble;
		CommentNode->bCommentBubbleVisible_InDetailsPanel = CommentNode->bCommentBubblePinned = ASCSettings->bGlobalShowBubbleWhenZoomed;
	}
	
	bCachedBubbleVisibility = CommentNode->bCommentBubbleVisible;
	bCachedColorCommentBubble = CommentNode->bColorCommentBubble;

	// Set widget colors
	OpacityValue = ASCSettings->MinimumControlOpacity;
	CommentControlsTextColor = FLinearColor(1, 1, 1, OpacityValue);
	CommentControlsColor = FLinearColor(CommentNode->CommentColor.R, CommentNode->CommentColor.G, CommentNode->CommentColor.B, OpacityValue);
}

SAutoSizeCommentsGraphNode::~SAutoSizeCommentsGraphNode()
{
	SaveToCache();
}

void SAutoSizeCommentsGraphNode::InitializeColor(const UAutoSizeCommentsSettings* ASCSettings, const bool bIsPresetStyle, const bool bIsHeaderComment)
{
	// don't need to initialize if our color is a preset style or is a header comment 
	if (bIsPresetStyle || bIsHeaderComment)
		return;

    // Set comment color
    FLinearColor DefaultColor = ASCSettings->DefaultCommentColor;

    if (ASCSettings->bUseRandomColor)
    {
    	if (CommentNode->CommentColor == DefaultColor || CommentNode->CommentColor == FLinearColor::White) // only randomize if the node has the default color
    	{
    		RandomizeColor();
    	}
    }
    else if (ASCSettings->bAggressivelyUseDefaultColor)
    {
    	CommentNode->CommentColor = DefaultColor;
    }
    else if (CommentNode->CommentColor == FLinearColor::White)
    {
    	CommentNode->CommentColor = DefaultColor;
    }
}

void SAutoSizeCommentsGraphNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
{
	/** Copied from SGraphNodeComment::MoveTo */
	if (!bIsMoving)
	{
		if (GetDefault<UAutoSizeCommentsSettings>()->bRefreshContainingNodesOnMove)
		{
			RefreshNodesInsideComment(ECommentCollisionMethod::ASC_Collision_Contained);
			bIsMoving = true;
		}
	}
	
	FVector2D PositionDelta = NewPosition - GetPosition();
	
	FVector2D NewPos = GetPosition() + PositionDelta;
	SGraphNode::MoveTo(NewPos, NodeFilter);

	FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
	
	if (!(KeysState.IsAltDown() && KeysState.IsControlDown()) && !IsHeaderComment())
	{
		if (CommentNode && CommentNode->MoveMode == ECommentBoxMode::GroupMovement)
		{
			// Now update any nodes which are touching the comment but *not* selected
			// Selected nodes will be moved as part of the normal selection code
			TSharedPtr<SGraphPanel> Panel = GetOwnerPanel();

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

FReply SAutoSizeCommentsGraphNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsEditable.Get())
	{
		CachedAnchorPoint = GetAnchorPoint(MyGeometry, MouseEvent);
		if (CachedAnchorPoint != NONE)
		{
			DragSize = UserSize;
			bUserIsDragging = true;
			
			// ResizeTransaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "Resize Comment Node", "Resize Comment Node")));
			// CommentNode->Modify();
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}

		const FVector2D MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (CanBeSelected(MousePositionInNode))
		{
			if (GetDefault<UAutoSizeCommentsSettings>()->bRefreshContainingNodesOnMove)
			{
				bIsMoving = false;
			}
		}
	}

	return SGraphNode::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentsGraphNode::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && bUserIsDragging)
	{
		bUserIsDragging = false;
		CachedAnchorPoint = NONE;
		RefreshNodesInsideComment();

#if ENGINE_MINOR_VERSION >= 23
		TArray<UEdGraphNode*> AllNodes = GetNodeObj()->GetGraph()->Nodes;
		for (auto& Node : AllNodes)
		{
			Node->SetNodeUnrelated(false);
		}
#endif
		
		return FReply::Handled().ReleaseMouseCapture();
	}

	return SGraphNode::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentsGraphNode::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bUserIsDragging)
	{
		FVector2D Padding(5, 5);
		FVector2D MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		MousePositionInNode.X = FMath::RoundToInt(MousePositionInNode.X);
		MousePositionInNode.Y = FMath::RoundToInt(MousePositionInNode.Y);
		
		int32 OldNodeWidth = GraphNode->NodeWidth;
		int32 OldNodeHeight = GraphNode->NodeHeight;

		FVector2D NewSize = UserSize;

		bool bAnchorLeft = false;
		bool bAnchorTop = false;

		// LEFT
		if (CachedAnchorPoint == LEFT || CachedAnchorPoint == TOP_LEFT || CachedAnchorPoint == BOTTOM_LEFT)
		{
			bAnchorLeft = true;
			NewSize.X -= MousePositionInNode.X - Padding.X;
		}

		// RIGHT
		if (CachedAnchorPoint == RIGHT || CachedAnchorPoint == TOP_RIGHT || CachedAnchorPoint == BOTTOM_RIGHT)
		{
			NewSize.X = MousePositionInNode.X + Padding.X;
		}

		// TOP
		if (CachedAnchorPoint == TOP || CachedAnchorPoint == TOP_LEFT || CachedAnchorPoint == TOP_RIGHT)
		{
			bAnchorTop = true;
			NewSize.Y -= MousePositionInNode.Y - Padding.Y;
		}

		// BOTTOM
		if (CachedAnchorPoint == BOTTOM || CachedAnchorPoint == BOTTOM_LEFT || CachedAnchorPoint == BOTTOM_RIGHT)
		{
			NewSize.Y = MousePositionInNode.Y + Padding.Y;
		}

		AdjustMinSize(NewSize);

		if (GetDefault<UAutoSizeCommentsSettings>()->bSnapToGridWhileResizing)
		{
			SnapVectorToGrid(NewSize);
		}
		
		if (UserSize != NewSize)
		{
			UserSize = NewSize;
			
			GetNodeObj()->ResizeNode(UserSize);

			if (bAnchorLeft)
			{
				int32 DeltaWidth = GraphNode->NodeWidth - OldNodeWidth;
				GraphNode->NodePosX -= DeltaWidth;
			}
			
			if (bAnchorTop)
			{
				int32 DeltaHeight = GraphNode->NodeHeight - OldNodeHeight;
				GraphNode->NodePosY -= DeltaHeight;
			}
		}

#if ENGINE_MINOR_VERSION >= 23
		TArray<UEdGraphNode*> AllNodes = GetNodeObj()->GetGraph()->Nodes;
		for (auto& Node : AllNodes)
		{
			Node->SetNodeUnrelated(true);
		}	

		GetNodeObj()->SetNodeUnrelated(false);
		
		TArray<TSharedPtr<SGraphNode>> Nodes;
		QueryNodesUnderComment(Nodes);
		for (TSharedPtr<SGraphNode> Node : Nodes)
		{
			Node->GetNodeObj()->SetNodeUnrelated(false);
		}
#endif
	}
		
	return SGraphNode::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SAutoSizeCommentsGraphNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
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

void SAutoSizeCommentsGraphNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	bool bRequireUpdate = false;
	
	UpdateRefreshDelay();

	if (RefreshNodesDelay == 0 && !IsHeaderComment() && !bUserIsDragging)
	{
		FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();

		bool bIsAltDown = KeysState.IsAltDown();
		if (!bIsAltDown)
		{
			if (bPreviousAltDown) // refresh when the alt key is released
			{
				RefreshNodesInsideComment(GetDefault<UAutoSizeCommentsSettings>()->AltCollisionMethod);
			}

			ResizeToFit();

			MoveEmptyCommentBoxes();

			// if (ResizeTransaction.IsValid())
			// {
			// 	ResizeTransaction.Reset();
			// }
		}

		bPreviousAltDown = bIsAltDown;
	}

	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	const auto ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
	
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

	// Update global color bubble and bubble visibility
	if (ASCSettings->bEnableGlobalSettings)
	{
		if (CommentNode->bColorCommentBubble != ASCSettings->bGlobalColorBubble)
		{
			bRequireUpdate = true;
			CommentNode->bColorCommentBubble = ASCSettings->bGlobalColorBubble;
		}

		if (CommentNode->bCommentBubbleVisible_InDetailsPanel != ASCSettings->bGlobalShowBubbleWhenZoomed)
		{
			CommentNode->bCommentBubbleVisible_InDetailsPanel = CommentNode->bCommentBubblePinned = ASCSettings->bGlobalShowBubbleWhenZoomed;

			if (CommentBubble.IsValid())
			{
				CommentBubble->UpdateBubble();
			}
		}
	}
	else // Otherwise update when cached values have changed
	{
		if (bCachedBubbleVisibility != CommentNode->bCommentBubbleVisible_InDetailsPanel)
		{
			bCachedBubbleVisibility = CommentNode->bCommentBubbleVisible_InDetailsPanel;
			if (CommentBubble.IsValid())
			{
				CommentBubble->UpdateBubble();
			}
		}

		if (bCachedColorCommentBubble != CommentNode->bColorCommentBubble)
		{
			bRequireUpdate = true;
			bCachedColorCommentBubble = CommentNode->bColorCommentBubble;
		}
	}

	// Update cached font size
	if (CachedFontSize != CommentNode->FontSize)
	{
		bRequireUpdate = true;
	}

	if (CachedNumPresets != ASCSettings->PresetStyles.Num())
	{
		bRequireUpdate = true;
	}

	if (bRequireUpdate)
	{
		UpdateGraphNode();
	}

	UpdateColors(InDeltaTime);
}

void SAutoSizeCommentsGraphNode::UpdateGraphNode()
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
	
	// No pins in a comment box
	InputPins.Empty();
	OutputPins.Empty();

	// Avoid standard box model too
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	SetupErrorReporting();

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);

	CommentStyle = FEditorStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.CommentBlock.TitleInlineEditableText");
	CommentStyle.EditableTextBoxStyle.Font.Size = CommentNode->FontSize;
	CommentStyle.TextStyle.Font.Size = CommentNode->FontSize;
	CachedFontSize = CommentNode->FontSize;

	// Create comment bubble
	if (!ASCSettings->bHideCommentBubble)
	{
		CommentBubble = SNew(SCommentBubble)
			.GraphNode(GraphNode)
			.Text(this, &SAutoSizeCommentsGraphNode::GetNodeComment)
			.OnTextCommitted(this, &SAutoSizeCommentsGraphNode::OnNameTextCommited)
			.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentBubbleColor)
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
	
	// Create the toggle header button
	ToggleHeaderButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleHeaderButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Toggle between a header node and a resizing node"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("H")))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	TSharedRef<SBox> AnchorBox =
		SNew(SBox).WidthOverride(16).HeightOverride(16)
		[
			SNew(SBorder).BorderImage(FEditorStyle::GetBrush("Tutorials.Border"))
		];

	const bool bHideCornerPoints = ASCSettings->bHideCornerPoints;
	
	CreateCommentControls();

	CreateColorControls();

	ETextJustify::Type CommentTextAlignment = ASCSettings->CommentTextAlignment;
	
	TSharedRef<SInlineEditableTextBlock> CommentTextBlock = SAssignNew(InlineEditableText, SInlineEditableTextBlock)
		.Style(&CommentStyle)
		.Text(this, &SAutoSizeCommentsGraphNode::GetEditableNodeTitleAsText)
		.OnVerifyTextChanged(this, &SAutoSizeCommentsGraphNode::OnVerifyNameTextChanged)
		.OnTextCommitted(this, &SAutoSizeCommentsGraphNode::OnNameTextCommited)
		.IsReadOnly(this, &SAutoSizeCommentsGraphNode::IsNameReadOnly)
		.IsSelected(this, &SAutoSizeCommentsGraphNode::IsSelectedExclusively)
		.WrapTextAt(this, &SAutoSizeCommentsGraphNode::GetWrapAt)
		.MultiLine(true)
		.ModiferKeyForNewLine(EModifierKey::Shift)
		.Justification(CommentTextAlignment);
	
	// Create the top horizontal box containing anchor points (header comments don't need these)
	TSharedRef<SHorizontalBox> TopHBox = SNew(SHorizontalBox);

	if (!bHideCornerPoints)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Top).AttachWidget(AnchorBox);
	}

	FMargin CommentTextPadding = ASCSettings->CommentTextPadding;
	TopHBox->AddSlot().Padding(CommentTextPadding).FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(CommentTextBlock);
	
	if (!ASCSettings->bHideHeaderButton)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top).AttachWidget(ToggleHeaderButton.ToSharedRef());
	}
	
	if (!bHideCornerPoints)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top).AttachWidget(AnchorBox);
	}

	// Create the bottom horizontal box containing comment controls and anchor points (header comments don't need these)
	TSharedRef<SHorizontalBox> BottomHBox = SNew(SHorizontalBox);
	if (!IsHeaderComment())
	{
		if (!bHideCornerPoints)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AttachWidget(AnchorBox);
		}

		if (!ASCSettings->bHideCommentBoxControls)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill).AttachWidget(CommentControls.ToSharedRef());
		}
		
		BottomHBox->AddSlot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(FEditorStyle::GetBrush("NoBorder")));

		if (!bHideCornerPoints)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Bottom).AttachWidget(AnchorBox);
		}
	}

	// Create the title bar
	SAssignNew(TitleBar, SBorder)
		.BorderImage(FEditorStyle::GetBrush("Graph.Node.TitleBackground"))
		.BorderBackgroundColor(this, &SAutoSizeCommentsGraphNode::GetCommentTitleBarColor)
		.HAlign(HAlign_Fill).VAlign(VAlign_Top)
		[
			TopHBox
		];

	if (!GetDefault<UAutoSizeCommentsSettings>()->bDisableTooltip)
	{
		TitleBar->SetToolTipText(TAttribute<FText>(this, &SGraphNode::GetNodeTooltip));
	}

	// Create the main vertical box containing all the widgets
	auto MainVBox = SNew(SVerticalBox);
	MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(TitleBar.ToSharedRef());
	MainVBox->AddSlot().AutoHeight().Padding(1.0f).AttachWidget(ErrorReporting->AsWidget());
	if (!IsHeaderComment() && (!ASCSettings->bHidePresets || !GetDefault<UAutoSizeCommentsSettings>()->bHideRandomizeButton))
	{
		MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(ColorControls.ToSharedRef());
	}
	MainVBox->AddSlot().FillHeight(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(FEditorStyle::GetBrush("NoBorder")));

	MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).AttachWidget(BottomHBox);
	
	ContentScale.Bind(this, &SGraphNode::GetContentScale);
	GetOrAddSlot(ENodeZone::Center).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Kismet.Comment.Background"))
		.ColorAndOpacity(FLinearColor::White)
		.BorderBackgroundColor(this, &SAutoSizeCommentsGraphNode::GetCommentBodyColor)
		.AddMetaData<FGraphNodeMetaData>(TagMeta)
		[
			MainVBox
		]
	];
}

FVector2D SAutoSizeCommentsGraphNode::ComputeDesiredSize(float) const
{
	return UserSize;
}

bool SAutoSizeCommentsGraphNode::IsNameReadOnly() const
{
	return !IsEditable.Get() || SGraphNode::IsNameReadOnly();
}

void SAutoSizeCommentsGraphNode::SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel)
{
	SGraphNode::SetOwner(OwnerPanel);

	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		return;
	}

	if (LoadCache())
	{
		return;
	}

	if (!GetDefault<UAutoSizeCommentsSettings>()->bIgnoreSelectedNodesOnCreation && AnySelectedNodes() && AddInitialNodes())
	{
		return;
	}

	if (GetDefault<UAutoSizeCommentsSettings>()->bDetectNodesContainedForNewComments)
	{
		// Refresh the nodes under the comment
		RefreshNodesDelay = 2;
	}
}

bool SAutoSizeCommentsGraphNode::CanBeSelected(const FVector2D& MousePositionInNode) const
{
	const FVector2D Size = GetDesiredSize();
	return MousePositionInNode.X >= 0 && MousePositionInNode.X <= Size.X && MousePositionInNode.Y >= 0 && MousePositionInNode.Y <= GetTitleBarHeight();
}

FVector2D SAutoSizeCommentsGraphNode::GetDesiredSizeForMarquee() const
{
	return FVector2D(UserSize.X, GetTitleBarHeight());
}

FCursorReply SAutoSizeCommentsGraphNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ToggleHeaderButton->IsHovered() || ColorControls->IsHovered() || CommentControls->IsHovered())
	{
		return FCursorReply::Unhandled();
	}

	auto AnchorPoint = GetAnchorPoint(MyGeometry, CursorEvent);
	
	if (AnchorPoint == TOP_LEFT || AnchorPoint == BOTTOM_RIGHT)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
	}

	if (AnchorPoint == TOP_RIGHT || AnchorPoint == BOTTOM_LEFT)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
	}

	if (AnchorPoint == TOP || AnchorPoint == BOTTOM)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
	}
		
	if (AnchorPoint == LEFT || AnchorPoint == RIGHT)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	const FVector2D LocalMouseCoordinates = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	if (CanBeSelected(LocalMouseCoordinates))
	{
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}

	return FCursorReply::Unhandled();
}

int32 SAutoSizeCommentsGraphNode::GetSortDepth() const
{
	if (IsHeaderComment())
		return 1;
	
	if (IsSelectedExclusively())
		return 0;
	
	return CommentNode ? CommentNode->CommentDepth : -1;
}

FReply SAutoSizeCommentsGraphNode::HandleRandomizeColorButtonClicked()
{
	RandomizeColor();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleHeaderButtonClicked()
{
	FPresetCommentStyle Style = GetMutableDefault<UAutoSizeCommentsSettings>()->HeaderStyle;

	if (CommentNode->CommentColor == Style.Color)
	{
		CommentNode->CommentColor = FLinearColor::MakeRandomColor();
		CommentNode->FontSize = GetMutableDefault<UAutoSizeCommentsSettings>()->DefaultFontSize;
		AdjustMinSize(UserSize);
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

FReply SAutoSizeCommentsGraphNode::HandleRefreshButtonClicked()
{
	if (AnySelectedNodes())
	{
		CommentNode->ClearNodesUnderComment();
		AddAllSelectedNodes();
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandlePresetButtonClicked(FPresetCommentStyle Style)
{
	CommentNode->CommentColor = Style.Color;
	CommentNode->FontSize = Style.FontSize;
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleAddButtonClicked()
{
	AddAllSelectedNodes();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleSubtractButtonClicked()
{
	RemoveAllSelectedNodes();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleClearButtonClicked()
{
	CommentNode->ClearNodesUnderComment();
	UpdateExistingCommentNodes();
	return FReply::Handled();
}

bool SAutoSizeCommentsGraphNode::AddInitialNodes()
{
	if (CommentNode->GetNodesUnderComment().Num() > 0)
		return false;
	
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	bool bDidAddAnything = false;

	if (!OwnerPanel.IsValid())
		return false;
	
	auto SelectedNodes = OwnerPanel->SelectionManager.GetSelectedNodes();
	const auto IsCommentNode = [](UObject* Obj)
	{
		return Cast<UEdGraphNode_Comment>(Obj) != nullptr;
	};

	const auto SelectedCommentNodes = SelectedNodes.Array().FilterByPredicate(IsCommentNode);
	if (SelectedCommentNodes.Num() > 0)
	{
		return false;
	}
	
	for (UObject* SelectedObj : SelectedNodes)
	{
		if (CanAddNode(SelectedObj))
		{
			CommentNode->AddNodeUnderComment(SelectedObj);
			bDidAddAnything = true;
		}
	}

	if (bDidAddAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentsGraphNode::AddAllSelectedNodes()
{
	bool bDidAddAnything = false;
	
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	auto SelectedNodes = OwnerPanel->SelectionManager.GetSelectedNodes();
	for (UObject* SelectedObj : SelectedNodes)
	{
		if (CanAddNode(SelectedObj))
		{
			CommentNode->AddNodeUnderComment(SelectedObj);
			bDidAddAnything = true;
		}
	}

	if (bDidAddAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentsGraphNode::RemoveAllSelectedNodes()
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

void SAutoSizeCommentsGraphNode::UpdateColors(float InDeltaTime)
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
	
	if (IsHovered())
	{
		if (OpacityValue < 1.f)
		{
			const float FadeUpAmt = InDeltaTime * 5.f;
			OpacityValue = FMath::Min(OpacityValue + FadeUpAmt, 1.f);
		}
	}
	else
	{
		if (OpacityValue > ASCSettings->MinimumControlOpacity)
		{
			const float FadeDownAmt = InDeltaTime * 5.f;
			OpacityValue = FMath::Max(OpacityValue - FadeDownAmt, ASCSettings->MinimumControlOpacity);
		}
	}

	CommentControlsColor = FLinearColor(CommentNode->CommentColor.R, CommentNode->CommentColor.G, CommentNode->CommentColor.B, OpacityValue);
	CommentControlsTextColor.A = OpacityValue;
}

FSlateRect SAutoSizeCommentsGraphNode::GetTitleRect() const
{
	const FVector2D NodePosition = GetPosition();
	FVector2D NodeSize = TitleBar.IsValid() ? TitleBar->GetDesiredSize() : GetDesiredSize();
	const FSlateRect TitleBarOffset(13, 8, -3, 0);

	return FSlateRect(NodePosition.X, NodePosition.Y, NodePosition.X + NodeSize.X, NodePosition.Y + NodeSize.Y) + TitleBarOffset;
}

void SAutoSizeCommentsGraphNode::UpdateRefreshDelay()
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
			RefreshNodesInsideComment(ASC_Collision_Point);
		}
	}
}

void SAutoSizeCommentsGraphNode::RefreshNodesInsideComment(ECommentCollisionMethod OverrideCollisionMethod)
{
	CommentNode->ClearNodesUnderComment();

	TArray<TSharedPtr<SGraphNode>> OutNodes;
	QueryNodesUnderComment(OutNodes, OverrideCollisionMethod);
	for (TSharedPtr<SGraphNode> Node : OutNodes)
	{
		if (CanAddNode(Node))
		{
			CommentNode->AddNodeUnderComment(Node->GetNodeObj());
		}
	}

	UpdateExistingCommentNodes();
}

float SAutoSizeCommentsGraphNode::GetTitleBarHeight() const
{
	return TitleBar.IsValid() ? TitleBar->GetDesiredSize().Y : 0.0f;
}

void SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes()
{
	// Get list of all other comment nodes
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> OtherCommentNodes = GetOtherCommentNodes();

	// Do nothing if we have no nodes under ourselves
	if (CommentNode->GetNodesUnderComment().Num() == 0)
	{
		if (GetDefault<UAutoSizeCommentsSettings>()->bMoveEmptyCommentBoxes)
		{
			for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : OtherCommentNodes)
			{
				UEdGraphNode_Comment* OtherComment =  OtherCommentNode->GetCommentNodeObj();
				
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

	for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : OtherCommentNodes)
	{
		UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();
		
		// if the other comment node contains us, remove ourselves
		if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
		{
			TSet<UObject*> NodesToRemove;
			NodesToRemove.Emplace(CommentNode);
			RemoveNodesFromUnderComment(OtherComment, NodesToRemove);
		}
	}

	for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : OtherCommentNodes)
	{
		UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();
		
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
			const auto FilterComments = [](UObject* Node) { return Cast<UEdGraphNode_Comment>(Node) == nullptr; };
			const auto OtherNodesWithoutComments = OtherComment->GetNodesUnderComment().FilterByPredicate(FilterComments);

			// check if all nodes in the other comment box are within our comment box
			bool bAllNodesContainedUnderSelf = !OtherNodesWithoutComments.ContainsByPredicate([this](UObject* NodeUnderOther)
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

FSlateColor SAutoSizeCommentsGraphNode::GetCommentBodyColor() const
{
	return CommentNode ? CommentNode->CommentColor : FLinearColor::White;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentTitleBarColor() const
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

FSlateColor SAutoSizeCommentsGraphNode::GetCommentBubbleColor() const
{
	if (CommentNode)
	{
		const FLinearColor Color = CommentNode->bColorCommentBubble
			? (CommentNode->CommentColor * 0.6f)
			: GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

		return FLinearColor(Color.R, Color.G, Color.B);
	}
	else
	{
		const FLinearColor Color = FLinearColor::White * 0.6f;
		return FLinearColor(Color.R, Color.G, Color.B);
	}
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentControlsColor() const
{
	return CommentControlsColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentControlsTextColor() const
{
	return CommentControlsTextColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetPresetColor(FLinearColor Color) const
{
	FLinearColor MyColor = Color;
	MyColor.A = OpacityValue;
	return MyColor;
}

float SAutoSizeCommentsGraphNode::GetWrapAt() const
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
	const float HeaderSize = ASCSettings->bHideHeaderButton ? 0 : 20;
	const float AnchorPointWidth = ASCSettings->bHideCornerPoints ? 0 : 32;
	const float TextPadding = ASCSettings->CommentTextPadding.Left + ASCSettings->CommentTextPadding.Right;
	return FMath::Max(0.f, CachedWidth - AnchorPointWidth - HeaderSize - TextPadding - 12);
}

void SAutoSizeCommentsGraphNode::ResizeToFit()
{
	// Resize the node
	TArray<UObject*> UnfilteredNodesUnderComment = CommentNode->GetNodesUnderComment();

	SGraphPanel* Panel = GetOwnerPanel().Get();
	UEdGraph* Graph = Panel->GetGraphObj();
	TArray<UObject*> Nodes;
	Graph->GetNodesOfClass(Nodes);
		
	// Here we clear and add as there is no remove function
	CommentNode->ClearNodesUnderComment();

	for (UObject* Obj : UnfilteredNodesUnderComment)
	{
		// Make sure that we haven't somehow added ourselves
		check(Obj != CommentNode);

		// If a node gets deleted it can still stay inside the comment box
		// So checks if the node is still on the graph
		if (Obj != nullptr && !Obj->IsPendingKillOrUnreachable() && Nodes.Contains(Obj))
		{
			CommentNode->AddNodeUnderComment(Obj);
		}
	}
	
	// resize to fit the bounds of the nodes under the comment
	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
		
		// get bounds and apply padding
		const FVector2D Padding = ASCSettings->CommentNodePadding;
		
		const float VerticalPadding = FMath::Max(ASCSettings->MinimumVerticalPadding, Padding.Y); // ensure we can always see the buttons

		const float TopPadding = (!ASCSettings->bHidePresets || !ASCSettings->bHideRandomizeButton) ? VerticalPadding : Padding.Y;

		const float BottomPadding = !ASCSettings->bHideCommentBoxControls ? VerticalPadding : Padding.Y;
		
		const FSlateRect Bounds = GetBoundsForNodesInside().ExtendBy(FMargin(Padding.X, TopPadding, Padding.X, BottomPadding));

		const float TitleBarHeight = GetTitleBarHeight();
		const float CommentBubbleHeight = CommentBubble.IsValid() && CommentBubble->IsBubbleVisible() ? CommentBubble->GetDesiredSize().Y : 0;

		// check if size has changed
		FVector2D CurrSize = Bounds.GetSize();
		CurrSize.Y += TitleBarHeight;
		CurrSize.Y -= CommentBubbleHeight;

		if (!UserSize.Equals(CurrSize, .1f))
		{
			UserSize = CurrSize;
			GetNodeObj()->ResizeNode(CurrSize);
		}

		// check if location has changed
		FVector2D DesiredPos = Bounds.GetTopLeft();
		DesiredPos.Y -= TitleBarHeight - CommentBubbleHeight;

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

void SAutoSizeCommentsGraphNode::MoveEmptyCommentBoxes()
{
	TArray<UObject*> UnderComment = CommentNode->GetNodesUnderComment();

	TSharedPtr<SGraphPanel> OwnerPanel = OwnerGraphPanelPtr.Pin();

	bool bIsSelected = OwnerPanel->SelectionManager.IsNodeSelected(GraphNode);

	bool bIsContained = false;
	for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : GetOtherCommentNodes())
	{
		UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();
		
		if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
		{
			bIsContained = true;
			break;
		}
	}

	// if the comment node is empty, move away from other comment nodes
	if (UnderComment.Num() == 0 && GetDefault<UAutoSizeCommentsSettings>()->bMoveEmptyCommentBoxes && !bIsSelected && !bIsContained && !IsHeaderComment())
	{
		FVector2D TotalMovement(0, 0);

		bool bAnyCollision = false;

		for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : GetOtherCommentNodes())
		{
			if (OtherCommentNode->IsHeaderComment())
			{
				continue;
			}
			
			UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();
			
			if (OtherComment->GetNodesUnderComment().Contains(CommentNode))
			{
				continue;
			}

			FSlateRect OtherBounds = GetCommentBounds(OtherComment);
			FSlateRect MyBounds = GetCommentBounds(CommentNode);

			if (FSlateRect::DoRectanglesIntersect(OtherBounds, MyBounds))
			{
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

		TotalMovement *= GetDefault<UAutoSizeCommentsSettings>()->EmptyCommentBoxSpeed;

		GraphNode->NodePosX += TotalMovement.X;
		GraphNode->NodePosY += TotalMovement.Y;
	}
}

void SAutoSizeCommentsGraphNode::CreateCommentControls()
{
	// Create the replace button
	TSharedRef<SButton> ReplaceButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleRefreshButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Replace with selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("R")))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	// Create the add button
	TSharedRef<SButton> AddButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleAddButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Add selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				TSharedRef<SWidget>(
					SNew(SImage)
					.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					.Image(FCoreStyle::Get().GetBrush("EditableComboBox.Add")
					))
			]
		];


	// Create the remove button
	TSharedRef<SButton> RemoveButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleSubtractButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Remove selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				TSharedRef<SWidget>(
					SNew(SImage)
					.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					.Image(FCoreStyle::Get().GetBrush("EditableComboBox.Delete")
					))
			]
		];

	// Create the clear button
	TSharedRef<SButton> ClearButton = SNew(SButton)
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleClearButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.ToolTipText(FText::FromString("Clear all nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				TSharedRef<SWidget>(
					SNew(SImage)
					.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					.Image(FCoreStyle::Get().GetBrush("TrashCan_Small")
					))
			]
		];

	// Create the comment controls
	CommentControls = SNew(SHorizontalBox);
	CommentControls->AddSlot().AttachWidget(ReplaceButton);
	CommentControls->AddSlot().AttachWidget(AddButton);
	CommentControls->AddSlot().AttachWidget(RemoveButton);
	CommentControls->AddSlot().AttachWidget(ClearButton);
}

void SAutoSizeCommentsGraphNode::CreateColorControls()
{
	// Create the color controls
	ColorControls = SNew(SHorizontalBox);

	TArray<FPresetCommentStyle> Presets = GetMutableDefault<UAutoSizeCommentsSettings>()->PresetStyles;
	CachedNumPresets = Presets.Num();

	if (!IsHeaderComment()) // header comments don't need color presets
	{
		ColorControls->AddSlot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(FEditorStyle::GetBrush("NoBorder")));

		auto Buttons = SNew(SHorizontalBox);
		ColorControls->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Fill).AttachWidget(Buttons);
		
		if (!GetDefault<UAutoSizeCommentsSettings>()->bHidePresets)
		{
			for (FPresetCommentStyle Preset : Presets)
			{
				FLinearColor ColorWithoutOpacity = Preset.Color;
				ColorWithoutOpacity.A = 1;

				TSharedRef<SButton> Button = SNew(SButton)
					.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetPresetColor, ColorWithoutOpacity)
					.OnClicked(this, &SAutoSizeCommentsGraphNode::HandlePresetButtonClicked, Preset)
					.ContentPadding(FMargin(2, 2))
					.ToolTipText(FText::FromString("Set preset color"))
					[
						SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
					];

				Buttons->AddSlot().AttachWidget(Button);
			}
		}

		if (!GetDefault<UAutoSizeCommentsSettings>()->bHideRandomizeButton)
		{
			// Create the random color button
			TSharedRef<SButton> RandomColorButton = SNew(SButton)
				.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
				.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleRandomizeColorButtonClicked)
				.ContentPadding(FMargin(2, 2))
				.ToolTipText(FText::FromString("Randomize the color of the comment box"))
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString("?")))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
					]
				];
			
			Buttons->AddSlot().AttachWidget(RandomColorButton);
		}
	}
}

/***************************
****** Util functions ******
****************************/

bool SAutoSizeCommentsGraphNode::RemoveNodesFromUnderComment(UEdGraphNode_Comment* InCommentNode, TSet<UObject*>& NodesToRemove)
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

TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> SAutoSizeCommentsGraphNode::GetOtherCommentNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	FChildren* PanelChildren = OwnerPanel->GetAllChildren();
	int32 NumChildren = PanelChildren->Num();

	// Get list of all other comment nodes
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> OtherCommentNodes;
	for (int32 NodeIndex = 0; NodeIndex < NumChildren; ++NodeIndex)
	{
		TSharedPtr<SGraphNode> SomeNodeWidget = StaticCastSharedRef<SGraphNode>(PanelChildren->GetChildAt(NodeIndex));
		TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode = StaticCastSharedPtr<SAutoSizeCommentsGraphNode>(SomeNodeWidget);
		if (ASCGraphNode.IsValid())
		{
			UObject* GraphObject = SomeNodeWidget->GetObjectBeingDisplayed();
			if (UEdGraphNode_Comment* GraphCommentNode = Cast<UEdGraphNode_Comment>(GraphObject))
			{
				if (GraphCommentNode != CommentNode)
					OtherCommentNodes.Add(ASCGraphNode);
			}
		}
	}

	return OtherCommentNodes;
}

FSlateRect SAutoSizeCommentsGraphNode::GetCommentBounds(UEdGraphNode_Comment* InCommentNode)
{
	FVector2D Point(InCommentNode->NodePosX, InCommentNode->NodePosY);
	FVector2D Extent(InCommentNode->NodeWidth, InCommentNode->NodeHeight);
	return FSlateRect::FromPointAndExtent(Point, Extent);
}

void SAutoSizeCommentsGraphNode::SnapVectorToGrid(FVector2D& Vector)
{
	const float SnapSize = SNodePanel::GetSnapGridSize();
	Vector.X = SnapSize * FMath::RoundToFloat(Vector.X / SnapSize);
	Vector.Y = SnapSize * FMath::RoundToFloat(Vector.Y / SnapSize);
}

bool SAutoSizeCommentsGraphNode::IsLocalPositionInCorner(const FVector2D& MousePositionInNode) const
{
	FVector2D CornerBounds = GetDesiredSize() - FVector2D(40, 40);
	return MousePositionInNode.Y >= CornerBounds.Y && MousePositionInNode.X >= CornerBounds.X;
}

ASC_AnchorPoint SAutoSizeCommentsGraphNode::GetAnchorPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	const FVector2D MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	const FVector2D Size = GetDesiredSize();

	// check the mouse position is actually in the node
	if (MousePositionInNode.X < 0 || MousePositionInNode.X > Size.X || 
		MousePositionInNode.Y < 0 || MousePositionInNode.Y > Size.Y)
	{
		return NONE;
	}

	const float SidePadding = 10.f;
	const float Top = AnchorSize;
	const float Left = AnchorSize;
	const float Right = Size.X - AnchorSize;
	const float Bottom = Size.Y - AnchorSize;

	if (!IsHeaderComment()) // header comments should only have anchor points to resize left and right
	{
		if (MousePositionInNode.X > Right && MousePositionInNode.Y > Bottom)
		{
			return BOTTOM_RIGHT;
		}
		if (MousePositionInNode.X < Left && MousePositionInNode.Y < Top)
		{
			return TOP_LEFT;
		}
		if (MousePositionInNode.X < Left && MousePositionInNode.Y > Bottom)
		{
			return BOTTOM_LEFT;
		}
		if (MousePositionInNode.X > Right && MousePositionInNode.Y < Top)
		{
			return TOP_RIGHT;
		}
		if (MousePositionInNode.Y < SidePadding)
		{
			return TOP;
		}
		if (MousePositionInNode.Y > Size.Y - SidePadding)
		{
			return BOTTOM;
		}
	}
	if (MousePositionInNode.X < SidePadding)
	{
		return LEFT;
	}
	if (MousePositionInNode.X > Size.X - SidePadding)
	{
		return RIGHT;
	}

	return NONE;
}

bool SAutoSizeCommentsGraphNode::IsHeaderComment() const
{
	return CommentNode->CommentColor == GetMutableDefault<UAutoSizeCommentsSettings>()->HeaderStyle.Color;
}

bool SAutoSizeCommentsGraphNode::IsPresetStyle()
{
	for (FPresetCommentStyle Style : GetMutableDefault<UAutoSizeCommentsSettings>()->PresetStyles)
		if (CommentNode->CommentColor == Style.Color && CommentNode->FontSize == Style.FontSize)
			return true;

	return false;
}

bool SAutoSizeCommentsGraphNode::LoadCache()
{
	FAutoSizeCommentsCacheFile& SizeCache = IAutoSizeCommentsModule::Get().GetSizeCache();
	TArray<UEdGraphNode*> OutNodesUnder;
	if (SizeCache.GetNodesUnderComment(SharedThis(this), OutNodesUnder))
	{
		CommentNode->ClearNodesUnderComment();
		for (UEdGraphNode* Node : OutNodesUnder)
		{
			if (!HasNodeBeenDeleted(Node))
			{
				CommentNode->AddNodeUnderComment(Node);
			}
		}
		
		return true;
	}
	
	return false;
}

void SAutoSizeCommentsGraphNode::SaveToCache()
{
	FASCCommentData& GraphData = IAutoSizeCommentsModule::Get().GetSizeCache().GetGraphData(CommentNode->GetGraph());
	
	if (HasNodeBeenDeleted(CommentNode))
	{
		GraphData.CommentData.Remove(CommentNode->NodeGuid);
	}
	else
	{
		FASCNodesInside& NodesInside = GraphData.CommentData.FindOrAdd(CommentNode->NodeGuid);
		NodesInside.NodeGuids.Empty();
		
		// update cache file
		const auto Nodes = GetEdGraphNodesUnderComment(CommentNode);
		for (auto Node : Nodes)
		{
			if (!HasNodeBeenDeleted(Node))
			{
				NodesInside.NodeGuids.Add(Node->NodeGuid);
			}
		}
	}
}

void SAutoSizeCommentsGraphNode::QueryNodesUnderComment(TArray<TSharedPtr<SGraphNode>>& OutNodesUnderComment, ECommentCollisionMethod OverrideCollisionMethod)
{
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
		if (GraphObject == nullptr || GraphObject == CommentNode)
		{
			continue;
		}

		// check if the node bounds collides with our bounds
		const FVector2D SomeNodePosition = SomeNodeWidget->GetPosition();
		const FVector2D SomeNodeSize = SomeNodeWidget->GetDesiredSize();
		const FSlateRect NodeGeometryGraphSpace = FSlateRect::FromPointAndExtent(SomeNodePosition, SomeNodeSize);

		bool bIsOverlapping = false;
		ECommentCollisionMethod CollisionMethod = OverrideCollisionMethod == ASC_Collision_Default
			? GetDefault<UAutoSizeCommentsSettings>()->ResizeCollisionMethod.GetValue()
			: OverrideCollisionMethod;
		
		switch (CollisionMethod)
		{
		case ASC_Collision_Point:
			bIsOverlapping = CommentRect.ContainsPoint(SomeNodePosition);
			break;
		case ASC_Collision_Intersect:
			CommentRect.IntersectionWith(NodeGeometryGraphSpace, bIsOverlapping);
			break;
		case ASC_Collision_Contained:
			bIsOverlapping = FSlateRect::IsRectangleContained(CommentRect, NodeGeometryGraphSpace);
			break;
		case ASC_Collision_Default:
			bIsOverlapping = FSlateRect::IsRectangleContained(CommentRect, NodeGeometryGraphSpace);
			break;
		default: ;
		}
		
		if (bIsOverlapping)
		{
			OutNodesUnderComment.Add(SomeNodeWidget);
		}
	}
}

void SAutoSizeCommentsGraphNode::RandomizeColor()
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	if (ASCSettings->bUseRandomColorFromList)
	{
		const int RandIndex = FMath::Rand() % ASCSettings->PredefinedRandomColorList.Num();
		if (ASCSettings->PredefinedRandomColorList.IsValidIndex(RandIndex))
		{
			CommentNode->CommentColor = ASCSettings->PredefinedRandomColorList[RandIndex];
		}
	}
	else
	{
		CommentNode->CommentColor = FLinearColor::MakeRandomColor();
		CommentNode->CommentColor.A = ASCSettings->RandomColorOpacity;
	}
}

void SAutoSizeCommentsGraphNode::AdjustMinSize(FVector2D& InSize)
{
	const float MinY = IsHeaderComment() ? 0 : 80;
	InSize = FVector2D(FMath::Max(125.f, InSize.X), FMath::Max(GetTitleBarHeight() + MinY, InSize.Y));
}

bool SAutoSizeCommentsGraphNode::HasNodeBeenDeleted(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return true;
	}
	
	return !CommentNode->GetGraph()->Nodes.Contains(Node);
}

bool SAutoSizeCommentsGraphNode::CanAddNode(TSharedPtr<SGraphNode> OtherGraphNode) const
{
	UObject* GraphObject = OtherGraphNode->GetObjectBeingDisplayed();
	if (GraphObject == nullptr || GraphObject == CommentNode)
	{
		return false;
	}

	if (CommentNode->GetNodesUnderComment().Contains(GraphObject))
	{
		return false;
	}

	if (GetDefault<UAutoSizeCommentsSettings>()->bIgnoreKnotNodes && Cast<UK2Node_Knot>(GraphObject) != nullptr)
	{
		return false;
	}
	
	if (Cast<UEdGraphNode_Comment>(GraphObject))
	{
		TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode = StaticCastSharedPtr<SAutoSizeCommentsGraphNode>(OtherGraphNode);
		if (!ASCNode.IsValid() || !ASCNode->IsHeaderComment())
		{
			return false;
		}
	}

	return true;
}

bool SAutoSizeCommentsGraphNode::CanAddNode(UObject* Node) const
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel.IsValid())
		return false;
	
	UEdGraphNode* EdGraphNode = Cast<UEdGraphNode>(Node);
	if (!EdGraphNode)
		return false;
	
	TSharedPtr<SGraphNode> NodeAsGraphNode = OwnerPanel->GetNodeWidgetFromGuid(EdGraphNode->NodeGuid);
	if (!NodeAsGraphNode.IsValid())
		return false;
	
	return CanAddNode(NodeAsGraphNode);
}

FSlateRect SAutoSizeCommentsGraphNode::GetNodeBounds(UEdGraphNode* Node)
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

		if (CommentBubble.IsValid() && CommentBubble->IsBubbleVisible())
		{
			FVector2D CommentBubbleSize = CommentBubble->GetSize();

			Pos.Y -= CommentBubbleSize.Y;
			Size.Y += CommentBubbleSize.Y;
		}
	}

	return FSlateRect::FromPointAndExtent(Pos, Size);
}

bool SAutoSizeCommentsGraphNode::AnySelectedNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	return OwnerPanel->SelectionManager.GetSelectedNodes().Num() > 0;
}

FSlateRect SAutoSizeCommentsGraphNode::GetBoundsForNodesInside()
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

TArray<UEdGraphNode*> SAutoSizeCommentsGraphNode::GetEdGraphNodesUnderComment(UEdGraphNode_Comment* InCommentNode) const
{
	TArray<UEdGraphNode*> OutNodes;
	for (UObject* Obj : CommentNode->GetNodesUnderComment())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			OutNodes.Add(Node);
		}
	}

	return OutNodes;
}
