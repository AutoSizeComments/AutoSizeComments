// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsCacheFile.h"

#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Core/Public/HAL/PlatformFileManager.h"
#include "Core/Public/Misc/CoreDelegates.h"
#include "Core/Public/Misc/FileHelper.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor/GraphEditor/Public/SGraphPanel.h"
#include "EngineSettings/Classes/GeneralProjectSettings.h"
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include "Misc/LazySingleton.h"
#include "Projects/Public/Interfaces/IPluginManager.h"

FAutoSizeCommentsCacheFile& FAutoSizeCommentsCacheFile::Get()
{
	return TLazySingleton<FAutoSizeCommentsCacheFile>::Get();
}

void FAutoSizeCommentsCacheFile::TearDown()
{
	TLazySingleton<FAutoSizeCommentsCacheFile>::TearDown();
}

void FAutoSizeCommentsCacheFile::Init()
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnFilesLoaded().AddRaw(this, &FAutoSizeCommentsCacheFile::LoadCache);

	FCoreDelegates::OnPreExit.AddRaw(this, &FAutoSizeCommentsCacheFile::SaveCache);
}

void FAutoSizeCommentsCacheFile::LoadCache()
{
	if (!GetDefault<UAutoSizeCommentsSettings>()->bSaveCommentNodeDataToFile)
	{
		return;
	}

	if (bHasLoaded)
	{
		return;
	}

	bHasLoaded = true;

	const FString CachePath = GetCachePath();
	const FString OldCachePath = GetAlternateCachePath();

	FString FileData;
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CachePath))
	{
		FFileHelper::LoadFileToString(FileData, *CachePath);

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &CacheData, 0, 0))
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Loaded auto size comments cache: %s"), *GetCachePath(true));
		}
		else
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Failed to load auto size comments cache: %s"), *GetCachePath(true));
		}
	}
	else
	{
		FFileHelper::LoadFileToString(FileData, *OldCachePath);

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &CacheData, 0, 0))
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Loaded auto size comments cache from old cache path: %s"), *GetAlternateCachePath(true));
		}
		else
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Failed to load auto size comments cache from old cache path: %s"), *GetAlternateCachePath(true));
		}
	}

	CleanupFiles();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnFilesLoaded().RemoveAll(this);
}

void FAutoSizeCommentsCacheFile::SaveCache()
{
	if (!GetDefault<UAutoSizeCommentsSettings>()->bSaveCommentNodeDataToFile)
	{
		return;
	}

	const double StartTime = FPlatformTime::Seconds();

	const auto CachePath = GetCachePath();

	// Write data to file
	FString JsonAsString;
	FJsonObjectConverter::UStructToJsonObjectString(CacheData, JsonAsString);
	FFileHelper::SaveStringToFile(JsonAsString, *CachePath);
	const double TimeTaken = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
	UE_LOG(LogAutoSizeComments, Log, TEXT("Saved cache to %s took %6.2fms"), *GetCachePath(true), TimeTaken);
}

void FAutoSizeCommentsCacheFile::DeleteCache()
{
	const FString ProjectCachePath = GetProjectCachePath();
	const FString PluginCachePath = GetPluginCachePath();

	CacheData.PackageData.Reset();

	if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*ProjectCachePath))
	{
		UE_LOG(LogAutoSizeComments, Log, TEXT("Deleted project cache file at %s"), *GetCachePath(true));
	}

	if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PluginCachePath))
	{
		UE_LOG(LogAutoSizeComments, Log, TEXT("Deleted plugin cache file at %s"), *GetCachePath(true));
	}
}

void FAutoSizeCommentsCacheFile::CleanupFiles()
{
	if (GetDefault<UAutoSizeCommentsSettings>()->bDisablePackageCleanup)
	{
		return;
	}

	// Get all assets
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> AllAssets;

	// Get package guids from assets
	TSet<FName> CurrentPackageNames;

#if ASC_UE_VERSION_OR_LATER(5, 0)
	TArray<FAssetData> Assets;
	FARFilter Filter;
	AssetRegistry.GetAllAssets(Assets, true);
	for (FAssetData& Asset : Assets)
	{
		CurrentPackageNames.Add(Asset.PackageName);
	}
#else
	const auto& AssetDataMap = AssetRegistry.GetAssetRegistryState()->GetObjectPathToAssetDataMap();
	for (const TPair<FName, const FAssetData*>& AssetDataPair : AssetDataMap)
	{
		const FAssetData* AssetData = AssetDataPair.Value;
		CurrentPackageNames.Add(AssetData->PackageName);
	}
#endif

	// Remove missing files
	TArray<FName> OldPackageGuids;
	CacheData.PackageData.GetKeys(OldPackageGuids);
	for (FName PackageGuid : OldPackageGuids)
	{
		if (!CurrentPackageNames.Contains(PackageGuid))
		{
			CacheData.PackageData.Remove(PackageGuid);
		}
	}
}

void FAutoSizeCommentsCacheFile::UpdateCommentState(UEdGraphNode_Comment* Comment)
{
	if (!Comment)
	{
		UE_LOG(LogAutoSizeComments, Warning, TEXT("Tried to update null comment"));
		return;
	}

	UEdGraph* Graph = Comment->GetGraph();
	if (!Graph)
	{
		UE_LOG(LogAutoSizeComments, Warning, TEXT("Tried to update comment with null graph"));
		return;
	}

	if (FASCUtils::HasNodeBeenDeleted(Comment))
	{
		GetGraphData(Graph).CommentData.Remove(Comment->NodeGuid);
	}
	else
	{
		UpdateNodesUnderComment(Comment);
	}
}

void FAutoSizeCommentsCacheFile::UpdateNodesUnderComment(UEdGraphNode_Comment* Comment)
{
	UEdGraph* Graph = Comment->GetGraph();
	if (!Graph)
	{
		UE_LOG(LogAutoSizeComments, Warning, TEXT("Tried to update comment with null graph"));
		return;
	}

	FASCCommentData& CommentData = GetGraphData(Graph).CommentData.FindOrAdd(Comment->NodeGuid);

	const TArray<UEdGraphNode*> NodesUnder = FASCUtils::GetNodesUnderComment(Comment);
	CommentData.NodeGuids.Reset(NodesUnder.Num());

	// update nodes under
	for (UEdGraphNode* Node : NodesUnder)
	{
		if (!FASCUtils::HasNodeBeenDeleted(Node))
		{
			CommentData.NodeGuids.Add(Node->NodeGuid);
		}
	}
}

FASCGraphData& FAutoSizeCommentsCacheFile::GetGraphData(UEdGraph* Graph)
{
	UPackage* Package = Graph->GetOutermost();

	FASCPackageData& PackageData = CacheData.PackageData.FindOrAdd(Package->GetFName());

	return PackageData.GraphData.FindOrAdd(Graph->GraphGuid);
}

FString FAutoSizeCommentsCacheFile::GetProjectCachePath(bool bFullPath)
{
	return FPaths::ProjectDir() / TEXT("Saved") / TEXT("AutoSizeComments") / TEXT("AutoSizeCommentsCache.json");
}

FString FAutoSizeCommentsCacheFile::GetPluginCachePath(bool bFullPath)
{
	FString PluginDir = IPluginManager::Get().FindPlugin("AutoSizeComments")->GetBaseDir();

	if (bFullPath)
	{
		PluginDir = FPaths::ConvertRelativePathToFull(PluginDir);
	}

	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
	const FGuid& ProjectID = ProjectSettings->ProjectID;

	return PluginDir + "/ASCCache/" + ProjectID.ToString() + ".json";
}

FString FAutoSizeCommentsCacheFile::GetCachePath(bool bFullPath)
{
	const bool bIsProject = GetDefault<UAutoSizeCommentsSettings>()->CacheSaveLocation == EASCCacheSaveLocation::Project;
	return bIsProject ? GetProjectCachePath(bFullPath) : GetPluginCachePath(bFullPath);
}

FString FAutoSizeCommentsCacheFile::GetAlternateCachePath(bool bFullPath)
{
	const bool bIsProject = GetDefault<UAutoSizeCommentsSettings>()->CacheSaveLocation == EASCCacheSaveLocation::Project;
	return bIsProject ? GetPluginCachePath(bFullPath) : GetProjectCachePath(bFullPath);
}

bool FAutoSizeCommentsCacheFile::GetNodesUnderComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode, TArray<UEdGraphNode*>& OutNodesUnderComment)
{
	UEdGraphNode* Node = ASCNode->GetNodeObj();
	UEdGraph* Graph = Node->GetGraph();
	FASCGraphData& Data = GetGraphData(Graph);
	if (Data.CommentData.Contains(Node->NodeGuid))
	{
		for (FGuid NodeInsideGuid : Data.CommentData[Node->NodeGuid].NodeGuids)
		{
			for (UEdGraphNode* NodeOnGraph : Graph->Nodes)
			{
				if (NodeOnGraph->NodeGuid == NodeInsideGuid)
				{
					OutNodesUnderComment.Add(NodeOnGraph);
					break;
				}
			}
		}

		return true;
	}

	return false;
}

FASCCommentData& FAutoSizeCommentsCacheFile::GetCommentData(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode)
{
	return GetCommentData(ASCNode->GetNodeObj());
}

FASCCommentData& FAutoSizeCommentsCacheFile::GetCommentData(UEdGraphNode* CommentNode)
{
	UEdGraph* Graph = CommentNode->GetGraph();
	FASCGraphData& Data = GetGraphData(Graph);
	return Data.CommentData.FindOrAdd(CommentNode->NodeGuid);
}

void FAutoSizeCommentsCacheFile::PrintCache()
{
	for (auto& Package : CacheData.PackageData)
	{
		for (auto& GraphData : Package.Value.GraphData)
		{
			UE_LOG(LogAutoSizeComments, VeryVerbose, TEXT("Graph %s"), *GraphData.Key.ToString());
			for (auto& CommentData : GraphData.Value.CommentData)
			{
				UE_LOG(LogAutoSizeComments, VeryVerbose, TEXT("\tComment %s"), *CommentData.Key.ToString());
				for (auto NodeGuid : CommentData.Value.NodeGuids)
				{
					UE_LOG(LogAutoSizeComments, VeryVerbose, TEXT("\t\tNode %s"), *NodeGuid.ToString());
				}
			}
		}
	}
}

void FAutoSizeCommentsCacheFile::OnPreExit()
{
	if (GetDefault<UAutoSizeCommentsSettings>()->bSaveCommentDataOnExit)
	{
		SaveCache();
	}
}

void FASCGraphData::CleanupGraph(UEdGraph* Graph)
{
	// Get all the current nodes from the graph
	TSet<FGuid> CurrentNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		CurrentNodes.Add(Node->NodeGuid);
	}

	// Remove any missing guids from the cached comments nodes
	TArray<FGuid> NodesToRemove;
	for (auto& Elem : CommentData)
	{
		const FGuid& CommentNode = Elem.Key;
		if (!CurrentNodes.Contains(CommentNode))
		{
			NodesToRemove.Add(CommentNode);
			break;
		}

		TArray<FGuid>& ContainingNodes = Elem.Value.NodeGuids;
		for (int i = ContainingNodes.Num() - 1; i >= 0; --i)
		{
			const FGuid& Node = ContainingNodes[i];
			if (!CurrentNodes.Contains(Node))
			{
				ContainingNodes.Remove(Node);
			}
		}
	}

	for (const FGuid& NodeGuid : NodesToRemove)
	{
		CommentData.Remove(NodeGuid);
	}
}
