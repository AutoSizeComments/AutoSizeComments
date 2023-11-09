// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsCacheFile.h"

#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "GeneralProjectSettings.h"
#include "JsonObjectConverter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/LazySingleton.h"
#include "UObject/MetaData.h"

static FName NAME_ASC_GRAPH_DATA = FName("ASCGraphData");

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
	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
		AssetRegistryModule->Get().OnFilesLoaded().AddRaw(this, &FAutoSizeCommentsCacheFile::LoadCacheFromFile);
	}

	FCoreDelegates::OnPreExit.AddRaw(this, &FAutoSizeCommentsCacheFile::SaveCacheToFile);
	FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FAutoSizeCommentsCacheFile::OnObjectLoaded);
}

void FAutoSizeCommentsCacheFile::Cleanup()
{
	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
		AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
	}

	FCoreDelegates::OnPreExit.RemoveAll(this);
	FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);
}

void FAutoSizeCommentsCacheFile::LoadCacheFromFile()
{
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

FASCCacheData FAutoSizeCommentsCacheFile::CreateCacheFromFile()
{
	FASCCacheData NewCacheData;

	const FString CachePath = GetCachePath();
	const FString OldCachePath = GetAlternateCachePath();

	FString FileData;
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CachePath))
	{
		FFileHelper::LoadFileToString(FileData, *CachePath);

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &NewCacheData, 0, 0))
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

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &NewCacheData, 0, 0))
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Loaded auto size comments cache from old cache path: %s"), *GetAlternateCachePath(true));
		}
		else
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("Failed to load auto size comments cache from old cache path: %s"), *GetAlternateCachePath(true));
		}
	}

	return NewCacheData;
}

void FAutoSizeCommentsCacheFile::InitMetaData()
{
	
}

void FAutoSizeCommentsCacheFile::SaveCacheToFile()
{
	if (UAutoSizeCommentsSettings::Get().CacheSaveMethod != EASCCacheSaveMethod::File)
	{
		return;
	}

	// Don't save the cache while cooking 
	if (GIsCookerLoadingPackage)
	{
		return;
	}

	for (UEdGraph* Graph : FAutoSizeCommentGraphHandler::Get().GetActiveGraphs())
	{
		FASCGraphData& CacheGraphData = GetGraphData(Graph);
		CacheGraphData.CleanupGraph(Graph);
	}

	const double StartTime = FPlatformTime::Seconds();

	const auto CachePath = GetCachePath();

	// Write data to file
	FString JsonAsString;
	FJsonObjectConverter::UStructToJsonObjectString(CacheData, JsonAsString, 0, 0, 0, nullptr, UAutoSizeCommentsSettings::Get().bPrettyPrintCommentCacheJSON);
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
	if (UAutoSizeCommentsSettings::Get().bDisablePackageCleanup)
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

FASCCommentData& FAutoSizeCommentsCacheFile::GetCommentData(UEdGraphNode_Comment* Comment)
{
	return GetGraphData(Comment->GetGraph()).GetCommentData(Comment);
}

FASCGraphData& FAutoSizeCommentsCacheFile::GetGraphData(UEdGraph* Graph)
{
	check(Graph);

	// load from cache file class
	if (UAutoSizeCommentsSettings::Get().CacheSaveMethod == EASCCacheSaveMethod::File)
	{
		FASCGraphData& GraphData = GetCacheFileGraphData(Graph);
		if (GraphData.bInitialized)
		{
			return GraphData;
		}

		GraphData.bInitialized = true;

		// if we have no data, try loading from the package's meta data
		if (GraphData.IsEmpty())
		{
			GraphData.LoadFromPackageMetaData(Graph);
			GraphData.bInitialized = true;
		}

		return GraphData;
	}

	// load and store from package metadata
	{
		FASCGraphHandlerData& GraphHandlerData = FAutoSizeCommentGraphHandler::Get().GetGraphHandlerData(Graph);
		FASCGraphData& GraphData = GraphHandlerData.GraphCacheData;
		if (GraphData.bInitialized)
		{
			return GraphData;
		}

		if (GraphData.LoadFromPackageMetaData(Graph))
		{
			GraphData.bInitialized = true;
			return GraphData;
		}

		// we failed to load from the package meta data, load from the cache file
		GraphData = GetCacheFileGraphData(Graph);
		GraphData.bInitialized = true;

		// write any data we got from the cache file to the meta data
		GraphData.SaveToPackageMetaData(Graph);

		return GraphData;
	}
}

bool FAutoSizeCommentsCacheFile::RemoveGraphData(UEdGraph* Graph)
{
	UPackage* Package = Graph->GetOutermost();
	FASCPackageData& PackageData = CacheData.PackageData.FindOrAdd(Package->GetFName());
	return PackageData.GraphData.Remove(Graph->GraphGuid) > 0;
}

FASCPackageData* FAutoSizeCommentsCacheFile::FindPackageData(UPackage* Package)
{
	return CacheData.PackageData.Find(Package->GetFName());
}

void FAutoSizeCommentsCacheFile::ClearPackageMetaData(UEdGraph* Graph)
{
	if (UPackage* AssetPackage = Graph->GetPackage())
	{
		if (UMetaData* MetaData = AssetPackage->GetMetaData())
		{
			MetaData->RemoveValue(Graph, NAME_ASC_GRAPH_DATA);
		}
	}
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
	const bool bIsProject = UAutoSizeCommentsSettings::Get().CacheSaveLocation == EASCCacheSaveLocation::Project;
	return bIsProject ? GetProjectCachePath(bFullPath) : GetPluginCachePath(bFullPath);
}

FString FAutoSizeCommentsCacheFile::GetAlternateCachePath(bool bFullPath)
{
	const bool bIsProject = UAutoSizeCommentsSettings::Get().CacheSaveLocation == EASCCacheSaveLocation::Project;
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

void FAutoSizeCommentsCacheFile::OnObjectLoaded(UObject* Obj)
{
	// when a package is reloaded, we want to make the comment read the latest
	// data from this cache (for when you revert commit or file)
	if (FASCPackageData* PackageData = FindPackageData(Obj->GetPackage()))
	{
		for (auto& GraphData : PackageData->GraphData)
		{
			GraphData.Value.bInitialized = false;
		}
	}
}

FASCGraphData& FAutoSizeCommentsCacheFile::GetCacheFileGraphData(UEdGraph* Graph)
{
	UPackage* Package = Graph->GetOutermost();
	FASCPackageData& PackageData = CacheData.PackageData.FindOrAdd(Package->GetFName());
	FASCGraphData& GraphData = PackageData.GraphData.FindOrAdd(Graph->GraphGuid);
	return GraphData;
}

void FAutoSizeCommentsCacheFile::OnPreExit()
{
	if (UAutoSizeCommentsSettings::Get().bSaveCommentDataOnExit)
	{
		SaveCacheToFile();
	}
}

void FASCCommentData::UpdateNodesUnderComment(UEdGraphNode_Comment* Comment)
{
	if (!Comment)
	{
		return;
	}

	const TArray<UEdGraphNode*> NodesUnder = FASCUtils::GetNodesUnderComment(Comment);
	NodeGuids.Reset(NodesUnder.Num());

	// update nodes under
	for (UEdGraphNode* Node : NodesUnder)
	{
		if (!FASCUtils::HasNodeBeenDeleted(Node))
		{
			NodeGuids.Add(Node->NodeGuid);
		}
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
			continue;
		}

		TArray<FGuid>& ContainingNodes = Elem.Value.NodeGuids;
		for (int i = ContainingNodes.Num() - 1; i >= 0; --i)
		{
			const FGuid& Node = ContainingNodes[i];
			if (!CurrentNodes.Contains(Node))
			{
				ContainingNodes.RemoveAt(i);
			}
		}
	}

	for (const FGuid& NodeGuid : NodesToRemove)
	{
		CommentData.Remove(NodeGuid);
	}
}

bool FASCGraphData::LoadFromPackageMetaData(UEdGraph* Graph)
{
	if (!Graph)
	{
		return false;
	}

	if (UPackage* AssetPackage = Graph->GetPackage())
	{
		if (UMetaData* MetaData = AssetPackage->GetMetaData())
		{
			if (const FString* GraphDataAsString = MetaData->FindValue(Graph, NAME_ASC_GRAPH_DATA))
			{
				if (FJsonObjectConverter::JsonObjectStringToUStruct(*GraphDataAsString, this, 0, 0))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FASCGraphData::SaveToPackageMetaData(UEdGraph* Graph)
{
	if (!Graph)
	{
		return;
	}

	if (UPackage* AssetPackage = Graph->GetPackage())
	{
		if (UMetaData* MetaData = AssetPackage->GetMetaData())
		{
			CleanupGraph(Graph);

			FString GraphDataAsString;
			if (FJsonObjectConverter::UStructToJsonObjectString(*this, GraphDataAsString))
			{
				MetaData->SetValue(Graph, NAME_ASC_GRAPH_DATA, *GraphDataAsString);
			}

			MetaData->RemoveMetaDataOutsidePackage();
		}
	}
}

FASCCommentData& FASCGraphData::GetCommentData(UEdGraphNode_Comment* Comment)
{
	check(Comment);
	return CommentData.FindOrAdd(Comment->NodeGuid);
}
