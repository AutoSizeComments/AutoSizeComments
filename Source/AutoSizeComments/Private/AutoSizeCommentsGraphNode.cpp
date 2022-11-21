// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphNode.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsInputProcessor.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsStyle.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditorSettings.h"
#include "K2Node_Knot.h"
#include "SCommentBubble.h"
#include "SGraphPanel.h"
#include "TutorialMetaData.h"
#include "Framework/Application/SlateApplication.h"
#include "Runtime/Engine/Classes/EdGraph/EdGraph.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
//#include "ScopedTransaction.h"

void SAutoSizeCommentsGraphNode::Construct(const FArguments& InArgs, class UEdGraphNode* InNode)
{
	GraphNode = InNode;

	CommentNode = Cast<UEdGraphNode_Comment>(InNode);

	// check if we are a header comment
	bIsHeader = GetCommentData().IsHeader();

	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	const bool bIsPresetStyle = IsPresetStyle();

	// init color
	InitializeColor(ASCSettings, bIsPresetStyle, bIsHeader);

	// use default font
	if (ASCSettings->bUseDefaultFontSize && !bIsHeader && !bIsPresetStyle)
	{
		CommentNode->FontSize = ASCSettings->DefaultFontSize;
	}

	bCachedBubbleVisibility = CommentNode->bCommentBubbleVisible;
	bCachedColorCommentBubble = CommentNode->bColorCommentBubble;

	// Set widget colors
	OpacityValue = ASCSettings->MinimumControlOpacity;
	CommentControlsTextColor = FLinearColor(1, 1, 1, OpacityValue);
	CommentControlsColor = FLinearColor(CommentNode->CommentColor.R, CommentNode->CommentColor.G, CommentNode->CommentColor.B, OpacityValue);

	// Pull out sizes
	UserSize.X = InNode->NodeWidth;
	UserSize.Y = InNode->NodeHeight;

	UpdateGraphNode();
}

SAutoSizeCommentsGraphNode::~SAutoSizeCommentsGraphNode()
{
	if (!bInitialized)
	{
		return;
	}

	if (FASCState::Get().GetASCComment(CommentNode).Get() == this)
	{
		FASCState::Get().RemoveComment(CommentNode);
	}
}

void SAutoSizeCommentsGraphNode::OnDeleted()
{
}

void SAutoSizeCommentsGraphNode::InitializeColor(const UAutoSizeCommentsSettings* ASCSettings, const bool bIsPresetStyle, const bool bIsHeaderComment)
{
	if (bIsHeaderComment)
	{
		ApplyHeaderStyle();
		return;
	}

	// don't need to initialize if our color is a preset style
	if (bIsPresetStyle)
	{
		return;
	}

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

void SAutoSizeCommentsGraphNode::InitializeCommentBubbleSettings()
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	if (ASCSettings->bEnableCommentBubbleDefaults)
	{
		CommentNode->bColorCommentBubble = ASCSettings->bDefaultColorCommentBubble;
		CommentNode->bCommentBubbleVisible_InDetailsPanel = ASCSettings->bDefaultShowBubbleWhenZoomed;
		CommentNode->bCommentBubblePinned = ASCSettings->bDefaultShowBubbleWhenZoomed;
		CommentNode->SetMakeCommentBubbleVisible(ASCSettings->bDefaultShowBubbleWhenZoomed);
		bRequireUpdate = true;
	}
}

#if ASC_UE_VERSION_OR_LATER(4, 27)
void SAutoSizeCommentsGraphNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
#else
void SAutoSizeCommentsGraphNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
#endif
{
	/** Copied from SGraphNodeComment::MoveTo */
	if (!bIsMoving)
	{
		if (GetDefault<UAutoSizeCommentsSettings>()->bRefreshContainingNodesOnMove)
		{
			RefreshNodesInsideComment(ECommentCollisionMethod::Contained);
			bIsMoving = true;
		}
	}

	FVector2D PositionDelta = NewPosition - GetPosition();

	FVector2D NewPos = GetPosition() + PositionDelta;

#if ASC_UE_VERSION_OR_LATER(4, 27)
	SGraphNode::MoveTo(NewPos, NodeFilter, bMarkDirty);
#else
	SGraphNode::MoveTo(NewPos, NodeFilter);
#endif

	FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();

	const ECommentCollisionMethod AltCollisionMethod = GetDefault<UAutoSizeCommentsSettings>()->AltCollisionMethod;
	if (KeysState.IsAltDown() && AltCollisionMethod != ECommentCollisionMethod::Disabled)
	{
		// still update collision when we alt-control drag
		TArray<UEdGraphNode*> NodesUnderComment;
		QueryNodesUnderComment(NodesUnderComment, AltCollisionMethod);
		SetNodesRelated(NodesUnderComment);
	}
	else if (IsSingleSelectedNode())
	{
		const TArray<UEdGraphNode*> NodesUnderComment = GetEdGraphNodesUnderComment(CommentNode);
		SetNodesRelated(NodesUnderComment);
	}

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
					if (!Panel->SelectionManager.IsNodeSelected(Node))
					{
						if (TSharedPtr<SGraphNode> PanelGraphNode = FASCUtils::GetGraphNode(Panel, Node))
						{
							if (!NodeFilter.Find(PanelGraphNode))
							{
								NodeFilter.Add(PanelGraphNode);
								Node->Modify();
								Node->NodePosX += PositionDelta.X;
								Node->NodePosY += PositionDelta.Y;
							}
						}
					}
				}
			}
		}
	}
}

FReply SAutoSizeCommentsGraphNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsEditable.Get())
	{
		return FReply::Unhandled();
	}

	if (MouseEvent.GetEffectingButton() == GetResizeKey() && AreResizeModifiersDown())
	{
		CachedAnchorPoint = GetAnchorPoint(MyGeometry, MouseEvent);
		if (CachedAnchorPoint != EASCAnchorPoint::None)
		{
			DragSize = UserSize;
			bUserIsDragging = true;

			// deselect all nodes when we are trying to resize
			GetOwnerPanel()->SelectionManager.ClearSelectionSet();

			// ResizeTransaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "Resize Comment Node", "Resize Comment Node")));
			// CommentNode->Modify();
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
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
	if ((MouseEvent.GetEffectingButton() == GetResizeKey()) && bUserIsDragging)
	{
		bUserIsDragging = false;
		CachedAnchorPoint = EASCAnchorPoint::None;
		RefreshNodesInsideComment(GetDefault<UAutoSizeCommentsSettings>()->ResizeCollisionMethod, GetDefault<UAutoSizeCommentsSettings>()->bIgnoreKnotNodesWhenResizing);

		ResetNodesUnrelated();

		ResizeToFit();

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
		if (CachedAnchorPoint == EASCAnchorPoint::Left || CachedAnchorPoint == EASCAnchorPoint::TopLeft || CachedAnchorPoint == EASCAnchorPoint::BottomLeft)
		{
			bAnchorLeft = true;
			NewSize.X -= MousePositionInNode.X - Padding.X;
		}

		// RIGHT
		if (CachedAnchorPoint == EASCAnchorPoint::Right || CachedAnchorPoint == EASCAnchorPoint::TopRight || CachedAnchorPoint == EASCAnchorPoint::BottomRight)
		{
			NewSize.X = MousePositionInNode.X + Padding.X;
		}

		// TOP
		if (CachedAnchorPoint == EASCAnchorPoint::Top || CachedAnchorPoint == EASCAnchorPoint::TopLeft || CachedAnchorPoint == EASCAnchorPoint::TopRight)
		{
			bAnchorTop = true;
			NewSize.Y -= MousePositionInNode.Y - Padding.Y;
		}

		// BOTTOM
		if (CachedAnchorPoint == EASCAnchorPoint::Bottom || CachedAnchorPoint == EASCAnchorPoint::BottomLeft || CachedAnchorPoint == EASCAnchorPoint::BottomRight)
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

#if ASC_UE_VERSION_OR_LATER(4, 23)
		TArray<UEdGraphNode*> Nodes;
		QueryNodesUnderComment(Nodes, GetDefault<UAutoSizeCommentsSettings>()->ResizeCollisionMethod);
		SetNodesRelated(Nodes);
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
	// Otherwise let the graph handle it, to allow spline interactions to work when they overlap with a comment node
	return FReply::Unhandled();
}

void SAutoSizeCommentsGraphNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::Tick"), STAT_ASC_Tick, STATGROUP_AutoSizeComments);

	if (!bInitialized)
	{
		// if we are not initialized we are most likely a preview node, pull size from the comment 
		UserSize.X = CommentNode->NodeWidth;
		UserSize.Y = CommentNode->NodeHeight;
		return;
	}

	if (FASCUtils::IsGraphReadOnly(GetOwnerPanel()))
	{
		return;
	}

	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	bAreControlsEnabled = FAutoSizeCommentsInputProcessor::Get().IsInputChordDown(ASCSettings->EnableCommentControlsKey);

	// We need to call this on tick since there are quite a few methods of deleting
	// nodes without any callbacks (undo, collapse to function / macro...)
	RemoveInvalidNodes();

	if (ASCSettings->ResizingMode == EASCResizingMode::Disabled)
	{
		UserSize.X = CommentNode->NodeWidth;
		UserSize.Y = CommentNode->NodeHeight;
	}

	UpdateRefreshDelay();

	if (RefreshNodesDelay == 0 && !IsHeaderComment() && !bUserIsDragging)
	{
		const FModifierKeysState& KeysState = FSlateApplication::Get().GetModifierKeys();

		const bool bIsAltDown = KeysState.IsAltDown();
		if (!bIsAltDown)
		{
			const ECommentCollisionMethod& AltCollisionMethod = GetDefault<UAutoSizeCommentsSettings>()->AltCollisionMethod;

			// still update collision when we alt-control drag
			const bool bUseAltCollision = AltCollisionMethod != ECommentCollisionMethod::Disabled;

			// refresh when the alt key is released
			if (bPreviousAltDown && bUseAltCollision)
			{
				OnAltReleased();
			}

			if (ASCSettings->ResizingMode == EASCResizingMode::Always)
			{
				ResizeToFit();
			}
			else if (ASCSettings->ResizingMode == EASCResizingMode::Reactive &&
				FAutoSizeCommentGraphHandler::Get().HasCommentChanged(CommentNode))
			{
				FAutoSizeCommentGraphHandler::Get().UpdateCommentChangeState(CommentNode);
				ResizeToFit();
			}

			MoveEmptyCommentBoxes();

			// if (ResizeTransaction.IsValid())
			// {
			// 	ResizeTransaction.Reset();
			// }
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

	// Otherwise update when cached values have changed
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
		bRequireUpdate = false;
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

	CommentStyle = ASC_STYLE_CLASS::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.CommentBlock.TitleInlineEditableText");
#if ASC_UE_VERSION_OR_LATER(5, 1)
	CommentStyle.EditableTextBoxStyle.TextStyle.Font.Size = CommentNode->FontSize;
#else
	CommentStyle.EditableTextBoxStyle.Font.Size = CommentNode->FontSize;
#endif
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
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleHeaderButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Toggle between a header node and a resizing node"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("H")))
				.Font(ASC_GET_FONT_STYLE("BoldFont"))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	const auto MakeAnchorBox = []()
	{
		return SNew(SBox).WidthOverride(16).HeightOverride(16).Visibility(EVisibility::Visible)
		[
			SNew(SBorder).BorderImage(FASCStyle::Get().GetBrush("ASC.AnchorBox"))
		];
	};

	const bool bHideCornerPoints = ASCSettings->bHideCornerPoints;

	CreateCommentControls();

	CreateColorControls();

	ETextJustify::Type CommentTextAlignment = ASCSettings->CommentTextAlignment;

	TSharedRef<SInlineEditableTextBlock> CommentTextBlock = SAssignNew(InlineEditableText, SInlineEditableTextBlock)
		.Style(&CommentStyle)
		.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentTextColor)
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
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Top).AttachWidget(MakeAnchorBox());
	}

	FMargin CommentTextPadding = ASCSettings->CommentTextPadding;
	TopHBox->AddSlot().Padding(CommentTextPadding).FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Top).AttachWidget(CommentTextBlock);

	if (!ASCSettings->bHideHeaderButton)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top).AttachWidget(ToggleHeaderButton.ToSharedRef());
	}

	if (!bHideCornerPoints)
	{
		TopHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Top).AttachWidget(MakeAnchorBox());
	}

	// Create the bottom horizontal box containing comment controls and anchor points (header comments don't need these)
	TSharedRef<SHorizontalBox> BottomHBox = SNew(SHorizontalBox);
	if (!IsHeaderComment())
	{ 
		if (!bHideCornerPoints)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AttachWidget(MakeAnchorBox());
		}

		if (!ASCSettings->bHideCommentBoxControls)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Fill).AttachWidget(CommentControls.ToSharedRef());
		}

		BottomHBox->AddSlot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(ASC_STYLE_CLASS::Get().GetBrush("NoBorder")));

		if (!bHideCornerPoints)
		{
			BottomHBox->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Bottom).AttachWidget(MakeAnchorBox());
		}
	}

	// Create the title bar
	SAssignNew(TitleBar, SBorder)
		.BorderImage(ASC_STYLE_CLASS::Get().GetBrush("Graph.Node.TitleBackground"))
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
	MainVBox->AddSlot().FillHeight(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(ASC_STYLE_CLASS::Get().GetBrush("NoBorder")));

	MainVBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).AttachWidget(BottomHBox);

	ContentScale.Bind(this, &SGraphNode::GetContentScale);
	GetOrAddSlot(ENodeZone::Center).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(ASC_STYLE_CLASS::Get().GetBrush("Kismet.Comment.Background"))
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

	if (!IsValidGraphPanel(OwnerPanel))
	{
		return;
	}

	TArray<TWeakObjectPtr<UObject>> InitialSelectedNodes;
	for (UObject* SelectedNode : OwnerPanel->SelectionManager.GetSelectedNodes())
	{
		InitialSelectedNodes.Add(SelectedNode);
	}

	// since the graph node is created twice, we need to delay initialization so the correct graph node gets initialized
	const auto InitNode = [](TWeakPtr<SAutoSizeCommentsGraphNode> NodePtr, const TArray<TWeakObjectPtr<UObject>>& SelectedNodes)
	{
		if (NodePtr.IsValid())
		{
			NodePtr.Pin()->InitializeASCNode(SelectedNodes);
		}
	};

	const auto Delegate = FTimerDelegate::CreateLambda(InitNode, SharedThis(this), InitialSelectedNodes);
	GEditor->GetTimerManager()->SetTimerForNextTick(Delegate);
}


void SAutoSizeCommentsGraphNode::InitializeASCNode(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes)
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!CommentNode || !OwnerPanel)
	{
		return;
	}

	TSharedPtr<SGraphNode> NodeWidget = OwnerPanel->GetNodeWidgetFromGuid(CommentNode->NodeGuid);
	if (NodeWidget != AsShared())
	{
		return;
	}

	// if there is already a registered comment do nothing
	if (TSharedPtr<SAutoSizeCommentsGraphNode> RegisteredComment = FASCState::Get().GetASCComment(CommentNode))
	{
		if (RegisteredComment.Get() != this)
		{
			return;
		}
	}

	if (!bInitialized)
	{
		bInitialized = true;

		// register graph
		FASCState::Get().RegisterComment(SharedThis(this));

		// init graph handler for containing graph
		FAutoSizeCommentGraphHandler::Get().BindToGraph(CommentNode->GetGraph());

		FAutoSizeCommentGraphHandler::Get().RegisterActiveGraphPanel(GetOwnerPanel());

		if (!FAutoSizeCommentGraphHandler::Get().HasCommentChanged(CommentNode))
		{
			FAutoSizeCommentGraphHandler::Get().UpdateCommentChangeState(CommentNode);
		}

		// if we failed to register the comment (since it has already been registered) do nothing
		if (FAutoSizeCommentGraphHandler::Get().RegisterComment(CommentNode))
		{
			InitializeNodesUnderComment(InitialSelectedNodes);
		}

		FASCCommentData& CommentData = GetCommentData();
		if (!CommentData.HasBeenInitialized())
		{
			CommentData.SetInitialized(true);
			InitializeCommentBubbleSettings();
		}
	}
}

void SAutoSizeCommentsGraphNode::InitializeNodesUnderComment(const TArray<TWeakObjectPtr<UObject>>& InitialSelectedNodes)
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel)
	{
		return;
	}

	if (!CommentNode)
	{
		return;
	}

	if (IsHeaderComment())
	{
		return;
	}

	// skip loading from cache if the comment node already contains any nodes (e.g. when reloading visuals)
	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		return;
	}

	LoadCache();

	FASCCommentData& CommentData = GetCommentData();
	if (CommentData.HasBeenInitialized())
	{
		return;
	}

	// check if we actually found anything from the node cache
	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		return;
	}

	// if this node is selected then we have been copy pasted, don't add all selected nodes
	if (InitialSelectedNodes.Contains(CommentNode))
	{
		RefreshNodesDelay = 2;
		return;
	}

	TArray<UObject*> SelectedNodes;
	for (TWeakObjectPtr<UObject> Node : InitialSelectedNodes)
	{
		if (Node.IsValid())
		{
			SelectedNodes.Add(Node.Get());
		}
	}

	// add all selected nodes
	if (SelectedNodes.Num() > 0 && !GetDefault<UAutoSizeCommentsSettings>()->bIgnoreSelectedNodesOnCreation)
	{
		AddAllNodesUnderComment(SelectedNodes);
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &SAutoSizeCommentsGraphNode::ResizeToFit));
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
	if (ToggleHeaderButton->IsHovered() ||
		ColorControls->IsHovered() ||
		CommentControls->IsHovered() ||
		(ResizeButton && ResizeButton->IsHovered()))
	{
		return FCursorReply::Unhandled();
	}

	if (AreResizeModifiersDown())
	{
		auto AnchorPoint = GetAnchorPoint(MyGeometry, CursorEvent);

		if (AnchorPoint == EASCAnchorPoint::TopLeft || AnchorPoint == EASCAnchorPoint::BottomRight)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
		}

		if (AnchorPoint == EASCAnchorPoint::TopRight || AnchorPoint == EASCAnchorPoint::BottomLeft)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
		}

		if (AnchorPoint == EASCAnchorPoint::Top || AnchorPoint == EASCAnchorPoint::Bottom)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
		}

		if (AnchorPoint == EASCAnchorPoint::Left || AnchorPoint == EASCAnchorPoint::Right)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		}
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
	if (!CommentNode)
	{
		return -1;
	}

	if (IsHeaderComment())
	{
		return 0;
	}

	if (IsSelectedExclusively())
	{
		return 0;
	}

	// Check if mouse is above titlebar for sort depth so comments can be dragged on first click
	const FVector2D LocalPos = GetCachedGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	if (CanBeSelected(LocalPos))
	{
		return 0;
	}

	return CommentNode->CommentDepth;
}

FReply SAutoSizeCommentsGraphNode::HandleRandomizeColorButtonClicked()
{
	RandomizeColor();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleResizeButtonClicked()
{
	ResizeToFit();
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleHeaderButtonClicked()
{
	SetIsHeader(!bIsHeader);
	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandleRefreshButtonClicked()
{
	if (AnySelectedNodes())
	{
		FASCUtils::ClearCommentNodes(CommentNode);
		AddAllSelectedNodes();
	}

	return FReply::Handled();
}

FReply SAutoSizeCommentsGraphNode::HandlePresetButtonClicked(const FPresetCommentStyle Style)
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
	FASCUtils::ClearCommentNodes(CommentNode);
	UpdateExistingCommentNodes();
	return FReply::Handled();
}

bool SAutoSizeCommentsGraphNode::AddInitialNodes()
{
	if (CommentNode->GetNodesUnderComment().Num() > 0)
	{
		return false;
	}

	return AddAllSelectedNodes();
}

bool SAutoSizeCommentsGraphNode::AddAllSelectedNodes()
{
	bool bDidAddAnything = false;

	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel)
	{
		return false;
	}

	const FGraphPanelSelectionSet& SelectedNodes = OwnerPanel->SelectionManager.GetSelectedNodes();
	for (UObject* SelectedObj : SelectedNodes)
	{
		if (CanAddNode(SelectedObj))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, SelectedObj, false);
			bDidAddAnything = true;
		}
	}

	FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(CommentNode);

	if (bDidAddAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentsGraphNode::AddAllNodesUnderComment(const TArray<UObject*>& Nodes, const bool bUpdateExistingComments)
{
	bool bDidAddAnything = false;
	for (UObject* Node : Nodes)
	{
		if (CanAddNode(Node))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, Node);
			bDidAddAnything = true;
		}
	}

	if (bDidAddAnything && bUpdateExistingComments)
	{
		UpdateExistingCommentNodes();
	}

	return bDidAddAnything;
}

bool SAutoSizeCommentsGraphNode::IsValidGraphPanel(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel)
	{
		return false;
	}

	static TArray<FString> InvalidTypes = { "SBlueprintDiff", "SGraphPreviewer" };
	if (FASCUtils::GetParentWidgetOfTypes(GraphPanel, InvalidTypes))
	{
		return false;
	}

	return true;
}

void SAutoSizeCommentsGraphNode::RemoveInvalidNodes()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::RemoveInvalidNodes"), STAT_ASC_RemoveInvalidNodes, STATGROUP_AutoSizeComments);
	const TArray<UObject*>& UnfilteredNodesUnderComment = CommentNode->GetNodesUnderComment();

	const TArray<UEdGraphNode*>& GraphNodes = GetOwnerPanel()->GetGraphObj()->Nodes;

	// Remove all invalid objects
	TSet<UObject*> InvalidObjects;
	for (UObject* Obj : UnfilteredNodesUnderComment)
	{
		// Make sure that we haven't somehow added ourselves
		check(Obj != CommentNode);

		// If a node gets deleted it can still stay inside the comment box
		// So checks if the node is still on the graph
		if (!GraphNodes.Contains(Obj))
		{
			InvalidObjects.Add(Obj);
		}
	}

	FASCUtils::RemoveNodesFromComment(CommentNode, InvalidObjects);
}

bool SAutoSizeCommentsGraphNode::RemoveAllSelectedNodes()
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();

	bool bDidRemoveAnything = false;

	const FGraphPanelSelectionSet SelectedNodes = OwnerPanel->SelectionManager.GetSelectedNodes();
	const FCommentNodeSet NodesUnderComment = CommentNode->GetNodesUnderComment();

	// Clear all nodes under comment
	FASCUtils::ClearCommentNodes(CommentNode, false);

	// Add back the nodes under comment while filtering out any which are selected
	for (UObject* NodeUnderComment : NodesUnderComment)
	{
		if (!SelectedNodes.Contains(NodeUnderComment))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, NodeUnderComment, false);
		}
		else
		{
			bDidRemoveAnything = true;
		}
	}

	FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(CommentNode);

	if (bDidRemoveAnything)
	{
		UpdateExistingCommentNodes();
	}

	return bDidRemoveAnything;
}

void SAutoSizeCommentsGraphNode::UpdateColors(const float InDeltaTime)
{
	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	const bool bIsCommentControlsKeyDown = ASCSettings->EnableCommentControlsKey.Key.IsValid() && bAreControlsEnabled;

	if (bIsCommentControlsKeyDown || (!ASCSettings->EnableCommentControlsKey.Key.IsValid() && IsHovered()))
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
	const FVector2D NodeSize = TitleBar.IsValid() ? TitleBar->GetDesiredSize() : GetDesiredSize();
	const FSlateRect TitleBarOffset(13, 8, -3, 0);

	return FSlateRect(NodePosition.X, NodePosition.Y, NodePosition.X + NodeSize.X, NodePosition.Y + NodeSize.Y) + TitleBarOffset;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentTextColor() const
{
	const FLinearColor TransparentGray(1.0f, 1.0f, 1.0f, 0.4f);
	return IsNodeUnrelated() ? TransparentGray : FLinearColor::White;
}

void SAutoSizeCommentsGraphNode::UpdateRefreshDelay()
{
	if (GetDesiredSize().IsZero())
	{
		return;
	}

	const FVector2D NodePosition = GetPosition();
	const FVector2D BottomRight = NodePosition + FVector2D(1, 1);
	if (!OwnerGraphPanelPtr.Pin().Get()->IsRectVisible(NodePosition, BottomRight))
	{
		return;
	}

	if (RefreshNodesDelay > 0)
	{
		RefreshNodesDelay -= 1;

		if (RefreshNodesDelay == 0)
		{
			RefreshNodesInsideComment(ECommentCollisionMethod::Point);

			ResizeToFit();

			if (GetDefault<UAutoSizeCommentsSettings>()->bEnableFixForSortDepthIssue)
			{
				FAutoSizeCommentGraphHandler::Get().RequestGraphVisualRefresh(GetOwnerPanel());
			}
		}
	}
}

void SAutoSizeCommentsGraphNode::RefreshNodesInsideComment(const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots, const bool bUpdateExistingComments)
{
	if (OverrideCollisionMethod == ECommentCollisionMethod::Disabled)
	{
		return;
	}

	TArray<UEdGraphNode*> OutNodes;
	QueryNodesUnderComment(OutNodes, OverrideCollisionMethod, bIgnoreKnots);
	OutNodes = OutNodes.FilterByPredicate(IsMajorNode);

	const TSet<UEdGraphNode*> NodesUnderComment(GetEdGraphNodesUnderComment(CommentNode).FilterByPredicate(IsMajorNode));
	const TSet<UEdGraphNode*> NewNodeSet(OutNodes);


	// nodes inside did not change, do nothing
	if (NodesUnderComment.Num() == NewNodeSet.Num() && NodesUnderComment.Includes(NewNodeSet))
	{
		return;
	}

	FASCUtils::ClearCommentNodes(CommentNode, false);
	for (UEdGraphNode* Node : OutNodes)
	{
		if (CanAddNode(Node, bIgnoreKnots))
		{
			FASCUtils::AddNodeIntoComment(CommentNode, Node, false);
		}
	}

	if (bUpdateExistingComments)
	{
		UpdateExistingCommentNodes();
	}

	FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(CommentNode);
}

float SAutoSizeCommentsGraphNode::GetTitleBarHeight() const
{
	return TitleBar.IsValid() ? TitleBar->GetDesiredSize().Y : 0.0f;
}

void SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes()
{
	UpdateExistingCommentNodes(nullptr, nullptr);
}

void SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes(const TArray<UEdGraphNode_Comment*>* OldParentComments, const TArray<UObject*>* OldCommentContains)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::UpdateExistingCommentNodes"), STAT_ASC_UpdateExistingCommentNodes, STATGROUP_AutoSizeComments);

	// Get list of all other comment nodes
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> OtherCommentNodes = GetOtherCommentNodes();

	TArray<UObject*> OurMainNodes = CommentNode->GetNodesUnderComment().FilterByPredicate(IsMajorNode);

	TArray<UEdGraphNode_Comment*> CurrentParentComments = GetParentComments();

	// Remove ourselves from our parent comments, as we will be adding ourselves later if required
	for (UEdGraphNode_Comment* ParentComment : CurrentParentComments)
	{
		TSet<UObject*> NodesToRemove = { CommentNode };
		FASCUtils::RemoveNodesFromComment(ParentComment, NodesToRemove);
	}

	// Do nothing if we have no nodes under ourselves
	if (CommentNode->GetNodesUnderComment().Num() == 0)
	{
		return;
	}

	bool bNeedsPurging = false;
	for (TSharedPtr<SAutoSizeCommentsGraphNode> OtherCommentNode : OtherCommentNodes)
	{
		UEdGraphNode_Comment* OtherComment = OtherCommentNode->GetCommentNodeObj();

		if (OtherComment == CommentNode)
		{
			continue;
		}

		const auto OtherMainNodes = OtherComment->GetNodesUnderComment().FilterByPredicate(IsMajorNode);

		if (OtherMainNodes.Num() == 0)
		{
			continue;
		}

		// check if all nodes in the other comment box are within our comment box AND we are not inside the other comment already
		const bool bAllNodesContainedUnderSelf = !OtherMainNodes.ContainsByPredicate([&OurMainNodes](UObject* NodeUnderOther)
		{
			return !OurMainNodes.Contains(NodeUnderOther);
		});

		bool bDontAddSameSet;

		// Check if we should add the comment if the same main node set
		if (OldParentComments && OldCommentContains)
		{
			const bool bPreviouslyWasParent = OldParentComments->Contains(OtherComment);
			const bool bWeAreFreshNode = OldParentComments->Num() == 0 && OldCommentContains->Num() == 0;
			bDontAddSameSet = OurMainNodes.Num() == OtherMainNodes.Num() && (bPreviouslyWasParent || bWeAreFreshNode);
		}
		else
		{
			const bool bPreviouslyWasParent = CurrentParentComments.Contains(OtherComment);
			bDontAddSameSet = OurMainNodes.Num() == OtherMainNodes.Num() && bPreviouslyWasParent;
		}

		// we contain all of the other comment, add the other comment (unless we already contain it)
		if (bAllNodesContainedUnderSelf && !bDontAddSameSet)
		{
			// add the other comment into ourself
			FASCUtils::AddNodeIntoComment(CommentNode, OtherComment);
			bNeedsPurging = true;
		}
		else
		{
			// check if all nodes in the other comment box are within our comment box
			const bool bAllNodesContainedUnderOther = !OurMainNodes.ContainsByPredicate([&OtherMainNodes](UObject* NodeUnderSelf)
			{
				return !OtherMainNodes.Contains(NodeUnderSelf);
			});

			// other comment contains all of our nodes, add ourself into the other comment
			if (bAllNodesContainedUnderOther) 
			{
				// add the ourselves into the other comment
				FASCUtils::AddNodeIntoComment(OtherComment, CommentNode);
				bNeedsPurging = true;
			}
		}
	}

	if (bNeedsPurging && GetDefault<UAutoSizeCommentsSettings>()->bEnableFixForSortDepthIssue)
	{
		FAutoSizeCommentGraphHandler::Get().RequestGraphVisualRefresh(GetOwnerPanel());
	}
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentBodyColor() const
{
	if (!CommentNode)
	{
		return FLinearColor::White;
	}

	return IsNodeUnrelated()
		? CommentNode->CommentColor * FLinearColor(0.5f, 0.5f, 0.5f, 0.4f)
		: CommentNode->CommentColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentTitleBarColor() const
{
	if (CommentNode)
	{
		const FLinearColor Color = CommentNode->CommentColor * 0.6f;
		return FLinearColor(Color.R, Color.G, Color.B, IsNodeUnrelated() ? 0.4f : 1.0f);
	}

	const FLinearColor Color = FLinearColor::White * 0.6f;
	return FLinearColor(Color.R, Color.G, Color.B);
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
	const FLinearColor Color = FLinearColor::White * 0.6f;
	return FLinearColor(Color.R, Color.G, Color.B);
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentControlsColor() const
{
	return CommentControlsColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetCommentControlsTextColor() const
{
	return CommentControlsTextColor;
}

FSlateColor SAutoSizeCommentsGraphNode::GetPresetColor(const FLinearColor Color) const
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

FASCCommentData& SAutoSizeCommentsGraphNode::GetCommentData() const
{
	return FAutoSizeCommentsCacheFile::Get().GetCommentData(GraphNode);
}

void SAutoSizeCommentsGraphNode::ResizeToFit()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::ResizeToFit"), STAT_ASC_ResizeToFit, STATGROUP_AutoSizeComments);

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
		{
			GetNodeObj()->ResizeNode(UserSize);
		}
	}
}

void SAutoSizeCommentsGraphNode::ApplyHeaderStyle()
{
	FPresetCommentStyle Style = GetDefault<UAutoSizeCommentsSettings>()->HeaderStyle;
	CommentNode->CommentColor = Style.Color;
	CommentNode->FontSize = Style.FontSize;
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
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleRefreshButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Replace with selected nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("R")))
				.Font(ASC_GET_FONT_STYLE("BoldFont"))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
			]
		];

	// Create the add button
	TSharedRef<SButton> AddButton = SNew(SButton)
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleAddButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
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
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleSubtractButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
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
		.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
		.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
		.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleClearButtonClicked)
		.ContentPadding(FMargin(2, 2))
		.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
		.ToolTipText(FText::FromString("Clear all nodes"))
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString("C")))
				.Font(ASC_GET_FONT_STYLE("BoldFont"))
				.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
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

	const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

	const TArray<FPresetCommentStyle>& Presets = ASCSettings->PresetStyles;
	CachedNumPresets = Presets.Num();

	if (!IsHeaderComment()) // header comments don't need color presets
	{
		if (!ASCSettings->bHideResizeButton && ASCSettings->ResizingMode != EASCResizingMode::Always)
		{
			// Create the resize button
			ResizeButton = SNew(SButton)
				.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
				.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
				.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleResizeButtonClicked)
				.ContentPadding(FMargin(2, 2))
				.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
				.ToolTipText(FText::FromString("Resize to containing nodes"))
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(12).HeightOverride(12)
					[
						SNew(SImage)
						.ColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsTextColor)
						.Image(ASC_STYLE_CLASS::Get().GetBrush("Icons.Refresh"))
					]
				];

			ColorControls->AddSlot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f).AttachWidget(ResizeButton.ToSharedRef());
		}

		ColorControls->AddSlot().FillWidth(1).HAlign(HAlign_Fill).VAlign(VAlign_Fill).AttachWidget(SNew(SBorder).BorderImage(ASC_STYLE_CLASS::Get().GetBrush("NoBorder")));

		auto Buttons = SNew(SHorizontalBox);
		ColorControls->AddSlot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Fill).AttachWidget(Buttons);

		if (!ASCSettings->bHidePresets)
		{
			for (const FPresetCommentStyle& Preset : Presets)
			{
				FLinearColor ColorWithoutOpacity = Preset.Color;
				ColorWithoutOpacity.A = 1;

				TSharedRef<SButton> Button = SNew(SButton)
					.ButtonStyle(ASC_STYLE_CLASS::Get(), "RoundButton")
					.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetPresetColor, ColorWithoutOpacity)
					.OnClicked(this, &SAutoSizeCommentsGraphNode::HandlePresetButtonClicked, Preset)
					.ContentPadding(FMargin(2, 2))
					.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
					.ToolTipText(FText::FromString("Set preset color"))
					[
						SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
					];

				Buttons->AddSlot().AttachWidget(Button);
			}
		}

		if (!ASCSettings->bHideRandomizeButton)
		{
			// Create the random color button
			TSharedRef<SButton> RandomColorButton = SNew(SButton)
				.ButtonStyle(ASC_STYLE_CLASS::Get(), "NoBorder")
				.ButtonColorAndOpacity(this, &SAutoSizeCommentsGraphNode::GetCommentControlsColor)
				.OnClicked(this, &SAutoSizeCommentsGraphNode::HandleRandomizeColorButtonClicked)
				.ContentPadding(FMargin(2, 2))
				.IsEnabled(this, &SAutoSizeCommentsGraphNode::AreControlsEnabled)
				.ToolTipText(FText::FromString("Randomize the color of the comment box"))
				[
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).WidthOverride(16).HeightOverride(16)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString("?")))
						.Font(ASC_GET_FONT_STYLE("BoldFont"))
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
				{
					OtherCommentNodes.Add(ASCGraphNode);
				}
			}
		}
	}

	return OtherCommentNodes;
}

TArray<UEdGraphNode_Comment*> SAutoSizeCommentsGraphNode::GetParentComments() const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAutoSizeCommentsGraphNode::GetParentComments"), STAT_ASC_GetParentComments, STATGROUP_AutoSizeComments);
	TArray<UEdGraphNode_Comment*> ParentComments;

	for (UEdGraphNode* OtherNode : CommentNode->GetGraph()->Nodes)
	{
		if (auto OtherComment = Cast<UEdGraphNode_Comment>(OtherNode))
		{
			if (OtherComment != CommentNode && OtherComment->GetNodesUnderComment().Contains(CommentNode))
			{
				ParentComments.Add(OtherComment);
			}
		}
	}

	return ParentComments;
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

void SAutoSizeCommentsGraphNode::SnapBoundsToGrid(FSlateRect& Bounds, int GridMultiplier)
{
	const float SnapSize = SNodePanel::GetSnapGridSize() * GridMultiplier;
	Bounds.Left = SnapSize * FMath::FloorToInt(Bounds.Left / SnapSize);
	Bounds.Right = SnapSize * FMath::CeilToInt(Bounds.Right / SnapSize);
	Bounds.Top = SnapSize * FMath::FloorToInt(Bounds.Top / SnapSize);
	Bounds.Bottom = SnapSize * FMath::CeilToInt(Bounds.Bottom / SnapSize);
}

bool SAutoSizeCommentsGraphNode::IsLocalPositionInCorner(const FVector2D& MousePositionInNode) const
{
	FVector2D CornerBounds = GetDesiredSize() - FVector2D(40, 40);
	return MousePositionInNode.Y >= CornerBounds.Y && MousePositionInNode.X >= CornerBounds.X;
}

EASCAnchorPoint SAutoSizeCommentsGraphNode::GetAnchorPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	const FVector2D MousePositionInNode = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	const FVector2D Size = GetDesiredSize();

	// check the mouse position is actually in the node
	if (MousePositionInNode.X < 0 || MousePositionInNode.X > Size.X ||
		MousePositionInNode.Y < 0 || MousePositionInNode.Y > Size.Y)
	{
		return EASCAnchorPoint::None;
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
			return EASCAnchorPoint::BottomRight;
		}
		if (MousePositionInNode.X < Left && MousePositionInNode.Y < Top)
		{
			return EASCAnchorPoint::TopLeft;
		}
		if (MousePositionInNode.X < Left && MousePositionInNode.Y > Bottom)
		{
			return EASCAnchorPoint::BottomLeft;
		}
		if (MousePositionInNode.X > Right && MousePositionInNode.Y < Top)
		{
			return EASCAnchorPoint::TopRight;
		}
		if (MousePositionInNode.Y < SidePadding)
		{
			return EASCAnchorPoint::Top;
		}
		if (MousePositionInNode.Y > Size.Y - SidePadding)
		{
			return EASCAnchorPoint::Bottom;
		}
	}
	if (MousePositionInNode.X < SidePadding)
	{
		return EASCAnchorPoint::Left;
	}
	if (MousePositionInNode.X > Size.X - SidePadding)
	{
		return EASCAnchorPoint::Right;
	}

	return EASCAnchorPoint::None;
}

void SAutoSizeCommentsGraphNode::SetIsHeader(bool bNewValue)
{
	bIsHeader = bNewValue;

	// update the comment data
	FASCCommentData& CommentData = GetCommentData();
	CommentData.SetHeader(bNewValue);

	if (bIsHeader) // apply header style
	{
		ApplyHeaderStyle();
		FASCUtils::ClearCommentNodes(CommentNode);
	}
	else  // undo header style
	{
		const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();
		if (ASCSettings->bUseRandomColor)
		{
			RandomizeColor();
		}
		else
		{
			CommentNode->CommentColor = ASCSettings->DefaultCommentColor;
		}

		CommentNode->FontSize = ASCSettings->DefaultFontSize;
		AdjustMinSize(UserSize);
	}

	UpdateGraphNode();
}

bool SAutoSizeCommentsGraphNode::IsHeaderComment() const
{
	return bIsHeader;
}

bool SAutoSizeCommentsGraphNode::IsHeaderComment(UEdGraphNode_Comment* OtherComment)
{
	return FAutoSizeCommentsCacheFile::Get().GetCommentData(OtherComment).IsHeader();
}

FKey SAutoSizeCommentsGraphNode::GetResizeKey() const
{
	const FInputChord ResizeChord = GetDefault<UAutoSizeCommentsSettings>()->ResizeChord;
	return ResizeChord.Key;
}

bool SAutoSizeCommentsGraphNode::AreResizeModifiersDown() const
{
	const FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
	const FInputChord ResizeChord = GetDefault<UAutoSizeCommentsSettings>()->ResizeChord;
	return KeysState.AreModifersDown(EModifierKey::FromBools(ResizeChord.bCtrl, ResizeChord.bAlt, ResizeChord.bShift, ResizeChord.bCmd));
}

bool SAutoSizeCommentsGraphNode::IsSingleSelectedNode() const
{
	TSharedPtr<SGraphPanel> OwnerPanel = OwnerGraphPanelPtr.Pin();
	return OwnerPanel->SelectionManager.GetSelectedNodes().Num() == 1 && OwnerPanel->SelectionManager.IsNodeSelected(GraphNode); 
}

bool SAutoSizeCommentsGraphNode::IsNodeUnrelated() const
{
#if ASC_UE_VERSION_OR_LATER(4, 23)
	return CommentNode->IsNodeUnrelated();
#else
	return false;
#endif
}

void SAutoSizeCommentsGraphNode::SetNodesRelated(const TArray<UEdGraphNode*>& Nodes, bool bIncludeSelf)
{
#if ASC_UE_VERSION_OR_LATER(4, 23)
	const TArray<UEdGraphNode*>& AllNodes = GetNodeObj()->GetGraph()->Nodes;
	for (UEdGraphNode* Node : AllNodes)
	{
		Node->SetNodeUnrelated(true);
	}

	if (bIncludeSelf)
	{
		GetCommentNodeObj()->SetNodeUnrelated(false);
	}

	for (UEdGraphNode* Node : Nodes)
	{
		Node->SetNodeUnrelated(false);
	}
#endif
}

void SAutoSizeCommentsGraphNode::ResetNodesUnrelated()
{
#if ASC_UE_VERSION_OR_LATER(4, 23)
	if (UEdGraph* Graph = GetNodeObj()->GetGraph())
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			Node->SetNodeUnrelated(false);
		}
	}
#endif
}

bool SAutoSizeCommentsGraphNode::AreControlsEnabled() const
{
	return !GetDefault<UAutoSizeCommentsSettings>()->EnableCommentControlsKey.Key.IsValid() || bAreControlsEnabled;
}

bool SAutoSizeCommentsGraphNode::IsPresetStyle()
{
	for (FPresetCommentStyle Style : GetMutableDefault<UAutoSizeCommentsSettings>()->PresetStyles)
	{
		if (CommentNode->CommentColor == Style.Color && CommentNode->FontSize == Style.FontSize)
		{
			return true;
		}
	}

	return false;
}

bool SAutoSizeCommentsGraphNode::LoadCache()
{
	TArray<UEdGraphNode*> OutNodesUnder;
	if (FAutoSizeCommentsCacheFile::Get().GetNodesUnderComment(SharedThis(this), OutNodesUnder))
	{
		for (UEdGraphNode* Node : OutNodesUnder)
		{
			if (!HasNodeBeenDeleted(Node))
			{
				FASCUtils::AddNodeIntoComment(CommentNode, Node, false);
			}
		}

		return true;
	}

	return false;
}

void SAutoSizeCommentsGraphNode::UpdateCache()
{
	FAutoSizeCommentsCacheFile::Get().UpdateCommentState(GetCommentNodeObj());
}

void SAutoSizeCommentsGraphNode::QueryNodesUnderComment(TArray<UEdGraphNode*>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots)
{
	TArray<TSharedPtr<SGraphNode>> OutGraphNodes;
	QueryNodesUnderComment(OutGraphNodes, OverrideCollisionMethod, bIgnoreKnots);
	for (TSharedPtr<SGraphNode>& Node : OutGraphNodes)
	{
		OutNodesUnderComment.Add(Node->GetNodeObj());
	}
}

void SAutoSizeCommentsGraphNode::QueryNodesUnderComment(TArray<TSharedPtr<SGraphNode>>& OutNodesUnderComment, const ECommentCollisionMethod OverrideCollisionMethod, const bool bIgnoreKnots)
{
	if (OverrideCollisionMethod == ECommentCollisionMethod::Disabled)
	{
		return;
	}

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
		ECommentCollisionMethod CollisionMethod = OverrideCollisionMethod;

		switch (CollisionMethod)
		{
			case ECommentCollisionMethod::Point:
				bIsOverlapping = CommentRect.ContainsPoint(SomeNodePosition);
				break;
			case ECommentCollisionMethod::Intersect:
				CommentRect.IntersectionWith(NodeGeometryGraphSpace, bIsOverlapping);
				break;
			case ECommentCollisionMethod::Contained:
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

bool SAutoSizeCommentsGraphNode::CanAddNode(const TSharedPtr<SGraphNode> OtherGraphNode, const bool bIgnoreKnots) const
{
	UObject* GraphObject = OtherGraphNode->GetObjectBeingDisplayed();
	if (GraphObject == nullptr || GraphObject == CommentNode)
	{
		return false;
	}

	if ((bIgnoreKnots || GetDefault<UAutoSizeCommentsSettings>()->bIgnoreKnotNodes) && Cast<UK2Node_Knot>(GraphObject) != nullptr)
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

bool SAutoSizeCommentsGraphNode::CanAddNode(const UObject* Node, const bool bIgnoreKnots) const
{
	TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel();
	if (!OwnerPanel.IsValid())
	{
		return false;
	}

	const UEdGraphNode* EdGraphNode = Cast<UEdGraphNode>(Node);
	if (!EdGraphNode)
	{
		return false;
	}

	TSharedPtr<SGraphNode> NodeAsGraphNode = OwnerPanel->GetNodeWidgetFromGuid(EdGraphNode->NodeGuid);
	if (!NodeAsGraphNode.IsValid())
	{
		return false;
	}

	return CanAddNode(NodeAsGraphNode, bIgnoreKnots);
}

void SAutoSizeCommentsGraphNode::OnAltReleased()
{
	FAutoSizeCommentGraphHandler::Get().ProcessAltReleased(GetOwnerPanel());
}

bool SAutoSizeCommentsGraphNode::IsCommentNode(UObject* Object)
{
	return Object->IsA(UEdGraphNode_Comment::StaticClass());
}

FSlateRect SAutoSizeCommentsGraphNode::GetNodeBounds(UEdGraphNode* Node)
{
	if (!Node)
	{
		return FSlateRect();
	}

	FVector2D Pos(Node->NodePosX, Node->NodePosY);

	FVector2D Size(300, 150);

	auto LocalGraphNode = FASCUtils::GetGraphNode(GetOwnerPanel(), Node);
	if (LocalGraphNode.IsValid())
	{
		Pos = LocalGraphNode->GetPosition();
		Size = LocalGraphNode->GetDesiredSize();

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
		{
			// if the node contains us and is higher depth, do not resize
			if (OtherCommentNode->GetNodesUnderComment().Contains(GraphNode) &&
				OtherCommentNode->CommentDepth > CommentNode->CommentDepth)
			{
				continue;
			}
		}

		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			Nodes.Add(Node);
		}
	}

	bool bBoundsInit = false;
	FSlateRect Bounds;
	for (int i = 0; i < Nodes.Num(); i++)
	{
		UEdGraphNode* Node = Nodes[i];

		if (!Node)
		{
			continue;
		}

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

bool SAutoSizeCommentsGraphNode::IsMajorNode(UObject* Object)
{
	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Object))
	{
		if (IsHeaderComment(CommentNode))
		{
			return true;
		}
	}
	else if (Object->IsA(UEdGraphNode::StaticClass()))
	{
		return true;
	}

	return false;
}