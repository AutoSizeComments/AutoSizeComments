// Copyright fpwong. All Rights Reserved.

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

	void UpdateNodesUnderComment(UEdGraphNode_Comment* Comment);

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

	bool bInitialized = false;

	void CleanupGraph(UEdGraph* Graph);

	bool LoadFromPackageMetaData(UEdGraph* Graph);
	void SaveToPackageMetaData(UEdGraph* Graph);

	bool IsEmpty() const { return CommentData.Num() == 0; }

	FASCCommentData& GetCommentData(UEdGraphNode_Comment* Comment);
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

	void Cleanup();

	void LoadCacheFromFile();

	FASCCacheData CreateCacheFromFile();

	void InitMetaData();

	void SaveCacheToFile();

	void DeleteCache();

	void CleanupFiles();

	void UpdateNodesUnderComment(UEdGraphNode_Comment* Comment) { GetCommentData(Comment).UpdateNodesUnderComment(Comment); }

	FASCCommentData& GetCommentData(UEdGraphNode_Comment* Comment);
	FASCGraphData& GetGraphData(UEdGraph* Graph);
	bool RemoveGraphData(UEdGraph* Graph);
	FASCPackageData* FindPackageData(UPackage* Package);

	void ClearPackageMetaData(UEdGraph* Graph);

	FString GetProjectCachePath(bool bFullPath = false);
	FString GetPluginCachePath(bool bFullPath = false);
	FString GetCachePath(bool bFullPath = false);
	FString GetAlternateCachePath(bool bFullPath = false);

	bool GetNodesUnderComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode, TArray<UEdGraphNode*>& OutNodesUnderComment);

	FASCCommentData& GetCommentData(UEdGraphNode* CommentNode);

	void PrintCache();

	void OnObjectLoaded(UObject* Obj);

protected:
	FASCGraphData& GetCacheFileGraphData(UEdGraph* Graph);

	bool bHasLoaded = false;

	FASCCacheData CacheData;

	void OnPreExit();
};
