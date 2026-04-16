// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ECABridge.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeECABridge() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
ECABRIDGE_API UClass* Z_Construct_UClass_UECABridge();
ECABRIDGE_API UClass* Z_Construct_UClass_UECABridge_NoRegister();
UPackage* Z_Construct_UPackage__Script_ECABridge();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UECABridge ***************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UECABridge;
UClass* UECABridge::GetPrivateStaticClass()
{
	using TClass = UECABridge;
	if (!Z_Registration_Info_UClass_UECABridge.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ECABridge"),
			Z_Registration_Info_UClass_UECABridge.InnerSingleton,
			StaticRegisterNativesUECABridge,
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
	return Z_Registration_Info_UClass_UECABridge.InnerSingleton;
}
UClass* Z_Construct_UClass_UECABridge_NoRegister()
{
	return UECABridge::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UECABridge_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "Comment", "/**\n * ECA Bridge\n * \n * Main coordinator for Epic Code Assistant integration.\n * Manages the MCP HTTP server and routes commands to the appropriate handlers.\n */" },
		{ "IncludePath", "ECABridge.h" },
		{ "ModuleRelativePath", "Public/ECABridge.h" },
		{ "ToolTip", "ECA Bridge\n\nMain coordinator for Epic Code Assistant integration.\nManages the MCP HTTP server and routes commands to the appropriate handlers." },
	};
#endif // WITH_METADATA

// ********** Begin Class UECABridge constinit property declarations *******************************
// ********** End Class UECABridge constinit property declarations *********************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UECABridge>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UECABridge_Statics
UObject* (*const Z_Construct_UClass_UECABridge_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_ECABridge,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UECABridge_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UECABridge_Statics::ClassParams = {
	&UECABridge::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UECABridge_Statics::Class_MetaDataParams), Z_Construct_UClass_UECABridge_Statics::Class_MetaDataParams)
};
void UECABridge::StaticRegisterNativesUECABridge()
{
}
UClass* Z_Construct_UClass_UECABridge()
{
	if (!Z_Registration_Info_UClass_UECABridge.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UECABridge.OuterSingleton, Z_Construct_UClass_UECABridge_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UECABridge.OuterSingleton;
}
UECABridge::UECABridge(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UECABridge);
UECABridge::~UECABridge() {}
// ********** End Class UECABridge *****************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_agilelens_Desktop_ECABridge_HostProject_Plugins_ECABridge_Source_ECABridge_Public_ECABridge_h__Script_ECABridge_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UECABridge, UECABridge::StaticClass, TEXT("UECABridge"), &Z_Registration_Info_UClass_UECABridge, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UECABridge), 973479286U) },
	};
}; // Z_CompiledInDeferFile_FID_agilelens_Desktop_ECABridge_HostProject_Plugins_ECABridge_Source_ECABridge_Public_ECABridge_h__Script_ECABridge_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_agilelens_Desktop_ECABridge_HostProject_Plugins_ECABridge_Source_ECABridge_Public_ECABridge_h__Script_ECABridge_2823168258{
	TEXT("/Script/ECABridge"),
	Z_CompiledInDeferFile_FID_agilelens_Desktop_ECABridge_HostProject_Plugins_ECABridge_Source_ECABridge_Public_ECABridge_h__Script_ECABridge_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_agilelens_Desktop_ECABridge_HostProject_Plugins_ECABridge_Source_ECABridge_Public_ECABridge_h__Script_ECABridge_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
