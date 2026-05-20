// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECADMXMutationCommands.h"

#if WITH_ECA_DMX

#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXEntityReference.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace
{
	UDMXLibrary* LoadDmxLibrary(const FString& Path)
	{
		FSoftObjectPath Soft(Path);
		return Soft.IsValid() ? Cast<UDMXLibrary>(Soft.TryLoad()) : nullptr;
	}

	UDMXEntityFixtureType* LoadFixtureType(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		FSoftObjectPath Soft(Path);
		return Soft.IsValid() ? Cast<UDMXEntityFixtureType>(Soft.TryLoad()) : nullptr;
	}

	bool SavePackageToDisk(UPackage* Package, UObject* Asset, FString& OutFilename, FString& OutError)
	{
		if (!Package || !Asset)
		{
			OutError = TEXT("Null package or asset passed to SavePackageToDisk");
			return false;
		}
		const FString PackageName = Package->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		OutFilename = Filename;

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GError;

		const FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *Filename, Args);
		if (Result.Result != ESavePackageResult::Success)
		{
			OutError = FString::Printf(TEXT("UPackage::Save failed with result code %d for '%s'."), (int32)Result.Result, *PackageName);
			return false;
		}
		return true;
	}
}

FECACommandResult FECACommand_CreateDmxLibrary::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString PackagePath;
	if (!GetStringParam(Params, TEXT("package_path"), PackagePath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("package_path is required"));
	}

	bool bOverwrite = false;
	GetBoolParam(Params, TEXT("overwrite"), bOverwrite, false);

	if (!FPackageName::IsValidLongPackageName(PackagePath))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("'%s' is not a valid long package name (e.g. /Game/Foo/Bar)."), *PackagePath));
	}

	const FString AssetName = FPackageName::GetShortName(PackagePath);
	const FString Filename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

	if (!bOverwrite && FPaths::FileExists(Filename))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Asset already exists at '%s'. Pass overwrite=true to replace."), *Filename));
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("CreatePackage('%s') returned null."), *PackagePath));
	}
	Package->FullyLoad();

	UDMXLibrary* Library = NewObject<UDMXLibrary>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	if (!Library)
	{
		return FECACommandResult::Error(TEXT("NewObject<UDMXLibrary> returned null."));
	}

	FAssetRegistryModule::AssetCreated(Library);
	Package->MarkPackageDirty();

	FString SaveError;
	FString SavedTo;
	if (!SavePackageToDisk(Package, Library, SavedTo, SaveError))
	{
		return FECACommandResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), Library->GetPathName());
	Result->SetStringField(TEXT("package_path"), PackagePath);
	Result->SetStringField(TEXT("asset_name"), AssetName);
	Result->SetStringField(TEXT("filename"), SavedTo);
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_CreateDmxLibrary);

FECACommandResult FECACommand_AddDmxFixture::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString LibraryPath;
	if (!GetStringParam(Params, TEXT("library_path"), LibraryPath, true))
	{
		return FECACommandResult::ValidationError(this, TEXT("library_path is required"));
	}

	UDMXLibrary* Library = LoadDmxLibrary(LibraryPath);
	if (!Library)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Failed to load UDMXLibrary at '%s'."), *LibraryPath));
	}

	int32 UniverseId = 1;
	GetIntParam(Params, TEXT("universe_id"), UniverseId, false);
	int32 StartingChannel = 1;
	GetIntParam(Params, TEXT("starting_channel"), StartingChannel, false);
	int32 ActiveMode = 0;
	GetIntParam(Params, TEXT("active_mode"), ActiveMode, false);

	FString FixtureName;
	GetStringParam(Params, TEXT("fixture_name"), FixtureName, false);

	FString FixtureTypePath;
	GetStringParam(Params, TEXT("fixture_type_path"), FixtureTypePath, false);

	bool bSave = true;
	GetBoolParam(Params, TEXT("save"), bSave, false);

	UDMXEntityFixtureType* FixtureType = LoadFixtureType(FixtureTypePath);
	bool bCreatedFixtureType = false;
	if (!FixtureType)
	{
		FDMXEntityFixtureTypeConstructionParams TypeParams;
		TypeParams.ParentDMXLibrary = Library;
		const FString TypeName = FixtureName.IsEmpty() ? TEXT("DefaultFixtureType") : FixtureName + TEXT("_Type");
		FixtureType = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(TypeParams, TypeName);
		if (!FixtureType)
		{
			return FECACommandResult::Error(TEXT("CreateFixtureTypeInLibrary returned null."));
		}
		bCreatedFixtureType = true;
	}

	FDMXEntityFixturePatchConstructionParams PatchParams;
	PatchParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureType);
	PatchParams.UniverseID = UniverseId;
	PatchParams.StartingAddress = StartingChannel;
	PatchParams.ActiveMode = ActiveMode;

	UDMXEntityFixturePatch* Patch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams, FixtureName);
	if (!Patch)
	{
		return FECACommandResult::Error(TEXT("CreateFixturePatchInLibrary returned null."));
	}

	UPackage* Package = Library->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	FString SavedTo;
	FString SaveError;
	bool bSaved = false;
	if (bSave && Package)
	{
		bSaved = SavePackageToDisk(Package, Library, SavedTo, SaveError);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("library_path"), Library->GetPathName());
	Result->SetStringField(TEXT("patch_id"), Patch->GetID().ToString(EGuidFormats::DigitsWithHyphens));
	Result->SetStringField(TEXT("patch_name"), Patch->GetDisplayName());
	Result->SetStringField(TEXT("fixture_type_path"), FixtureType->GetPathName());
	Result->SetBoolField(TEXT("created_fixture_type"), bCreatedFixtureType);
	Result->SetNumberField(TEXT("universe_id"), Patch->GetUniverseID());
	Result->SetNumberField(TEXT("starting_channel"), Patch->GetStartingChannel());
	Result->SetBoolField(TEXT("saved"), bSaved);
	if (bSave && !bSaved)
	{
		Result->SetStringField(TEXT("save_error"), SaveError);
	}
	if (bSaved)
	{
		Result->SetStringField(TEXT("filename"), SavedTo);
	}
	return FECACommandResult::Success(Result);
}

REGISTER_ECA_COMMAND(FECACommand_AddDmxFixture);

#endif // WITH_ECA_DMX
