// Fill out your copyright notice in the Description page of Project Settings.


#include "OtherGraphs/ASCComment_Material.h"

#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "Materials/MaterialExpressionComment.h"

void SASCComment_Material::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
{
	SAutoSizeCommentsGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);
	UpdateMaterialCommentLocation(Cast<UMaterialGraphNode_Comment>(GetCommentNodeObj()));
}

bool SASCComment_Material::SetNodePosition(UEdGraphNode* Node, int32 NodePosX, int32 NodePosY, bool bModify) const
{
	if (SAutoSizeCommentsGraphNode::SetNodePosition(Node, NodePosX, NodePosY, bModify))
	{
		if (auto MaterialComment = Cast<UMaterialGraphNode_Comment>(Node))
		{
			UpdateMaterialCommentLocation(MaterialComment);
		}

		return true;
	}

	return false;
}

void SASCComment_Material::UpdateMaterialCommentLocation(UMaterialGraphNode_Comment* MaterialComment) const
{
	if (!MaterialComment || !MaterialComment->MaterialExpressionComment)
	{
		return;
	}

	MaterialComment->MaterialExpressionComment->MaterialExpressionEditorX = MaterialComment->NodePosX;
	MaterialComment->MaterialExpressionComment->MaterialExpressionEditorY = MaterialComment->NodePosY;
	MaterialComment->MaterialExpressionComment->MarkPackageDirty();
	MaterialComment->MaterialDirtyDelegate.ExecuteIfBound();
}
