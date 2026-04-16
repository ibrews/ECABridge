// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "ECABridge.h"

#ifdef ECABRIDGE_ECABridge_generated_h
#error "ECABridge.generated.h already included, missing '#pragma once' in ECABridge.h"
#endif
#define ECABRIDGE_ECABridge_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class UECABridge ***************************************************************
struct Z_Construct_UClass_UECABridge_Statics;
ECABRIDGE_API UClass* Z_Construct_UClass_UECABridge_NoRegister();

#define FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h_21_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUECABridge(); \
	friend struct ::Z_Construct_UClass_UECABridge_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend ECABRIDGE_API UClass* ::Z_Construct_UClass_UECABridge_NoRegister(); \
public: \
	DECLARE_CLASS2(UECABridge, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/ECABridge"), Z_Construct_UClass_UECABridge_NoRegister) \
	DECLARE_SERIALIZER(UECABridge)


#define FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h_21_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UECABridge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UECABridge(UECABridge&&) = delete; \
	UECABridge(const UECABridge&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UECABridge); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UECABridge); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UECABridge) \
	NO_API virtual ~UECABridge();


#define FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h_18_PROLOG
#define FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h_21_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h_21_INCLASS_NO_PURE_DECLS \
	FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h_21_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UECABridge;

// ********** End Class UECABridge *****************************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_OpenClawTestBed_Plugins_ECABridge57_Source_ECABridge_Public_ECABridge_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
