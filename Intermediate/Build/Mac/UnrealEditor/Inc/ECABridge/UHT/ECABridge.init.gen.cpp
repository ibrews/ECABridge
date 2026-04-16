// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeECABridge_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_ECABridge;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_ECABridge()
	{
		if (!Z_Registration_Info_UPackage__Script_ECABridge.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/ECABridge",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000040,
			0x4395778C,
			0x81BFF4D2,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_ECABridge.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_ECABridge.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_ECABridge(Z_Construct_UPackage__Script_ECABridge, TEXT("/Script/ECABridge"), Z_Registration_Info_UPackage__Script_ECABridge, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x4395778C, 0x81BFF4D2));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
