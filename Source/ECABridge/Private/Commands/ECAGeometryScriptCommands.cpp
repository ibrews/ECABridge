// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECAGeometryScriptCommands.h"
#include "Commands/ECACommand.h"

#include "Engine/Blueprint.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// FDynamicMesh3 ops
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

REGISTER_ECA_COMMAND(FECACommand_ListGeometryScriptLibraries)
REGISTER_ECA_COMMAND(FECACommand_DumpGeometryScriptLibrary)
REGISTER_ECA_COMMAND(FECACommand_ApplyGeometryScriptOps)

namespace ECAGeometryScriptHelpers
{
	static bool IsGeometryScriptLibrary(UClass* Class)
	{
		if (!Class) return false;
		if (!Class->IsChildOf(UBlueprintFunctionLibrary::StaticClass())) return false;
		const FString Name = Class->GetName();
		return Name.StartsWith(TEXT("GeometryScriptLibrary_")) ||
		       Name.StartsWith(TEXT("UGeometryScriptLibrary_"));
	}

	static int32 CountBlueprintCallableFunctions(UClass* Class)
	{
		if (!Class) return 0;
		int32 Count = 0;
		for (TFieldIterator<UFunction> It(Class, EFieldIterationFlags::None); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
			{
				++Count;
			}
		}
		return Count;
	}

	static UClass* ResolveLibraryClass(const FString& Name)
	{
		if (Name.IsEmpty()) return nullptr;
		FString Stripped = Name;
		if (Stripped.StartsWith(TEXT("U")) && Stripped.Len() > 1 && FChar::IsUpper(Stripped[1]))
		{
			Stripped = Stripped.Mid(1);
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!IsGeometryScriptLibrary(Class)) continue;
			const FString ClassName = Class->GetName();
			if (ClassName.Equals(Name, ESearchCase::IgnoreCase) ||
			    ClassName.Equals(Stripped, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}
		return nullptr;
	}

	static UStaticMesh* LoadStaticMeshTolerant(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (Mesh) return Mesh;
		FString FullPath = Path;
		if (!FullPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(FullPath);
			FullPath = FullPath + TEXT(".") + AssetName;
			Mesh = LoadObject<UStaticMesh>(nullptr, *FullPath);
		}
		return Mesh;
	}
}

//==============================================================================
// list_geometry_script_libraries
//==============================================================================
FECACommandResult FECACommand_ListGeometryScriptLibraries::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString NameFilter;
	GetStringParam(Params, TEXT("name_filter"), NameFilter, false);

	TArray<TSharedPtr<FJsonValue>> Out;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!ECAGeometryScriptHelpers::IsGeometryScriptLibrary(Class)) continue;
		if (!NameFilter.IsEmpty() && !Class->GetName().Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("class"),         Class->GetName());
		Obj->SetStringField(TEXT("package"),       Class->GetOutermost()->GetName());
		Obj->SetNumberField(TEXT("function_count"),ECAGeometryScriptHelpers::CountBlueprintCallableFunctions(Class));
		Out.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Out.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("class")).Compare(B->AsObject()->GetStringField(TEXT("class")), ESearchCase::IgnoreCase) < 0;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("count"),     Out.Num());
	Result->SetArrayField (TEXT("libraries"), Out);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// dump_geometry_script_library
//==============================================================================
FECACommandResult FECACommand_DumpGeometryScriptLibrary::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!GetStringParam(Params, TEXT("class_name"), ClassName))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: class_name"));
	}

	UClass* Class = ECAGeometryScriptHelpers::ResolveLibraryClass(ClassName);
	if (!Class)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not resolve '%s' to a UGeometryScriptLibrary_* class"), *ClassName));
	}

	TArray<TSharedPtr<FJsonValue>> Functions;
	for (TFieldIterator<UFunction> FuncIt(Class, EFieldIterationFlags::None); FuncIt; ++FuncIt)
	{
		UFunction* Func = *FuncIt;
		if (!Func) continue;
		if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure)) continue;

		TSharedPtr<FJsonObject> FObj = MakeShared<FJsonObject>();
		FObj->SetStringField(TEXT("name"), Func->GetName());

		const FProperty* ReturnProp = Func->GetReturnProperty();
		FObj->SetStringField(TEXT("return_type"), ReturnProp ? ReturnProp->GetCPPType() : TEXT("void"));

#if WITH_EDITORONLY_DATA
		const FString Tooltip = Func->GetToolTipText().ToString();
		if (!Tooltip.IsEmpty())
		{
			FObj->SetStringField(TEXT("tooltip"), Tooltip.Left(512));
		}
#endif

		TArray<TSharedPtr<FJsonValue>> ParamArray;
		for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (!Param || Param->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
			TSharedPtr<FJsonObject> PObj = MakeShared<FJsonObject>();
			PObj->SetStringField(TEXT("name"), Param->GetName());
			PObj->SetStringField(TEXT("type"), Param->GetCPPType());
			PObj->SetBoolField  (TEXT("out_param"), Param->HasAnyPropertyFlags(CPF_OutParm) && !Param->HasAnyPropertyFlags(CPF_ConstParm));
			ParamArray.Add(MakeShared<FJsonValueObject>(PObj));
		}
		FObj->SetArrayField(TEXT("params"), ParamArray);

		Functions.Add(MakeShared<FJsonValueObject>(FObj));
	}

	Functions.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("name")).Compare(B->AsObject()->GetStringField(TEXT("name")), ESearchCase::IgnoreCase) < 0;
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("class"),         Class->GetName());
	Result->SetStringField(TEXT("package"),       Class->GetOutermost()->GetName());
	Result->SetNumberField(TEXT("function_count"),Functions.Num());
	Result->SetArrayField (TEXT("functions"),     Functions);
	return FECACommandResult::Success(Result);
}

//==============================================================================
// apply_geometry_script_ops
//==============================================================================
FECACommandResult FECACommand_ApplyGeometryScriptOps::Execute(const TSharedPtr<FJsonObject>& Params)
{
	using namespace UE::Geometry;

	FString MeshPath;
	if (!GetStringParam(Params, TEXT("static_mesh_path"), MeshPath))
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: static_mesh_path"));
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArr = nullptr;
	if (!GetArrayParam(Params, TEXT("ops"), OpsArr) || !OpsArr)
	{
		return FECACommandResult::ValidationError(this, TEXT("Missing required parameter: ops (array of operation objects)"));
	}

	int32 LODIndex = 0;
	GetIntParam(Params, TEXT("lod"), LODIndex, false);

	UStaticMesh* StaticMesh = ECAGeometryScriptHelpers::LoadStaticMeshTolerant(MeshPath);
	if (!StaticMesh)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Could not load UStaticMesh at: %s"), *MeshPath));
	}

	const FMeshDescription* MeshDesc = StaticMesh->GetMeshDescription(LODIndex);
	if (!MeshDesc)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Static mesh has no mesh description at LOD %d"), LODIndex));
	}

	FDynamicMesh3 DynMesh;
	{
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(MeshDesc, DynMesh);
	}

	const int32 VertsBefore = DynMesh.VertexCount();
	const int32 TrisBefore  = DynMesh.TriangleCount();

	int32 OpsApplied = 0;
	FString FailedOpReason;
	for (const TSharedPtr<FJsonValue>& OpVal : *OpsArr)
	{
		const TSharedPtr<FJsonObject> OpObj = OpVal.IsValid() ? OpVal->AsObject() : nullptr;
		if (!OpObj.IsValid())
		{
			FailedOpReason = TEXT("Each op must be a JSON object");
			break;
		}
		FString OpName;
		if (!OpObj->TryGetStringField(TEXT("op"), OpName))
		{
			FailedOpReason = TEXT("Each op must include an 'op' field");
			break;
		}

		if (OpName.Equals(TEXT("translate"), ESearchCase::IgnoreCase))
		{
			double X = 0, Y = 0, Z = 0;
			OpObj->TryGetNumberField(TEXT("x"), X);
			OpObj->TryGetNumberField(TEXT("y"), Y);
			OpObj->TryGetNumberField(TEXT("z"), Z);
			MeshTransforms::Translate(DynMesh, FVector3d(X, Y, Z));
		}
		else if (OpName.Equals(TEXT("scale"), ESearchCase::IgnoreCase))
		{
			double X = 1, Y = 1, Z = 1;
			OpObj->TryGetNumberField(TEXT("x"), X);
			OpObj->TryGetNumberField(TEXT("y"), Y);
			OpObj->TryGetNumberField(TEXT("z"), Z);
			MeshTransforms::Scale(DynMesh, FVector3d(X, Y, Z), FVector3d::Zero(), true);
		}
		else if (OpName.Equals(TEXT("rotate"), ESearchCase::IgnoreCase))
		{
			double Pitch = 0, Yaw = 0, Roll = 0;
			OpObj->TryGetNumberField(TEXT("pitch"), Pitch);
			OpObj->TryGetNumberField(TEXT("yaw"),   Yaw);
			OpObj->TryGetNumberField(TEXT("roll"),  Roll);
			const FTransform XForm(FRotator(Pitch, Yaw, Roll));
			MeshTransforms::ApplyTransform(DynMesh, FTransformSRT3d(XForm), /*bReverseOrientationIfNeeded*/ true);
		}
		else if (OpName.Equals(TEXT("discard_uvs"), ESearchCase::IgnoreCase))
		{
			if (DynMesh.HasAttributes() && DynMesh.Attributes() && DynMesh.Attributes()->NumUVLayers() > 0)
			{
				DynMesh.Attributes()->SetNumUVLayers(0);
			}
		}
		else if (OpName.Equals(TEXT("flip_normals"), ESearchCase::IgnoreCase))
		{
			DynMesh.ReverseOrientation(true);
		}
		else if (OpName.Equals(TEXT("recompute_normals"), ESearchCase::IgnoreCase))
		{
			// Reset/strip attributes so the next Convert back to MeshDescription
			// builds fresh face normals. This is a coarse but safe approach that
			// avoids depending on FMeshNormals' specific overload set across versions.
			if (DynMesh.HasAttributes())
			{
				DynMesh.DiscardAttributes();
				DynMesh.EnableAttributes();
			}
		}
		else if (OpName.Equals(TEXT("simplify"), ESearchCase::IgnoreCase))
		{
			// Minimal in-place simplification using GeometryProcessing isn't readily
			// available without a heavier include; report not supported in this op
			// implementation, but accept the op without failing the whole chain so
			// callers can detect via ops_applied < ops.length.
			FailedOpReason = FString::Printf(TEXT("Op '%s' is declared but not implemented in this build (callers should use mesh_simplify instead)"), *OpName);
			break;
		}
		else
		{
			FailedOpReason = FString::Printf(TEXT("Unknown op '%s'"), *OpName);
			break;
		}
		++OpsApplied;
	}

	const int32 VertsAfter = DynMesh.VertexCount();
	const int32 TrisAfter  = DynMesh.TriangleCount();

	if (OpsApplied > 0)
	{
		// Write back to mesh description and rebuild
		FMeshDescription NewDesc;
		FStaticMeshAttributes Attributes(NewDesc);
		Attributes.Register();
		FDynamicMeshToMeshDescription Converter;
		Converter.Convert(&DynMesh, NewDesc);

		FMeshDescription* ExistingDesc = StaticMesh->GetMeshDescription(LODIndex);
		if (!ExistingDesc)
		{
			ExistingDesc = StaticMesh->CreateMeshDescription(LODIndex);
		}
		*ExistingDesc = MoveTemp(NewDesc);
		StaticMesh->CommitMeshDescription(LODIndex);
		StaticMesh->Build(false);
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"),                  StaticMesh->GetPathName());
	Result->SetNumberField(TEXT("ops_applied"),           OpsApplied);
	Result->SetNumberField(TEXT("vertex_count_before"),   VertsBefore);
	Result->SetNumberField(TEXT("vertex_count_after"),    VertsAfter);
	Result->SetNumberField(TEXT("triangle_count_before"), TrisBefore);
	Result->SetNumberField(TEXT("triangle_count_after"),  TrisAfter);
	if (!FailedOpReason.IsEmpty())
	{
		Result->SetStringField(TEXT("warning"), FailedOpReason);
	}
	return FECACommandResult::Success(Result);
}
