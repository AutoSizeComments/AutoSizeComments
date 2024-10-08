// Fill out your copyright notice in the Description page of Project Settings.


#include "ASCTypes.h"

#include "AutoSizeCommentsUtils.h"
#include "EdGraph/EdGraphNode.h"

FASCNodeId::FASCNodeId(UEdGraphNode* InNode)
{
	check(InNode);
	if (FASCUtils::HasReliableGuid(InNode->GetGraph()))
	{
		Id = InNode->NodeGuid;
	}
	else
	{
		Id.Invalidate();
	}

	LocId = FIntPoint(InNode->NodePosX, InNode->NodePosY);
};
