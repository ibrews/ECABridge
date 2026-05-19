// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAPcgSettingsAssetCommands.h"
#include "Commands/ECACommand.h"

#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"

REGISTER_ECA_COMMAND(FECACommand_ExtractPCGSettingsAsset)

namespace ECAPcgSettingsAssetHelpers
{
	static UPCGNode* FindNodeById(UPCGGraph* Graph, const FString& NodeId)
	{
		if (!Graph) return nullptr;
		if (UPCGNode* In = Graph->GetInputNode())
		{
			if (In->GetName() == NodeId) return In;
		}
		if (UPCGNode* Out = Graph->GetOutputNode())
		{
			if (Out->GetName() == NodeId) return Out;
		}
		for (UPCGNode* Node : Graph->GetNodes())
		{
			if (Node && Node->GetName() == NodeId) return Node;
		}
		return nullptr;
	}
}

//==============================================================================
// extract_pcg_settings_asset
//==============================================================================
FECACommandResult FECACommand_ExtractPCGSettingsAsset::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString GraphPath, NodeId, AssetPath;
	if (!GetStringParam(Params, TEXT("graph_path"), GraphPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("graph_path is required"));
	}
	if (!GetStringParam(Params, TEXT("node_id"), NodeId, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("node_id is required"));
	}
	if (!GetStringParam(Params, TEXT("asset_path"), AssetPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("asset_path is required"));
	}
	if (!AssetPath.StartsWith(TEXT("/")))
	{
		return FECACommandResult::Error(TEXT("asset_path must be a full content path starting with '/'"));
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!Graph)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("PCGGraph not found at '%s'"), *GraphPath));
	}
	UPCGNode* Node = ECAPcgSettingsAssetHelpers::FindNodeById(Graph, NodeId);
	if (!Node)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("node_id '%s' not found in graph"), *NodeId));
	}
	UPCGSettings* SourceSettings = Node->GetSettings();
	if (!SourceSettings)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Node '%s' has no settings object to extract"), *NodeId));
	}

	if (StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	FString PackageName, AssetName;
	if (!AssetPath.Split(TEXT("."), &PackageName, &AssetName))
	{
		PackageName = AssetPath;
		AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	}
	if (AssetName.IsEmpty())
	{
		AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to create package '%s'"), *PackageName));
	}

	// Duplicate the inline settings into the new package as a standalone asset
	UPCGSettings* AssetSettings = DuplicateObject<UPCGSettings>(SourceSettings, Package, FName(*AssetName));
	if (!AssetSettings)
	{
		return FECACommandResult::Error(TEXT("DuplicateObject<UPCGSettings> returned null"));
	}
	AssetSettings->SetFlags(RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(AssetSettings);

	// Rewire the source node to a settings-instance referencing the new asset.
	UPCGSettingsInstance* Instance = NewObject<UPCGSettingsInstance>(Graph, UPCGSettingsInstance::StaticClass());
	Instance->SetSettings(AssetSettings);
	Node->SetSettingsInterface(Instance, /*bUpdatePins=*/false);

	// Save both packages: the new settings asset and the (modified) graph
	auto SaveOne = [](UObject* Asset) -> bool
	{
		UPackage* Pkg = Asset ? Asset->GetOutermost() : nullptr;
		if (!Pkg) return false;
		Pkg->MarkPackageDirty();
		const FString Pn = Pkg->GetName();
		const FString FileName = FPackageName::LongPackageNameToFilename(Pn, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Pkg, Asset, *FileName, SaveArgs);
	};
	if (!SaveOne(AssetSettings))
	{
		return FECACommandResult::Error(TEXT("Failed to save new settings asset package"));
	}
	SaveOne(Graph); // best-effort

	TSharedPtr<FJsonObject> Out = MakeResult();
	Out->SetStringField(TEXT("graph_path"), GraphPath);
	Out->SetStringField(TEXT("node_id"), NodeId);
	Out->SetStringField(TEXT("settings_class"), AssetSettings->GetClass()->GetName());
	Out->SetStringField(TEXT("asset_path"), AssetSettings->GetPathName());
	return FECACommandResult::Success(Out);
}
