// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AutoSizeCommentsGraphNode.h"

class UMaterialGraphNode_Comment;

class AUTOSIZECOMMENTS_API SASCComment_Material : public SAutoSizeCommentsGraphNode
{
protected:
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override;
	virtual bool SetNodePosition(UEdGraphNode* Node, int32 NodePosX, int32 NodePosY, bool bModify) const override;

	void UpdateMaterialCommentLocation(UMaterialGraphNode_Comment* MaterialComment) const;
};
