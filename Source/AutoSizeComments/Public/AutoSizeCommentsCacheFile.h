// Copyright 2019 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"

#include "AutoSizeCommentsCacheFile.generated.h"

class SAutoSizeCommentNode;
USTRUCT()
struct FASCNodesInside
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FGuid> NodeGuids; // containing nodes
};


USTRUCT()
struct FASCCommentData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FASCNodesInside> CommentData; // nodes -> nodes inside

	void CleanupGraph(UEdGraph* Graph);
};

USTRUCT()
struct FASCGraphData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FASCCommentData> GraphData; // graph -> comment nodes
};

USTRUCT()
struct FASCPackageData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FName, FASCGraphData> PackageData; // package -> graph
};

class FASCCacheFile
{
public:
	FASCCacheFile();

	FASCPackageData& GetPackageData() { return PackageData; }

	void LoadCache();

	void SaveCache();

	void DeleteCache();

	void CleanupFiles();

	FASCCommentData& GetGraphData(UEdGraph* Graph);

	FString GetCachePath();

	bool GetNodesUnderComment(TSharedPtr<SAutoSizeCommentNode> ASCNode, TArray<UEdGraphNode*>& OutNodesUnderComment);

private:
	FASCPackageData PackageData;
};
