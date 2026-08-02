// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniPointCacheExporterHJSON.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeHoudiniPointCacheExporterHJSON() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_HoudiniNiagaraEditor(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheExporterBase(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheExporterHJSON(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheExporterHJSON(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UHoudiniPointCacheExporterHJSON ******************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UHoudiniPointCacheExporterHJSON_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "IncludePath", "HoudiniPointCacheExporterHJSON.h" },
		{ "ModuleRelativePath", "Private/HoudiniPointCacheExporterHJSON.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UHoudiniPointCacheExporterHJSON constinit property declarations **********
// ********** End Class UHoudiniPointCacheExporterHJSON constinit property declarations ************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniPointCacheExporterHJSON>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCacheExporterBase,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagaraEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UHoudiniPointCacheExporterHJSON,
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
FClassRegistrationInfo Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON;
UClass* Z_Construct_UClass_UHoudiniPointCacheExporterHJSON(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UHoudiniPointCacheExporterHJSON;
		if (!Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("HoudiniPointCacheExporterHJSON"),
				Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON.InnerSingleton,
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
		return Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UHoudiniPointCacheExporterHJSON);
UHoudiniPointCacheExporterHJSON::~UHoudiniPointCacheExporterHJSON() {}
// ********** End Class UHoudiniPointCacheExporterHJSON ********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagaraEditor_Private_HoudiniPointCacheExporterHJSON_h__Script_HoudiniNiagaraEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniPointCacheExporterHJSON, TEXT("UHoudiniPointCacheExporterHJSON"), &Z_Registration_Info_UClass_UHoudiniPointCacheExporterHJSON, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniPointCacheExporterHJSON), 4254067912U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagaraEditor_Private_HoudiniPointCacheExporterHJSON_h__Script_HoudiniNiagaraEditor_441c81c7663df44d23b0580595e091c762eac498{
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
