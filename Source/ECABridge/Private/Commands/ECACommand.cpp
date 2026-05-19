// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECACommand.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

//------------------------------------------------------------------------------
// FECACommandResult
//------------------------------------------------------------------------------

FString FECACommandResult::ToJsonString() const
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), bSuccess);

	if (bSuccess)
	{
		if (ResultData.IsValid())
		{
			Response->SetObjectField(TEXT("result"), ResultData);
		}
	}
	else
	{
		Response->SetStringField(TEXT("error"), ErrorMessage);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	return OutputString;
}

FECACommandResult FECACommandResult::ValidationError(const IECACommand* Command, const FString& Reason)
{
	FECACommandResult Result;
	Result.bSuccess = false;

	if (!Command)
	{
		Result.ErrorMessage = Reason;
		return Result;
	}

	TSharedPtr<FJsonObject> Schema = Command->GetInputSchemaJson();

	FString SchemaJson;
	if (Schema.IsValid())
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SchemaJson);
		FJsonSerializer::Serialize(Schema.ToSharedRef(), Writer);
	}

	Result.ErrorMessage = FString::Printf(
		TEXT("Validation error for '%s': %s\nInput schema: %s"),
		*Command->GetName(),
		*Reason,
		SchemaJson.IsEmpty() ? TEXT("{}") : *SchemaJson);
	return Result;
}

TSharedPtr<FJsonObject> FECACommandResult::MakeImageContent(const TArray64<uint8>& Bytes, const FString& MimeType)
{
	TSharedPtr<FJsonObject> Block = MakeShared<FJsonObject>();
	Block->SetStringField(TEXT("type"), TEXT("image"));
	Block->SetStringField(TEXT("mimeType"), MimeType);
	Block->SetStringField(TEXT("data"), FBase64::Encode(Bytes.GetData(), Bytes.Num()));
	return Block;
}

TSharedPtr<FJsonObject> FECACommandResult::MakeImageContent(const TArray<uint8>& Bytes, const FString& MimeType)
{
	TSharedPtr<FJsonObject> Block = MakeShared<FJsonObject>();
	Block->SetStringField(TEXT("type"), TEXT("image"));
	Block->SetStringField(TEXT("mimeType"), MimeType);
	Block->SetStringField(TEXT("data"), FBase64::Encode(Bytes.GetData(), Bytes.Num()));
	return Block;
}

TSharedPtr<FJsonObject> MakeECAObjectSchema(const TArray<FECASchemaField>& Fields)
{
	TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("type"), TEXT("object"));

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	for (const FECASchemaField& F : Fields)
	{
		TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
		Prop->SetStringField(TEXT("type"), F.Type);
		if (!F.Description.IsEmpty())
		{
			Prop->SetStringField(TEXT("description"), F.Description);
		}
		if (F.Type == TEXT("array") && !F.ItemsType.IsEmpty())
		{
			TSharedPtr<FJsonObject> Items = MakeShared<FJsonObject>();
			Items->SetStringField(TEXT("type"), F.ItemsType);
			Prop->SetObjectField(TEXT("items"), Items);
		}
		Properties->SetObjectField(F.Name, Prop);
	}
	Schema->SetObjectField(TEXT("properties"), Properties);
	return Schema;
}

TSharedPtr<FJsonObject> IECACommand::GetInputSchemaJson() const
{
	TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
	InputSchema->SetStringField(TEXT("type"), TEXT("object"));

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Required;

	for (const FECACommandParam& Param : GetParameters())
	{
		TSharedPtr<FJsonObject> PropDef = MakeShared<FJsonObject>();

		FString JsonSchemaType = Param.Type;
		if (Param.Type == TEXT("any"))
		{
			PropDef->SetStringField(TEXT("description"), Param.Description + TEXT(" (accepts any JSON value)"));
		}
		else if (Param.Type == TEXT("vector") || Param.Type == TEXT("rotator") || Param.Type == TEXT("transform"))
		{
			PropDef->SetStringField(TEXT("type"), TEXT("object"));
			PropDef->SetStringField(TEXT("description"), Param.Description);
		}
		else if (Param.Type == TEXT("float") || Param.Type == TEXT("double"))
		{
			PropDef->SetStringField(TEXT("type"), TEXT("number"));
			PropDef->SetStringField(TEXT("description"), Param.Description);
		}
		else if (Param.Type == TEXT("int") || Param.Type == TEXT("int32") || Param.Type == TEXT("int64"))
		{
			PropDef->SetStringField(TEXT("type"), TEXT("integer"));
			PropDef->SetStringField(TEXT("description"), Param.Description);
		}
		else
		{
			PropDef->SetStringField(TEXT("type"), JsonSchemaType);
			PropDef->SetStringField(TEXT("description"), Param.Description);
		}

		if (!Param.DefaultValue.IsEmpty())
		{
			PropDef->SetStringField(TEXT("default"), Param.DefaultValue);
		}

		Properties->SetObjectField(Param.Name, PropDef);

		if (Param.bRequired)
		{
			Required.Add(MakeShared<FJsonValueString>(Param.Name));
		}
	}

	InputSchema->SetObjectField(TEXT("properties"), Properties);
	if (Required.Num() > 0)
	{
		InputSchema->SetArrayField(TEXT("required"), Required);
	}

	return InputSchema;
}

//------------------------------------------------------------------------------
// IECACommand - Parameter Helpers
//------------------------------------------------------------------------------

bool IECACommand::GetStringParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, FString& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	if (Params->TryGetStringField(Name, OutValue))
	{
		return true;
	}
	
	return false;
}

bool IECACommand::GetIntParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, int32& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	double Value;
	if (Params->TryGetNumberField(Name, Value))
	{
		OutValue = FMath::RoundToInt(Value);
		return true;
	}
	
	return false;
}

bool IECACommand::GetFloatParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, double& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	return Params->TryGetNumberField(Name, OutValue);
}

bool IECACommand::GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, bool& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	return Params->TryGetBoolField(Name, OutValue);
}

bool IECACommand::GetVectorParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, FVector& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	// Try object format first: {"x": 1, "y": 2, "z": 3}
	const TSharedPtr<FJsonObject>* VecObj;
	if (Params->TryGetObjectField(Name, VecObj))
	{
		OutValue.X = (*VecObj)->GetNumberField(TEXT("x"));
		OutValue.Y = (*VecObj)->GetNumberField(TEXT("y"));
		OutValue.Z = (*VecObj)->GetNumberField(TEXT("z"));
		return true;
	}
	
	// Try array format: [1, 2, 3]
	const TArray<TSharedPtr<FJsonValue>>* ArrValue;
	if (Params->TryGetArrayField(Name, ArrValue) && ArrValue->Num() >= 3)
	{
		OutValue.X = (*ArrValue)[0]->AsNumber();
		OutValue.Y = (*ArrValue)[1]->AsNumber();
		OutValue.Z = (*ArrValue)[2]->AsNumber();
		return true;
	}
	
	// Try string format: "1,2,3" or "1, 2, 3"
	FString StrValue;
	if (Params->TryGetStringField(Name, StrValue))
	{
		TArray<FString> Parts;
		StrValue.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			OutValue.X = FCString::Atof(*Parts[0].TrimStartAndEnd());
			OutValue.Y = FCString::Atof(*Parts[1].TrimStartAndEnd());
			OutValue.Z = FCString::Atof(*Parts[2].TrimStartAndEnd());
			return true;
		}
	}
	
	return false;
}

bool IECACommand::GetRotatorParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, FRotator& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	// Try object format first: {"pitch": 1, "yaw": 2, "roll": 3}
	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(Name, RotObj))
	{
		OutValue.Pitch = (*RotObj)->GetNumberField(TEXT("pitch"));
		OutValue.Yaw = (*RotObj)->GetNumberField(TEXT("yaw"));
		OutValue.Roll = (*RotObj)->GetNumberField(TEXT("roll"));
		return true;
	}
	
	// Try array format: [pitch, yaw, roll]
	const TArray<TSharedPtr<FJsonValue>>* ArrValue;
	if (Params->TryGetArrayField(Name, ArrValue) && ArrValue->Num() >= 3)
	{
		OutValue.Pitch = (*ArrValue)[0]->AsNumber();
		OutValue.Yaw = (*ArrValue)[1]->AsNumber();
		OutValue.Roll = (*ArrValue)[2]->AsNumber();
		return true;
	}
	
	// Try string format: "pitch,yaw,roll" or "pitch, yaw, roll"
	FString StrValue;
	if (Params->TryGetStringField(Name, StrValue))
	{
		TArray<FString> Parts;
		StrValue.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			OutValue.Pitch = FCString::Atof(*Parts[0].TrimStartAndEnd());
			OutValue.Yaw = FCString::Atof(*Parts[1].TrimStartAndEnd());
			OutValue.Roll = FCString::Atof(*Parts[2].TrimStartAndEnd());
			return true;
		}
	}
	
	return false;
}

bool IECACommand::GetArrayParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, const TArray<TSharedPtr<FJsonValue>>*& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	return Params->TryGetArrayField(Name, OutValue);
}

bool IECACommand::GetObjectParam(const TSharedPtr<FJsonObject>& Params, const FString& Name, const TSharedPtr<FJsonObject>*& OutValue, bool bRequired)
{
	if (!Params.IsValid())
	{
		return false;
	}
	
	return Params->TryGetObjectField(Name, OutValue);
}

//------------------------------------------------------------------------------
// IECACommand - Result Helpers
//------------------------------------------------------------------------------

TSharedPtr<FJsonObject> IECACommand::MakeResult()
{
	return MakeShared<FJsonObject>();
}

TSharedPtr<FJsonObject> IECACommand::VectorToJson(const FVector& Vec)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("x"), Vec.X);
	Obj->SetNumberField(TEXT("y"), Vec.Y);
	Obj->SetNumberField(TEXT("z"), Vec.Z);
	return Obj;
}

TSharedPtr<FJsonObject> IECACommand::RotatorToJson(const FRotator& Rot)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("pitch"), Rot.Pitch);
	Obj->SetNumberField(TEXT("yaw"), Rot.Yaw);
	Obj->SetNumberField(TEXT("roll"), Rot.Roll);
	return Obj;
}

TSharedPtr<FJsonObject> IECACommand::TransformToJson(const FTransform& Transform)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("location"), VectorToJson(Transform.GetLocation()));
	Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.Rotator()));
	Obj->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
	return Obj;
}

//------------------------------------------------------------------------------
// IECACommand - Common Operations
//------------------------------------------------------------------------------

UWorld* IECACommand::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

AActor* IECACommand::FindActorByName(const FString& ActorName)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return nullptr;
	}
	
	// Check if the name contains wildcards
	bool bHasWildcard = ActorName.Contains(TEXT("*")) || ActorName.Contains(TEXT("?"));
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		FString ActorLabel = Actor->GetActorLabel();
		FString ActorObjName = Actor->GetName();
		
		if (bHasWildcard)
		{
			// Wildcard matching (case-insensitive)
			if (ActorLabel.MatchesWildcard(ActorName, ESearchCase::IgnoreCase) ||
			    ActorObjName.MatchesWildcard(ActorName, ESearchCase::IgnoreCase))
			{
				return Actor;
			}
		}
		else
		{
			// Exact matching (case-insensitive)
			if (ActorLabel.Equals(ActorName, ESearchCase::IgnoreCase) ||
			    ActorObjName.Equals(ActorName, ESearchCase::IgnoreCase))
			{
				return Actor;
			}
		}
	}
	
	return nullptr;
}

UBlueprint* IECACommand::LoadBlueprintByPath(const FString& BlueprintPath)
{
	return LoadObject<UBlueprint>(nullptr, *BlueprintPath);
}

//------------------------------------------------------------------------------
// FECACommandRegistry
//------------------------------------------------------------------------------

FECACommandRegistry& FECACommandRegistry::Get()
{
	static FECACommandRegistry Instance;
	return Instance;
}

void FECACommandRegistry::RegisterCommand(TSharedPtr<IECACommand> Command)
{
	if (!Command.IsValid())
	{
		return;
	}
	
	FScopeLock Lock(&CommandsLock);
	
	const FString Name = Command->GetName();
	if (Commands.Contains(Name))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ECABridge] Command '%s' already registered, replacing"), *Name);
	}
	
	Commands.Add(Name, Command);
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered command: %s (%s)"), *Name, *Command->GetCategory());
}

void FECACommandRegistry::UnregisterCommand(const FString& Name)
{
	FScopeLock Lock(&CommandsLock);
	Commands.Remove(Name);
}

TSharedPtr<IECACommand> FECACommandRegistry::GetCommand(const FString& Name) const
{
	FScopeLock Lock(&CommandsLock);
	const TSharedPtr<IECACommand>* Found = Commands.Find(Name);
	return Found ? *Found : nullptr;
}

TArray<TSharedPtr<IECACommand>> FECACommandRegistry::GetAllCommands() const
{
	FScopeLock Lock(&CommandsLock);
	TArray<TSharedPtr<IECACommand>> Result;
	Commands.GenerateValueArray(Result);
	return Result;
}

TArray<TSharedPtr<IECACommand>> FECACommandRegistry::GetCommandsByCategory(const FString& Category) const
{
	FScopeLock Lock(&CommandsLock);
	TArray<TSharedPtr<IECACommand>> Result;
	
	for (const auto& Pair : Commands)
	{
		if (Pair.Value->GetCategory() == Category)
		{
			Result.Add(Pair.Value);
		}
	}
	
	return Result;
}

bool FECACommandRegistry::HasCommand(const FString& Name) const
{
	FScopeLock Lock(&CommandsLock);
	return Commands.Contains(Name);
}

TArray<FString> FECACommandRegistry::GetCategories() const
{
	FScopeLock Lock(&CommandsLock);
	TSet<FString> Categories;
	
	for (const auto& Pair : Commands)
	{
		Categories.Add(Pair.Value->GetCategory());
	}
	
	return Categories.Array();
}

FECACommandResult FECACommandRegistry::ExecuteCommand(const FString& Name, const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<IECACommand> Command = GetCommand(Name);
	
	if (!Command.IsValid())
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Unknown command: %s"), *Name));
	}
	
	return Command->Execute(Params);
}

void FECACommandRegistry::LogCommands() const
{
	FScopeLock Lock(&CommandsLock);
	
	UE_LOG(LogTemp, Log, TEXT("[ECABridge] Registered commands (%d total):"), Commands.Num());
	
	TArray<FString> Categories = GetCategories();
	Categories.Sort();
	
	for (const FString& Category : Categories)
	{
		UE_LOG(LogTemp, Log, TEXT("  [%s]"), *Category);
		
		for (const auto& Pair : Commands)
		{
			if (Pair.Value->GetCategory() == Category)
			{
				UE_LOG(LogTemp, Log, TEXT("    - %s: %s"), *Pair.Key, *Pair.Value->GetDescription());
			}
		}
	}
}
