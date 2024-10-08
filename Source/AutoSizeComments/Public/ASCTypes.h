// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ASCTypes.generated.h"


class UEdGraphNode;

USTRUCT()
struct FASCNodeId
{
	GENERATED_BODY()

	FASCNodeId()
		: Id(FGuid())
		, LocId(FIntPoint())
	{
	};

	explicit FASCNodeId(UEdGraphNode* InNode);

	UPROPERTY()
	FGuid Id;

	UPROPERTY()
	FIntPoint LocId;

	bool operator==(const FASCNodeId& Other) const
	{
		return Id.IsValid() ? (Id == Other.Id) : (Other.LocId == LocId);
	}

	bool operator==(UEdGraphNode* OtherNode) const
	{
		return (*this) == FASCNodeId(OtherNode);
	}

	FString ToString()
	{
		return FString::Printf(TEXT("%s %s"), *Id.ToString(), *LocId.ToString());
	}

	friend uint32 GetTypeHash(const FASCNodeId& NodeId)
	{
		return NodeId.Id.IsValid() ? GetTypeHash(NodeId.Id) : GetTypeHash(NodeId.LocId);
	}
};
