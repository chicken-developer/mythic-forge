// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniPointCacheExporterHBJSON.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeHoudiniPointCacheExporterHBJSON() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_HoudiniNiagaraEditor(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheExporterBase(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheExporterHBJSON(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheExporterHBJSON(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UHoudiniPointCacheExporterHBJSON *****************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UHoudiniPointCacheExporterHBJSON_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "IncludePath", "HoudiniPointCacheExporterHBJSON.h" },
		{ "ModuleRelativePath", "Private/HoudiniPointCacheExporterHBJSON.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UHoudiniPointCacheExporterHBJSON constinit property declarations *********
// ********** End Class UHoudiniPointCacheExporterHBJSON constinit property declarations ***********
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniPointCacheExporterHBJSON>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCacheExporterBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagaraEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UHoudiniPointCacheExporterHBJSON,
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
	0x000000A8u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON;
UClass* Z_Construct_UClass_UHoudiniPointCacheExporterHBJSON(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UHoudiniPointCacheExporterHBJSON;
		if (!Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("HoudiniPointCacheExporterHBJSON"),
				Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON.InnerSingleton,
				nullptr,
				DataSizeOf<TClass>(),
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
		return Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UHoudiniPointCacheExporterHBJSON);
UHoudiniPointCacheExporterHBJSON::~UHoudiniPointCacheExporterHBJSON() {}
// ********** End Class UHoudiniPointCacheExporterHBJSON *******************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagaraEditor_Private_HoudiniPointCacheExporterHBJSON_h__Script_HoudiniNiagaraEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniPointCacheExporterHBJSON, TEXT("UHoudiniPointCacheExporterHBJSON"), &Z_Registration_Info_UClass_UHoudiniPointCacheExporterHBJSON, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniPointCacheExporterHBJSON), 3041499081U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagaraEditor_Private_HoudiniPointCacheExporterHBJSON_h__Script_HoudiniNiagaraEditor_c4904558ced84a5247b28d90c64603b2bf8e500c{
	TEXT("/Script/HoudiniNiagaraEditor"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
