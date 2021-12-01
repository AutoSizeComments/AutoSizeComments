// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsCacheFile.h"

#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "AssetRegistry/Public/AssetRegistryState.h"
#include "Core/Public/HAL/PlatformFilemanager.h"
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
	AssetRegistry.OnFilesLoaded().AddLambda([&]()
	{
		LoadCache();
	});

	FCoreDelegates::OnPreExit.AddRaw(this, &FAutoSizeCommentsCacheFile::SaveCache);
}

void FAutoSizeCommentsCacheFile::LoadCache()
{
	if (!GetDefault<UAutoSizeCommentsSettings>()->bSaveCommentNodeDataToFile)
	{
		return;
	}

	const auto CachePath = GetCachePath();

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CachePath))
	{
		FString FileData;
		FFileHelper::LoadFileToString(FileData, *CachePath);

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &PackageData, 0, 0))
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Loaded auto size comments cache: %s"), *CachePath);
		}
		else
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Failed to load auto size comments cache: %s"), *CachePath);
		}
	}

	CleanupFiles();
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
	FJsonObjectConverter::UStructToJsonObjectString(PackageData, JsonAsString);
	FFileHelper::SaveStringToFile(JsonAsString, *CachePath);
	const double TimeTaken = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
	UE_LOG(LogAutoSizeComments, Log, TEXT("Saved node cache to %s took %6.2fms"), *CachePath, TimeTaken);
}

void FAutoSizeCommentsCacheFile::DeleteCache()
{
	FString CachePath = GetCachePath();

	if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*CachePath))
	{
		UE_LOG(LogAutoSizeComments, Log, TEXT("Deleted cache file at %s"), *CachePath);
	}
	else
	{
		UE_LOG(LogAutoSizeComments, Log, TEXT("Delete cache failed: Cache file does not exist or is read-only %s"), *CachePath);
	}
}

void FAutoSizeCommentsCacheFile::CleanupFiles()
{
	// Get all assets
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> AllAssets;

	// Get package guids from assets
	TSet<FName> CurrentPackageNames;
	const auto& AssetDataMap = AssetRegistry.GetAssetRegistryState()->GetObjectPathToAssetDataMap();
	for (const TPair<FName, const FAssetData*>& AssetDataPair : AssetDataMap)
	{
		const FAssetData* AssetData = AssetDataPair.Value;
		CurrentPackageNames.Add(AssetData->PackageName);
	}

	// Remove missing files
	TArray<FName> OldPackageGuids;
	PackageData.PackageData.GetKeys(OldPackageGuids);
	for (FName PackageGuid : OldPackageGuids)
	{
		if (!CurrentPackageNames.Contains(PackageGuid))
		{
			PackageData.PackageData.Remove(PackageGuid);
		}
	}
}

void FAutoSizeCommentsCacheFile::UpdateComment(UEdGraphNode_Comment* Comment)
{
	if (!Comment)
	{
		UE_LOG(LogAutoSizeComments, Warning, TEXT("Tried to update null comment"));
		return;
	}

	if (!FASCState::Get().HasRegisteredComment(Comment))
	{
		return;
	}

	UEdGraph* Graph = Comment->GetGraph();
	if (!Graph)
	{
		UE_LOG(LogAutoSizeComments, Warning, TEXT("Tried to update comment with null graph"));
		return;
	}

	FASCCommentData& GraphData = GetGraphData(Graph);

	if (FASCUtils::HasNodeBeenDeleted(Comment))
	{
		GraphData.CommentData.Remove(Comment->NodeGuid);
	}
	else
	{
		FASCNodesInside& NodesInside = GraphData.CommentData.FindOrAdd(Comment->NodeGuid);
		NodesInside.NodeGuids.Empty();

		// update cache file
		const TArray<UEdGraphNode*> Nodes = FASCUtils::GetNodesUnderComment(Comment);
		for (UEdGraphNode* Node : Nodes)
		{
			if (!FASCUtils::HasNodeBeenDeleted(Node))
			{
				NodesInside.NodeGuids.Add(Node->NodeGuid);
			}
		}
	}
}

FASCCommentData& FAutoSizeCommentsCacheFile::GetGraphData(UEdGraph* Graph)
{
	UPackage* Package = Graph->GetOutermost();

	FASCGraphData& CacheData = PackageData.PackageData.FindOrAdd(Package->GetFName());

	return CacheData.GraphData.FindOrAdd(Graph->GraphGuid);
}

FString FAutoSizeCommentsCacheFile::GetCachePath()
{
	const FString PluginDir = IPluginManager::Get().FindPlugin("AutoSizeComments")->GetBaseDir();

	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
	const FGuid& ProjectID = ProjectSettings->ProjectID;

	return PluginDir + "/ASCCache/" + ProjectID.ToString() + ".json";
}

bool FAutoSizeCommentsCacheFile::GetNodesUnderComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode, TArray<UEdGraphNode*>& OutNodesUnderComment)
{
	UEdGraphNode* Node = ASCNode->GetNodeObj();
	UEdGraph* Graph = Node->GetGraph();
	TSharedPtr<SGraphPanel> GraphPanel = ASCNode->GetOwnerPanel();
	FASCCommentData& Data = GetGraphData(Graph);
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

void FAutoSizeCommentsCacheFile::PrintCache()
{
	for (auto& Package : PackageData.PackageData)
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

void FASCCommentData::CleanupGraph(UEdGraph* Graph)
{
	// Get all the current nodes and pins from the graph
	TSet<FGuid> CurrentNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		CurrentNodes.Add(Node->NodeGuid);
	}

	// Remove any missing guids from the cached comments nodes
	TArray<FGuid> CachedNodeGuids;
	CommentData.GetKeys(CachedNodeGuids);

	TArray<FGuid> NodesToRemove;
	for (auto& Elem : CommentData)
	{
		FGuid CommentNode = Elem.Key;
		if (!CurrentNodes.Contains(CommentNode))
		{
			NodesToRemove.Add(CommentNode);
			break;
		}
		auto ContainingNodes = Elem.Value.NodeGuids;
		for (auto Node : ContainingNodes)
		{
			if (!CurrentNodes.Contains(Node))
			{
				Elem.Value.NodeGuids.Remove(Node);
			}
		}
	}

	for (FGuid NodeGuid : NodesToRemove)
	{
		CommentData.Remove(NodeGuid);
	}
}