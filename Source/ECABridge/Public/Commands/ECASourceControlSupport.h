// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

/**
 * Shared helpers for the source-control command set. Each command in the
 * SourceControl category converts /Game/... asset paths to absolute .uasset
 * filenames before handing them to ISourceControlProvider; this header avoids
 * duplicating that bit of plumbing across every cpp.
 */
namespace ECASourceControlSupport
{
	/** Strip a trailing object-name suffix (".Foo") and convert /Game/Pkg to an absolute
	 *  filename. Tries the asset extension first, then the map extension. If neither file
	 *  exists yet (the mark_for_add case) the asset-extension path is returned anyway so
	 *  the caller can pass it through to SCC. */
	inline bool ResolveAssetPathToFilename(const FString& AssetPath, FString& OutFilename, FString& OutError)
	{
		FString PackageName = AssetPath;
		int32 DotIdx;
		if (PackageName.FindChar(TEXT('.'), DotIdx))
		{
			PackageName = PackageName.Left(DotIdx);
		}

		FString Filename;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension()))
		{
			const FString FullAsset = FPaths::ConvertRelativePathToFull(Filename);
			if (FPaths::FileExists(FullAsset))
			{
				OutFilename = FullAsset;
				return true;
			}

			FString MapFilename;
			if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, MapFilename, FPackageName::GetMapPackageExtension()))
			{
				const FString FullMap = FPaths::ConvertRelativePathToFull(MapFilename);
				if (FPaths::FileExists(FullMap))
				{
					OutFilename = FullMap;
					return true;
				}
			}

			// Neither file is on disk yet (e.g. mark_for_add for a deleted file). Return the
			// asset-extension path so the SCC operation can still receive a usable filename.
			OutFilename = FullAsset;
			return true;
		}

		OutError = FString::Printf(TEXT("Could not resolve package '%s' to a filename."), *AssetPath);
		return false;
	}

	/** Pull either `asset_path` (string) or `asset_paths` (array) out of Params and
	 *  return the union. Returns false with OutError set when neither is provided. */
	inline bool GetAssetPathsParam(const TSharedPtr<FJsonObject>& Params, TArray<FString>& OutPaths, FString& OutError)
	{
		if (!Params.IsValid())
		{
			OutError = TEXT("Missing parameters.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* ArrayField = nullptr;
		if (Params->TryGetArrayField(TEXT("asset_paths"), ArrayField) && ArrayField)
		{
			for (const TSharedPtr<FJsonValue>& Val : *ArrayField)
			{
				FString Str;
				if (Val.IsValid() && Val->TryGetString(Str) && !Str.IsEmpty())
				{
					OutPaths.Add(Str);
				}
			}
		}

		FString Single;
		if (Params->TryGetStringField(TEXT("asset_path"), Single) && !Single.IsEmpty())
		{
			OutPaths.Add(Single);
		}

		if (OutPaths.Num() == 0)
		{
			OutError = TEXT("Provide 'asset_path' (string) or 'asset_paths' (string array).");
			return false;
		}
		return true;
	}

	/** Resolve a batch of asset paths to filenames, accumulating any unresolved entries. */
	inline void ResolveAssetPathBatch(
		const TArray<FString>& AssetPaths,
		TArray<FString>& OutFilenames,
		TArray<TSharedPtr<FJsonValue>>& OutUnresolved)
	{
		for (const FString& AssetPath : AssetPaths)
		{
			FString Filename;
			FString Err;
			if (ResolveAssetPathToFilename(AssetPath, Filename, Err))
			{
				OutFilenames.Add(Filename);
			}
			else
			{
				TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("asset_path"), AssetPath);
				Entry->SetStringField(TEXT("error"), Err);
				OutUnresolved.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}
	}
}
