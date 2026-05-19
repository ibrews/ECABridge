// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECADMXCommands.h"

#if WITH_ECA_DMX

#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"

namespace
{
	UDMXLibrary* LoadLibraryByPath(const FString& Path)
	{
		FSoftObjectPath SoftPath(Path);
		if (!SoftPath.IsValid())
		{
			return nullptr;
		}
		return Cast<UDMXLibrary>(SoftPath.TryLoad());
	}
}

FECACommandResult FECACommand_ListDmxLibraries::Execute(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetData> Assets;
	AR.GetAssetsByClass(UDMXLibrary::StaticClass()->GetClassPathName(), Assets);

	TArray<TSharedPtr<FJsonValue>> Libs;
	Libs.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Entry->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
		Entry->SetStringField(TEXT("asset_path"), Asset.GetObjectPathString());

		// Load to count patches. AssetRegistry can lazy-load but for patch_count
		// we need the live UObject.
		int32 PatchCount = 0;
		if (UDMXLibrary* Lib = Cast<UDMXLibrary>(Asset.GetAsset()))
		{
			PatchCount = Lib->GetEntitiesTypeCast<UDMXEntityFixturePatch>().Num();
		}
		Entry->SetNumberField(TEXT("patch_count"), PatchCount);

		Libs.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Result->SetNumberField(TEXT("count"), Libs.Num());
	Result->SetArrayField(TEXT("libraries"), Libs);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_ListDmxLibraries);

FECACommandResult FECACommand_DumpDmxPatch::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString LibraryPath;
	if (!GetStringParam(Params, TEXT("library_path"), LibraryPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("library_path is required"));
	}

	UDMXLibrary* Library = LoadLibraryByPath(LibraryPath);
	if (!Library)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load UDMXLibrary at '%s'."), *LibraryPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("library_path"), LibraryPath);
	Result->SetStringField(TEXT("library_name"), Library->GetName());

	TArray<UDMXEntityFixturePatch*> Patches = Library->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	TArray<TSharedPtr<FJsonValue>> PatchArr;
	PatchArr.Reserve(Patches.Num());
	for (UDMXEntityFixturePatch* Patch : Patches)
	{
		if (!Patch) continue;
		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"), Patch->GetDisplayName());
		P->SetStringField(TEXT("entity_id"), Patch->GetID().ToString(EGuidFormats::DigitsWithHyphens));
		P->SetNumberField(TEXT("universe_id"), Patch->GetUniverseID());
		P->SetNumberField(TEXT("starting_channel"), Patch->GetStartingChannel());

		FString FixtureTypeName;
		if (UDMXEntityFixtureType* FT = Patch->GetFixtureType())
		{
			FixtureTypeName = FT->GetDisplayName();
		}
		P->SetStringField(TEXT("fixture_type"), FixtureTypeName);

		PatchArr.Add(MakeShared<FJsonValueObject>(P));
	}

	Result->SetNumberField(TEXT("patch_count"), PatchArr.Num());
	Result->SetArrayField(TEXT("patches"), PatchArr);

	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_DumpDmxPatch);

#endif // WITH_ECA_DMX
