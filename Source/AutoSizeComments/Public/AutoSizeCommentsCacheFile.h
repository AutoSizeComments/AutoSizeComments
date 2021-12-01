// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "AutoSizeCommentsCacheFile.generated.h"

class UEdGraphNode_Comment;
class SAutoSizeCommentsGraphNode;

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCNodesInside
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FGuid> NodeGuids; // containing nodes
};

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCCommentData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FASCNodesInside> CommentData; // nodes -> nodes inside

	void CleanupGraph(UEdGraph* Graph);
};

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCGraphData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FASCCommentData> GraphData; // graph -> comment nodes
};

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCPackageData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FName, FASCGraphData> PackageData; // package -> graph
};

class AUTOSIZECOMMENTS_API FAutoSizeCommentsCacheFile
{
public:
	static FAutoSizeCommentsCacheFile& Get();
	static void TearDown();

	FAutoSizeCommentsCacheFile() = default;

	FASCPackageData& GetPackageData() { return PackageData; }

	void Init();

	void LoadCache();

	void SaveCache();

	void DeleteCache();

	void CleanupFiles();

	void UpdateComment(UEdGraphNode_Comment* Comment);

	FASCCommentData& GetGraphData(UEdGraph* Graph);

	FString GetCachePath();

	bool GetNodesUnderComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode, TArray<UEdGraphNode*>& OutNodesUnderComment);

	void PrintCache();

private:
	FASCPackageData PackageData;

	void OnPreExit();
};
