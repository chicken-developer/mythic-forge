// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniPointCacheFactory.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeHoudiniPointCacheFactory() {}

// ********** Begin Cross Module References ********************************************************
UNREALED_API UClass* Z_Construct_UClass_UFactory(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_HoudiniNiagaraEditor(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheFactory(ETypeConstructPhase);
HOUDININIAGARAEDITOR_API UClass* Z_Construct_UClass_UHoudiniPointCacheFactory(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UHoudiniPointCacheFactory ************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UHoudiniPointCacheFactory_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** A factory for HoudiniPointCache assets. */" },
#endif
		{ "IncludePath", "HoudiniPointCacheFactory.h" },
		{ "ModuleRelativePath", "Public/HoudiniPointCacheFactory.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "A factory for HoudiniPointCache assets." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UHoudiniPointCacheFactory constinit property declarations ****************
// ********** End Class UHoudiniPointCacheFactory constinit property declarations ******************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniPointCacheFactory>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UFactory,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagaraEditor,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UHoudiniPointCacheFactory,
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
	0x000800A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UHoudiniPointCacheFactory;
UClass* Z_Construct_UClass_UHoudiniPointCacheFactory(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UHoudiniPointCacheFactory;
		if (!Z_Registration_Info_UClass_UHoudiniPointCacheFactory.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("HoudiniPointCacheFactory"),
				Z_Registration_Info_UClass_UHoudiniPointCacheFactory.InnerSingleton,
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
		return Z_Registration_Info_UClass_UHoudiniPointCacheFactory.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UHoudiniPointCacheFactory.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniPointCacheFactory.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UHoudiniPointCacheFactory.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UHoudiniPointCacheFactory);
UHoudiniPointCacheFactory::~UHoudiniPointCacheFactory() {}
// ********** End Class UHoudiniPointCacheFactory **************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagaraEditor_Public_HoudiniPointCacheFactory_h__Script_HoudiniNiagaraEditor_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniPointCacheFactory, TEXT("UHoudiniPointCacheFactory"), &Z_Registration_Info_UClass_UHoudiniPointCacheFactory, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniPointCacheFactory), 924673137U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagaraEditor_Public_HoudiniPointCacheFactory_h__Script_HoudiniNiagaraEditor_1ccffd476df8b548b4a8a810ebd2d3740781f95d{
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
