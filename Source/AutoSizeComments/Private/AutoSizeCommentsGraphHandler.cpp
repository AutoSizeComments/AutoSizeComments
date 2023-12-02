// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphHandler.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditAction.h"
#include "K2Node_Knot.h"
#include "SGraphPanel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/LazySingleton.h"
#include "Widgets/Notifications/SNotificationList.h"

#if ASC_UE_VERSION_OR_LATER(5, 0)
#include "UObject/ObjectSaveContext.h"
#endif

#if ASC_UE_VERSION_OR_LATER(5, 1)
#include "Misc/TransactionObjectEvent.h"
#endif


struct FASCZoomLevel
{
public:
	FASCZoomLevel(float InZoomAmount, const FText& InDisplayText, EGraphRenderingLOD::Type InLOD)
		: DisplayText(FText::Format(INVTEXT("ASC Zoom {0}"), InDisplayText))
	 , ZoomAmount(InZoomAmount)
	 , LOD(InLOD)
	{
	}

public:
	FText DisplayText;
	float ZoomAmount;
	EGraphRenderingLOD::Type LOD;
};

struct FASCZoomLevelsContainer final : FZoomLevelsContainer
{
	FASCZoomLevelsContainer()
	{
		ZoomLevels.Reserve(20);
		ZoomLevels.Add(FASCZoomLevel(0.100f, FText::FromString(TEXT("-12")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.125f, FText::FromString(TEXT("-11")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.150f, FText::FromString(TEXT("-10")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.175f, FText::FromString(TEXT("-9")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.200f, FText::FromString(TEXT("-8")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.225f, FText::FromString(TEXT("-7")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.250f, FText::FromString(TEXT("-6")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.375f, FText::FromString(TEXT("-5")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.500f, FText::FromString(TEXT("-4")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.675f, FText::FromString(TEXT("-3")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.750f, FText::FromString(TEXT("-2")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(0.875f, FText::FromString(TEXT("-1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.000f, FText::FromString(TEXT("1:1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.250f, FText::FromString(TEXT("+1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.375f, FText::FromString(TEXT("+2")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.500f, FText::FromString(TEXT("+3")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.675f, FText::FromString(TEXT("+4")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.750f, FText::FromString(TEXT("+5")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(1.875f, FText::FromString(TEXT("+6")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FASCZoomLevel(2.000f, FText::FromString(TEXT("+7")), EGraphRenderingLOD::DefaultDetail));
	}

	virtual float GetZoomAmount(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].ZoomAmount;
	}

	virtual int32 GetNearestZoomLevel(float InZoomAmount) const override
	{
		for (int32 ZoomLevelIndex=0; ZoomLevelIndex < GetNumZoomLevels(); ++ZoomLevelIndex)
		{
			if (InZoomAmount <= GetZoomAmount(ZoomLevelIndex))
			{
				return ZoomLevelIndex;
			}
		}

		return GetDefaultZoomLevel();
	}

	virtual FText GetZoomText(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].DisplayText;
	}

	virtual int32 GetNumZoomLevels() const override
	{
		return ZoomLevels.Num();
	}

	virtual int32 GetDefaultZoomLevel() const override
	{
		return 12;
	}

	virtual EGraphRenderingLOD::Type GetLOD(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].LOD;
	}

	TArray<FASCZoomLevel> ZoomLevels;
};


FAutoSizeCommentGraphHandler& FAutoSizeCommentGraphHandler::Get()
{
	return TLazySingleton<FAutoSizeCommentGraphHandler>::Get();
}

void FAutoSizeCommentGraphHandler::TearDown()
{
	TLazySingleton<FAutoSizeCommentGraphHandler>::TearDown();
}

void FAutoSizeCommentGraphHandler::BindDelegates()
{
	bPendingSave = false;
#if ASC_UE_VERSION_OR_LATER(5, 0)
	FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectPreSave);
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::Tick));
#else
	FCoreUObjectDelegates::OnObjectSaved.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectSaved);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::Tick));
#endif

	FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectTransacted);
}

void FAutoSizeCommentGraphHandler::UnbindDelegates()
{
#if ASC_UE_VERSION_OR_LATER(5, 0)
	FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);
#else
	FCoreUObjectDelegates::OnObjectSaved.RemoveAll(this);
#endif
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

	for (const auto& Kvp : GraphDatas)
	{
		if (Kvp.Key.IsValid())
		{
			Kvp.Key->RemoveOnGraphChangedHandler(Kvp.Value.OnGraphChangedHandle);
		}
	}

	GraphDatas.Empty();

#if ASC_UE_VERSION_OR_LATER(5, 0)
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
#else
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
#endif
}

void FAutoSizeCommentGraphHandler::BindToGraph(UEdGraph* Graph)
{
	if (!Graph || GraphDatas.Contains(Graph))
	{
		return;
	}

	GraphDatas.Remove(nullptr);

	FASCGraphHandlerData GraphData;
	GraphData.OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::OnGraphChanged));

	GraphDatas.Add(Graph, GraphData);

	CheckCacheDataError(Graph);
}

void FAutoSizeCommentGraphHandler::OnGraphChanged(const FEdGraphEditAction& Action)
{
	if ((Action.Action & GRAPHACTION_AddNode) != 0 && Action.bUserInvoked)
	{
		// only handle single node added 
		if (Action.Nodes.Num() == 1)
		{
			UEdGraphNode* NewNode = const_cast<UEdGraphNode*>(Action.Nodes.Array()[0]);

			// delay 1 tick as some nodes do not have their pins setup correctly on creation
			GEditor->GetTimerManager()->SetTimerForNextTick(
				FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::OnNodeAdded, TWeakObjectPtr<UEdGraphNode>(NewNode)));
		}
	}
	else if ((Action.Action & GRAPHACTION_RemoveNode) != 0)
	{
		OnNodeDeleted(Action);
	}
}

void FAutoSizeCommentGraphHandler::AutoInsertIntoCommentNodes(TWeakObjectPtr<UEdGraphNode> NewNode, TWeakObjectPtr<UEdGraphNode> LastSelectedNode)
{
	EASCAutoInsertComment AutoInsertStyle = UAutoSizeCommentsSettings::Get().AutoInsertComment;
	if (AutoInsertStyle == EASCAutoInsertComment::Never)
	{
		return;
	}

	if (!NewNode.IsValid() || !IsValid(NewNode.Get()))
	{
		return;
	}

	if (!LastSelectedNode.IsValid() || !IsValid(LastSelectedNode.Get()))
	{
		return;
	}

	UEdGraph* Graph = NewNode->GetGraph();
	const auto IsSelectedNode = [&LastSelectedNode](UEdGraphNode* LinkedNode) { return LinkedNode == LastSelectedNode; };

	TArray<UEdGraphNode*> LinkedInput = FASCUtils::GetLinkedNodes(NewNode.Get(), EGPD_Input);
	TArray<UEdGraphNode*> LinkedOutput = FASCUtils::GetLinkedNodes(NewNode.Get(), EGPD_Output);

	UEdGraphNode** SelectedInput = LinkedInput.FindByPredicate(IsSelectedNode);
	UEdGraphNode** SelectedOutput = LinkedOutput.FindByPredicate(IsSelectedNode);

	struct FLocal
	{
		static void TakeCommentNode(UEdGraph* Graph, UEdGraphNode* Node, UEdGraphNode* NodeToTakeFrom)
		{
			if (!Graph || !Node || !NodeToTakeFrom)
				return;

			TArray<UEdGraphNode_Comment*> CommentNodes;
			Graph->GetNodesOfClass(CommentNodes);
			auto ContainingComments = FASCUtils::GetContainingCommentNodes(CommentNodes, NodeToTakeFrom);
			for (UEdGraphNode_Comment* CommentNode : ContainingComments)
			{
				CommentNode->AddNodeUnderComment(Node);
			}
		};
	};

	// always include parameter nodes (no exec pins)
	const auto IsExecPin = [](const UEdGraphPin* Pin){ return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec; };
	if (!NewNode->Pins.ContainsByPredicate(IsExecPin))
	{
		AutoInsertStyle = EASCAutoInsertComment::Always;
	}

	if (AutoInsertStyle == EASCAutoInsertComment::Surrounded)
	{
		UEdGraphNode* NodeA = nullptr;
		UEdGraphNode* NodeB = nullptr;
		if (SelectedInput)
		{
			NodeA = *SelectedInput;
			NodeB = LinkedOutput.Num() > 0 ? LinkedOutput[0] : nullptr;
		}
		else if (SelectedOutput)
		{
			NodeA = *SelectedOutput;
			NodeB = LinkedInput.Num() > 0 ? LinkedInput[0] : nullptr;
		}

		if (NodeA == nullptr || NodeB == nullptr)
		{
			return;
		}

		TArray<UEdGraphNode_Comment*> CommentNodes;
		Graph->GetNodesOfClass(CommentNodes);
		auto ContainingCommentsA = FASCUtils::GetContainingCommentNodes(CommentNodes, NodeA);
		auto ContainingCommentsB = FASCUtils::GetContainingCommentNodes(CommentNodes, NodeB);

		ContainingCommentsA.RemoveAll([&ContainingCommentsB](UEdGraphNode_Comment* Comment)
		{
			return !ContainingCommentsB.Contains(Comment);
		});

		if (ContainingCommentsA.Num() > 0)
		{
			FLocal::TakeCommentNode(Graph, NewNode.Get(), NodeA);
		}
	}
	else if (AutoInsertStyle == EASCAutoInsertComment::Always)
	{
		if (SelectedInput)
		{
			FLocal::TakeCommentNode(Graph, NewNode.Get(), *SelectedInput);
		}
		else if (SelectedOutput)
		{
			FLocal::TakeCommentNode(Graph, NewNode.Get(), *SelectedOutput);
		}
	}
}

void FAutoSizeCommentGraphHandler::RegisterActiveGraphPanel(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (GraphPanel.IsValid() && !ActiveGraphPanels.Contains(GraphPanel))
	{
		if (UAutoSizeCommentsSettings::Get().bUseMaxDetailNodes)
		{
			// TODO: Find fallback for <5.0 if possible
#if ASC_UE_VERSION_OR_LATER(5, 0)
			// init the graph panel
			GraphPanel->SetZoomLevelsContainer<FASCZoomLevelsContainer>();
#endif
		}

		ActiveGraphPanels.Add(GraphPanel);
	}
}

void FAutoSizeCommentGraphHandler::RequestGraphVisualRefresh(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (bPendingGraphVisualRequest)
	{
		return;
	}

	bPendingGraphVisualRequest = true;

	const auto Delegate = FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::RefreshGraphVisualRefresh, TWeakPtr<SGraphPanel>(GraphPanel));
	GEditor->GetTimerManager()->SetTimerForNextTick(Delegate);
}

void FAutoSizeCommentGraphHandler::RefreshGraphVisualRefresh(TWeakPtr<SGraphPanel> GraphPanel)
{
	bPendingGraphVisualRequest = false;

	if (!GraphPanel.IsValid())
	{
		return;
	}

	GraphPanel.Pin()->PurgeVisualRepresentation();

	const auto UpdateGraphPanel = [](TWeakPtr<SGraphPanel> LocalPanel)
	{
		if (LocalPanel.IsValid())
		{
			LocalPanel.Pin()->Update();
		}
	};

	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda(UpdateGraphPanel, GraphPanel));
}

EASCResizingMode FAutoSizeCommentGraphHandler::GetResizingMode(UEdGraph* Graph) const
{
	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	if (Graph)
	{
		if (UClass* Class = Graph->GetClass())
		{
			if (const FASCGraphSettings* GraphSettings = ASCSettings.GraphSettingsOverride.Find(Class->GetFName()))
			{
				return GraphSettings->ResizingMode;
			}
		}
	}

	return ASCSettings.ResizingMode;
}

void FAutoSizeCommentGraphHandler::CheckCacheDataError(UEdGraph* Graph)
{
	FASCGraphData& GraphData = FAutoSizeCommentsCacheFile::Get().GetGraphData(Graph);
	TArray<UEdGraphNode_Comment*> Comments = FASCUtils::GetCommentsFromGraph(Graph);

	// skip if we have no comment data for this graph
	if (GraphData.CommentData.Num() == 0)
	{
		return;
	}

	TSet<FGuid> OldGuids;
	GraphData.CommentData.GetKeys(OldGuids);

	TSet<FGuid> CurrentGuids;
	for (UEdGraphNode_Comment* Comment : Comments)
	{
		CurrentGuids.Add(Comment->NodeGuid);
	}

	const int MissingGuids = OldGuids.Difference(CurrentGuids).Num();

	// this is to check if the nodes are having their guid's refreshed somehow
	// so if we have the same number of nodes and they are all wrong, print an error msg
	if (OldGuids.Num() == CurrentGuids.Num() && MissingGuids == OldGuids.Num())
	{
		UE_LOG(LogAutoSizeComments, Error, TEXT(
			"Graph '%s' has failed to load all comments\n"
			"Please report issue this at: https://github.com/AutoSizeComments/AutoSizeComments/issues/13\n"
			"Please describe any special details of your project (for example: did you just upgrade your project to a new engine version?"),
			*Graph->GetName());

		FNotificationInfo Info(INVTEXT("ASC: Graph has failed to load comments, see output log for more details"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		GraphData.CommentData.Empty();
	}
}

EGraphRenderingLOD::Type FAutoSizeCommentGraphHandler::GetGraphLOD(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel.IsValid())
	{
		return EGraphRenderingLOD::Type::DefaultDetail;
	}

	// if we use MaxDetailNodes, we are setting the LOD to DefaultDetail all the time
	// we need to calculate the LOD without the setting on to fix things such as comment bubbles
	if (!UAutoSizeCommentsSettings::Get().bUseMaxDetailNodes)
	{
		return GraphPanel->GetCurrentLOD();
	}

	UEdGraph* Graph = GraphPanel->GetGraphObj();
	if (!Graph)
	{
		return EGraphRenderingLOD::Type::DefaultDetail;
	}

	FASCGraphHandlerData& Data = GetGraphHandlerData(Graph);

	const float ZoomAmount = GraphPanel->GetZoomAmount();

	if (ZoomAmount != Data.LastZoomLevel)
	{
		Data.LastZoomLevel = ZoomAmount;

		if (ZoomAmount <= 0.200f)
		{
			Data.LastLOD = EGraphRenderingLOD::LowestDetail;
		}
		else if (ZoomAmount <= 0.250f)
		{
			Data.LastLOD = EGraphRenderingLOD::LowDetail;
		}
		else if (ZoomAmount <= 0.675f)
		{
			Data.LastLOD = EGraphRenderingLOD::MediumDetail;
		}
		else if (ZoomAmount <= 1.375f)
		{
			Data.LastLOD = EGraphRenderingLOD::DefaultDetail;
		}
		else
		{
			Data.LastLOD = EGraphRenderingLOD::FullyZoomedIn;
		}
	}

	return Data.LastLOD;
}

void FAutoSizeCommentGraphHandler::ProcessAltReleased(TSharedPtr<SGraphPanel> GraphPanel)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAutoSizeCommentGraphHandler::ProcessAltReleased"), STAT_ASC_ProcessAltReleased, STATGROUP_AutoSizeComments);

	if (!GraphPanel)
	{
		return;
	}

	auto Graph = GraphPanel->GetGraphObj();
	if (!Graph)
	{
		return;
	}

	if (bProcessedAltReleased)
	{
		return;
	}

	bProcessedAltReleased = true;

	TArray<UEdGraphNode_Comment*> CommentNodes;
	Graph->GetNodesOfClass(CommentNodes);

	TSet<UObject*> SelectedNodes;
	for (auto Node : GraphPanel->SelectionManager.GetSelectedNodes())
	{
		SelectedNodes.Add(Node);
		if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(Node))
		{
			SelectedNodes.Append(SelectedComment->GetNodesUnderComment());
		}
	}

	TMap<TSharedPtr<SAutoSizeCommentsGraphNode>, const TArray<UObject*>> OldCommentContains;
	TMap<UEdGraphNode_Comment*, TArray<UEdGraphNode_Comment*>> OldParentMap;

	TArray<TSharedPtr<SAutoSizeCommentsGraphNode>> ASCGraphNodes;
	TSet<TSharedPtr<SAutoSizeCommentsGraphNode>> ChangedGraphNodes; 

	// gather asc graph nodes and store the comment data for later
	for (UEdGraphNode_Comment* CommentNode : CommentNodes)
	{
		TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode = FASCState::Get().GetASCComment(CommentNode);
		if (!ASCGraphNode)
		{
			continue;
		}

		if (ASCGraphNode->IsHeaderComment())
		{
			continue;
		}

		ASCGraphNodes.Add(ASCGraphNode);

		OldCommentContains.Add(ASCGraphNode, CommentNode->GetNodesUnderComment());

		for (UObject* ContainedObj : CommentNode->GetNodesUnderComment())
		{
			if (UEdGraphNode_Comment* ContainedComment = Cast<UEdGraphNode_Comment>(ContainedObj))
			{
				OldParentMap.FindOrAdd(ContainedComment).Add(CommentNode);
			}
		}
	}

	// update their containing nodes
	for (TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode : ASCGraphNodes)
	{
		UEdGraphNode_Comment* CommentNode = ASCGraphNode->GetCommentNodeObj();

		const ECommentCollisionMethod& AltCollisionMethod = UAutoSizeCommentsSettings::Get().AltCollisionMethod;

		if (SelectedNodes.Contains(CommentNode))
		{
			ASCGraphNode->RefreshNodesInsideComment(AltCollisionMethod, UAutoSizeCommentsSettings::Get().bIgnoreKnotNodesWhenPressingAlt, false);
			ChangedGraphNodes.Add(ASCGraphNode);
		}
		else
		{
			TArray<UEdGraphNode*> OutNodes;
			ASCGraphNode->QueryNodesUnderComment(OutNodes, AltCollisionMethod);
			OutNodes = OutNodes.FilterByPredicate(SAutoSizeCommentsGraphNode::IsMajorNode);

			TSet<UObject*> NewSelection(CommentNode->GetNodesUnderComment());
			bool bChanged = false;
			for (UObject* Node : SelectedNodes)
			{
				if (OutNodes.Contains(Node))
				{
					bool bAlreadyInSet = false;
					NewSelection.Add(Node, &bAlreadyInSet);
					bChanged = !bAlreadyInSet;
				}
				else
				{
					if (NewSelection.Remove(Node) > 0)
					{
						bChanged = true;
					}
				}
			}

			if (bChanged)
			{
				CommentNode->ClearNodesUnderComment();
				ASCGraphNode->AddAllNodesUnderComment(NewSelection.Array(), false);
				ChangedGraphNodes.Add(ASCGraphNode);
			}
		}
	}

	// update existing comment nodes using the parent comments stored earlier 
	for (TSharedPtr<SAutoSizeCommentsGraphNode> GraphNode : ChangedGraphNodes)
	{
		const TArray<UEdGraphNode_Comment*>* OldParents = OldParentMap.Find(GraphNode->GetCommentNodeObj());
		const TArray<UObject*>* OldContains = OldCommentContains.Find(GraphNode);
		GraphNode->UpdateExistingCommentNodes(OldParents, OldContains);
	}

	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda([&]
	{
		bProcessedAltReleased = false;
	}));
}

void FAutoSizeCommentGraphHandler::UpdateCommentChangeState(UEdGraphNode_Comment* Comment)
{
	UEdGraph* Graph = Comment->GetGraph();
	if (!Graph)
	{
		return;
	}

	FASCGraphHandlerData& GraphData = GraphDatas.FindOrAdd(Graph);
	GraphData.CommentChangeData.FindOrAdd(Comment->NodeGuid).UpdateComment(Comment);
}

bool FAutoSizeCommentGraphHandler::HasCommentChangeState(UEdGraphNode_Comment* Comment) const
{
	if (UEdGraph* Graph = Comment->GetGraph())
	{
		if (const FASCGraphHandlerData* GraphData = GraphDatas.Find(Graph))
		{
			return GraphData->CommentChangeData.Contains(Comment->NodeGuid);
		}
	}

	return false;
}

bool FAutoSizeCommentGraphHandler::HasCommentChanged(UEdGraphNode_Comment* Comment)
{
	if (FASCGraphHandlerData* GraphData = GraphDatas.Find(Comment->GetGraph()))
	{
		if (FASCCommentChangeData* CommentChangeData = GraphData->CommentChangeData.Find(Comment->NodeGuid))
		{
			return CommentChangeData->HasCommentChanged(Comment);
		}
	}

	return false;
}

TArray<UEdGraph*> FAutoSizeCommentGraphHandler::GetActiveGraphs()
{
	TArray<TWeakObjectPtr<UEdGraph>> GraphWeakPtrs;
	GraphDatas.GetKeys(GraphWeakPtrs);

	TArray<UEdGraph*> ActiveGraphs;

	for (TWeakObjectPtr<UEdGraph> GraphWeakPtr : GraphWeakPtrs)
	{
		if (GraphWeakPtr.IsValid())
		{
			ActiveGraphs.Add(GraphWeakPtr.Get());
		}
	}

	return ActiveGraphs;
}

bool FAutoSizeCommentGraphHandler::Tick(float DeltaTime)
{
	UpdateNodeUnrelatedState();

	return true;
}

void FAutoSizeCommentGraphHandler::UpdateNodeUnrelatedState()
{
	if (!UAutoSizeCommentsSettings::Get().bHighlightContainingNodesOnSelection)
	{
		return;
	}

	// iterate by graph panel to access the selection manager (otherwise we could use the graph)
	for (int i = ActiveGraphPanels.Num() - 1; i >= 0; --i)
	{
		// remove any invalid graph panels
		if (!ActiveGraphPanels[i].IsValid())
		{
			ActiveGraphPanels.RemoveAt(i);
			continue;
		}

		TSharedPtr<SGraphPanel> GraphPanel = ActiveGraphPanels[i].Pin();
		UEdGraph* Graph = GraphPanel->GetGraphObj();

		if (FASCGraphHandlerData* GraphData = GraphDatas.Find(Graph))
		{
			// gather selected comments
			TArray<UEdGraphNode_Comment*> SelectedComments;

			bool bSelectedNonComment = false;
			for (UObject* SelectedObj : GraphPanel->SelectionManager.SelectedNodes)
			{
				if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(SelectedObj))
				{
					SelectedComments.Add(SelectedComment);
				}
				else
				{
					bSelectedNonComment = true;
					break;
				}
			}

			// clear the unrelated nodes and empty the last selection set
			if (bSelectedNonComment || (SelectedComments.Num() == 0 && GraphData->LastSelectionSet.Num() != 0))
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					Node->SetNodeUnrelated(false);
				}

				GraphData->LastSelectionSet.Empty();
				continue;
			}

			bool bRefreshSelectedNodes = false;

			// check if we need to refresh the selected nodes by seeing if we have deselected any nodes
			for (TWeakObjectPtr<UEdGraphNode_Comment> LastSelected : GraphData->LastSelectionSet)
			{
				if (LastSelected.IsValid())
				{
					if (!SelectedComments.Contains(LastSelected.Get()))
					{
						bRefreshSelectedNodes = true;
						break;
					}
				}
			}

			// update the selection set
			TArray<TWeakObjectPtr<UEdGraphNode_Comment>> LastSelection = GraphData->LastSelectionSet;
			GraphData->LastSelectionSet.Empty();
			for (UEdGraphNode_Comment* SelectedComment : SelectedComments)
			{
				GraphData->LastSelectionSet.Add(SelectedComment);

				// also check if we need to refresh the selected nodes
				if (!bRefreshSelectedNodes && !LastSelection.Contains(SelectedComment))
				{
					bRefreshSelectedNodes = true;
				}
			}

			if (bRefreshSelectedNodes)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					Node->SetNodeUnrelated(true);
				}

				for (TWeakObjectPtr<UEdGraphNode_Comment> Comment : GraphData->LastSelectionSet)
				{
					Comment->SetNodeUnrelated(false);

					for (UEdGraphNode* Node : FASCUtils::GetNodesUnderComment(Comment.Get()))
					{
						Node->SetNodeUnrelated(false);
					}
				}
			}
		}
	}
}

void FAutoSizeCommentGraphHandler::OnNodeAdded(TWeakObjectPtr<UEdGraphNode> NewNodePtr)
{
	if (!NewNodePtr.IsValid())
	{
		return;
	}

	UEdGraphNode* NewNode = NewNodePtr.Get();
	UEdGraphNode* SelectedNode = nullptr;

	if (UAutoSizeCommentsSettings::Get().bAutoRenameNewComments)
	{
		if (UEdGraphNode_Comment* NewComment = Cast<UEdGraphNode_Comment>(NewNode))
		{
			if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(NewComment))
			{
				ASCComment->RequestRename();
			}
		}
	}

	// get the owner panel from the first ASCComment
	TArray<UEdGraphNode_Comment*> Comments;
	NewNode->GetGraph()->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);

	if (Comments.Num() > 0)
	{
		if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(Comments[0]))
		{
			if (TSharedPtr<SGraphPanel> OwnerPanel = ASCComment->GetOwnerPanel())
			{
				if (OwnerPanel->SelectionManager.SelectedNodes.Num() == 1)
				{
					// get the selected node from the owner panel
					SelectedNode = Cast<UEdGraphNode>(OwnerPanel->SelectionManager.SelectedNodes.Array()[0]);
				}
			}
		}
	}

	// handle knot nodes differently: since they can be created by double
	// clicking they may not have a previous selected node, instead check the first input pin
	if (!SelectedNode)
	{
		if (UK2Node_Knot* NewKnot = Cast<UK2Node_Knot>(NewNode))
		{
			if (UEdGraphPin* KnotInputPin = NewKnot->GetInputPin())
			{
				if (KnotInputPin->LinkedTo.Num() > 0)
				{
					SelectedNode = KnotInputPin->LinkedTo[0]->GetOwningNode();
				}
			}
		}
	}

	if (!SelectedNode)
	{
		return;
	}

	AutoInsertIntoCommentNodes(NewNode, SelectedNode);
}

void FAutoSizeCommentGraphHandler::OnNodeDeleted(const FEdGraphEditAction& Action)
{
	// remove any deleted nodes from their containing comments
	if (Action.Graph)
	{
		TArray<UEdGraphNode_Comment*> Comments;
		Action.Graph->GetNodesOfClass<UEdGraphNode_Comment>(Comments);

		// is there a better way of converting this set of const ptrs to non-const ptrs?
		TSet<UObject*> NodeToRemove;
		for (const UEdGraphNode* Node : Action.Nodes)
		{
			NodeToRemove.Add(const_cast<UEdGraphNode*>(Node));
		}

		for (UEdGraphNode_Comment* Comment : Comments)
		{
			FASCUtils::RemoveNodesFromComment(Comment, NodeToRemove);
		}
	}

	// reset unrelated nodes for deleted comments
	bool bResetUnrelatedNodes = false;
	for (const UEdGraphNode* Node : Action.Nodes)
	{
		const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
		if (!Comment)
		{
			return;
		}

		if (Action.Graph)
		{
			bResetUnrelatedNodes = true;
		}

		TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode = FASCState::Get().GetASCComment(Comment);
		if (ASCNode.IsValid())
		{
			ASCNode->OnDeleted();
		}
	}

	if (bResetUnrelatedNodes)
	{
		for (UEdGraphNode* NodeToUpdate : Action.Graph->Nodes)
		{
			NodeToUpdate->SetNodeUnrelated(false);
		}
	}
}

#if ASC_UE_VERSION_OR_LATER(5, 0)
void FAutoSizeCommentGraphHandler::OnObjectPreSave(UObject* Object, FObjectPreSaveContext Context)
{
	OnObjectSaved(Object);
}
#endif

void FAutoSizeCommentGraphHandler::OnObjectSaved(UObject* Object)
{
	if (!UAutoSizeCommentsSettings::Get().bSaveCommentDataOnSavingGraph)
	{
		return;
	}

	// upon saving a graph, save all comments to cache
	if (UEdGraph* Graph = Cast<UEdGraph>(Object))
	{
		if (!GraphDatas.Contains(Graph))
		{
			return;
		}

		FASCGraphData& CacheGraphData = FAutoSizeCommentsCacheFile::Get().GetGraphData(Graph);

		TArray<UEdGraphNode_Comment*> Comments;
		Graph->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);

		if (!bPendingSave)
		{
			bPendingSave = true;
			GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::SaveSizeCache));
		}

		if (UAutoSizeCommentsSettings::Get().CacheSaveMethod == EASCCacheSaveMethod::MetaData)
		{
			// we should do this now since this will edit the package
			CacheGraphData.SaveToPackageMetaData(Graph);
		}
		else
		{
			// make sure we aren't storing old data if we disable this setting after using it for a while
			FAutoSizeCommentsCacheFile::Get().ClearPackageMetaData(Graph);
		}
	}
}

void FAutoSizeCommentGraphHandler::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (!Object)
	{
		return;
	}

	// we are probably currently dragging a node around so don't update now
	if (FSlateApplication::Get().GetModifierKeys().IsAltDown())
	{
		return;
	}

	if (Event.GetEventType() == ETransactionObjectEventType::UndoRedo ||
		Event.GetEventType() == ETransactionObjectEventType::Finalized)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
		{
			if (GetResizingMode(Node->GetGraph()) != EASCResizingMode::Disabled)
			{
				GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::UpdateContainingComments, TWeakObjectPtr<UEdGraphNode>(Node)));
			}
		}
		
	}
}

void FAutoSizeCommentGraphHandler::SaveSizeCache()
{
	FAutoSizeCommentsCacheFile::Get().SaveCacheToFile();
	bPendingSave = false;
}

void FAutoSizeCommentGraphHandler::UpdateContainingComments(TWeakObjectPtr<UEdGraphNode> Node)
{
	if (!Node.IsValid() || !IsValid(Node.Get()))
	{
		return;
	}

	UEdGraph* Graph = Node->GetGraph();
	if (!IsValid(Graph))
	{
		return;
	}

	TArray<UEdGraphNode_Comment*> Comments;

	// check if any node on the graph contains the new node
	Graph->GetNodesOfClass<UEdGraphNode_Comment>(Comments);
	for (UEdGraphNode_Comment* Comment : Comments)
	{
		if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(Comment))
		{
			TArray<UEdGraphNode*> NodesUnderComment;
			FAutoSizeCommentsCacheFile::Get().GetNodesUnderComment(ASCComment, NodesUnderComment);
			if (NodesUnderComment.Contains(Node))
			{
				ASCComment->ResizeToFit();
			}
		}
	}
}
