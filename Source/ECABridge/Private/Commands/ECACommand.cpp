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

// Title-case helper: "actor_name" -> "Actor Name". Used to surface a human
// label on each schema property so clients (Claude Code's tools panel, EDA,
// Inspector tools) can render a nicer label than the raw snake-case key.
// This mirrors what Python MCP servers get for free via Pydantic field titles.
static FString ECASchemaTitleFromSnake(const FString& Snake)
{
	FString Out;
	Out.Reserve(Snake.Len());
	bool bCapNext = true;
	for (TCHAR C : Snake)
	{
		if (C == TEXT('_') || C == TEXT('-'))
		{
			Out.AppendChar(TEXT(' '));
			bCapNext = true;
		}
		else if (bCapNext)
		{
			Out.AppendChar(FChar::ToUpper(C));
			bCapNext = false;
		}
		else
		{
			Out.AppendChar(C);
		}
	}
	return Out;
}

TSharedPtr<FJsonObject> IECACommand::GetInputSchemaJson() const
{
	TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
	// Native MCP servers (Epic's `ModelContextProtocol` plugin + most reference
	// SDKs) advertise JSON Schema draft-07 with `additionalProperties:false` so
	// clients reject typos in argument names instead of silently dropping them.
	// We match that shape here.
	InputSchema->SetStringField(TEXT("$schema"), TEXT("http://json-schema.org/draft-07/schema#"));
	InputSchema->SetStringField(TEXT("type"), TEXT("object"));
	InputSchema->SetStringField(TEXT("title"), ECASchemaTitleFromSnake(GetName()));
	if (!GetDescription().IsEmpty())
	{
		InputSchema->SetStringField(TEXT("description"), GetDescription());
	}

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Required;

	for (const FECACommandParam& Param : GetParameters())
	{
		TSharedPtr<FJsonObject> PropDef = MakeShared<FJsonObject>();
		PropDef->SetStringField(TEXT("title"), ECASchemaTitleFromSnake(Param.Name));

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
	// Strict mode: unknown keys fail validation. Matches native plugin behavior
	// and protects LLM clients from silently-dropped typos.
	InputSchema->SetBoolField(TEXT("additionalProperties"), false);

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
// FECAProgressRegistry
//------------------------------------------------------------------------------

FECAProgressRegistry& FECAProgressRegistry::Get()
{
	static FECAProgressRegistry Instance;
	return Instance;
}

void FECAProgressRegistry::EnsureTlsSlot()
{
	if (TlsSlot == 0xFFFFFFFFu)
	{
		TlsSlot = FPlatformTLS::AllocTlsSlot();
	}
}

void FECAProgressRegistry::BindTokenToThread(const FString& Token)
{
	FScopeLock ScopedLock(&Lock);
	EnsureTlsSlot();
	FString* Prev = static_cast<FString*>(FPlatformTLS::GetTlsValue(TlsSlot));
	if (Prev) { delete Prev; }
	FPlatformTLS::SetTlsValue(TlsSlot, Token.IsEmpty() ? nullptr : new FString(Token));
}

void FECAProgressRegistry::ClearThreadBinding()
{
	FScopeLock ScopedLock(&Lock);
	if (TlsSlot == 0xFFFFFFFFu) return;
	FString* Prev = static_cast<FString*>(FPlatformTLS::GetTlsValue(TlsSlot));
	if (Prev) { delete Prev; FPlatformTLS::SetTlsValue(TlsSlot, nullptr); }
}

FString FECAProgressRegistry::GetCurrentToken() const
{
	if (TlsSlot == 0xFFFFFFFFu)
	{
		return FString();
	}
	FString* Heap = static_cast<FString*>(FPlatformTLS::GetTlsValue(TlsSlot));
	return Heap ? *Heap : FString();
}

void FECAProgressRegistry::SetListener(FListener Callback)
{
	FScopeLock ScopedLock(&Lock);
	ListenerFn = MoveTemp(Callback);
}

void FECAProgressRegistry::Report(double Progress, double Total, const FString& Message)
{
	const FString Token = GetCurrentToken();
	if (Token.IsEmpty())
	{
		return;
	}
	FListener Copy;
	{
		FScopeLock ScopedLock(&Lock);
		Copy = ListenerFn;
	}
	if (Copy)
	{
		Copy(Token, Progress, Total, Message);
	}
}

//------------------------------------------------------------------------------
// FECACancellationRegistry
//------------------------------------------------------------------------------

FECACancellationRegistry& FECACancellationRegistry::Get()
{
	static FECACancellationRegistry Instance;
	return Instance;
}

void FECACancellationRegistry::EnsureTlsSlot()
{
	if (TlsSlot == 0xFFFFFFFFu)
	{
		TlsSlot = FPlatformTLS::AllocTlsSlot();
	}
}

TSharedRef<FECACancellationToken> FECACancellationRegistry::RegisterRequest(const FString& RequestId)
{
	FScopeLock ScopedLock(&Lock);
	EnsureTlsSlot();

	TSharedRef<FECACancellationToken> Token = MakeShared<FECACancellationToken>();
	if (!RequestId.IsEmpty())
	{
		Tokens.Add(RequestId, Token);
		// Stash the request ID on the calling thread so commands can resolve back
		// to their token via IsCurrentRequestCancelled().
		FString* Heap = new FString(RequestId);
		FString* Prev = static_cast<FString*>(FPlatformTLS::GetTlsValue(TlsSlot));
		if (Prev) { delete Prev; }
		FPlatformTLS::SetTlsValue(TlsSlot, Heap);
	}
	return Token;
}

void FECACancellationRegistry::UnregisterRequest(const FString& RequestId)
{
	FScopeLock ScopedLock(&Lock);
	if (!RequestId.IsEmpty())
	{
		Tokens.Remove(RequestId);
	}
	if (TlsSlot != 0xFFFFFFFFu)
	{
		FString* Prev = static_cast<FString*>(FPlatformTLS::GetTlsValue(TlsSlot));
		if (Prev) { delete Prev; FPlatformTLS::SetTlsValue(TlsSlot, nullptr); }
	}
}

bool FECACancellationRegistry::Cancel(const FString& RequestId)
{
	FScopeLock ScopedLock(&Lock);
	if (TSharedRef<FECACancellationToken>* Found = Tokens.Find(RequestId))
	{
		(*Found)->Cancel();
		return true;
	}
	return false;
}

FString FECACancellationRegistry::GetCurrentRequestId() const
{
	if (TlsSlot == 0xFFFFFFFFu)
	{
		return FString();
	}
	FString* Heap = static_cast<FString*>(FPlatformTLS::GetTlsValue(TlsSlot));
	return Heap ? *Heap : FString();
}

bool FECACancellationRegistry::IsCurrentRequestCancelled() const
{
	const FString Id = GetCurrentRequestId();
	if (Id.IsEmpty())
	{
		return false;
	}
	FScopeLock ScopedLock(&Lock);
	if (const TSharedRef<FECACancellationToken>* Found = Tokens.Find(Id))
	{
		return (*Found)->IsCancelled();
	}
	return false;
}

//------------------------------------------------------------------------------
// FECACommandRegistry
//------------------------------------------------------------------------------

const FString FECACommandRegistry::MetaCategory = TEXT("Meta");

FECACommandRegistry& FECACommandRegistry::Get()
{
	static FECACommandRegistry Instance;
	return Instance;
}

void FECACommandRegistry::SetLazyMode(bool bEnabled)
{
	FScopeLock Lock(&CommandsLock);
	bLazyMode = bEnabled;
	// Meta tools must always remain visible so clients can discover the rest.
	LoadedCategories.Add(MetaCategory);
}

bool FECACommandRegistry::IsLazyMode() const
{
	FScopeLock Lock(&CommandsLock);
	return bLazyMode;
}

int32 FECACommandRegistry::LoadCategory(const FString& Category)
{
	bool bChanged = false;
	int32 N = 0;
	TFunction<void()> CallbackCopy;
	{
		FScopeLock Lock(&CommandsLock);
		bChanged = !LoadedCategories.Contains(Category);
		LoadedCategories.Add(Category);
		for (const auto& Pair : Commands)
		{
			if (Pair.Value.IsValid() && Pair.Value->GetCategory() == Category)
			{
				++N;
			}
		}
		CallbackCopy = OnVisibleToolsChanged;
	}
	if (bChanged && CallbackCopy)
	{
		CallbackCopy();
	}
	return N;
}

bool FECACommandRegistry::UnloadCategory(const FString& Category)
{
	bool bRemoved = false;
	TFunction<void()> CallbackCopy;
	{
		FScopeLock Lock(&CommandsLock);
		if (Category == MetaCategory)
		{
			return false; // Never hide the meta-tools.
		}
		bRemoved = LoadedCategories.Remove(Category) > 0;
		CallbackCopy = OnVisibleToolsChanged;
	}
	if (bRemoved && CallbackCopy)
	{
		CallbackCopy();
	}
	return bRemoved;
}

void FECACommandRegistry::SetOnVisibleToolsChanged(TFunction<void()> Callback)
{
	FScopeLock Lock(&CommandsLock);
	OnVisibleToolsChanged = MoveTemp(Callback);
}

bool FECACommandRegistry::IsCategoryVisible(const FString& Category) const
{
	FScopeLock Lock(&CommandsLock);
	if (!bLazyMode)
	{
		return true;
	}
	return LoadedCategories.Contains(Category);
}

TArray<FString> FECACommandRegistry::GetLoadedCategories() const
{
	FScopeLock Lock(&CommandsLock);
	return LoadedCategories.Array();
}

TArray<TSharedPtr<IECACommand>> FECACommandRegistry::GetVisibleCommands(const FString& CategoryFilter) const
{
	FScopeLock Lock(&CommandsLock);
	TArray<TSharedPtr<IECACommand>> Result;
	Result.Reserve(Commands.Num());

	const bool bFilterSet = !CategoryFilter.IsEmpty();

	for (const auto& Pair : Commands)
	{
		if (!Pair.Value.IsValid()) continue;
		const FString Cat = Pair.Value->GetCategory();

		if (bFilterSet)
		{
			// Explicit ask wins: bypass lazy mode entirely.
			if (Cat == CategoryFilter)
			{
				Result.Add(Pair.Value);
			}
			continue;
		}

		if (bLazyMode && !LoadedCategories.Contains(Cat))
		{
			continue;
		}
		Result.Add(Pair.Value);
	}
	return Result;
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

int32 FECACommandRegistry::UnregisterByCategory(const FString& Category)
{
	FScopeLock Lock(&CommandsLock);

	TArray<FString> ToRemove;
	for (const auto& Pair : Commands)
	{
		if (Pair.Value.IsValid() && Pair.Value->GetCategory() == Category)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Name : ToRemove)
	{
		Commands.Remove(Name);
	}
	return ToRemove.Num();
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

namespace
{
	/** Compute Levenshtein distance between two case-folded strings. Capped at MaxDist
	 *  for early-out cheapness — anything > cap returns cap+1. */
	int32 LevenshteinCapped(const FString& A, const FString& B, int32 MaxDist)
	{
		const int32 LenA = A.Len();
		const int32 LenB = B.Len();
		if (FMath::Abs(LenA - LenB) > MaxDist)
		{
			return MaxDist + 1;
		}
		if (LenA == 0) { return LenB; }
		if (LenB == 0) { return LenA; }

		TArray<int32> Prev; Prev.SetNumUninitialized(LenB + 1);
		TArray<int32> Curr; Curr.SetNumUninitialized(LenB + 1);
		for (int32 j = 0; j <= LenB; ++j) { Prev[j] = j; }

		for (int32 i = 1; i <= LenA; ++i)
		{
			Curr[0] = i;
			int32 RowMin = Curr[0];
			for (int32 j = 1; j <= LenB; ++j)
			{
				const int32 Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
				Curr[j] = FMath::Min3(Prev[j] + 1, Curr[j - 1] + 1, Prev[j - 1] + Cost);
				RowMin = FMath::Min(RowMin, Curr[j]);
			}
			if (RowMin > MaxDist)
			{
				return MaxDist + 1;
			}
			Swap(Prev, Curr);
		}
		return Prev[LenB];
	}
}

TArray<FString> FECACommandRegistry::SuggestSimilarCommands(const FString& Input, int32 MaxResults) const
{
	TArray<FString> Suggestions;
	if (Input.IsEmpty() || MaxResults <= 0)
	{
		return Suggestions;
	}

	const FString InputLower = Input.ToLower();

	struct FCandidate
	{
		FString Name;
		int32 Tier = 3;     // 0=exact-prefix, 1=substring, 2=fuzzy
		int32 Distance = 0; // tie-breaker within tier (lower = better)
	};
	TArray<FCandidate> Candidates;

	const int32 MaxDist = FMath::Max(2, InputLower.Len() / 3);

	{
		FScopeLock Lock(&CommandsLock);
		Candidates.Reserve(Commands.Num());
		for (const auto& Pair : Commands)
		{
			const FString& Name = Pair.Key;
			const FString NameLower = Name.ToLower();
			FCandidate C; C.Name = Name;

			if (NameLower.StartsWith(InputLower))
			{
				C.Tier = 0;
				C.Distance = Name.Len() - InputLower.Len();
				Candidates.Add(C);
				continue;
			}
			if (NameLower.Contains(InputLower))
			{
				C.Tier = 1;
				C.Distance = Name.Len() - InputLower.Len();
				Candidates.Add(C);
				continue;
			}
			const int32 D = LevenshteinCapped(InputLower, NameLower, MaxDist);
			if (D <= MaxDist)
			{
				C.Tier = 2;
				C.Distance = D;
				Candidates.Add(C);
			}
		}
	}

	Candidates.Sort([](const FCandidate& X, const FCandidate& Y)
	{
		if (X.Tier != Y.Tier) { return X.Tier < Y.Tier; }
		if (X.Distance != Y.Distance) { return X.Distance < Y.Distance; }
		return X.Name < Y.Name;
	});

	const int32 N = FMath::Min(MaxResults, Candidates.Num());
	for (int32 i = 0; i < N; ++i)
	{
		Suggestions.Add(Candidates[i].Name);
	}
	return Suggestions;
}

FECACommandResult FECACommandRegistry::ExecuteCommand(const FString& Name, const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<IECACommand> Command = GetCommand(Name);

	if (!Command.IsValid())
	{
		const TArray<FString> Suggestions = SuggestSimilarCommands(Name, 3);
		if (Suggestions.Num() > 0)
		{
			return FECACommandResult::Error(FString::Printf(
				TEXT("Unknown command: %s. Did you mean: %s?"),
				*Name, *FString::Join(Suggestions, TEXT(", "))));
		}
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
