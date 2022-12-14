// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "AutoSizeCommentsCacheFile.generated.h"

class UEdGraphNode_Comment;
class SAutoSizeCommentsGraphNode;

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCCommentData
{
	GENERATED_USTRUCT_BODY()

	/* Containing nodes */
	UPROPERTY()
	TArray<FGuid> NodeGuids;

	void SetHeader(bool bValue) { bHeader = bValue != 0; }
	bool IsHeader() const { return static_cast<bool>(bHeader); }

	void SetInitialized(bool bValue) { bInit = bValue != 0; }
	bool HasBeenInitialized() const { return static_cast<bool>(bInit); }

private:
	/* Is this node a header node */
	UPROPERTY()
	uint32 bHeader = 0;

	/* Has the node been initialized */
	UPROPERTY()
	uint32 bInit = 0;
};

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCGraphData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FASCCommentData> CommentData; // node guid -> comment data

	void CleanupGraph(UEdGraph* Graph);
};

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCPackageData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FASCGraphData> GraphData; // graph guid -> graph data
};

USTRUCT()
struct AUTOSIZECOMMENTS_API FASCCacheData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FName, FASCPackageData> PackageData; // package -> graph data
};

class AUTOSIZECOMMENTS_API FAutoSizeCommentsCacheFile
{
public:
	static FAutoSizeCommentsCacheFile& Get();
	static void TearDown();

	FAutoSizeCommentsCacheFile() = default;

	FASCCacheData& GetCacheData() { return CacheData; }

	void Init();

	void LoadCache();

	void SaveCache();

	void DeleteCache();

	void CleanupFiles();

	void UpdateCommentState(UEdGraphNode_Comment* Comment);
	void UpdateNodesUnderComment(UEdGraphNode_Comment* Comment);

	FASCGraphData& GetGraphData(UEdGraph* Graph);

	FString GetProjectCachePath(bool bFullPath = false);
	FString GetPluginCachePath(bool bFullPath = false);
	FString GetCachePath(bool bFullPath = false);
	FString GetAlternateCachePath(bool bFullPath = false);

	bool GetNodesUnderComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode, TArray<UEdGraphNode*>& OutNodesUnderComment);

	FASCCommentData& GetCommentData(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode);

	FASCCommentData& GetCommentData(UEdGraphNode* CommentNode);

	void PrintCache();

private:
	bool bHasLoaded = false;

	FASCCacheData CacheData;

	void OnPreExit();
};
