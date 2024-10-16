// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AutoSizeCommentsGraphNode.h"
#include "EdGraph/RigVMEdGraphSchema.h"

class AUTOSIZECOMMENTS_API SASCComment_ControlRig : public SAutoSizeCommentsGraphNode
{
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override;
	virtual bool SetNodePosition(UEdGraphNode* Node, int32 NodePosX, int32 NodePosY, bool bModify) const override;
	virtual bool ResizeNode(int32 NewWidth, int32 NewHeight, bool bModify) override;

	const URigVMEdGraphSchema* GetRigVMSchema();
	URigVMController* GetRigVMController();
};
