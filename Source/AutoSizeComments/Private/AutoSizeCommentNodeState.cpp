// Fill out your copyright notice in the Description page of Project Settings.

#include "AutoSizeCommentNodeState.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "K2Node_Knot.h"
#include "Misc/LazySingleton.h"
#include "Subsystems/AssetEditorSubsystem.h"

void UASCNodeState::Cleanup()
{
	if (IsRooted())
	{
		RemoveFromRoot();
		MarkAsGarbage();
	}
}

FASCNodeStateManager& FASCNodeStateManager::Get()
{
	FASCNodeStateManager& Manager = TLazySingleton<FASCNodeStateManager>::Get();
	if (!Manager.bInitialized)
	{
		Manager.Init();
	}

	return Manager;
}

void FASCNodeStateManager::TearDown()
{
	FASCNodeStateManager::Get().Cleanup();
	TLazySingleton<FASCNodeStateManager>::TearDown();
}

bool FASCNodeStateManager::Init()
{
	if (bInitialized)
	{
		return false;
	}

	bInitialized = true;

	FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FASCNodeStateManager::CleanupCommentStateMap);
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		AssetEditorSubsystem->OnAssetEditorRequestClose().AddRaw(this, &FASCNodeStateManager::CleanupOnAssetClosed);
	}

	return true;
}

void FASCNodeStateManager::Cleanup()
{
	if (!bInitialized)
	{
		return;
	}

	bInitialized = false;
	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);

	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		AssetEditorSubsystem->OnAssetEditorRequestClose().RemoveAll(this);
	}
}

void FASCNodeStateManager::CleanupCommentStateMap()
{
	TSet<FGuid> InvalidNodes;

	for (const auto& Elem : CommentStateMap)
	{
		UASCNodeState* NodeState = Elem.Value;
		if (!NodeState->CommentNode.IsValid())
		{
			InvalidNodes.Add(Elem.Key);
			NodeState->Cleanup();
		}
	}

	for (const FGuid& InvalidNode : InvalidNodes)
	{
		CommentStateMap.Remove(InvalidNode);
	}
}

void FASCNodeStateManager::CleanupOnAssetClosed(UObject* Asset, EAssetEditorCloseReason CloseReason)
{
	TSet<FGuid> InvalidNodes;

	for (const auto& Elem : CommentStateMap)
	{
		UASCNodeState* NodeState = Elem.Value;

		// if the comment node is invalid, remove our node state
		if (!NodeState->CommentNode.IsValid())
		{
			InvalidNodes.Add(Elem.Key);
			NodeState->Cleanup();
		}
		else 
		{
			UObject* OuterMostObj = NodeState->CommentNode->GetOutermostObject();
			if (OuterMostObj == Asset)
			{
				InvalidNodes.Add(Elem.Key);
				NodeState->Cleanup();
			}
		}
	}

	for (const FGuid& InvalidNode : InvalidNodes)
	{
		CommentStateMap.Remove(InvalidNode);
	}
}

UASCNodeState* FASCNodeStateManager::GetCommentState(UEdGraphNode_Comment* Comment)
{
	if (!Comment)
	{
		return nullptr;
	}

	if (UASCNodeState* Obj = CommentStateMap.FindRef(Comment->NodeGuid))
	{
		if (IsValid(Obj))
		{
			return Obj;
		}

		return nullptr;
	}

	UASCNodeState* NewState = NewObject<UASCNodeState>();
	CommentStateMap.Add(Comment->NodeGuid, NewState);
	NewState->Initialize(Comment);
	return NewState;
}

UASCNodeState::UASCNodeState()
{
	SetFlags(RF_Transactional);
}

UASCNodeState* UASCNodeState::Get(UEdGraphNode_Comment* Comment)
{
	return FASCNodeStateManager::Get().GetCommentState(Comment);
}

void UASCNodeState::Initialize(UEdGraphNode_Comment* Comment)
{
	CommentNode = Comment;
	AddToRoot();
	InitializeFromCache();
}

void UASCNodeState::PostEditUndo()
{
	UObject::PostEditUndo();
	if (CommentNode.IsValid())
	{
		// check if the header state changed from undoing
		if (bPrevIsHeader != bIsHeader)
		{
			WriteHeaderToComment();

			bPrevIsHeader = bIsHeader;
			TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode = FASCState::Get().GetASCComment(CommentNode.Get());
			if (ASCGraphNode.IsValid())
			{
				ASCGraphNode->SetHeaderStyle(bIsHeader, true);
			}
		}

		WriteNodesToComment();
		FAutoSizeCommentGraphHandler::Get().UpdateCommentChangeState(CommentNode.Get());
	}
}

void UASCNodeState::InitializeFromCache()
{
	TArray<UEdGraphNode*> OutNodesUnder;
	if (FAutoSizeCommentsCacheFile::Get().GetNodesUnderComment(CommentNode.Get(), OutNodesUnder))
	{
		ReplaceNodes(OutNodesUnder, false);
	}
}

bool UASCNodeState::WriteNodesToComment()
{
	if (!CommentNode.IsValid())
	{
		return false;
	}

	// check if the nodes inside has actually changed
	if (CommentNode->GetNodesUnderComment().Num() == NodesUnderComment.Num())
	{
		const bool bMissingNode = CommentNode->GetNodesUnderComment().ContainsByPredicate([&](UObject* Node) { return !NodesUnderComment.Contains(Cast<UEdGraphNode>(Node)); });
		if (!bMissingNode)
		{
			return false;
		}
	}

	CommentNode->ClearNodesUnderComment();
	for (UEdGraphNode* Node : NodesUnderComment)
	{
		if (Node)
		{
			CommentNode->AddNodeUnderComment(Node);
		}
	}

	FASCCommentData& CommentData = FAutoSizeCommentsCacheFile::Get().GetCommentData(CommentNode.Get());
	CommentData.UpdateNodesUnderComment(CommentNode.Get());


	return true;
}

bool UASCNodeState::WriteHeaderToComment()
{
	FASCCommentData& CommentData = FAutoSizeCommentsCacheFile::Get().GetCommentData(CommentNode.Get());
	CommentData.SetHeader(bIsHeader);
	return true;
}

bool UASCNodeState::SyncStateToComment()
{
	if (!CommentNode.IsValid())
	{
		return false;
	}

	TSet<UEdGraphNode*> CurrentNodes = TSet(FASCUtils::GetNodesUnderComment(CommentNode.Get()));
	const bool bHasChanges = CurrentNodes.Num() != NodesUnderComment.Num() || NodesUnderComment.Difference(CurrentNodes).Num() > 0;
	if (bHasChanges)
	{
		ReplaceNodes(CurrentNodes.Array());
		return true;
	}

	return false;
}

void UASCNodeState::UpdateCommentStateChange(bool bUpdateParentComments)
{
	if (bUpdateParentComments)
	{
		UpdateParentComments();
	}

	// UE_LOG(LogAutoSizeComments, Warning, TEXT("Update changed %s %d,%d %d,%d"), *CommentNode->NodeComment, CommentNode->NodePosX, CommentNode->NodePosY, CommentNode->NodeWidth, CommentNode->NodeHeight);
	if (WriteNodesToComment())
	{
		// don't update the graph node if it's been deleted off the graph
		TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode = FASCState::Get().GetASCComment(CommentNode.Get());
		if (ASCGraphNode.IsValid())
		{
			if (!FASCUtils::HasNodeBeenDeleted(CommentNode.Get()))
			{
				ASCGraphNode->HandleCommentNodeStateChanged(this);
			}
		}
	}
}

bool UASCNodeState::CanAddNode(UEdGraphNode* Node, bool bIgnoreKnots)
{
	if (Node == nullptr || Node == CommentNode || NodesUnderComment.Contains(Node))
	{
		return false;
	}

	if ((bIgnoreKnots || UAutoSizeCommentsSettings::Get().bIgnoreKnotNodes) && Cast<UK2Node_Knot>(Node) != nullptr)
	{
		return false;
	}

	if (UEdGraphNode_Comment* OtherComment = Cast<UEdGraphNode_Comment>(Node))
	{
		if (UASCNodeState* OtherState = UASCNodeState::Get(OtherComment))
		{
			if (OtherState->IsHeader())
			{
				return false;
			}
		}

		if (FASCUtils::DoesCommentContainComment(OtherComment, CommentNode.Get()))
		{
			return false;
		}
	}

	return true;
}

bool UASCNodeState::AddNode(UEdGraphNode* Node, bool bUpdateComment)
{
	if (!Node || Node == CommentNode || !CommentNode.IsValid())
	{
		return false;
	}

	if (!CanAddNode(Node))
	{
		return false;
	}

	Modify();
	NodesUnderComment.Add(Node);

	if (bUpdateComment)
	{
		UpdateCommentStateChange();
	}

	return true;
}

bool UASCNodeState::RemoveNode(UEdGraphNode* Node, bool bUpdateComment)
{
	Modify();

	const bool bRemovedSomething = NodesUnderComment.Remove(Node) > 0;

	if (bUpdateComment && bRemovedSomething)
	{
		UpdateCommentStateChange();
	}

	return bRemovedSomething;
}

bool UASCNodeState::AddNodes(const TArray<UEdGraphNode*>& Nodes, bool bUpdateComment)
{
	Modify();

	bool bChanged = false;
	for (UEdGraphNode* Node : Nodes)
	{
		bChanged |= AddNode(Node, false);
	}

	if (bUpdateComment && bChanged)
	{
		UpdateCommentStateChange();
	}

	return bChanged;
}

bool UASCNodeState::RemoveNodes(const TArray<UEdGraphNode*>& Nodes, bool bUpdateComment)
{
	Modify();

	bool bChanged = false;

	for (UEdGraphNode* Node : Nodes)
	{
		bChanged |= RemoveNode(Node, false);
	}

	if (bUpdateComment && bChanged)
	{
		UpdateCommentStateChange();
	}

	return bChanged;
}

void UASCNodeState::ClearNodes(bool bUpdateComment)
{
	if (NodesUnderComment.Num() == 0)
	{
		return;
	}

	Modify();
	NodesUnderComment.Empty();

	if (bUpdateComment)
	{
		UpdateCommentStateChange();
	}
}

bool UASCNodeState::ReplaceNodes(const TArray<UEdGraphNode*>& Nodes, bool bUpdateComment)
{
	// check if the nodes inside has actually changed
	if (Nodes.Num() == NodesUnderComment.Num())
	{
		const bool bMissingNode = Nodes.ContainsByPredicate([&](UObject* Node) { return !NodesUnderComment.Contains(Cast<UEdGraphNode>(Node)); });
		if (!bMissingNode)
		{
			return false;
		}
	}

	Modify();
	ClearNodes(false);
	AddNodes(Nodes, bUpdateComment);

	return true;
}

TSet<UEdGraphNode*> UASCNodeState::GetMajorNodesUnderComment()
{
	TSet<TWeakObjectPtr<UEdGraphNode>> Test;
	TSet<UEdGraphNode*> OutNodes;

	for (UEdGraphNode* Node : NodesUnderComment)
	{
		if (SAutoSizeCommentsGraphNode::IsMajorNode(Node))
		{
			OutNodes.Add(Node);
		}
	}

	return OutNodes;
}

void UASCNodeState::AddChild(UASCNodeState* ChildNodeState)
{
	if (ChildNodeState)
	{
		ChildNodeState->Modify();
		Modify();
		ChildComments.Add(ChildNodeState);
		ChildNodeState->ParentComments.Add(this);
		NodesUnderComment.Add(ChildNodeState->CommentNode.Get());
	}
}

void UASCNodeState::RemoveChild(UASCNodeState* ChildNodeState)
{
	Modify();
	ChildComments.Remove(ChildNodeState);

	if (ChildNodeState)
	{
		ChildNodeState->Modify();
		ChildNodeState->ParentComments.Remove(this);
		NodesUnderComment.Remove(ChildNodeState->CommentNode.Get());
	}
}

void UASCNodeState::UpdateParentComments()
{
	// check all the comments in our graph
	TArray<UEdGraphNode_Comment*> AllComments = FASCUtils::GetCommentsFromGraph(CommentNode->GetGraph());

	const TSet<UEdGraphNode*> MajorNodes = GetMajorNodesUnderComment();

	TSet<UASCNodeState*> DirtyNodes;// = { this };

	for (UEdGraphNode_Comment* OtherComment : AllComments)
	{
		if (OtherComment == CommentNode)
		{
			continue;
		}

		UASCNodeState* OtherCommentState = Get(OtherComment);

		TSet<UEdGraphNode*> OtherMajorNodes = OtherCommentState->GetMajorNodesUnderComment();
		bool bShouldAddOtherNode = OtherMajorNodes.Num() > 0;

		bool bCheckNewRelationship = true;

		if (ParentComments.Contains(OtherCommentState))
		{
			// if not included from the parent, remove ourselves
			if (MajorNodes.IsEmpty() || !OtherMajorNodes.Includes(MajorNodes))
			{
				OtherCommentState->RemoveChild(this);
				DirtyNodes.Add(OtherCommentState);
			}
			else
			{
				bCheckNewRelationship = false;
			}
		}
		else if (ChildComments.Contains(OtherCommentState))
		{
			// if we don't include the child, remove it
			if (OtherMajorNodes.IsEmpty() || !MajorNodes.Includes(OtherMajorNodes))
			{
				RemoveChild(OtherCommentState);
				DirtyNodes.Add(OtherCommentState);
			}
			else
			{
				bCheckNewRelationship = false;
				NodesUnderComment.Add(OtherComment);
			}
		}
		// else
		if (bCheckNewRelationship)
		{
			// if we have more nodes than the other comment, then try to add the other comment
			if (bShouldAddOtherNode && MajorNodes.Num() > OtherMajorNodes.Num())
			{
				// if we contain the other comment, add the other comment
				if (!OtherMajorNodes.IsEmpty() && MajorNodes.Includes(OtherMajorNodes))
				{
					AddChild(OtherCommentState);
					DirtyNodes.Add(OtherCommentState);
				}
			}
			// otherwise check if the other comment contains us
			else if (!MajorNodes.IsEmpty() && !OtherMajorNodes.IsEmpty())
			{
				// otherwise check if the other comment contains us
				if (OtherMajorNodes.Includes(MajorNodes))
				{
					OtherCommentState->AddChild(this);
					DirtyNodes.Add(OtherCommentState);
				}
			}
		}
	}

	for (UASCNodeState* DirtyNode : DirtyNodes)
	{
		DirtyNode->UpdateCommentStateChange(false);
	}
}

bool UASCNodeState::SetIsHeader(bool bNewValue)
{
	// do nothing if we are already a header
	if (bIsHeader == bNewValue)
	{
		return false;
	}

	Modify();

	bIsHeader = bNewValue;
	bPrevIsHeader = bIsHeader;

	if (!bIsHeader)
	{
		ClearNodes(true);
	}

	WriteHeaderToComment();

	TSharedPtr<SAutoSizeCommentsGraphNode> ASCGraphNode = FASCState::Get().GetASCComment(CommentNode.Get());
	if (ASCGraphNode.IsValid())
	{
		ASCGraphNode->SetHeaderStyle(bIsHeader, true);
	}

	return true;
}

void UASCNodeState::SetStyle(UEdGraphNode_Comment* Comment)
{
	
}
