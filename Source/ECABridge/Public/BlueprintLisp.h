// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * BlueprintLisp - A LISP-like DSL for representing Blueprint graphs
 * 
 * This module provides bidirectional conversion between Blueprint graphs and a
 * simple S-expression syntax that's easy for AI to read, understand, and generate.
 * 
 * Example syntax:
 * 
 *   (event BeginPlay
 *     (let player (GetPlayerCharacter 0))
 *     (branch (IsValid player)
 *       :true (seq
 *         (let health (call player GetHealth))
 *         (PrintString (format "Health: {}" health)))
 *       :false
 *         (PrintString "No player!")))
 * 
 * Core forms:
 *   (event Name ...)           - Event node
 *   (func Name ...)            - Function definition  
 *   (let var expr)             - Local variable
 *   (set var expr)             - Set variable
 *   (seq ...)                  - Sequence node (execution flow)
 *   (branch cond :true :false) - Branch node
 *   (foreach item coll ...)    - ForEach loop
 *   (call target func args...) - Call function on object
 *   (delay seconds)            - Delay node
 *   (cast Type var body)       - Cast with success exec
 *   (vec x y z)                - Vector literal
 *   (rot p y r)                - Rotator literal
 */

//------------------------------------------------------------------------------
// S-Expression AST
//------------------------------------------------------------------------------

/** Types of S-expression nodes */
enum class ELispNodeType : uint8
{
	Null,       // Empty/null
	Symbol,     // Identifier: foo, BeginPlay, GetHealth
	Keyword,    // Keyword: :true, :false, :pin-name
	Number,     // Numeric literal: 42, 3.14
	String,     // String literal: "hello"
	List,       // List: (a b c)
};

/** Forward declaration */
struct FLispNode;

/** Shared pointer to a LISP node */
using FLispNodePtr = TSharedPtr<FLispNode>;

/** A node in the S-expression AST */
struct ECABRIDGE_API FLispNode
{
	ELispNodeType Type = ELispNodeType::Null;
	
	// Value storage (only one is used based on Type)
	FString StringValue;      // For Symbol, Keyword, String
	double NumberValue = 0.0; // For Number
	TArray<FLispNodePtr> Children; // For List
	
	// Source location for error reporting
	int32 Line = 0;
	int32 Column = 0;
	
	// Constructors
	FLispNode() = default;
	
	static FLispNodePtr MakeNil();
	static FLispNodePtr MakeSymbol(const FString& Name);
	static FLispNodePtr MakeKeyword(const FString& Name);
	static FLispNodePtr MakeNumber(double Value);
	static FLispNodePtr MakeString(const FString& Value);
	static FLispNodePtr MakeList(const TArray<FLispNodePtr>& Items = {});
	
	// Type checks
	bool IsNil() const { return Type == ELispNodeType::Null; }
	bool IsSymbol() const { return Type == ELispNodeType::Symbol; }
	bool IsKeyword() const { return Type == ELispNodeType::Keyword; }
	bool IsNumber() const { return Type == ELispNodeType::Number; }
	bool IsString() const { return Type == ELispNodeType::String; }
	bool IsList() const { return Type == ELispNodeType::List; }
	
	// Check if this is a list starting with a specific symbol
	bool IsForm(const FString& FormName) const;
	
	// Get the form name (first element if list with symbol head)
	FString GetFormName() const;
	
	// List accessors
	int32 Num() const { return Children.Num(); }
	FLispNodePtr operator[](int32 Index) const;
	FLispNodePtr Get(int32 Index) const;
	
	// Find keyword argument in list: (foo :key value) -> returns value for :key
	FLispNodePtr GetKeywordArg(const FString& Keyword) const;
	bool HasKeyword(const FString& Keyword) const;
	
	// Convert to string representation
	FString ToString(bool bPretty = false, int32 IndentLevel = 0) const;
};

//------------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------------

/** Result of parsing */
struct ECABRIDGE_API FLispParseResult
{
	bool bSuccess = false;
	FString Error;
	int32 ErrorLine = 0;
	int32 ErrorColumn = 0;
	TArray<FLispNodePtr> Nodes; // Top-level expressions
	
	static FLispParseResult Success(const TArray<FLispNodePtr>& Nodes);
	static FLispParseResult Failure(const FString& Error, int32 Line = 0, int32 Column = 0);
};

/** S-expression parser */
class ECABRIDGE_API FLispParser
{
public:
	/** Parse a string containing one or more S-expressions */
	static FLispParseResult Parse(const FString& Source);
	
private:
	FLispParser(const FString& Source);
	
	FLispParseResult DoParse();
	FLispNodePtr ParseExpr();
	FLispNodePtr ParseList();
	FLispNodePtr ParseAtom();
	FLispNodePtr ParseString();
	FLispNodePtr ParseNumber();
	FLispNodePtr ParseSymbolOrKeyword();
	
	void SkipWhitespaceAndComments();
	TCHAR Peek() const;
	TCHAR Advance();
	bool IsAtEnd() const;
	bool Match(TCHAR Expected);
	
	FLispParseResult MakeError(const FString& Message);
	
	const FString& Source;
	int32 Position = 0;
	int32 Line = 1;
	int32 Column = 1;
};

//------------------------------------------------------------------------------
// Blueprint Graph Conversion
//------------------------------------------------------------------------------

/** Options for converting Blueprint to Lisp */
struct ECABRIDGE_API FBlueprintToLispOptions
{
	bool bIncludeComments = true;
	bool bIncludePositions = false; // Include node positions as metadata
	bool bPrettyPrint = true;
};

/** Options for converting Lisp to Blueprint */
struct ECABRIDGE_API FLispToBlueprintOptions
{
	bool bClearExisting = false; // Clear existing nodes before adding
	bool bAutoLayout = true;     // Auto-arrange nodes after creation
};

/** Result of Blueprint conversion */
struct ECABRIDGE_API FBlueprintLispResult
{
	bool bSuccess = false;
	FString Error;
	FString LispCode;                    // For BP -> Lisp
	TSharedPtr<FJsonObject> GraphData;   // For Lisp -> BP (intermediate JSON)
	TArray<FString> Warnings;
	
	static FBlueprintLispResult Success(const FString& Code);
	static FBlueprintLispResult SuccessWithData(TSharedPtr<FJsonObject> Data);
	static FBlueprintLispResult Failure(const FString& Error);
};

/**
 * Main converter class for Blueprint <-> Lisp conversion
 */
class ECABRIDGE_API FBlueprintLispConverter
{
public:
	/** Convert a Blueprint graph to Lisp code */
	static FBlueprintLispResult BlueprintToLisp(
		const FString& BlueprintPath,
		const FString& GraphName = TEXT("EventGraph"),
		const FBlueprintToLispOptions& Options = FBlueprintToLispOptions());
	
	/** Convert Lisp code to Blueprint graph operations (returns JSON for ECA commands) */
	static FBlueprintLispResult LispToBlueprint(
		const FString& LispCode,
		const FString& BlueprintPath,
		const FString& GraphName = TEXT("EventGraph"),
		const FLispToBlueprintOptions& Options = FLispToBlueprintOptions());
	
	/** Validate Lisp code without generating Blueprint */
	static FBlueprintLispResult ValidateLisp(const FString& LispCode);
	
private:
	// Blueprint -> Lisp helpers
	static FLispNodePtr NodeToLisp(const TSharedPtr<FJsonObject>& Node, 
		const TMap<FString, TSharedPtr<FJsonObject>>& NodeMap,
		TSet<FString>& VisitedNodes,
		const FBlueprintToLispOptions& Options);
	
	static FLispNodePtr ExecChainToLisp(const FString& StartNodeId,
		const TMap<FString, TSharedPtr<FJsonObject>>& NodeMap,
		TSet<FString>& VisitedNodes,
		const FBlueprintToLispOptions& Options);
	
	static FLispNodePtr PureNodeToLisp(const TSharedPtr<FJsonObject>& Node,
		const TMap<FString, TSharedPtr<FJsonObject>>& NodeMap,
		TSet<FString>& VisitedNodes);
	
	// Lisp -> Blueprint helpers  
	static bool ProcessForm(const FLispNodePtr& Form,
		TSharedPtr<FJsonObject> GraphData,
		TMap<FString, FString>& VariableMap,
		FString& OutError);
	
	static TSharedPtr<FJsonObject> CreateNodeJson(const FString& NodeType,
		const TMap<FString, FString>& Properties = {});
	
	static bool ResolveExpression(const FLispNodePtr& Expr,
		TSharedPtr<FJsonObject> GraphData,
		TMap<FString, FString>& VariableMap,
		FString& OutNodeId,
		FString& OutPinName,
		FString& OutError);
};

//------------------------------------------------------------------------------
// Utility Functions
//------------------------------------------------------------------------------

namespace BlueprintLisp
{
	/** Format Lisp code with proper indentation */
	ECABRIDGE_API FString PrettyPrint(const FString& LispCode);
	
	/** Minify Lisp code (remove extra whitespace and comments) */
	ECABRIDGE_API FString Minify(const FString& LispCode);
	
	/** Extract all symbol names from Lisp code */
	ECABRIDGE_API TArray<FString> ExtractSymbols(const FString& LispCode);
	
	/** Check if a string is a valid symbol name */
	ECABRIDGE_API bool IsValidSymbol(const FString& Name);
}
