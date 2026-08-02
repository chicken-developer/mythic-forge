// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "NiagaraDataInterfaceHoudini.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeNiagaraDataInterfaceHoudini() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FLinearColor(ETypeConstructPhase);
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector(ETypeConstructPhase);
NIAGARA_API UClass* Z_Construct_UClass_UNiagaraDataInterface(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase);
HOUDININIAGARA_API UScriptStruct* Z_Construct_UScriptStruct_FHoudiniEvent(ETypeConstructPhase);
HOUDININIAGARA_API UClass* Z_Construct_UClass_UNiagaraDataInterfaceHoudini(ETypeConstructPhase);
HOUDININIAGARA_API UClass* Z_Construct_UClass_UHoudiniPointCache(ETypeConstructPhase);
HOUDININIAGARA_API UClass* Z_Construct_UClass_UNiagaraDataInterfaceHoudini(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FHoudiniEvent *****************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FHoudiniEvent_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FHoudiniEvent>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FHoudiniEvent); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Position_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Normal_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Impulse_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Velocity_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Time_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Life_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Color_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Type_MetaData[] = {
		{ "Category", "Houdini Event" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FHoudiniEvent constinit property declarations *********************
	static const UECodeGen_Private::FStructPropertyParams NewProp_Position;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Normal;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Impulse;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Velocity;
	static const UECodeGen_Private::FIntPropertyParams NewProp_PointID;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Time;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Life;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Color;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Type;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FHoudiniEvent constinit property declarations ***********************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHoudiniEvent>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FHoudiniEvent Property Definitions ********************************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Position = { "Position", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Position), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Position_MetaData), NewProp_Position_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Normal = { "Normal", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Normal), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Normal_MetaData), NewProp_Normal_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Impulse = { "Impulse", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Impulse), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Impulse_MetaData), NewProp_Impulse_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Velocity = { "Velocity", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Velocity), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Velocity_MetaData), NewProp_Velocity_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Time = { "Time", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Time), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Time_MetaData), NewProp_Time_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Life = { "Life", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Life), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Life_MetaData), NewProp_Life_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Color = { "Color", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Color), Z_Construct_UScriptStruct_FLinearColor, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Color_MetaData), NewProp_Color_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Type = { "Type", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniEvent, Type), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Type_MetaData), NewProp_Type_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Position,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Normal,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Impulse,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Velocity,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Time,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Life,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Color,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Type,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FHoudiniEvent Property Definitions **********************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagara,
	nullptr,
	&NewStructOps,
	"HoudiniEvent",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FHoudiniEvent>(),
	alignof(FHoudiniEvent),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000001),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FHoudiniEvent;
UScriptStruct* Z_Construct_UScriptStruct_FHoudiniEvent(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FHoudiniEvent.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FHoudiniEvent.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FHoudiniEvent, (UObject*)Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase::Outer), TEXT("HoudiniEvent"));
		}
		return Z_Registration_Info_UScriptStruct_FHoudiniEvent.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FHoudiniEvent.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FHoudiniEvent.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FHoudiniEvent.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FHoudiniEvent *******************************************************

// ********** Begin Class UNiagaraDataInterfaceHoudini *********************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UNiagaraDataInterfaceHoudini_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Niagara" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Data Interface allowing sampling of UHoudiniPointCache assets (CSV, .json (binary) files) files. */" },
#endif
		{ "DisplayName", "Houdini Point Cache Info" },
		{ "IncludePath", "NiagaraDataInterfaceHoudini.h" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Data Interface allowing sampling of UHoudiniPointCache assets (CSV, .json (binary) files) files." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_HoudiniPointCacheAsset_MetaData[] = {
		{ "Category", "Houdini Niagara" },
		{ "DisplayName", "Houdini Point Cache Asset" },
		{ "ModuleRelativePath", "Classes/NiagaraDataInterfaceHoudini.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UNiagaraDataInterfaceHoudini constinit property declarations *************
	static const UECodeGen_Private::FObjectPropertyParams NewProp_HoudiniPointCacheAsset;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UNiagaraDataInterfaceHoudini constinit property declarations ***************
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UNiagaraDataInterfaceHoudini>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UNiagaraDataInterfaceHoudini Property Definitions ************************
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_HoudiniPointCacheAsset = { "HoudiniPointCacheAsset", nullptr, (EPropertyFlags)0x0114000000000001, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, nullptr, nullptr, 1, STRUCT_OFFSET(UNiagaraDataInterfaceHoudini, HoudiniPointCacheAsset), Z_Construct_UClass_UHoudiniPointCache, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_HoudiniPointCacheAsset_MetaData), NewProp_HoudiniPointCacheAsset_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_HoudiniPointCacheAsset,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UNiagaraDataInterfaceHoudini Property Definitions **************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UNiagaraDataInterface,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagara,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UNiagaraDataInterfaceHoudini,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x001010A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini;
UClass* Z_Construct_UClass_UNiagaraDataInterfaceHoudini(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UNiagaraDataInterfaceHoudini;
		if (!Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("NiagaraDataInterfaceHoudini"),
				Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini.InnerSingleton,
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
		return Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UNiagaraDataInterfaceHoudini);
UNiagaraDataInterfaceHoudini::~UNiagaraDataInterfaceHoudini() {}
// ********** End Class UNiagaraDataInterfaceHoudini ***********************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagara_Classes_NiagaraDataInterfaceHoudini_h__Script_HoudiniNiagara_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FHoudiniEvent, Z_Construct_UScriptStruct_FHoudiniEvent_Statics::NewStructOps, TEXT("HoudiniEvent"),&Z_Registration_Info_UScriptStruct_FHoudiniEvent, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FHoudiniEvent), 329589823U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UNiagaraDataInterfaceHoudini, TEXT("UNiagaraDataInterfaceHoudini"), &Z_Registration_Info_UClass_UNiagaraDataInterfaceHoudini, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UNiagaraDataInterfaceHoudini), 3366980851U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_ThreeDreamingKnights_Plugins_HoudiniNiagara_Source_HoudiniNiagara_Classes_NiagaraDataInterfaceHoudini_h__Script_HoudiniNiagara_2c5a1f114361f5c4b138d1952880da23813943db{
	TEXT("/Script/HoudiniNiagara"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
