// Fill out your copyright notice in the Description page of Project Settings.


#include "OtherGraphs/ASCComment_ControlRig.h"

#include "EdGraphNode_Comment.h"
#include "RigVMBlueprint.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphSchema.h"

void SASCComment_ControlRig::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
{
	SAutoSizeCommentsGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);
}

bool SASCComment_ControlRig::SetNodePosition(UEdGraphNode* Node, int32 NodePosX, int32 NodePosY, bool bModify) const
{
	if (SAutoSizeCommentsGraphNode::SetNodePosition(Node, NodePosX, NodePosY, bModify))
	{
		if (const URigVMEdGraphSchema* RigSchema = Cast<URigVMEdGraphSchema>(GraphNode->GetSchema()))
		{
			RigSchema->SetNodePosition(Node, FVector2D(NodePosX, NodePosY), false);
		}

		return true;
	}

	return false;
}

bool SASCComment_ControlRig::ResizeNode(int32 NewWidth, int32 NewHeight, bool bModify)
{
	if (SAutoSizeCommentsGraphNode::ResizeNode(NewWidth, NewHeight, bModify))
	{
		if (UEdGraphNode_Comment* Comment = GetCommentNodeObj())
		{
			if (URigVMController* Controller = GetRigVMController())
			{
				Controller->OpenUndoBracket(TEXT("Resize Comment Box"));
				Controller->SetNodeSizeByName(Comment->GetFName(), FVector2D(NewWidth, NewHeight), true, false, true);
				Controller->CloseUndoBracket();
			}
		}

		return true;
	}

	return false;
}

const URigVMEdGraphSchema* SASCComment_ControlRig::GetRigVMSchema()
{
	if (!GraphNode)
	{
		return nullptr;
	}

	return Cast<URigVMEdGraphSchema>(GraphNode->GetSchema());
}

URigVMController* SASCComment_ControlRig::GetRigVMController()
{
	if (GraphNode)
	{
		if (URigVMEdGraph* Graph = Cast<URigVMEdGraph>(GraphNode->GetOuter()))
		{
			if (URigVMBlueprint* Blueprint = Cast<URigVMBlueprint>(Graph->GetOuter()))
			{
				if (URigVMController* Controller = Blueprint->GetController(Graph))
				{
					return Controller;
				}
			}
		}
	}

	return nullptr;
}
