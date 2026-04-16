// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ECABridgeSettings.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeECABridgeSettings() {}

// ********** Begin Cross Module References ********************************************************
DEVELOPERSETTINGS_API UClass* Z_Construct_UClass_UDeveloperSettings();
ECABRIDGE_API UClass* Z_Construct_UClass_UECABridgeSettings();
ECABRIDGE_API UClass* Z_Construct_UClass_UECABridgeSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_ECABridge();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UECABridgeSettings *******************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UECABridgeSettings;
UClass* UECABridgeSettings::GetPrivateStaticClass()
{
	using TClass = UECABridgeSettings;
	if (!Z_Registration_Info_UClass_UECABridgeSettings.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ECABridgeSettings"),
			Z_Registration_Info_UClass_UECABridgeSettings.InnerSingleton,
			StaticRegisterNativesUECABridgeSettings,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UECABridgeSettings.InnerSingleton;
}
UClass* Z_Construct_UClass_UECABridgeSettings_NoRegister()
{
	return UECABridgeSettings::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UECABridgeSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * ECA Bridge Settings\n * \n * Configure the MCP server that allows AI assistants (Claude Desktop, Cursor, etc.)\n * to control Unreal Editor.\n */" },
#endif
		{ "DisplayName", "ECA Bridge" },
		{ "IncludePath", "ECABridgeSettings.h" },
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "ECA Bridge Settings\n\nConfigure the MCP server that allows AI assistants (Claude Desktop, Cursor, etc.)\nto control Unreal Editor." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ServerPort_MetaData[] = {
		{ "Category", "Server" },
		{ "ClampMax", "65535" },
		{ "ClampMin", "1024" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Port for the MCP HTTP server (for Claude Desktop, Cursor, etc.) */" },
#endif
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Port for the MCP HTTP server (for Claude Desktop, Cursor, etc.)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAutoStart_MetaData[] = {
		{ "Category", "Server" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Whether to start the server automatically when the editor launches */" },
#endif
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Whether to start the server automatically when the editor launches" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CommandTimeoutSeconds_MetaData[] = {
		{ "Category", "Server" },
		{ "ClampMax", "300" },
		{ "ClampMin", "1" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Maximum time (in seconds) to wait for a command to complete */" },
#endif
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Maximum time (in seconds) to wait for a command to complete" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bVerboseLogging_MetaData[] = {
		{ "Category", "Debugging" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Enable verbose logging for debugging */" },
#endif
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Enable verbose logging for debugging" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bLogCommands_MetaData[] = {
		{ "Category", "Debugging" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Log all incoming commands (may contain sensitive data) */" },
#endif
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Log all incoming commands (may contain sensitive data)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAllowRemoteConnections_MetaData[] = {
		{ "Category", "Security" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Allow connections from external IPs (security risk - use with caution) */" },
#endif
		{ "ModuleRelativePath", "Public/ECABridgeSettings.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Allow connections from external IPs (security risk - use with caution)" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UECABridgeSettings constinit property declarations ***********************
	static const UECodeGen_Private::FIntPropertyParams NewProp_ServerPort;
	static void NewProp_bAutoStart_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAutoStart;
	static const UECodeGen_Private::FIntPropertyParams NewProp_CommandTimeoutSeconds;
	static void NewProp_bVerboseLogging_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bVerboseLogging;
	static void NewProp_bLogCommands_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bLogCommands;
	static void NewProp_bAllowRemoteConnections_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAllowRemoteConnections;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UECABridgeSettings constinit property declarations *************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UECABridgeSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UECABridgeSettings_Statics

// ********** Begin Class UECABridgeSettings Property Definitions **********************************
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_ServerPort = { "ServerPort", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UECABridgeSettings, ServerPort), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ServerPort_MetaData), NewProp_ServerPort_MetaData) };
void Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAutoStart_SetBit(void* Obj)
{
	((UECABridgeSettings*)Obj)->bAutoStart = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAutoStart = { "bAutoStart", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UECABridgeSettings), &Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAutoStart_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAutoStart_MetaData), NewProp_bAutoStart_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_CommandTimeoutSeconds = { "CommandTimeoutSeconds", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UECABridgeSettings, CommandTimeoutSeconds), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CommandTimeoutSeconds_MetaData), NewProp_CommandTimeoutSeconds_MetaData) };
void Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bVerboseLogging_SetBit(void* Obj)
{
	((UECABridgeSettings*)Obj)->bVerboseLogging = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bVerboseLogging = { "bVerboseLogging", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UECABridgeSettings), &Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bVerboseLogging_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bVerboseLogging_MetaData), NewProp_bVerboseLogging_MetaData) };
void Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bLogCommands_SetBit(void* Obj)
{
	((UECABridgeSettings*)Obj)->bLogCommands = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bLogCommands = { "bLogCommands", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UECABridgeSettings), &Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bLogCommands_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bLogCommands_MetaData), NewProp_bLogCommands_MetaData) };
void Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAllowRemoteConnections_SetBit(void* Obj)
{
	((UECABridgeSettings*)Obj)->bAllowRemoteConnections = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAllowRemoteConnections = { "bAllowRemoteConnections", nullptr, (EPropertyFlags)0x0010040000004001, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UECABridgeSettings), &Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAllowRemoteConnections_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAllowRemoteConnections_MetaData), NewProp_bAllowRemoteConnections_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UECABridgeSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_ServerPort,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAutoStart,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_CommandTimeoutSeconds,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bVerboseLogging,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bLogCommands,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECABridgeSettings_Statics::NewProp_bAllowRemoteConnections,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UECABridgeSettings_Statics::PropPointers) < 2048);
// ********** End Class UECABridgeSettings Property Definitions ************************************
UObject* (*const Z_Construct_UClass_UECABridgeSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UDeveloperSettings,
	(UObject* (*)())Z_Construct_UPackage__Script_ECABridge,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UECABridgeSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UECABridgeSettings_Statics::ClassParams = {
	&UECABridgeSettings::StaticClass,
	"EditorPerProjectUserSettings",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UECABridgeSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UECABridgeSettings_Statics::PropPointers),
	0,
	0x001000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UECABridgeSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UECABridgeSettings_Statics::Class_MetaDataParams)
};
void UECABridgeSettings::StaticRegisterNativesUECABridgeSettings()
{
}
UClass* Z_Construct_UClass_UECABridgeSettings()
{
	if (!Z_Registration_Info_UClass_UECABridgeSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UECABridgeSettings.OuterSingleton, Z_Construct_UClass_UECABridgeSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UECABridgeSettings.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UECABridgeSettings);
UECABridgeSettings::~UECABridgeSettings() {}
// ********** End Class UECABridgeSettings *********************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridgeSettings_h__Script_ECABridge_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UECABridgeSettings, UECABridgeSettings::StaticClass, TEXT("UECABridgeSettings"), &Z_Registration_Info_UClass_UECABridgeSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UECABridgeSettings), 2838618502U) },
	};
}; // Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridgeSettings_h__Script_ECABridge_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridgeSettings_h__Script_ECABridge_765003680{
	TEXT("/Script/ECABridge"),
	Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridgeSettings_h__Script_ECABridge_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridgeSettings_h__Script_ECABridge_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
