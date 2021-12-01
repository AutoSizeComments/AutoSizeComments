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
	FCoreUObjectDelegates::OnObjectSaved.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectSaved);
	FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FAutoSizeCommentGraphHandler::OnObjectTransacted);
}

void FAutoSizeCommentGraphHandler::UnbindDelegates()
{
	FCoreUObjectDelegates::OnObjectSaved.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

	for (const auto& Kvp: GraphHandles)
	{
		if (Kvp.Key.IsValid())
		{
			Kvp.Key->RemoveOnGraphChangedHandler(Kvp.Value);
		}
	}

	GraphHandles.Empty();
}

void FAutoSizeCommentGraphHandler::BindToGraph(UEdGraph* Graph)
{
	if (!Graph || GraphHandles.Contains(Graph))
	{
		return;
	}

	const FDelegateHandle Handle = Graph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FAutoSizeCommentGraphHandler::OnGraphChanged));
	GraphHandles.Add(Graph, Handle);
}

void FAutoSizeCommentGraphHandler::OnGraphChanged(const FEdGraphEditAction& Action)
{
	if (Action.Action == GRAPHACTION_AddNode)
	{
		OnNodeAdded(Action);
	}
	else if (Action.Action == GRAPHACTION_RemoveNode)
	{
		OnNodeDeleted(Action);
	}

}

void FAutoSizeCommentGraphHandler::AutoInsertIntoCommentNodes(TWeakObjectPtr<UEdGraphNode> NewNode, TWeakObjectPtr<UEdGraphNode> LastSelectedNode)
{
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
	TArray<UEdGraphNode*> LinkedInput = FASCUtils::GetLinkedNodes(NewNode.Get(), EGPD_Input).FilterByPredicate(IsSelectedNode);
	TArray<UEdGraphNode*> LinkedOutput = FASCUtils::GetLinkedNodes(NewNode.Get(), EGPD_Output).FilterByPredicate(IsSelectedNode);

	struct FLocal
	{
		static void TakeCommentNode(UEdGraph* Graph, UEdGraphNode* Node, UEdGraphNode* NodeToTakeFrom)
		{
			TArray<UEdGraphNode_Comment*> CommentNodes;
			Graph->GetNodesOfClass(CommentNodes);
			auto ContainingComments = FASCUtils::GetContainingCommentNodes(CommentNodes, NodeToTakeFrom);
			for (UEdGraphNode_Comment* CommentNode : ContainingComments)
			{
				CommentNode->AddNodeUnderComment(Node);
			}
		};
	};

	const auto AutoInsertStyle = GetDefault<UAutoSizeCommentsSettings>()->AutoInsertComment;
	if (AutoInsertStyle == EASCAutoInsertComment::Surrounded)
	{
		if (LinkedInput.Num() == 1 && LinkedOutput.Num() == 1)
		{
			TArray<UEdGraphNode_Comment*> CommentNodes;
			Graph->GetNodesOfClass(CommentNodes);
			auto ContainingCommentsA = FASCUtils::GetContainingCommentNodes(CommentNodes, LinkedOutput[0]);
			auto ContainingCommentsB = FASCUtils::GetContainingCommentNodes(CommentNodes, LinkedInput[0]);
	
			ContainingCommentsA.RemoveAll([&ContainingCommentsB](UEdGraphNode_Comment* Comment)
			{
				return !ContainingCommentsB.Contains(Comment);
			});
	
			if (ContainingCommentsA.Num() > 0)
			{
				FLocal::TakeCommentNode(Graph, NewNode.Get(), ContainingCommentsA[0]);
			}
		}
	}
	else if (AutoInsertStyle == EASCAutoInsertComment::Always)
	{
		if (LinkedOutput.Num() == 1)
		{
			FLocal::TakeCommentNode(Graph, NewNode.Get(), LinkedOutput[0]);
		}
	
		if (LinkedInput.Num() == 1)
		{
			FLocal::TakeCommentNode(Graph, NewNode.Get(), LinkedInput[0]);
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

void FAutoSizeCommentGraphHandler::OnObjectSaved(UObject* Object)
{
	if (!GetDefault<UAutoSizeCommentsSettings>()->bSaveCommentDataOnSavingGraph)
	{
		return;
	}

	FAutoSizeCommentsCacheFile& SizeCache = FAutoSizeCommentsCacheFile::Get();

	// upon saving a graph, save all comments to cache
	if (UEdGraph* Graph = Cast<UEdGraph>(Object))
	{
		TArray<UEdGraphNode_Comment*> Comments;
		Graph->GetNodesOfClassEx<UEdGraphNode_Comment>(Comments);

		for (UEdGraphNode_Comment* Comment : Comments)
		{
			SizeCache.UpdateComment(Comment);
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
