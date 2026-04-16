// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Commands/ECAListItemData.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeECAListItemData() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
ECABRIDGE_API UClass* Z_Construct_UClass_UECAListItemData();
ECABRIDGE_API UClass* Z_Construct_UClass_UECAListItemData_NoRegister();
UPackage* Z_Construct_UPackage__Script_ECABridge();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UECAListItemData *********************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UECAListItemData;
UClass* UECAListItemData::GetPrivateStaticClass()
{
	using TClass = UECAListItemData;
	if (!Z_Registration_Info_UClass_UECAListItemData.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ECAListItemData"),
			Z_Registration_Info_UClass_UECAListItemData.InnerSingleton,
			StaticRegisterNativesUECAListItemData,
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
	return Z_Registration_Info_UClass_UECAListItemData.InnerSingleton;
}
UClass* Z_Construct_UClass_UECAListItemData_NoRegister()
{
	return UECAListItemData::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UECAListItemData_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * Simple UObject wrapper for list item data.\n * UListView requires UObject* items - this wraps arbitrary JSON data into a UObject\n * that can be passed to the list and retrieved by entry widgets via IUserObjectListEntry.\n */" },
#endif
		{ "IncludePath", "Commands/ECAListItemData.h" },
		{ "IsBlueprintBase", "false" },
		{ "ModuleRelativePath", "Public/Commands/ECAListItemData.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Simple UObject wrapper for list item data.\nUListView requires UObject* items - this wraps arbitrary JSON data into a UObject\nthat can be passed to the list and retrieved by entry widgets via IUserObjectListEntry." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemDataJson_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** JSON string containing the item's data payload */" },
#endif
		{ "ModuleRelativePath", "Public/Commands/ECAListItemData.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "JSON string containing the item's data payload" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemIndex_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Index of this item in the list */" },
#endif
		{ "ModuleRelativePath", "Public/Commands/ECAListItemData.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Index of this item in the list" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UECAListItemData constinit property declarations *************************
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemDataJson;
	static const UECodeGen_Private::FIntPropertyParams NewProp_ItemIndex;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UECAListItemData constinit property declarations ***************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UECAListItemData>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UECAListItemData_Statics

// ********** Begin Class UECAListItemData Property Definitions ************************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UECAListItemData_Statics::NewProp_ItemDataJson = { "ItemDataJson", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UECAListItemData, ItemDataJson), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemDataJson_MetaData), NewProp_ItemDataJson_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UECAListItemData_Statics::NewProp_ItemIndex = { "ItemIndex", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UECAListItemData, ItemIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemIndex_MetaData), NewProp_ItemIndex_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UECAListItemData_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECAListItemData_Statics::NewProp_ItemDataJson,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UECAListItemData_Statics::NewProp_ItemIndex,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UECAListItemData_Statics::PropPointers) < 2048);
// ********** End Class UECAListItemData Property Definitions **************************************
UObject* (*const Z_Construct_UClass_UECAListItemData_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_ECABridge,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UECAListItemData_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UECAListItemData_Statics::ClassParams = {
	&UECAListItemData::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UECAListItemData_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UECAListItemData_Statics::PropPointers),
	0,
	0x000800A8u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UECAListItemData_Statics::Class_MetaDataParams), Z_Construct_UClass_UECAListItemData_Statics::Class_MetaDataParams)
};
void UECAListItemData::StaticRegisterNativesUECAListItemData()
{
}
UClass* Z_Construct_UClass_UECAListItemData()
{
	if (!Z_Registration_Info_UClass_UECAListItemData.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UECAListItemData.OuterSingleton, Z_Construct_UClass_UECAListItemData_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UECAListItemData.OuterSingleton;
}
UECAListItemData::UECAListItemData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UECAListItemData);
UECAListItemData::~UECAListItemData() {}
// ********** End Class UECAListItemData ***********************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_Commands_ECAListItemData_h__Script_ECABridge_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UECAListItemData, UECAListItemData::StaticClass, TEXT("UECAListItemData"), &Z_Registration_Info_UClass_UECAListItemData, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UECAListItemData), 94556020U) },
	};
}; // Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_Commands_ECAListItemData_h__Script_ECABridge_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_Commands_ECAListItemData_h__Script_ECABridge_1995411611{
	TEXT("/Script/ECABridge"),
	Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_Commands_ECAListItemData_h__Script_ECABridge_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_Commands_ECAListItemData_h__Script_ECABridge_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
