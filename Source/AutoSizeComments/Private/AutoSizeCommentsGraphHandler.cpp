// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphHandler.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditAction.h"
#include "K2Node_Knot.h"
#include "SGraphPanel.h"
#include "Misc/LazySingleton.h"

#if ASC_UE_VERSION_OR_LATER(5, 0)
#include "UObject/ObjectSaveContext.h"
#endif

#if ASC_UE_VERSION_OR_LATER(5, 1)
#include "Misc/TransactionObjectEvent.h"
#endif

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
}

void FAutoSizeCommentGraphHandler::OnGraphChanged(const FEdGraphEditAction& Action)
{
	if ((Action.Action & GRAPHACTION_AddNode) != 0)
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
	EASCAutoInsertComment AutoInsertStyle = GetDefault<UAutoSizeCommentsSettings>()->AutoInsertComment;
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

		const ECommentCollisionMethod& AltCollisionMethod = GetDefault<UAutoSizeCommentsSettings>()->AltCollisionMethod;

		if (SelectedNodes.Contains(CommentNode))
		{
			ASCGraphNode->RefreshNodesInsideComment(AltCollisionMethod, GetDefault<UAutoSizeCommentsSettings>()->bIgnoreKnotNodesWhenPressingAlt, false);
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

bool FAutoSizeCommentGraphHandler::RegisterComment(UEdGraphNode_Comment* Comment)
{
	if (Comment)
	{
		if (UEdGraph* Graph = Comment->GetGraph())
		{
			if (FASCGraphHandlerData* GraphData = GraphDatas.Find(Graph))
			{
				if (!GraphData->RegisteredComments.Contains(Comment))
				{
					GraphData->RegisteredComments.Add(Comment);
					return true;
				}
			}
		}
	}

	return false;
}

void FAutoSizeCommentGraphHandler::UpdateCommentChangeState(UEdGraphNode_Comment* Comment)
{
	UEdGraph* Graph = Comment->GetGraph();
	if (!Graph)
	{
		return;
	}

	FASCGraphHandlerData& GraphData = GraphDatas.FindOrAdd(Comment->GetGraph());
	GraphData.CommentChangeData.FindOrAdd(Comment).UpdateComment(Comment);
}

bool FAutoSizeCommentGraphHandler::HasCommentChanged(UEdGraphNode_Comment* Comment)
{
	if (FASCGraphHandlerData* GraphData = GraphDatas.Find(Comment->GetGraph()))
	{
		if (FASCCommentChangeData* CommentChangeData = GraphData->CommentChangeData.Find(Comment))
		{
			return CommentChangeData->HasCommentChanged(Comment);
		}
	}

	return false;
}

bool FAutoSizeCommentGraphHandler::Tick(float DeltaTime)
{
	UpdateNodeUnrelatedState();

	return true;
}

void FAutoSizeCommentGraphHandler::UpdateNodeUnrelatedState()
{
	if (!GetDefault<UAutoSizeCommentsSettings>()->bHighlightContainingNodesOnSelection)
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
	if (!GetDefault<UAutoSizeCommentsSettings>()->bSaveCommentDataOnSavingGraph)
	{
		return;
	}

	FAutoSizeCommentsCacheFile& CacheFile = FAutoSizeCommentsCacheFile::Get();

	// upon saving a graph, save all comments to cache
	if (UEdGraph* Graph = Cast<UEdGraph>(Object))
	{
		if (!GraphDatas.Contains(Graph))
		{
			return;
		}

		TArray<UEdGraphNode_Comment*> Comments;
		Graph->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);

		for (UEdGraphNode_Comment* Comment : Comments)
		{
			CacheFile.UpdateCommentState(Comment);
		}

		if (!bPendingSave)
		{
			bPendingSave = true;
			GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::SaveSizeCache));
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
		if (GetDefault<UAutoSizeCommentsSettings>()->ResizingMode == EASCResizingMode::Always ||
			GetDefault<UAutoSizeCommentsSettings>()->ResizingMode == EASCResizingMode::Reactive)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::UpdateContainingComments, TWeakObjectPtr<UEdGraphNode>(Node)));
			}
		}
	}
}

void FAutoSizeCommentGraphHandler::SaveSizeCache()
{
	FAutoSizeCommentsCacheFile::Get().SaveCache();
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
