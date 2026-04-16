// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintLisp.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

//------------------------------------------------------------------------------
// FLispNode Implementation
//------------------------------------------------------------------------------

FLispNodePtr FLispNode::MakeNil()
{
	return MakeShared<FLispNode>();
}

FLispNodePtr FLispNode::MakeSymbol(const FString& Name)
{
	auto Node = MakeShared<FLispNode>();
	Node->Type = ELispNodeType::Symbol;
	Node->StringValue = Name;
	return Node;
}

FLispNodePtr FLispNode::MakeKeyword(const FString& Name)
{
	auto Node = MakeShared<FLispNode>();
	Node->Type = ELispNodeType::Keyword;
	Node->StringValue = Name;
	return Node;
}

FLispNodePtr FLispNode::MakeNumber(double Value)
{
	auto Node = MakeShared<FLispNode>();
	Node->Type = ELispNodeType::Number;
	Node->NumberValue = Value;
	return Node;
}

FLispNodePtr FLispNode::MakeString(const FString& Value)
{
	auto Node = MakeShared<FLispNode>();
	Node->Type = ELispNodeType::String;
	Node->StringValue = Value;
	return Node;
}

FLispNodePtr FLispNode::MakeList(const TArray<FLispNodePtr>& Items)
{
	auto Node = MakeShared<FLispNode>();
	Node->Type = ELispNodeType::List;
	Node->Children = Items;
	return Node;
}

bool FLispNode::IsForm(const FString& FormName) const
{
	if (!IsList() || Children.Num() == 0)
	{
		return false;
	}
	
	const auto& First = Children[0];
	return First.IsValid() && First->IsSymbol() && First->StringValue.Equals(FormName, ESearchCase::IgnoreCase);
}

FString FLispNode::GetFormName() const
{
	if (IsList() && Children.Num() > 0 && Children[0].IsValid() && Children[0]->IsSymbol())
	{
		return Children[0]->StringValue;
	}
	return TEXT("");
}

FLispNodePtr FLispNode::operator[](int32 Index) const
{
	return Get(Index);
}

FLispNodePtr FLispNode::Get(int32 Index) const
{
	if (Index >= 0 && Index < Children.Num())
	{
		return Children[Index];
	}
	return MakeNil();
}

FLispNodePtr FLispNode::GetKeywordArg(const FString& Keyword) const
{
	if (!IsList())
	{
		return MakeNil();
	}
	
	FString KeywordWithColon = Keyword.StartsWith(TEXT(":")) ? Keyword : (TEXT(":") + Keyword);
	
	for (int32 i = 0; i < Children.Num() - 1; i++)
	{
		if (Children[i].IsValid() && Children[i]->IsKeyword() && 
			Children[i]->StringValue.Equals(KeywordWithColon, ESearchCase::IgnoreCase))
		{
			return Children[i + 1];
		}
	}
	
	return MakeNil();
}

bool FLispNode::HasKeyword(const FString& Keyword) const
{
	if (!IsList())
	{
		return false;
	}
	
	FString KeywordWithColon = Keyword.StartsWith(TEXT(":")) ? Keyword : (TEXT(":") + Keyword);
	
	for (const auto& Child : Children)
	{
		if (Child.IsValid() && Child->IsKeyword() && 
			Child->StringValue.Equals(KeywordWithColon, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	
	return false;
}

FString FLispNode::ToString(bool bPretty, int32 IndentLevel) const
{
	FString Indent = bPretty ? FString::ChrN(IndentLevel * 2, ' ') : TEXT("");
	FString Newline = bPretty ? TEXT("\n") : TEXT("");
	FString Space = bPretty ? TEXT(" ") : TEXT(" ");
	
	switch (Type)
	{
	case ELispNodeType::Null:
		return TEXT("nil");
		
	case ELispNodeType::Symbol:
		return StringValue;
		
	case ELispNodeType::Keyword:
		return StringValue; // Already includes ':'
		
	case ELispNodeType::Number:
		if (FMath::IsNearlyEqual(NumberValue, FMath::RoundToDouble(NumberValue)))
		{
			return FString::Printf(TEXT("%d"), (int64)NumberValue);
		}
		return FString::SanitizeFloat(NumberValue);
		
	case ELispNodeType::String:
		{
			FString Escaped = StringValue;
			Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
			Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
			Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
			return FString::Printf(TEXT("\"%s\""), *Escaped);
		}
		
	case ELispNodeType::List:
		{
			if (Children.Num() == 0)
			{
				return TEXT("()");
			}
			
			// Check if this is a "complex" form that should be multi-line
			bool bMultiLine = bPretty && Children.Num() > 3;
			if (!bMultiLine && bPretty)
			{
				// Also go multi-line if any child is a list
				for (const auto& Child : Children)
				{
					if (Child.IsValid() && Child->IsList() && Child->Children.Num() > 2)
					{
						bMultiLine = true;
						break;
					}
				}
			}
			
			// Check for special forms that should always be multi-line
			FString FormName = GetFormName();
			if (bPretty && (FormName == TEXT("event") || FormName == TEXT("func") || 
				FormName == TEXT("macro") || FormName == TEXT("seq") ||
				FormName == TEXT("branch") || FormName == TEXT("foreach") ||
				FormName == TEXT("switch")))
			{
				bMultiLine = true;
			}
			
			FString Result = TEXT("(");
			
			if (bMultiLine)
			{
				for (int32 i = 0; i < Children.Num(); i++)
				{
					if (i == 0)
					{
						// First element (form name) on same line
						Result += Children[i].IsValid() ? Children[i]->ToString(false, 0) : TEXT("nil");
					}
					else if (Children[i].IsValid() && Children[i]->IsKeyword())
					{
						// Keyword on new line
						Result += Newline + Indent + TEXT("  ");
						Result += Children[i]->ToString(false, 0);
						// Value follows keyword on same line or next depending on complexity
						if (i + 1 < Children.Num())
						{
							i++;
							if (Children[i].IsValid() && Children[i]->IsList() && Children[i]->Children.Num() > 2)
							{
								Result += Newline + Indent + TEXT("    ");
								Result += Children[i]->ToString(bPretty, IndentLevel + 2);
							}
							else
							{
								Result += TEXT(" ");
								Result += Children[i].IsValid() ? Children[i]->ToString(bPretty, IndentLevel + 2) : TEXT("nil");
							}
						}
					}
					else
					{
						Result += Newline + Indent + TEXT("  ");
						Result += Children[i].IsValid() ? Children[i]->ToString(bPretty, IndentLevel + 1) : TEXT("nil");
					}
				}
				Result += TEXT(")");
			}
			else
			{
				for (int32 i = 0; i < Children.Num(); i++)
				{
					if (i > 0) Result += TEXT(" ");
					Result += Children[i].IsValid() ? Children[i]->ToString(false, 0) : TEXT("nil");
				}
				Result += TEXT(")");
			}
			
			return Result;
		}
	}
	
	return TEXT("nil");
}

//------------------------------------------------------------------------------
// FLispParseResult Implementation
//------------------------------------------------------------------------------

FLispParseResult FLispParseResult::Success(const TArray<FLispNodePtr>& InNodes)
{
	FLispParseResult Result;
	Result.bSuccess = true;
	Result.Nodes = InNodes;
	return Result;
}

FLispParseResult FLispParseResult::Failure(const FString& InError, int32 InLine, int32 InColumn)
{
	FLispParseResult Result;
	Result.bSuccess = false;
	Result.Error = InError;
	Result.ErrorLine = InLine;
	Result.ErrorColumn = InColumn;
	return Result;
}

//------------------------------------------------------------------------------
// FLispParser Implementation
//------------------------------------------------------------------------------

FLispParser::FLispParser(const FString& InSource)
	: Source(InSource)
{
}

FLispParseResult FLispParser::Parse(const FString& Source)
{
	FLispParser Parser(Source);
	return Parser.DoParse();
}

FLispParseResult FLispParser::DoParse()
{
	TArray<FLispNodePtr> Nodes;
	
	SkipWhitespaceAndComments();
	
	while (!IsAtEnd())
	{
		FLispNodePtr Expr = ParseExpr();
		if (!Expr.IsValid())
		{
			return MakeError(TEXT("Failed to parse expression"));
		}
		Nodes.Add(Expr);
		SkipWhitespaceAndComments();
	}
	
	return FLispParseResult::Success(Nodes);
}

FLispNodePtr FLispParser::ParseExpr()
{
	SkipWhitespaceAndComments();
	
	if (IsAtEnd())
	{
		return nullptr;
	}
	
	TCHAR c = Peek();
	
	if (c == '(')
	{
		return ParseList();
	}
	else if (c == '"')
	{
		return ParseString();
	}
	else if (c == ':')
	{
		return ParseSymbolOrKeyword();
	}
	else if (FChar::IsDigit(c) || (c == '-' && Position + 1 < Source.Len() && FChar::IsDigit(Source[Position + 1])))
	{
		return ParseNumber();
	}
	else
	{
		return ParseSymbolOrKeyword();
	}
}

FLispNodePtr FLispParser::ParseList()
{
	int32 StartLine = Line;
	int32 StartColumn = Column;
	
	if (!Match('('))
	{
		return nullptr;
	}
	
	TArray<FLispNodePtr> Items;
	SkipWhitespaceAndComments();
	
	while (!IsAtEnd() && Peek() != ')')
	{
		FLispNodePtr Item = ParseExpr();
		if (!Item.IsValid())
		{
			return nullptr;
		}
		Items.Add(Item);
		SkipWhitespaceAndComments();
	}
	
	if (!Match(')'))
	{
		return nullptr;
	}
	
	auto Node = FLispNode::MakeList(Items);
	Node->Line = StartLine;
	Node->Column = StartColumn;
	return Node;
}

FLispNodePtr FLispParser::ParseString()
{
	int32 StartLine = Line;
	int32 StartColumn = Column;
	
	if (!Match('"'))
	{
		return nullptr;
	}
	
	FString Value;
	
	while (!IsAtEnd() && Peek() != '"')
	{
		TCHAR c = Advance();
		
		if (c == '\\' && !IsAtEnd())
		{
			TCHAR Escaped = Advance();
			switch (Escaped)
			{
			case 'n': Value += '\n'; break;
			case 't': Value += '\t'; break;
			case 'r': Value += '\r'; break;
			case '"': Value += '"'; break;
			case '\\': Value += '\\'; break;
			default: Value += Escaped; break;
			}
		}
		else
		{
			Value += c;
		}
	}
	
	if (!Match('"'))
	{
		return nullptr;
	}
	
	auto Node = FLispNode::MakeString(Value);
	Node->Line = StartLine;
	Node->Column = StartColumn;
	return Node;
}

FLispNodePtr FLispParser::ParseNumber()
{
	int32 StartLine = Line;
	int32 StartColumn = Column;
	
	FString NumStr;
	
	// Optional negative sign
	if (Peek() == '-')
	{
		NumStr += Advance();
	}
	
	// Integer part
	while (!IsAtEnd() && FChar::IsDigit(Peek()))
	{
		NumStr += Advance();
	}
	
	// Decimal part
	if (!IsAtEnd() && Peek() == '.' && Position + 1 < Source.Len() && FChar::IsDigit(Source[Position + 1]))
	{
		NumStr += Advance(); // '.'
		while (!IsAtEnd() && FChar::IsDigit(Peek()))
		{
			NumStr += Advance();
		}
	}
	
	// Exponent
	if (!IsAtEnd() && (Peek() == 'e' || Peek() == 'E'))
	{
		NumStr += Advance();
		if (!IsAtEnd() && (Peek() == '+' || Peek() == '-'))
		{
			NumStr += Advance();
		}
		while (!IsAtEnd() && FChar::IsDigit(Peek()))
		{
			NumStr += Advance();
		}
	}
	
	double Value = FCString::Atod(*NumStr);
	
	auto Node = FLispNode::MakeNumber(Value);
	Node->StringValue = NumStr;  // Store original string to detect floats like "24.0"
	Node->Line = StartLine;
	Node->Column = StartColumn;
	return Node;
}

FLispNodePtr FLispParser::ParseSymbolOrKeyword()
{
	int32 StartLine = Line;
	int32 StartColumn = Column;
	
	bool bIsKeyword = (Peek() == ':');
	FString Name;
	
	if (bIsKeyword)
	{
		Name += Advance(); // Include the ':'
	}
	
	// Symbol chars: alphanumeric, underscore, hyphen, plus, minus, asterisk, slash, etc.
	auto IsSymbolChar = [](TCHAR c) -> bool
	{
		return FChar::IsAlnum(c) || c == '_' || c == '-' || c == '+' || c == '*' || 
			   c == '/' || c == '?' || c == '!' || c == '<' || c == '>' || c == '=' ||
			   c == '.' || c == '@' || c == '&';
	};
	
	while (!IsAtEnd() && IsSymbolChar(Peek()))
	{
		Name += Advance();
	}
	
	if (Name.IsEmpty() || (bIsKeyword && Name.Len() == 1))
	{
		return nullptr;
	}
	
	// Check for special symbols
	if (Name.Equals(TEXT("nil"), ESearchCase::IgnoreCase))
	{
		return FLispNode::MakeNil();
	}
	if (Name.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		auto Node = FLispNode::MakeSymbol(TEXT("true"));
		Node->Line = StartLine;
		Node->Column = StartColumn;
		return Node;
	}
	if (Name.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		auto Node = FLispNode::MakeSymbol(TEXT("false"));
		Node->Line = StartLine;
		Node->Column = StartColumn;
		return Node;
	}
	
	FLispNodePtr Node = bIsKeyword ? FLispNode::MakeKeyword(Name) : FLispNode::MakeSymbol(Name);
	Node->Line = StartLine;
	Node->Column = StartColumn;
	return Node;
}

void FLispParser::SkipWhitespaceAndComments()
{
	while (!IsAtEnd())
	{
		TCHAR c = Peek();
		
		if (FChar::IsWhitespace(c))
		{
			Advance();
		}
		else if (c == ';')
		{
			// Line comment - skip to end of line
			while (!IsAtEnd() && Peek() != '\n')
			{
				Advance();
			}
		}
		else
		{
			break;
		}
	}
}

TCHAR FLispParser::Peek() const
{
	if (IsAtEnd()) return '\0';
	return Source[Position];
}

TCHAR FLispParser::Advance()
{
	if (IsAtEnd()) return '\0';
	
	TCHAR c = Source[Position++];
	
	if (c == '\n')
	{
		Line++;
		Column = 1;
	}
	else
	{
		Column++;
	}
	
	return c;
}

bool FLispParser::IsAtEnd() const
{
	return Position >= Source.Len();
}

bool FLispParser::Match(TCHAR Expected)
{
	if (IsAtEnd() || Peek() != Expected)
	{
		return false;
	}
	Advance();
	return true;
}

FLispParseResult FLispParser::MakeError(const FString& Message)
{
	return FLispParseResult::Failure(Message, Line, Column);
}

//------------------------------------------------------------------------------
// FBlueprintLispResult Implementation
//------------------------------------------------------------------------------

FBlueprintLispResult FBlueprintLispResult::Success(const FString& Code)
{
	FBlueprintLispResult Result;
	Result.bSuccess = true;
	Result.LispCode = Code;
	return Result;
}

FBlueprintLispResult FBlueprintLispResult::SuccessWithData(TSharedPtr<FJsonObject> Data)
{
	FBlueprintLispResult Result;
	Result.bSuccess = true;
	Result.GraphData = Data;
	return Result;
}

FBlueprintLispResult FBlueprintLispResult::Failure(const FString& InError)
{
	FBlueprintLispResult Result;
	Result.bSuccess = false;
	Result.Error = InError;
	return Result;
}

//------------------------------------------------------------------------------
// FBlueprintLispConverter Implementation
//------------------------------------------------------------------------------

FBlueprintLispResult FBlueprintLispConverter::ValidateLisp(const FString& LispCode)
{
	FLispParseResult ParseResult = FLispParser::Parse(LispCode);
	
	if (!ParseResult.bSuccess)
	{
		return FBlueprintLispResult::Failure(FString::Printf(
			TEXT("Parse error at line %d, column %d: %s"),
			ParseResult.ErrorLine, ParseResult.ErrorColumn, *ParseResult.Error));
	}
	
	// Validate each top-level form
	for (const auto& Node : ParseResult.Nodes)
	{
		if (!Node.IsValid() || !Node->IsList())
		{
			return FBlueprintLispResult::Failure(TEXT("Top-level expressions must be lists"));
		}
		
		FString FormName = Node->GetFormName();
		if (FormName.IsEmpty())
		{
			return FBlueprintLispResult::Failure(TEXT("List must start with a symbol"));
		}
		
		// Check for valid top-level forms
		static const TSet<FString> ValidForms = {
			TEXT("event"), TEXT("func"), TEXT("macro"), TEXT("var"), TEXT("comment")
		};
		
		if (!ValidForms.Contains(FormName.ToLower()))
		{
			return FBlueprintLispResult::Failure(FString::Printf(
				TEXT("Unknown top-level form: %s. Expected: event, func, macro, var"), *FormName));
		}
	}
	
	FBlueprintLispResult Result;
	Result.bSuccess = true;
	Result.LispCode = LispCode;
	return Result;
}

FBlueprintLispResult FBlueprintLispConverter::BlueprintToLisp(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FBlueprintToLispOptions& Options)
{
	// This will be implemented to work with ECABridge's existing Blueprint introspection
	// For now, return a placeholder
	return FBlueprintLispResult::Failure(TEXT("BlueprintToLisp not yet implemented - use the ECA command"));
}

FBlueprintLispResult FBlueprintLispConverter::LispToBlueprint(
	const FString& LispCode,
	const FString& BlueprintPath,
	const FString& GraphName,
	const FLispToBlueprintOptions& Options)
{
	// Parse the Lisp code
	FLispParseResult ParseResult = FLispParser::Parse(LispCode);
	
	if (!ParseResult.bSuccess)
	{
		return FBlueprintLispResult::Failure(FString::Printf(
			TEXT("Parse error at line %d, column %d: %s"),
			ParseResult.ErrorLine, ParseResult.ErrorColumn, *ParseResult.Error));
	}
	
	// Create the graph data structure
	TSharedPtr<FJsonObject> GraphData = MakeShared<FJsonObject>();
	GraphData->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	GraphData->SetStringField(TEXT("graph_name"), GraphName);
	
	TArray<TSharedPtr<FJsonValue>> NodesArray;
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	
	GraphData->SetArrayField(TEXT("nodes"), NodesArray);
	GraphData->SetArrayField(TEXT("connections"), ConnectionsArray);
	
	// Process each top-level form
	TMap<FString, FString> VariableMap; // Lisp variable name -> node ID
	
	for (const auto& Form : ParseResult.Nodes)
	{
		FString Error;
		if (!ProcessForm(Form, GraphData, VariableMap, Error))
		{
			return FBlueprintLispResult::Failure(Error);
		}
	}
	
	return FBlueprintLispResult::SuccessWithData(GraphData);
}

bool FBlueprintLispConverter::ProcessForm(
	const FLispNodePtr& Form,
	TSharedPtr<FJsonObject> GraphData,
	TMap<FString, FString>& VariableMap,
	FString& OutError)
{
	if (!Form.IsValid() || !Form->IsList() || Form->Num() == 0)
	{
		OutError = TEXT("Invalid form");
		return false;
	}
	
	FString FormName = Form->GetFormName().ToLower();
	
	// TODO: Implement form processing
	// This is where we'll convert each Lisp form to Blueprint nodes
	
	if (FormName == TEXT("event"))
	{
		// (event EventName body...)
		if (Form->Num() < 2)
		{
			OutError = TEXT("event requires at least an event name");
			return false;
		}
		
		// Get event name
		FLispNodePtr EventNameNode = Form->Get(1);
		if (!EventNameNode.IsValid() || !EventNameNode->IsSymbol())
		{
			OutError = TEXT("event name must be a symbol");
			return false;
		}
		
		FString EventName = EventNameNode->StringValue;
		
		// TODO: Create event node and process body
		// For now, just validate
		return true;
	}
	else if (FormName == TEXT("func"))
	{
		// (func FuncName :inputs (...) :outputs (...) body...)
		return true;
	}
	else if (FormName == TEXT("macro"))
	{
		// (macro MacroName :inputs (...) body...)
		return true;
	}
	else if (FormName == TEXT("var"))
	{
		// (var VarName Type)
		return true;
	}
	else if (FormName == TEXT("comment"))
	{
		// (comment "text")
		return true;
	}
	
	OutError = FString::Printf(TEXT("Unknown form: %s"), *FormName);
	return false;
}

TSharedPtr<FJsonObject> FBlueprintLispConverter::CreateNodeJson(
	const FString& NodeType,
	const TMap<FString, FString>& Properties)
{
	TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
	Node->SetStringField(TEXT("node_type"), NodeType);
	
	for (const auto& Pair : Properties)
	{
		Node->SetStringField(Pair.Key, Pair.Value);
	}
	
	return Node;
}

bool FBlueprintLispConverter::ResolveExpression(
	const FLispNodePtr& Expr,
	TSharedPtr<FJsonObject> GraphData,
	TMap<FString, FString>& VariableMap,
	FString& OutNodeId,
	FString& OutPinName,
	FString& OutError)
{
	// TODO: Implement expression resolution
	// This converts a Lisp expression to a node/pin reference
	return true;
}

//------------------------------------------------------------------------------
// Utility Functions Implementation
//------------------------------------------------------------------------------

namespace BlueprintLisp
{

FString PrettyPrint(const FString& LispCode)
{
	FLispParseResult ParseResult = FLispParser::Parse(LispCode);
	if (!ParseResult.bSuccess)
	{
		return LispCode; // Return original if parse fails
	}
	
	FString Result;
	for (int32 i = 0; i < ParseResult.Nodes.Num(); i++)
	{
		if (i > 0)
		{
			Result += TEXT("\n\n");
		}
		Result += ParseResult.Nodes[i]->ToString(true, 0);
	}
	
	return Result;
}

FString Minify(const FString& LispCode)
{
	FLispParseResult ParseResult = FLispParser::Parse(LispCode);
	if (!ParseResult.bSuccess)
	{
		return LispCode;
	}
	
	FString Result;
	for (int32 i = 0; i < ParseResult.Nodes.Num(); i++)
	{
		if (i > 0)
		{
			Result += TEXT(" ");
		}
		Result += ParseResult.Nodes[i]->ToString(false, 0);
	}
	
	return Result;
}

TArray<FString> ExtractSymbols(const FString& LispCode)
{
	TSet<FString> SymbolSet;
	
	FLispParseResult ParseResult = FLispParser::Parse(LispCode);
	if (!ParseResult.bSuccess)
	{
		return TArray<FString>();
	}
	
	TFunction<void(const FLispNodePtr&)> CollectSymbols = [&](const FLispNodePtr& Node)
	{
		if (!Node.IsValid()) return;
		
		if (Node->IsSymbol())
		{
			SymbolSet.Add(Node->StringValue);
		}
		else if (Node->IsList())
		{
			for (const auto& Child : Node->Children)
			{
				CollectSymbols(Child);
			}
		}
	};
	
	for (const auto& Node : ParseResult.Nodes)
	{
		CollectSymbols(Node);
	}
	
	return SymbolSet.Array();
}

bool IsValidSymbol(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return false;
	}
	
	// First char must be letter or underscore
	TCHAR First = Name[0];
	if (!FChar::IsAlpha(First) && First != '_')
	{
		return false;
	}
	
	// Rest can be alphanumeric, underscore, or hyphen
	for (int32 i = 1; i < Name.Len(); i++)
	{
		TCHAR c = Name[i];
		if (!FChar::IsAlnum(c) && c != '_' && c != '-')
		{
			return false;
		}
	}
	
	return true;
}

} // namespace BlueprintLisp
