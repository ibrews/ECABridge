// Copyright Epic Games, Inc. All Rights Reserved.
//
// Meta-tools: small, always-visible commands that let an MCP client discover
// and lazy-load the rest of the tool surface. When lazy mode is enabled at
// module startup, these are the only commands tools/list returns by default —
// cutting the baseline payload from ~330KB to ~5KB.

#include "Commands/ECACommand.h"
#include "ECAMCPServer.h"

class FECACommand_ListCategories : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_categories"); }
	virtual FString GetDescription() const override
	{
		return TEXT("List every command category and the number of commands in each. "
			"Use this first when lazy tool registration is on, then call load_category "
			"to surface the categories you need.");
	}
	virtual FString GetCategory() const override { return FECACommandRegistry::MetaCategory; }

	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("categories"), TEXT("array"),   TEXT("Each entry: { name, count, loaded }"), TEXT("object") },
			{ TEXT("lazy_mode"),  TEXT("boolean"), TEXT("Whether the registry is in lazy mode (tools/list filtered to loaded categories)") },
			{ TEXT("total"),      TEXT("integer"), TEXT("Total commands across all categories") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FECACommandRegistry& Registry = FECACommandRegistry::Get();
		const TArray<FString> Cats = Registry.GetCategories();
		const TArray<TSharedPtr<IECACommand>> All = Registry.GetAllCommands();

		TMap<FString, int32> CountByCat;
		for (const TSharedPtr<IECACommand>& Cmd : All)
		{
			if (Cmd.IsValid())
			{
				CountByCat.FindOrAdd(Cmd->GetCategory())++;
			}
		}

		TArray<FString> SortedCats = Cats;
		SortedCats.Sort();

		TArray<TSharedPtr<FJsonValue>> CatArray;
		for (const FString& Cat : SortedCats)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), Cat);
			Entry->SetNumberField(TEXT("count"), CountByCat.FindRef(Cat));
			Entry->SetBoolField(TEXT("loaded"), Registry.IsCategoryVisible(Cat));
			CatArray.Add(MakeShared<FJsonValueObject>(Entry));
		}

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("categories"), CatArray);
		Result->SetBoolField(TEXT("lazy_mode"), Registry.IsLazyMode());
		Result->SetNumberField(TEXT("total"), All.Num());
		return FECACommandResult::Success(Result);
	}
};

class FECACommand_DescribeCategory : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_category"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Return the names + descriptions of every command in the given category, "
			"without surfacing them in tools/list. Use to preview what load_category would expose.");
	}
	virtual FString GetCategory() const override { return FECACommandRegistry::MetaCategory; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("category"), TEXT("string"), TEXT("Category name (case-sensitive). Get the list from list_categories."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("category"), TEXT("string"), TEXT("The requested category") },
			{ TEXT("commands"), TEXT("array"),  TEXT("Each entry: { name, description }"), TEXT("object") },
			{ TEXT("count"),    TEXT("integer"), TEXT("Number of commands in the category") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString Category;
		if (!GetStringParam(Params, TEXT("category"), Category, true) || Category.IsEmpty())
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing required 'category' parameter"));
		}

		const TArray<TSharedPtr<IECACommand>> Cmds = FECACommandRegistry::Get().GetCommandsByCategory(Category);

		TArray<TSharedPtr<FJsonValue>> CmdArray;
		for (const TSharedPtr<IECACommand>& Cmd : Cmds)
		{
			if (!Cmd.IsValid()) continue;
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), Cmd->GetName());
			Entry->SetStringField(TEXT("description"), Cmd->GetDescription());
			CmdArray.Add(MakeShared<FJsonValueObject>(Entry));
		}

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("category"), Category);
		Result->SetArrayField(TEXT("commands"), CmdArray);
		Result->SetNumberField(TEXT("count"), CmdArray.Num());
		return FECACommandResult::Success(Result);
	}
};

class FECACommand_LoadCategory : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("load_category"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Mark a command category as loaded so its tools appear in subsequent "
			"tools/list responses. The commands were already callable via tools/call; this "
			"only widens the discovery surface. Pair with notifications/tools/list_changed.");
	}
	virtual FString GetCategory() const override { return FECACommandRegistry::MetaCategory; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("category"), TEXT("string"), TEXT("Category name (case-sensitive). Use list_categories to discover."), true }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("category"),       TEXT("string"),  TEXT("The category that was loaded") },
			{ TEXT("loaded_count"),   TEXT("integer"), TEXT("Number of commands now surfaced by tools/list from this category") },
			{ TEXT("total_loaded"),   TEXT("array"),   TEXT("All categories currently visible in tools/list"), TEXT("string") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString Category;
		if (!GetStringParam(Params, TEXT("category"), Category, true) || Category.IsEmpty())
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing required 'category' parameter"));
		}

		FECACommandRegistry& Registry = FECACommandRegistry::Get();
		const int32 LoadedCount = Registry.LoadCategory(Category);

		TArray<FString> All = Registry.GetLoadedCategories();
		All.Sort();
		TArray<TSharedPtr<FJsonValue>> AllArr;
		for (const FString& C : All)
		{
			AllArr.Add(MakeShared<FJsonValueString>(C));
		}

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("category"), Category);
		Result->SetNumberField(TEXT("loaded_count"), LoadedCount);
		Result->SetArrayField(TEXT("total_loaded"), AllArr);
		return FECACommandResult::Success(Result);
	}
};

class FECACommand_ContinueResponse : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("continue_response"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Fetch the next chunk of a previously truncated tool-call response. "
			"Pass the `_continuation_token` from a tool result whose `_remaining_chars` is "
			"non-zero. Result text concatenates onto the previous chunk to reconstruct the "
			"full JSON. Optional max_bytes overrides the server cap for this call.");
	}
	virtual FString GetCategory() const override { return FECACommandRegistry::MetaCategory; }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("token"),     TEXT("string"),  TEXT("Continuation token returned by a chunked tool-call response."), true },
			{ TEXT("max_bytes"), TEXT("integer"), TEXT("Optional chunk-size cap for this fetch (default 65536)."), false, TEXT("65536") }
		};
	}

	virtual TSharedPtr<FJsonObject> GetOutputSchema() const override
	{
		return MakeECAObjectSchema({
			{ TEXT("chunk"),     TEXT("string"),  TEXT("Next slice of the original result text") },
			{ TEXT("chars"),     TEXT("integer"), TEXT("Length of this chunk in characters") },
			{ TEXT("final"),     TEXT("boolean"), TEXT("True when this chunk drained the remainder") }
		});
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString Token;
		if (!GetStringParam(Params, TEXT("token"), Token, true) || Token.IsEmpty())
		{
			return FECACommandResult::ValidationError(this, TEXT("Missing required 'token' parameter"));
		}

		int32 MaxBytes = 65536;
		GetIntParam(Params, TEXT("max_bytes"), MaxBytes, false);
		if (MaxBytes <= 0) MaxBytes = 65536;

		bool bFinal = false;
		const FString Chunk = FECAResponseChunkCache::Get().FetchNext(Token, MaxBytes, bFinal);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("chunk"), Chunk);
		Result->SetNumberField(TEXT("chars"), Chunk.Len());
		Result->SetBoolField(TEXT("final"), bFinal);
		return FECACommandResult::Success(Result);
	}
};

REGISTER_ECA_COMMAND(FECACommand_ListCategories)
REGISTER_ECA_COMMAND(FECACommand_DescribeCategory)
REGISTER_ECA_COMMAND(FECACommand_LoadCategory)
REGISTER_ECA_COMMAND(FECACommand_ContinueResponse)
