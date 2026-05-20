// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAMaterialParameterCollectionCommands.h"
#include "Commands/ECACommand.h"

#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

REGISTER_ECA_COMMAND(FECACommand_CreateMPC)
REGISTER_ECA_COMMAND(FECACommand_AddMPCScalarParameter)
REGISTER_ECA_COMMAND(FECACommand_AddMPCVectorParameter)
REGISTER_ECA_COMMAND(FECACommand_DumpMPC)
REGISTER_ECA_COMMAND(FECACommand_SetMPCScalarRuntime)
REGISTER_ECA_COMMAND(FECACommand_SetMPCVectorRuntime)

namespace ECAMPCHelpers
{
	static UMaterialParameterCollection* LoadMPC(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, *Path);
		if (MPC) return MPC;

		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			MPC = LoadObject<UMaterialParameterCollection>(nullptr, *FullPath);
		}
		return MPC;
	}

	static FLinearColor JsonToLinearColor(const TSharedPtr<FJsonObject>& Obj, const FLinearColor& Fallback)
	{
		if (!Obj.IsValid()) return Fallback;
		FLinearColor C = Fallback;
		double D = 0.0;
		if (Obj->TryGetNumberField(TEXT("r"), D)) C.R = static_cast<float>(D);
		if (Obj->TryGetNumberField(TEXT("g"), D)) C.G = static_cast<float>(D);
		if (Obj->TryGetNumberField(TEXT("b"), D)) C.B = static_cast<float>(D);
		if (Obj->TryGetNumberField(TEXT("a"), D)) C.A = static_cast<float>(D);
		return C;
	}

	static TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& C)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("r"), C.R);
		Obj->SetNumberField(TEXT("g"), C.G);
		Obj->SetNumberField(TEXT("b"), C.B);
		Obj->SetNumberField(TEXT("a"), C.A);
		return Obj;
	}
}

//==============================================================================
// create_mpc
//==============================================================================
FECACommandResult FECACommand_CreateMPC::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	FString Name;
	if (!GetStringParam(Params, TEXT("path"), Path)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: path"));
	if (!GetStringParam(Params, TEXT("name"), Name)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: name"));

	bool bOverwrite = false;
	GetBoolParam(Params, TEXT("overwrite"), bOverwrite, false);

	if (!Path.EndsWith(TEXT("/"))) { Path += TEXT("/"); }
	const FString PackagePath = Path + Name;

	UMaterialParameterCollection* ExistingMPC = nullptr;
	if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *PackagePath))
	{
		ExistingMPC = Cast<UMaterialParameterCollection>(Existing);
		if (!ExistingMPC)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Asset exists at '%s' but is %s, not UMaterialParameterCollection"), *PackagePath, *Existing->GetClass()->GetName()));
		}
		if (!bOverwrite)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("UMaterialParameterCollection already exists at '%s'. Pass overwrite=true."), *PackagePath));
		}
	}

	UMaterialParameterCollection* MPC = ExistingMPC;
	if (!MPC)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package) return FECACommandResult::Error(TEXT("Failed to create package"));

		MPC = NewObject<UMaterialParameterCollection>(Package, *Name, RF_Public | RF_Standalone);
		if (!MPC) return FECACommandResult::Error(TEXT("Failed to create UMaterialParameterCollection"));
		FAssetRegistryModule::AssetCreated(MPC);
	}
	MPC->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), MPC->GetPathName());
	Result->SetBoolField  (TEXT("created"), ExistingMPC == nullptr);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// add_mpc_scalar_parameter
//==============================================================================
FECACommandResult FECACommand_AddMPCScalarParameter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString MPCPath, ParamName;
	if (!GetStringParam(Params, TEXT("mpc_path"),  MPCPath))   return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mpc_path"));
	if (!GetStringParam(Params, TEXT("parameter"), ParamName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: parameter"));

	double DefaultValue = 0.0;
	GetFloatParam(Params, TEXT("default_value"), DefaultValue, false);

	UMaterialParameterCollection* MPC = ECAMPCHelpers::LoadMPC(MPCPath);
	if (!MPC) return FECACommandResult::Error(FString::Printf(TEXT("Could not load MPC at: %s"), *MPCPath));

	const FName ParamFName(*ParamName);

	bool bAdded = true;
	const int32 ExistingIdx = MPC->ScalarParameters.IndexOfByPredicate(
		[&ParamFName](const FCollectionScalarParameter& P) { return P.ParameterName == ParamFName; });
	if (ExistingIdx != INDEX_NONE)
	{
		MPC->ScalarParameters[ExistingIdx].DefaultValue = static_cast<float>(DefaultValue);
		bAdded = false;
	}
	else
	{
		FCollectionScalarParameter NewParam;
		NewParam.ParameterName = ParamFName;
		NewParam.DefaultValue  = static_cast<float>(DefaultValue);
		MPC->ScalarParameters.Add(NewParam);
	}
	MPC->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("parameter"), ParamName);
	Result->SetBoolField  (TEXT("added"),     bAdded);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// add_mpc_vector_parameter
//==============================================================================
FECACommandResult FECACommand_AddMPCVectorParameter::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString MPCPath, ParamName;
	if (!GetStringParam(Params, TEXT("mpc_path"),  MPCPath))   return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mpc_path"));
	if (!GetStringParam(Params, TEXT("parameter"), ParamName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: parameter"));

	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	FLinearColor Default(0.f, 0.f, 0.f, 1.f);
	if (GetObjectParam(Params, TEXT("default_value"), ColorObj, false))
	{
		Default = ECAMPCHelpers::JsonToLinearColor(*ColorObj, Default);
	}

	UMaterialParameterCollection* MPC = ECAMPCHelpers::LoadMPC(MPCPath);
	if (!MPC) return FECACommandResult::Error(FString::Printf(TEXT("Could not load MPC at: %s"), *MPCPath));

	const FName ParamFName(*ParamName);

	bool bAdded = true;
	const int32 ExistingIdx = MPC->VectorParameters.IndexOfByPredicate(
		[&ParamFName](const FCollectionVectorParameter& P) { return P.ParameterName == ParamFName; });
	if (ExistingIdx != INDEX_NONE)
	{
		MPC->VectorParameters[ExistingIdx].DefaultValue = Default;
		bAdded = false;
	}
	else
	{
		FCollectionVectorParameter NewParam;
		NewParam.ParameterName = ParamFName;
		NewParam.DefaultValue  = Default;
		MPC->VectorParameters.Add(NewParam);
	}
	MPC->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("parameter"), ParamName);
	Result->SetBoolField  (TEXT("added"),     bAdded);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_mpc
//==============================================================================
FECACommandResult FECACommand_DumpMPC::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString MPCPath;
	if (!GetStringParam(Params, TEXT("mpc_path"), MPCPath)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mpc_path"));

	UMaterialParameterCollection* MPC = ECAMPCHelpers::LoadMPC(MPCPath);
	if (!MPC) return FECACommandResult::Error(FString::Printf(TEXT("Could not load MPC at: %s"), *MPCPath));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), MPC->GetPathName());

	TArray<TSharedPtr<FJsonValue>> Scalars;
	for (const FCollectionScalarParameter& P : MPC->ScalarParameters)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("parameter"),     P.ParameterName.ToString());
		Obj->SetNumberField(TEXT("default_value"), P.DefaultValue);
		Obj->SetStringField(TEXT("id"),            P.Id.ToString());
		Scalars.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Result->SetArrayField(TEXT("scalars"), Scalars);

	TArray<TSharedPtr<FJsonValue>> Vectors;
	for (const FCollectionVectorParameter& P : MPC->VectorParameters)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("parameter"),     P.ParameterName.ToString());
		Obj->SetObjectField(TEXT("default_value"), ECAMPCHelpers::LinearColorToJson(P.DefaultValue));
		Obj->SetStringField(TEXT("id"),            P.Id.ToString());
		Vectors.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Result->SetArrayField(TEXT("vectors"), Vectors);

	return FECACommandResult::Success(Result);
}

//==============================================================================
// set_mpc_scalar_runtime
//==============================================================================
FECACommandResult FECACommand_SetMPCScalarRuntime::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString MPCPath, ParamName;
	double Value = 0.0;
	if (!GetStringParam(Params, TEXT("mpc_path"),  MPCPath))   return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mpc_path"));
	if (!GetStringParam(Params, TEXT("parameter"), ParamName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: parameter"));
	if (!GetFloatParam (Params, TEXT("value"),     Value))     return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: value"));

	UMaterialParameterCollection* MPC = ECAMPCHelpers::LoadMPC(MPCPath);
	if (!MPC) return FECACommandResult::Error(FString::Printf(TEXT("Could not load MPC at: %s"), *MPCPath));

	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(MPC);
	if (!Instance) return FECACommandResult::Error(TEXT("Could not get UMaterialParameterCollectionInstance for editor world"));

	if (!Instance->SetScalarParameterValue(FName(*ParamName), static_cast<float>(Value)))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Scalar parameter '%s' not found on MPC '%s'"), *ParamName, *MPC->GetName()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("parameter"), ParamName);
	Result->SetNumberField(TEXT("value"),     Value);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// set_mpc_vector_runtime
//==============================================================================
FECACommandResult FECACommand_SetMPCVectorRuntime::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World) return FECACommandResult::Error(TEXT("No editor world available"));

	FString MPCPath, ParamName;
	if (!GetStringParam(Params, TEXT("mpc_path"),  MPCPath))   return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: mpc_path"));
	if (!GetStringParam(Params, TEXT("parameter"), ParamName)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: parameter"));

	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (!GetObjectParam(Params, TEXT("value"), ColorObj)) return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: value (object {r,g,b,a})"));
	const FLinearColor Color = ECAMPCHelpers::JsonToLinearColor(*ColorObj, FLinearColor::Black);

	UMaterialParameterCollection* MPC = ECAMPCHelpers::LoadMPC(MPCPath);
	if (!MPC) return FECACommandResult::Error(FString::Printf(TEXT("Could not load MPC at: %s"), *MPCPath));

	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(MPC);
	if (!Instance) return FECACommandResult::Error(TEXT("Could not get UMaterialParameterCollectionInstance for editor world"));

	if (!Instance->SetVectorParameterValue(FName(*ParamName), Color))
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Vector parameter '%s' not found on MPC '%s'"), *ParamName, *MPC->GetName()));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("parameter"), ParamName);
	Result->SetObjectField(TEXT("value"),     ECAMPCHelpers::LinearColorToJson(Color));
	return FECACommandResult::Success(Result);
}
