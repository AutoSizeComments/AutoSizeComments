// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphHandler.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditAction.h"
#include "SGraphPanel.h"
#include "Misc/LazySingleton.h"

#if ASC_UE_VERSION_OR_LATER(5, 0)
#include "UObject/ObjectSaveContext.h"
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

	FASCGraphHandlerData GraphData;
	GraphData.OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::OnGraphChanged));

	GraphDatas.Add(Graph, GraphData);
}

void FAutoSizeCommentGraphHandler::OnGraphChanged(const FEdGraphEditAction& Action)
{
	if ((Action.Action & GRAPHACTION_AddNode) != 0)
	{
		OnNodeAdded(Action);
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

void FAutoSizeCommentGraphHandler::OnNodeAdded(const FEdGraphEditAction& Action)
{
	if (Action.Nodes.Num() != 1)
	{
		return;
	}

	const UEdGraphNode* ConstNewNode = Action.Nodes.Array()[0];
	if (ConstNewNode->Pins.Num() == 0)
	{
		return;
	}

	// we don't want a const ptr
	UEdGraphNode* NewNode = ConstNewNode->Pins[0]->GetOwningNode();

	TArray<UEdGraphNode_Comment*> Comments;
	NewNode->GetGraph()->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);
	if (Comments.Num() == 0)
	{
		return;
	}

	TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(Comments[0]);
	if (!ASCComment.IsValid())
	{
		return;
	}

	TSharedPtr<SGraphPanel> OwnerPanel = ASCComment->GetOwnerPanel();
	if (!OwnerPanel || OwnerPanel->SelectionManager.SelectedNodes.Num() != 1)
	{
		return;
	}

	UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(OwnerPanel->SelectionManager.SelectedNodes.Array()[0]);

	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::AutoInsertIntoCommentNodes, TWeakObjectPtr<UEdGraphNode>(NewNode), TWeakObjectPtr<UEdGraphNode>(SelectedNode)));
}

void FAutoSizeCommentGraphHandler::OnNodeDeleted(const FEdGraphEditAction& Action)
{
	for (const UEdGraphNode* Node : Action.Nodes)
	{
		const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
		if (!Comment)
		{
			return;
		}

		TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode = FASCState::Get().GetASCComment(Comment);
		if (ASCNode.IsValid())
		{
			ASCNode->OnDeleted();
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
			CacheFile.UpdateNodesUnderComment(Comment);
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
	if (Event.GetEventType() != ETransactionObjectEventType::UndoRedo && Event.GetEventType() != ETransactionObjectEventType::Finalized)
	{
		return;
	}

	if (Event.GetEventType() == ETransactionObjectEventType::Finalized)
	{
		// we are probably currently dragging a node around so don't update now
		if (FSlateApplication::Get().GetModifierKeys().IsAltDown())
		{
			return;
		}
	}

	if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
	{
		if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(Cast<UEdGraphNode_Comment>(Node)))
		{
			ASCComment->bWasCopyPasted = true;
		}
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::UpdateContainingComments, TWeakObjectPtr<UEdGraphNode>(Node)));
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
		if (Comment->GetNodesUnderComment().Contains(Node))
		{
			if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(Comment))
			{
				ASCComment->ResizeToFit();
			}
		}
	}
}
