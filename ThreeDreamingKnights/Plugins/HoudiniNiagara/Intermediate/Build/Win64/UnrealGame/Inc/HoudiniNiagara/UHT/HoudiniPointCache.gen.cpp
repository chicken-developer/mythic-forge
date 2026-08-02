// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniPointCache.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeHoudiniPointCache() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FLinearColor(ETypeConstructPhase);
COREUOBJECT_API UClass* Z_Construct_UClass_UObject(ETypeConstructPhase);
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FQuat(ETypeConstructPhase);
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector(ETypeConstructPhase);
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector4(ETypeConstructPhase);
ENGINE_API UClass* Z_Construct_UClass_UAssetImportData(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase);
HOUDININIAGARA_API UEnum* Z_Construct_UEnum_HoudiniNiagara_EHoudiniAttributes(ETypeConstructPhase);
HOUDININIAGARA_API UEnum* Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType(ETypeConstructPhase);
HOUDININIAGARA_API UClass* Z_Construct_UClass_UHoudiniPointCache(ETypeConstructPhase);
HOUDININIAGARA_API UScriptStruct* Z_Construct_UScriptStruct_FPointIndexes(ETypeConstructPhase);
HOUDININIAGARA_API UClass* Z_Construct_UClass_UHoudiniPointCache(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Enum EHoudiniAttributes ********************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_HoudiniNiagara_EHoudiniAttributes_Statics
template<> HOUDININIAGARA_NON_ATTRIBUTED_API UEnum* StaticEnum<EHoudiniAttributes>()
{
	return Z_Construct_UEnum_HoudiniNiagara_EHoudiniAttributes(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "AGE.Name", "AGE" },
		{ "ALPHA.Name", "ALPHA" },
		{ "COLOR.Name", "COLOR" },
		{ "HOUDINI_ATTR_BEGIN.Name", "HOUDINI_ATTR_BEGIN" },
		{ "HOUDINI_ATTR_END.Name", "HOUDINI_ATTR_END" },
		{ "HOUDINI_ATTR_SIZE.Name", "HOUDINI_ATTR_SIZE" },
		{ "IMPULSE.Name", "IMPULSE" },
		{ "LIFE.Name", "LIFE" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
		{ "NORMAL.Name", "NORMAL" },
		{ "POINTID.Name", "POINTID" },
		{ "POSITION.Name", "POSITION" },
		{ "TIME.Name", "TIME" },
		{ "TYPE.Name", "TYPE" },
		{ "VELOCITY.Name", "VELOCITY" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "HOUDINI_ATTR_BEGIN", (int64)HOUDINI_ATTR_BEGIN },
		{ "POSITION", (int64)POSITION },
		{ "NORMAL", (int64)NORMAL },
		{ "TIME", (int64)TIME },
		{ "POINTID", (int64)POINTID },
		{ "LIFE", (int64)LIFE },
		{ "COLOR", (int64)COLOR },
		{ "ALPHA", (int64)ALPHA },
		{ "VELOCITY", (int64)VELOCITY },
		{ "TYPE", (int64)TYPE },
		{ "IMPULSE", (int64)IMPULSE },
		{ "AGE", (int64)AGE },
		{ "HOUDINI_ATTR_SIZE", (int64)HOUDINI_ATTR_SIZE },
		{ "HOUDINI_ATTR_END", (int64)HOUDINI_ATTR_END },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagara,
	nullptr,
	"EHoudiniAttributes",
	"EHoudiniAttributes",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::Regular,
	(uint8)UEnum::EUnderlyingType::int32,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EHoudiniAttributes;
UEnum* Z_Construct_UEnum_HoudiniNiagara_EHoudiniAttributes(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EHoudiniAttributes.OuterSingleton)
		{
			ZRIE_EHoudiniAttributes.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniNiagara_EHoudiniAttributes, (UObject*)Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase::Outer), TEXT("EHoudiniAttributes"));
		}
		return ZRIE_EHoudiniAttributes.OuterSingleton;
	}
	if (!ZRIE_EHoudiniAttributes.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EHoudiniAttributes.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EHoudiniAttributes.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EHoudiniAttributes **********************************************************

// ********** Begin ScriptStruct FPointIndexes *****************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FPointIndexes_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FPointIndexes>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FPointIndexes); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SampleIndexes_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Simple structure for storing all the sample indexes used for a given point\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Simple structure for storing all the sample indexes used for a given point" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FPointIndexes constinit property declarations *********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_SampleIndexes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_SampleIndexes;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FPointIndexes constinit property declarations ***********************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FPointIndexes>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FPointIndexes Property Definitions ********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_SampleIndexes_Inner = { "SampleIndexes", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_SampleIndexes = { "SampleIndexes", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(FPointIndexes, SampleIndexes), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SampleIndexes_MetaData), NewProp_SampleIndexes_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SampleIndexes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SampleIndexes,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FPointIndexes Property Definitions **********************************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagara,
	nullptr,
	&NewStructOps,
	"PointIndexes",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FPointIndexes>(),
	alignof(FPointIndexes),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000001),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FPointIndexes;
UScriptStruct* Z_Construct_UScriptStruct_FPointIndexes(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FPointIndexes.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FPointIndexes.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FPointIndexes, (UObject*)Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase::Outer), TEXT("PointIndexes"));
		}
		return Z_Registration_Info_UScriptStruct_FPointIndexes.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FPointIndexes.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FPointIndexes.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FPointIndexes.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FPointIndexes *******************************************************

// ********** Begin Enum EHoudiniPointCacheFileType ************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType_Statics
template<> HOUDININIAGARA_NON_ATTRIBUTED_API UEnum* StaticEnum<EHoudiniPointCacheFileType>()
{
	return Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BJSON.Name", "EHoudiniPointCacheFileType::BJSON" },
		{ "CSV.Name", "EHoudiniPointCacheFileType::CSV" },
		{ "Invalid.Name", "EHoudiniPointCacheFileType::Invalid" },
		{ "JSON.Name", "EHoudiniPointCacheFileType::JSON" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EHoudiniPointCacheFileType::Invalid", (int64)EHoudiniPointCacheFileType::Invalid },
		{ "EHoudiniPointCacheFileType::CSV", (int64)EHoudiniPointCacheFileType::CSV },
		{ "EHoudiniPointCacheFileType::JSON", (int64)EHoudiniPointCacheFileType::JSON },
		{ "EHoudiniPointCacheFileType::BJSON", (int64)EHoudiniPointCacheFileType::BJSON },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagara,
	nullptr,
	"EHoudiniPointCacheFileType",
	"EHoudiniPointCacheFileType",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EHoudiniPointCacheFileType;
UEnum* Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EHoudiniPointCacheFileType.OuterSingleton)
		{
			ZRIE_EHoudiniPointCacheFileType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType, (UObject*)Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase::Outer), TEXT("EHoudiniPointCacheFileType"));
		}
		return ZRIE_EHoudiniPointCacheFileType.OuterSingleton;
	}
	if (!ZRIE_EHoudiniPointCacheFileType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EHoudiniPointCacheFileType.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EHoudiniPointCacheFileType.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EHoudiniPointCacheFileType **************************************************

// ********** Begin Class UHoudiniPointCache Function GetAttributeIndexFromString ******************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetAttributeIndexFromString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetAttributeIndexFromString_Parms
	{
		FString Attribute;
		int32 AttributeIndex;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the attribute index for a given string. \n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the attribute index for a given string." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetAttributeIndexFromString constinit property declarations ***********
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetAttributeIndexFromString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetAttributeIndexFromString constinit property declarations *************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetAttributeIndexFromString Property Definitions **********************
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetAttributeIndexFromString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetAttributeIndexFromString_Parms, AttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetAttributeIndexFromString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetAttributeIndexFromString Property Definitions ************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetAttributeIndexFromString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetAttributeIndexFromString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetAttributeIndexFromString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetAttributeIndexFromString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetAttributeIndexFromString)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_AttributeIndex);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetAttributeIndexFromString(Z_Param_Attribute,Z_Param_Out_AttributeIndex);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetAttributeIndexFromString ********************

// ********** Begin Class UHoudiniPointCache Function GetAttributeIndexInArrayFromString ***********
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetAttributeIndexInArrayFromString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms
	{
		FString InAttribute;
		TArray<FString> InAttributeArray;
		int32 OutAttributeIndex;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the attribute index for a given string. This is a static version of the function that\n// takes the attribute name array as an argument as well.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the attribute index for a given string. This is a static version of the function that\ntakes the attribute name array as an argument as well." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InAttribute_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InAttributeArray_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetAttributeIndexInArrayFromString constinit property declarations ****
	static const UECodeGen_Private::FStrPropertyParams NewProp_InAttribute;
	static const UECodeGen_Private::FStrPropertyParams NewProp_InAttributeArray_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_InAttributeArray;
	static const UECodeGen_Private::FIntPropertyParams NewProp_OutAttributeIndex;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetAttributeIndexInArrayFromString constinit property declarations ******
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetAttributeIndexInArrayFromString Property Definitions ***************
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_InAttribute = { "InAttribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms, InAttribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InAttribute_MetaData), NewProp_InAttribute_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_InAttributeArray_Inner = { "InAttributeArray", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_InAttributeArray = { "InAttributeArray", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms, InAttributeArray), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InAttributeArray_MetaData), NewProp_InAttributeArray_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_OutAttributeIndex = { "OutAttributeIndex", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms, OutAttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_InAttribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_InAttributeArray_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_InAttributeArray,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_OutAttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetAttributeIndexInArrayFromString Property Definitions *****************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetAttributeIndexInArrayFromString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04422401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetAttributeIndexInArrayFromString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetAttributeIndexInArrayFromString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetAttributeIndexInArrayFromString)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_InAttribute);
	P_GET_TARRAY_REF(FString,Z_Param_Out_InAttributeArray);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_OutAttributeIndex);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=UHoudiniPointCache::GetAttributeIndexInArrayFromString(Z_Param_InAttribute,Z_Param_Out_InAttributeArray,Z_Param_Out_OutAttributeIndex);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetAttributeIndexInArrayFromString *************

// ********** Begin Class UHoudiniPointCache Function GetColorValue ********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetColorValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetColorValue_Parms
	{
		int64 sampleIndex;
		FLinearColor value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Color for a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Color for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetColorValue constinit property declarations *************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetColorValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetColorValue constinit property declarations ***************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetColorValue Property Definitions ************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetColorValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetColorValue_Parms, value), Z_Construct_UScriptStruct_FLinearColor, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetColorValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetColorValue Property Definitions **************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetColorValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetColorValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetColorValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetColorValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetColorValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_STRUCT_REF(FLinearColor,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetColorValue(Z_Param_Out_sampleIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetColorValue **********************************

// ********** Begin Class UHoudiniPointCache Function GetFloatValue ********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetFloatValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetFloatValue_Parms
	{
		int64 sampleIndex;
		int32 attrIndex;
		float value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the float value at a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the float value at a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_attrIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetFloatValue constinit property declarations *************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FIntPropertyParams NewProp_attrIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetFloatValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetFloatValue constinit property declarations ***************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetFloatValue Property Definitions ************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetFloatValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_attrIndex = { "attrIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetFloatValue_Parms, attrIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_attrIndex_MetaData), NewProp_attrIndex_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetFloatValue_Parms, value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetFloatValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_attrIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetFloatValue Property Definitions **************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetFloatValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetFloatValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetFloatValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetFloatValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetFloatValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_attrIndex);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetFloatValue(Z_Param_Out_sampleIndex,Z_Param_Out_attrIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetFloatValue **********************************

// ********** Begin Class UHoudiniPointCache Function GetFloatValueForString ***********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetFloatValueForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetFloatValueForString_Parms
	{
		int64 sampleIndex;
		FString Attribute;
		float value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the float value at a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the float value at a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetFloatValueForString constinit property declarations ****************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetFloatValueForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetFloatValueForString constinit property declarations ******************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetFloatValueForString Property Definitions ***************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetFloatValueForString_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetFloatValueForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetFloatValueForString_Parms, value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetFloatValueForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetFloatValueForString Property Definitions *****************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetFloatValueForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetFloatValueForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetFloatValueForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetFloatValueForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetFloatValueForString)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetFloatValueForString(Z_Param_Out_sampleIndex,Z_Param_Attribute,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetFloatValueForString *************************

// ********** Begin Class UHoudiniPointCache Function GetImpulseValue ******************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetImpulseValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetImpulseValue_Parms
	{
		int64 sampleIndex;
		float value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns an Impulse float value for a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns an Impulse float value for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetImpulseValue constinit property declarations ***********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetImpulseValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetImpulseValue constinit property declarations *************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetImpulseValue Property Definitions **********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetImpulseValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetImpulseValue_Parms, value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetImpulseValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetImpulseValue Property Definitions ************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetImpulseValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetImpulseValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetImpulseValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetImpulseValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetImpulseValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetImpulseValue(Z_Param_Out_sampleIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetImpulseValue ********************************

// ********** Begin Class UHoudiniPointCache Function GetLastPointIDToSpawnAtTime ******************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetLastPointIDToSpawnAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms
	{
		float time;
		int64 lastID;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Get the last pointID of the points to be spawned at time t\n// Invalid Index are used to indicate edge cases:\n// -1 will be returned if there is no points to spawn ( t is smaller than the first point time )\n// NumberOfSamples will be returned if all points in the CSV have been spawned ( t is higher than the last point time )\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Get the last pointID of the points to be spawned at time t\nInvalid Index are used to indicate edge cases:\n-1 will be returned if there is no points to spawn ( t is smaller than the first point time )\nNumberOfSamples will be returned if all points in the CSV have been spawned ( t is higher than the last point time )" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_time_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetLastPointIDToSpawnAtTime constinit property declarations ***********
	static const UECodeGen_Private::FFloatPropertyParams NewProp_time;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_lastID;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetLastPointIDToSpawnAtTime constinit property declarations *************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetLastPointIDToSpawnAtTime Property Definitions **********************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_time = { "time", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms, time), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_time_MetaData), NewProp_time_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_lastID = { "lastID", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms, lastID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_time,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_lastID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetLastPointIDToSpawnAtTime Property Definitions ************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetLastPointIDToSpawnAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetLastPointIDToSpawnAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetLastPointIDToSpawnAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetLastPointIDToSpawnAtTime)
{
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_time);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_lastID);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetLastPointIDToSpawnAtTime(Z_Param_Out_time,Z_Param_Out_lastID);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetLastPointIDToSpawnAtTime ********************

// ********** Begin Class UHoudiniPointCache Function GetLastSampleIndexAtTime *********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetLastSampleIndexAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms
	{
		float desiredTime;
		int64 lastSampleIndex;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Get the last sample index for a given time value (the sample with a time smaller or equal to desiredTime)\n// If the point cache doesn't have time informations, returns false and set LastsampleIndex to the last sample in the file\n// If desiredTime is smaller than the time value in the first sample, LastsampleIndex will be set to -1\n// If desiredTime is higher than the last time value in the last sample of the point cache, LastIndex will be set to the last sample's index\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Get the last sample index for a given time value (the sample with a time smaller or equal to desiredTime)\nIf the point cache doesn't have time informations, returns false and set LastsampleIndex to the last sample in the file\nIf desiredTime is smaller than the time value in the first sample, LastsampleIndex will be set to -1\nIf desiredTime is higher than the last time value in the last sample of the point cache, LastIndex will be set to the last sample's index" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_desiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetLastSampleIndexAtTime constinit property declarations **************
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_lastSampleIndex;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetLastSampleIndexAtTime constinit property declarations ****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetLastSampleIndexAtTime Property Definitions *************************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms, desiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_desiredTime_MetaData), NewProp_desiredTime_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_lastSampleIndex = { "lastSampleIndex", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms, lastSampleIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_lastSampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetLastSampleIndexAtTime Property Definitions ***************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetLastSampleIndexAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetLastSampleIndexAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetLastSampleIndexAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetLastSampleIndexAtTime)
{
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_desiredTime);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_lastSampleIndex);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetLastSampleIndexAtTime(Z_Param_Out_desiredTime,Z_Param_Out_lastSampleIndex);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetLastSampleIndexAtTime ***********************

// ********** Begin Class UHoudiniPointCache Function GetNormalValue *******************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetNormalValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetNormalValue_Parms
	{
		int64 sampleIndex;
		FVector value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Normal Vector3 for a given point in the point cache (converted to unreal's coordinate system)\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Normal Vector3 for a given point in the point cache (converted to unreal's coordinate system)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetNormalValue constinit property declarations ************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetNormalValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetNormalValue constinit property declarations **************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetNormalValue Property Definitions ***********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetNormalValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetNormalValue_Parms, value), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetNormalValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetNormalValue Property Definitions *************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetNormalValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetNormalValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetNormalValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetNormalValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetNormalValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetNormalValue(Z_Param_Out_sampleIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetNormalValue *********************************

// ********** Begin Class UHoudiniPointCache Function GetNumberOfAttributes ************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfAttributes_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetNumberOfAttributes_Parms
	{
		int32 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the number of attributes found in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the number of attributes found in the point cache" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetNumberOfAttributes constinit property declarations *****************
	static const UECodeGen_Private::FIntPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetNumberOfAttributes constinit property declarations *******************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetNumberOfAttributes Property Definitions ****************************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetNumberOfAttributes_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetNumberOfAttributes Property Definitions ******************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetNumberOfAttributes", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetNumberOfAttributes_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetNumberOfAttributes_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfAttributes(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetNumberOfAttributes)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int32*)Z_Param__Result=P_THIS->GetNumberOfAttributes();
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetNumberOfAttributes **************************

// ********** Begin Class UHoudiniPointCache Function GetNumberOfPoints ****************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfPoints_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetNumberOfPoints_Parms
	{
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the number of points found in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the number of points found in the point cache" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetNumberOfPoints constinit property declarations *********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetNumberOfPoints constinit property declarations ***********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetNumberOfPoints Property Definitions ********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetNumberOfPoints_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetNumberOfPoints Property Definitions **********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetNumberOfPoints", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetNumberOfPoints_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetNumberOfPoints_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfPoints(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetNumberOfPoints)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=P_THIS->GetNumberOfPoints();
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetNumberOfPoints ******************************

// ********** Begin Class UHoudiniPointCache Function GetNumberOfSamples ***************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfSamples_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetNumberOfSamples_Parms
	{
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the number of samples found in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the number of samples found in the point cache" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetNumberOfSamples constinit property declarations ********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetNumberOfSamples constinit property declarations **********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetNumberOfSamples Property Definitions *******************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetNumberOfSamples_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetNumberOfSamples Property Definitions *********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetNumberOfSamples", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetNumberOfSamples_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetNumberOfSamples_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfSamples(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetNumberOfSamples)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=P_THIS->GetNumberOfSamples();
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetNumberOfSamples *****************************

// ********** Begin Class UHoudiniPointCache Function GetPointFloatValueAtTime *********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointFloatValueAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointFloatValueAtTime_Parms
	{
		int64 PointID;
		int32 AttributeIndex;
		float desiredTime;
		float Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointFloatValueAtTime constinit property declarations **************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointFloatValueAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointFloatValueAtTime constinit property declarations ****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointFloatValueAtTime Property Definitions *************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointFloatValueAtTime_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointFloatValueAtTime_Parms, AttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointFloatValueAtTime_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointFloatValueAtTime_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointFloatValueAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointFloatValueAtTime Property Definitions ***************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointFloatValueAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointFloatValueAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointFloatValueAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointFloatValueAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointFloatValueAtTime)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FIntProperty,Z_Param_AttributeIndex);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointFloatValueAtTime(Z_Param_PointID,Z_Param_AttributeIndex,Z_Param_desiredTime,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointFloatValueAtTime ***********************

// ********** Begin Class UHoudiniPointCache Function GetPointIDsToSpawnAtTime *********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointIDsToSpawnAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms
	{
		float desiredTime;
		int64 MinID;
		int64 MaxID;
		int64 Count;
		int64 LastSpawnedPointID;
		float LastSpawnTime;
		float LastSpawnTimeRequest;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_desiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointIDsToSpawnAtTime constinit property declarations **************
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_MinID;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_MaxID;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_Count;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_LastSpawnedPointID;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_LastSpawnTime;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_LastSpawnTimeRequest;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointIDsToSpawnAtTime constinit property declarations ****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointIDsToSpawnAtTime Property Definitions *************************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, desiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_desiredTime_MetaData), NewProp_desiredTime_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_MinID = { "MinID", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, MinID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_MaxID = { "MaxID", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, MaxID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_Count = { "Count", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, Count), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_LastSpawnedPointID = { "LastSpawnedPointID", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, LastSpawnedPointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_LastSpawnTime = { "LastSpawnTime", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, LastSpawnTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_LastSpawnTimeRequest = { "LastSpawnTimeRequest", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms, LastSpawnTimeRequest), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MinID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MaxID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Count,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LastSpawnedPointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LastSpawnTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LastSpawnTimeRequest,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointIDsToSpawnAtTime Property Definitions ***************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointIDsToSpawnAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointIDsToSpawnAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointIDsToSpawnAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointIDsToSpawnAtTime)
{
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_desiredTime);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_MinID);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_MaxID);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_Count);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_LastSpawnedPointID);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_LastSpawnTime);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_LastSpawnTimeRequest);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointIDsToSpawnAtTime(Z_Param_Out_desiredTime,Z_Param_Out_MinID,Z_Param_Out_MaxID,Z_Param_Out_Count,Z_Param_Out_LastSpawnedPointID,Z_Param_Out_LastSpawnTime,Z_Param_Out_LastSpawnTimeRequest);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointIDsToSpawnAtTime ***********************

// ********** Begin Class UHoudiniPointCache Function GetPointInt32ValueAtTime *********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointInt32ValueAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms
	{
		int64 PointID;
		int32 AttributeIndex;
		float desiredTime;
		int32 Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Return the integer value of the point at the keyframe before the desired time. No value interpolation will take place.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Return the integer value of the point at the keyframe before the desired time. No value interpolation will take place." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointInt32ValueAtTime constinit property declarations **************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointInt32ValueAtTime constinit property declarations ****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointInt32ValueAtTime Property Definitions *************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms, AttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointInt32ValueAtTime Property Definitions ***************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointInt32ValueAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointInt32ValueAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointInt32ValueAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointInt32ValueAtTime)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FIntProperty,Z_Param_AttributeIndex);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointInt32ValueAtTime(Z_Param_PointID,Z_Param_AttributeIndex,Z_Param_desiredTime,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointInt32ValueAtTime ***********************

// ********** Begin Class UHoudiniPointCache Function GetPointLife *********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointLife_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointLife_Parms
	{
		int64 PointID;
		float Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Return a given point's life value at spawn\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Return a given point's life value at spawn" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointLife constinit property declarations **************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointLife_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointLife constinit property declarations ****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointLife Property Definitions *************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointLife_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointLife_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointLife_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointLife Property Definitions ***************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointLife", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointLife_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointLife_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointLife(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointLife)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointLife(Z_Param_Out_PointID,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointLife ***********************************

// ********** Begin Class UHoudiniPointCache Function GetPointLifeAtTime ***************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointLifeAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointLifeAtTime_Parms
	{
		int64 PointID;
		float DesiredTime;
		float Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Return a point's life for a given time value\n// Note this function currently behaves exactly the same as GetPointLife\n// since the Lifetime value is currently treated as a constant. This could\n// change in the future.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Return a point's life for a given time value\nNote this function currently behaves exactly the same as GetPointLife\nsince the Lifetime value is currently treated as a constant. This could\nchange in the future." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DesiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointLifeAtTime constinit property declarations ********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_DesiredTime;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointLifeAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointLifeAtTime constinit property declarations **********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointLifeAtTime Property Definitions *******************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointLifeAtTime_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_DesiredTime = { "DesiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointLifeAtTime_Parms, DesiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DesiredTime_MetaData), NewProp_DesiredTime_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointLifeAtTime_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointLifeAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DesiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointLifeAtTime Property Definitions *********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointLifeAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointLifeAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointLifeAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointLifeAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointLifeAtTime)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_DesiredTime);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointLifeAtTime(Z_Param_Out_PointID,Z_Param_Out_DesiredTime,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointLifeAtTime *****************************

// ********** Begin Class UHoudiniPointCache Function GetPointPositionAtTime ***********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointPositionAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointPositionAtTime_Parms
	{
		int64 PointID;
		float desiredTime;
		FVector Vector;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Position Value for a given point at a given time value (linearly interpolated)\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Position Value for a given point at a given time value (linearly interpolated)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_desiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointPositionAtTime constinit property declarations ****************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Vector;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointPositionAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointPositionAtTime constinit property declarations ******************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointPositionAtTime Property Definitions ***************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointPositionAtTime_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointPositionAtTime_Parms, desiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_desiredTime_MetaData), NewProp_desiredTime_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Vector = { "Vector", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointPositionAtTime_Parms, Vector), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointPositionAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Vector,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointPositionAtTime Property Definitions *****************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointPositionAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointPositionAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointPositionAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointPositionAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointPositionAtTime)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_desiredTime);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_Vector);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointPositionAtTime(Z_Param_Out_PointID,Z_Param_Out_desiredTime,Z_Param_Out_Vector);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointPositionAtTime *************************

// ********** Begin Class UHoudiniPointCache Function GetPointQuatValueAtTime **********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointQuatValueAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointQuatValueAtTime_Parms
	{
		int64 PointID;
		int32 AttributeIndex;
		float desiredTime;
		FQuat Quat;
		bool DoHoudiniToUnrealConversion;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Quat Value for a given point at a given time value (linearly interpolated)\n" },
#endif
		{ "CPP_Default_DoHoudiniToUnrealConversion", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Quat Value for a given point at a given time value (linearly interpolated)" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointQuatValueAtTime constinit property declarations ***************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Quat;
	static void NewProp_DoHoudiniToUnrealConversion_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointQuatValueAtTime_Parms*)Obj)->DoHoudiniToUnrealConversion = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoHoudiniToUnrealConversion;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointQuatValueAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointQuatValueAtTime constinit property declarations *****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointQuatValueAtTime Property Definitions **************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTime_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTime_Parms, AttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTime_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Quat = { "Quat", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTime_Parms, Quat), Z_Construct_UScriptStruct_FQuat, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoHoudiniToUnrealConversion = { "DoHoudiniToUnrealConversion", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointQuatValueAtTime_Parms), &UHT_STATICS::NewProp_DoHoudiniToUnrealConversion_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointQuatValueAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Quat,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoHoudiniToUnrealConversion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointQuatValueAtTime Property Definitions ****************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointQuatValueAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointQuatValueAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointQuatValueAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointQuatValueAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointQuatValueAtTime)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FIntProperty,Z_Param_AttributeIndex);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_STRUCT_REF(FQuat,Z_Param_Out_Quat);
	P_GET_UBOOL(Z_Param_DoHoudiniToUnrealConversion);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointQuatValueAtTime(Z_Param_PointID,Z_Param_AttributeIndex,Z_Param_desiredTime,Z_Param_Out_Quat,Z_Param_DoHoudiniToUnrealConversion);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointQuatValueAtTime ************************

// ********** Begin Class UHoudiniPointCache Function GetPointQuatValueAtTimeForString *************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointQuatValueAtTimeForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms
	{
		int64 PointID;
		FString Attribute;
		float desiredTime;
		FQuat Quat;
		bool DoHoudiniToUnrealConversion;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Quat Value for a given point at a given time value (linearly interpolated), via the attribute name\n" },
#endif
		{ "CPP_Default_DoHoudiniToUnrealConversion", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Quat Value for a given point at a given time value (linearly interpolated), via the attribute name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointQuatValueAtTimeForString constinit property declarations ******
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Quat;
	static void NewProp_DoHoudiniToUnrealConversion_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms*)Obj)->DoHoudiniToUnrealConversion = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoHoudiniToUnrealConversion;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointQuatValueAtTimeForString constinit property declarations ********
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointQuatValueAtTimeForString Property Definitions *****************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Quat = { "Quat", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms, Quat), Z_Construct_UScriptStruct_FQuat, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoHoudiniToUnrealConversion = { "DoHoudiniToUnrealConversion", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms), &UHT_STATICS::NewProp_DoHoudiniToUnrealConversion_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Quat,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoHoudiniToUnrealConversion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointQuatValueAtTimeForString Property Definitions *******************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointQuatValueAtTimeForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointQuatValueAtTimeForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointQuatValueAtTimeForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointQuatValueAtTimeForString)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_STRUCT_REF(FQuat,Z_Param_Out_Quat);
	P_GET_UBOOL(Z_Param_DoHoudiniToUnrealConversion);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointQuatValueAtTimeForString(Z_Param_PointID,Z_Param_Attribute,Z_Param_desiredTime,Z_Param_Out_Quat,Z_Param_DoHoudiniToUnrealConversion);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointQuatValueAtTimeForString ***************

// ********** Begin Class UHoudiniPointCache Function GetPointType *********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointType_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointType_Parms
	{
		int64 PointID;
		int32 Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Return a point's type at spawn\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Return a point's type at spawn" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointType constinit property declarations **************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointType_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointType constinit property declarations ****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointType Property Definitions *************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointType_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointType_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointType_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointType Property Definitions ***************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointType", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointType_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointType_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointType(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointType)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointType(Z_Param_Out_PointID,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointType ***********************************

// ********** Begin Class UHoudiniPointCache Function GetPointValueAtTime **************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointValueAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointValueAtTime_Parms
	{
		int64 PointID;
		int32 AttributeIndex;
		float desiredTime;
		float Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the value for a point at a given time value (linearly interpolated)\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the value for a point at a given time value (linearly interpolated)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttributeIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_desiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointValueAtTime constinit property declarations *******************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointValueAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointValueAtTime constinit property declarations *********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointValueAtTime Property Definitions ******************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTime_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTime_Parms, AttributeIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttributeIndex_MetaData), NewProp_AttributeIndex_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTime_Parms, desiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_desiredTime_MetaData), NewProp_desiredTime_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTime_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointValueAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointValueAtTime Property Definitions ********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointValueAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointValueAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointValueAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointValueAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointValueAtTime)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_AttributeIndex);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_desiredTime);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointValueAtTime(Z_Param_Out_PointID,Z_Param_Out_AttributeIndex,Z_Param_Out_desiredTime,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointValueAtTime ****************************

// ********** Begin Class UHoudiniPointCache Function GetPointValueAtTimeForString *****************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointValueAtTimeForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointValueAtTimeForString_Parms
	{
		int64 PointID;
		FString Attribute;
		float desiredTime;
		float Value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the value for a point at a given time value (linearly interpolated), via the attribute name\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the value for a point at a given time value (linearly interpolated), via the attribute name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_desiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointValueAtTimeForString constinit property declarations **********
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_Value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointValueAtTimeForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointValueAtTimeForString constinit property declarations ************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointValueAtTimeForString Property Definitions *********************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTimeForString_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTimeForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTimeForString_Parms, desiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_desiredTime_MetaData), NewProp_desiredTime_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointValueAtTimeForString_Parms, Value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointValueAtTimeForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointValueAtTimeForString Property Definitions ***********************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointValueAtTimeForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointValueAtTimeForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointValueAtTimeForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointValueAtTimeForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointValueAtTimeForString)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_desiredTime);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_Value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointValueAtTimeForString(Z_Param_Out_PointID,Z_Param_Attribute,Z_Param_Out_desiredTime,Z_Param_Out_Value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointValueAtTimeForString *******************

// ********** Begin Class UHoudiniPointCache Function GetPointVector4ValueAtTime *******************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointVector4ValueAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms
	{
		int64 PointID;
		int32 AttributeIndex;
		float desiredTime;
		FVector4 Vector;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Vector4 Value for a given point at a given time value (linearly interpolated)\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Vector4 Value for a given point at a given time value (linearly interpolated)" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointVector4ValueAtTime constinit property declarations ************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Vector;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointVector4ValueAtTime constinit property declarations **************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointVector4ValueAtTime Property Definitions ***********************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms, AttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Vector = { "Vector", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms, Vector), Z_Construct_UScriptStruct_FVector4, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Vector,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointVector4ValueAtTime Property Definitions *************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointVector4ValueAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointVector4ValueAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointVector4ValueAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointVector4ValueAtTime)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FIntProperty,Z_Param_AttributeIndex);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_STRUCT_REF(FVector4,Z_Param_Out_Vector);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointVector4ValueAtTime(Z_Param_PointID,Z_Param_AttributeIndex,Z_Param_desiredTime,Z_Param_Out_Vector);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointVector4ValueAtTime *********************

// ********** Begin Class UHoudiniPointCache Function GetPointVector4ValueAtTimeForString **********
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointVector4ValueAtTimeForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms
	{
		int64 PointID;
		FString Attribute;
		float desiredTime;
		FVector4 Vector;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Vector4 Value for a given point at a given time value (linearly interpolated), via the attribute name\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Vector4 Value for a given point at a given time value (linearly interpolated), via the attribute name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointVector4ValueAtTimeForString constinit property declarations ***
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Vector;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointVector4ValueAtTimeForString constinit property declarations *****
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointVector4ValueAtTimeForString Property Definitions **************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Vector = { "Vector", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms, Vector), Z_Construct_UScriptStruct_FVector4, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Vector,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointVector4ValueAtTimeForString Property Definitions ****************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointVector4ValueAtTimeForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointVector4ValueAtTimeForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointVector4ValueAtTimeForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointVector4ValueAtTimeForString)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_STRUCT_REF(FVector4,Z_Param_Out_Vector);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointVector4ValueAtTimeForString(Z_Param_PointID,Z_Param_Attribute,Z_Param_desiredTime,Z_Param_Out_Vector);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointVector4ValueAtTimeForString ************

// ********** Begin Class UHoudiniPointCache Function GetPointVectorValueAtTime ********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointVectorValueAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointVectorValueAtTime_Parms
	{
		int64 PointID;
		int32 AttributeIndex;
		float desiredTime;
		FVector Vector;
		bool DoSwap;
		bool DoScale;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Vector Value for a given point at a given time value (linearly interpolated)\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Vector Value for a given point at a given time value (linearly interpolated)" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointVectorValueAtTime constinit property declarations *************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FIntPropertyParams NewProp_AttributeIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Vector;
	static void NewProp_DoSwap_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVectorValueAtTime_Parms*)Obj)->DoSwap = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoSwap;
	static void NewProp_DoScale_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVectorValueAtTime_Parms*)Obj)->DoScale = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoScale;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVectorValueAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointVectorValueAtTime constinit property declarations ***************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointVectorValueAtTime Property Definitions ************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_AttributeIndex = { "AttributeIndex", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms, AttributeIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Vector = { "Vector", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms, Vector), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoSwap = { "DoSwap", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms), &UHT_STATICS::NewProp_DoSwap_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoScale = { "DoScale", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms), &UHT_STATICS::NewProp_DoScale_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVectorValueAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Vector,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoSwap,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoScale,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointVectorValueAtTime Property Definitions **************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointVectorValueAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointVectorValueAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointVectorValueAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointVectorValueAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointVectorValueAtTime)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FIntProperty,Z_Param_AttributeIndex);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_Vector);
	P_GET_UBOOL(Z_Param_DoSwap);
	P_GET_UBOOL(Z_Param_DoScale);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointVectorValueAtTime(Z_Param_PointID,Z_Param_AttributeIndex,Z_Param_desiredTime,Z_Param_Out_Vector,Z_Param_DoSwap,Z_Param_DoScale);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointVectorValueAtTime **********************

// ********** Begin Class UHoudiniPointCache Function GetPointVectorValueAtTimeForString ***********
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPointVectorValueAtTimeForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms
	{
		int64 PointID;
		FString Attribute;
		float desiredTime;
		FVector Vector;
		bool DoSwap;
		bool DoScale;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the Vector Value for a given point at a given time value (linearly interpolated), via the attribute name\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the Vector Value for a given point at a given time value (linearly interpolated), via the attribute name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPointVectorValueAtTimeForString constinit property declarations ****
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Vector;
	static void NewProp_DoSwap_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms*)Obj)->DoSwap = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoSwap;
	static void NewProp_DoScale_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms*)Obj)->DoScale = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoScale;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPointVectorValueAtTimeForString constinit property declarations ******
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPointVectorValueAtTimeForString Property Definitions ***************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms, PointID), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms, desiredTime), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Vector = { "Vector", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms, Vector), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoSwap = { "DoSwap", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms), &UHT_STATICS::NewProp_DoSwap_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoScale = { "DoScale", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms), &UHT_STATICS::NewProp_DoScale_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Vector,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoSwap,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoScale,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPointVectorValueAtTimeForString Property Definitions *****************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPointVectorValueAtTimeForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPointVectorValueAtTimeForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPointVectorValueAtTimeForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPointVectorValueAtTimeForString)
{
	P_GET_PROPERTY(FInt64Property,Z_Param_PointID);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_PROPERTY(FFloatProperty,Z_Param_desiredTime);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_Vector);
	P_GET_UBOOL(Z_Param_DoSwap);
	P_GET_UBOOL(Z_Param_DoScale);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPointVectorValueAtTimeForString(Z_Param_PointID,Z_Param_Attribute,Z_Param_desiredTime,Z_Param_Out_Vector,Z_Param_DoSwap,Z_Param_DoScale);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPointVectorValueAtTimeForString *************

// ********** Begin Class UHoudiniPointCache Function GetPositionValue *****************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetPositionValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetPositionValue_Parms
	{
		int64 sampleIndex;
		FVector value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Position Vector3 for a given point in the point cache (converted to unreal's coordinate system)\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Position Vector3 for a given point in the point cache (converted to unreal's coordinate system)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetPositionValue constinit property declarations **********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetPositionValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPositionValue constinit property declarations ************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPositionValue Property Definitions *********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPositionValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetPositionValue_Parms, value), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetPositionValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPositionValue Property Definitions ***********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetPositionValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetPositionValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetPositionValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetPositionValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetPositionValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetPositionValue(Z_Param_Out_sampleIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetPositionValue *******************************

// ********** Begin Class UHoudiniPointCache Function GetQuatValue *********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetQuatValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetQuatValue_Parms
	{
		int64 sampleIndex;
		int32 attrIndex;
		FQuat value;
		bool DoHoudiniToUnrealConversion;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Quat for a given point in the point cache\n" },
#endif
		{ "CPP_Default_DoHoudiniToUnrealConversion", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Quat for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_attrIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DoHoudiniToUnrealConversion_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetQuatValue constinit property declarations **************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FIntPropertyParams NewProp_attrIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_DoHoudiniToUnrealConversion_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetQuatValue_Parms*)Obj)->DoHoudiniToUnrealConversion = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoHoudiniToUnrealConversion;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetQuatValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetQuatValue constinit property declarations ****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetQuatValue Property Definitions *************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetQuatValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_attrIndex = { "attrIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetQuatValue_Parms, attrIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_attrIndex_MetaData), NewProp_attrIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetQuatValue_Parms, value), Z_Construct_UScriptStruct_FQuat, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoHoudiniToUnrealConversion = { "DoHoudiniToUnrealConversion", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetQuatValue_Parms), &UHT_STATICS::NewProp_DoHoudiniToUnrealConversion_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DoHoudiniToUnrealConversion_MetaData), NewProp_DoHoudiniToUnrealConversion_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetQuatValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_attrIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoHoudiniToUnrealConversion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetQuatValue Property Definitions ***************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetQuatValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetQuatValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetQuatValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetQuatValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetQuatValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_attrIndex);
	P_GET_STRUCT_REF(FQuat,Z_Param_Out_value);
	P_GET_UBOOL_REF(Z_Param_Out_DoHoudiniToUnrealConversion);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetQuatValue(Z_Param_Out_sampleIndex,Z_Param_Out_attrIndex,Z_Param_Out_value,Z_Param_Out_DoHoudiniToUnrealConversion);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetQuatValue ***********************************

// ********** Begin Class UHoudiniPointCache Function GetQuatValueForString ************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetQuatValueForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetQuatValueForString_Parms
	{
		int64 sampleIndex;
		FString Attribute;
		FQuat value;
		bool DoHoudiniToUnrealConversion;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Quat for a given point in the point cache by column name\n" },
#endif
		{ "CPP_Default_DoHoudiniToUnrealConversion", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Quat for a given point in the point cache by column name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DoHoudiniToUnrealConversion_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetQuatValueForString constinit property declarations *****************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_DoHoudiniToUnrealConversion_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetQuatValueForString_Parms*)Obj)->DoHoudiniToUnrealConversion = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoHoudiniToUnrealConversion;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetQuatValueForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetQuatValueForString constinit property declarations *******************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetQuatValueForString Property Definitions ****************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetQuatValueForString_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetQuatValueForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetQuatValueForString_Parms, value), Z_Construct_UScriptStruct_FQuat, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoHoudiniToUnrealConversion = { "DoHoudiniToUnrealConversion", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetQuatValueForString_Parms), &UHT_STATICS::NewProp_DoHoudiniToUnrealConversion_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DoHoudiniToUnrealConversion_MetaData), NewProp_DoHoudiniToUnrealConversion_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetQuatValueForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoHoudiniToUnrealConversion,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetQuatValueForString Property Definitions ******************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetQuatValueForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetQuatValueForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetQuatValueForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetQuatValueForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetQuatValueForString)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_STRUCT_REF(FQuat,Z_Param_Out_value);
	P_GET_UBOOL_REF(Z_Param_Out_DoHoudiniToUnrealConversion);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetQuatValueForString(Z_Param_Out_sampleIndex,Z_Param_Attribute,Z_Param_Out_value,Z_Param_Out_DoHoudiniToUnrealConversion);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetQuatValueForString **************************

// ********** Begin Class UHoudiniPointCache Function GetSampleIndexesForPointAtTime ***************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetSampleIndexesForPointAtTime_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms
	{
		int64 PointID;
		float desiredTime;
		int64 PrevSampleIndex;
		int64 NextSampleIndex;
		float PrevWeight;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns the previous and next sample indexes for reading the values of a specified point at a given time\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns the previous and next sample indexes for reading the values of a specified point at a given time" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointID_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_desiredTime_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetSampleIndexesForPointAtTime constinit property declarations ********
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PointID;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_desiredTime;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_PrevSampleIndex;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_NextSampleIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_PrevWeight;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetSampleIndexesForPointAtTime constinit property declarations **********
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetSampleIndexesForPointAtTime Property Definitions *******************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PointID = { "PointID", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms, PointID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointID_MetaData), NewProp_PointID_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_desiredTime = { "desiredTime", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms, desiredTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_desiredTime_MetaData), NewProp_desiredTime_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_PrevSampleIndex = { "PrevSampleIndex", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms, PrevSampleIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_NextSampleIndex = { "NextSampleIndex", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms, NextSampleIndex), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_PrevWeight = { "PrevWeight", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms, PrevWeight), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_desiredTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PrevSampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NextSampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PrevWeight,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetSampleIndexesForPointAtTime Property Definitions *********************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetSampleIndexesForPointAtTime", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetSampleIndexesForPointAtTime_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetSampleIndexesForPointAtTime(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetSampleIndexesForPointAtTime)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PointID);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_desiredTime);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_PrevSampleIndex);
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_NextSampleIndex);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_PrevWeight);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetSampleIndexesForPointAtTime(Z_Param_Out_PointID,Z_Param_Out_desiredTime,Z_Param_Out_PrevSampleIndex,Z_Param_Out_NextSampleIndex,Z_Param_Out_PrevWeight);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetSampleIndexesForPointAtTime *****************

// ********** Begin Class UHoudiniPointCache Function GetTimeValue *********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetTimeValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetTimeValue_Parms
	{
		int64 sampleIndex;
		float value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a time value for a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a time value for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetTimeValue constinit property declarations **************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetTimeValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetTimeValue constinit property declarations ****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetTimeValue Property Definitions *************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetTimeValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetTimeValue_Parms, value), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetTimeValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetTimeValue Property Definitions ***************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetTimeValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetTimeValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetTimeValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetTimeValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetTimeValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY_REF(FFloatProperty,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetTimeValue(Z_Param_Out_sampleIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetTimeValue ***********************************

// ********** Begin Class UHoudiniPointCache Function GetUseCustomCSVTitleRow **********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetUseCustomCSVTitleRow_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetUseCustomCSVTitleRow_Parms
	{
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Point Cache Settings" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetUseCustomCSVTitleRow constinit property declarations ***************
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetUseCustomCSVTitleRow_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetUseCustomCSVTitleRow constinit property declarations *****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetUseCustomCSVTitleRow Property Definitions **************************
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetUseCustomCSVTitleRow_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetUseCustomCSVTitleRow Property Definitions ****************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetUseCustomCSVTitleRow", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetUseCustomCSVTitleRow_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetUseCustomCSVTitleRow_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetUseCustomCSVTitleRow(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetUseCustomCSVTitleRow)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetUseCustomCSVTitleRow();
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetUseCustomCSVTitleRow ************************

// ********** Begin Class UHoudiniPointCache Function GetVector4Value ******************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetVector4Value_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetVector4Value_Parms
	{
		int64 sampleIndex;
		int32 attrIndex;
		FVector4 value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Vector4 for a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Vector4 for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_attrIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetVector4Value constinit property declarations ***********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FIntPropertyParams NewProp_attrIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVector4Value_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetVector4Value constinit property declarations *************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetVector4Value Property Definitions **********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVector4Value_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_attrIndex = { "attrIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVector4Value_Parms, attrIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_attrIndex_MetaData), NewProp_attrIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVector4Value_Parms, value), Z_Construct_UScriptStruct_FVector4, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVector4Value_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_attrIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetVector4Value Property Definitions ************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetVector4Value", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetVector4Value_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetVector4Value_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetVector4Value(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetVector4Value)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_attrIndex);
	P_GET_STRUCT_REF(FVector4,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetVector4Value(Z_Param_Out_sampleIndex,Z_Param_Out_attrIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetVector4Value ********************************

// ********** Begin Class UHoudiniPointCache Function GetVector4ValueForString *********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetVector4ValueForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetVector4ValueForString_Parms
	{
		int64 sampleIndex;
		FString Attribute;
		FVector4 value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Vector4 for a given point in the point cache by column name\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Vector4 for a given point in the point cache by column name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetVector4ValueForString constinit property declarations **************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVector4ValueForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetVector4ValueForString constinit property declarations ****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetVector4ValueForString Property Definitions *************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVector4ValueForString_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVector4ValueForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVector4ValueForString_Parms, value), Z_Construct_UScriptStruct_FVector4, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVector4ValueForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetVector4ValueForString Property Definitions ***************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetVector4ValueForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetVector4ValueForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetVector4ValueForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetVector4ValueForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetVector4ValueForString)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_STRUCT_REF(FVector4,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetVector4ValueForString(Z_Param_Out_sampleIndex,Z_Param_Attribute,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetVector4ValueForString ***********************

// ********** Begin Class UHoudiniPointCache Function GetVectorValue *******************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetVectorValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetVectorValue_Parms
	{
		int64 sampleIndex;
		int32 attrIndex;
		FVector value;
		bool DoSwap;
		bool DoScale;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/*\n\x09// Returns the string value at a given point in the point cache\n\x09""bool GetCSVStringValue( const int64& sampleIndex, const int32& attrIndex, FString& value );\n\x09// Returns the string value at a given point in the point cache\n\x09""bool GetCSVStringValue( const int64& sampleIndex, const FString& Attribute, FString& value );\n\x09*/// Returns a Vector3 for a given point in the point cache\n" },
#endif
		{ "CPP_Default_DoScale", "true" },
		{ "CPP_Default_DoSwap", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "// Returns the string value at a given point in the point cache\nbool GetCSVStringValue( const int64& sampleIndex, const int32& attrIndex, FString& value );\n// Returns the string value at a given point in the point cache\nbool GetCSVStringValue( const int64& sampleIndex, const FString& Attribute, FString& value );\n// Returns a Vector3 for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_attrIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DoSwap_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DoScale_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetVectorValue constinit property declarations ************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FIntPropertyParams NewProp_attrIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_DoSwap_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVectorValue_Parms*)Obj)->DoSwap = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoSwap;
	static void NewProp_DoScale_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVectorValue_Parms*)Obj)->DoScale = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoScale;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVectorValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetVectorValue constinit property declarations **************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetVectorValue Property Definitions ***********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVectorValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_attrIndex = { "attrIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVectorValue_Parms, attrIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_attrIndex_MetaData), NewProp_attrIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVectorValue_Parms, value), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoSwap = { "DoSwap", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVectorValue_Parms), &UHT_STATICS::NewProp_DoSwap_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DoSwap_MetaData), NewProp_DoSwap_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoScale = { "DoScale", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVectorValue_Parms), &UHT_STATICS::NewProp_DoScale_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DoScale_MetaData), NewProp_DoScale_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVectorValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_attrIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoSwap,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoScale,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetVectorValue Property Definitions *************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetVectorValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetVectorValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetVectorValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetVectorValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetVectorValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY_REF(FIntProperty,Z_Param_Out_attrIndex);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_value);
	P_GET_UBOOL_REF(Z_Param_Out_DoSwap);
	P_GET_UBOOL_REF(Z_Param_Out_DoScale);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetVectorValue(Z_Param_Out_sampleIndex,Z_Param_Out_attrIndex,Z_Param_Out_value,Z_Param_Out_DoSwap,Z_Param_Out_DoScale);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetVectorValue *********************************

// ********** Begin Class UHoudiniPointCache Function GetVectorValueForString **********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetVectorValueForString_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetVectorValueForString_Parms
	{
		int64 sampleIndex;
		FString Attribute;
		FVector value;
		bool DoSwap;
		bool DoScale;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Vector3 for a given point in the point cache by column name\n" },
#endif
		{ "CPP_Default_DoScale", "true" },
		{ "CPP_Default_DoSwap", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Vector3 for a given point in the point cache by column name" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Attribute_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DoSwap_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_DoScale_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetVectorValueForString constinit property declarations ***************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Attribute;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_DoSwap_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVectorValueForString_Parms*)Obj)->DoSwap = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoSwap;
	static void NewProp_DoScale_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVectorValueForString_Parms*)Obj)->DoScale = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_DoScale;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVectorValueForString_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetVectorValueForString constinit property declarations *****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetVectorValueForString Property Definitions **************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVectorValueForString_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_Attribute = { "Attribute", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVectorValueForString_Parms, Attribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Attribute_MetaData), NewProp_Attribute_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVectorValueForString_Parms, value), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoSwap = { "DoSwap", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVectorValueForString_Parms), &UHT_STATICS::NewProp_DoSwap_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DoSwap_MetaData), NewProp_DoSwap_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_DoScale = { "DoScale", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVectorValueForString_Parms), &UHT_STATICS::NewProp_DoScale_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_DoScale_MetaData), NewProp_DoScale_MetaData) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVectorValueForString_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Attribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoSwap,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_DoScale,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetVectorValueForString Property Definitions ****************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetVectorValueForString", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetVectorValueForString_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetVectorValueForString_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetVectorValueForString(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetVectorValueForString)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_PROPERTY(FStrProperty,Z_Param_Attribute);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_value);
	P_GET_UBOOL_REF(Z_Param_Out_DoSwap);
	P_GET_UBOOL_REF(Z_Param_Out_DoScale);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetVectorValueForString(Z_Param_Out_sampleIndex,Z_Param_Attribute,Z_Param_Out_value,Z_Param_Out_DoSwap,Z_Param_Out_DoScale);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetVectorValueForString ************************

// ********** Begin Class UHoudiniPointCache Function GetVelocityValue *****************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_GetVelocityValue_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventGetVelocityValue_Parms
	{
		int64 sampleIndex;
		FVector value;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Attributes Data" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Returns a Velocity Vector3 for a given point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Returns a Velocity Vector3 for a given point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_sampleIndex_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetVelocityValue constinit property declarations **********************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_sampleIndex;
	static const UECodeGen_Private::FStructPropertyParams NewProp_value;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventGetVelocityValue_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetVelocityValue constinit property declarations ************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetVelocityValue Property Definitions *********************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_sampleIndex = { "sampleIndex", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVelocityValue_Parms, sampleIndex), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_sampleIndex_MetaData), NewProp_sampleIndex_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_value = { "value", nullptr, (EPropertyFlags)0x0010000000000180, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniPointCache_eventGetVelocityValue_Parms, value), Z_Construct_UScriptStruct_FVector, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventGetVelocityValue_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_sampleIndex,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_value,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetVelocityValue Property Definitions ***********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "GetVelocityValue", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventGetVelocityValue_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54C20401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventGetVelocityValue_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_GetVelocityValue(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execGetVelocityValue)
{
	P_GET_PROPERTY_REF(FInt64Property,Z_Param_Out_sampleIndex);
	P_GET_STRUCT_REF(FVector,Z_Param_Out_value);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->GetVelocityValue(Z_Param_Out_sampleIndex,Z_Param_Out_value);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function GetVelocityValue *******************************

// ********** Begin Class UHoudiniPointCache Function SetUseCustomCSVTitleRow **********************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UHoudiniPointCache_SetUseCustomCSVTitleRow_Statics
struct UHT_STATICS
{
	struct HoudiniPointCache_eventSetUseCustomCSVTitleRow_Parms
	{
		bool bInUseCustomCSVTitleRow;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Houdini Point Cache Settings" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
#endif // WITH_METADATA

// ********** Begin Function SetUseCustomCSVTitleRow constinit property declarations ***************
	static void NewProp_bInUseCustomCSVTitleRow_SetBit(void* Obj)
	{
		((HoudiniPointCache_eventSetUseCustomCSVTitleRow_Parms*)Obj)->bInUseCustomCSVTitleRow = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bInUseCustomCSVTitleRow;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function SetUseCustomCSVTitleRow constinit property declarations *****************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function SetUseCustomCSVTitleRow Property Definitions **************************
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bInUseCustomCSVTitleRow = { "bInUseCustomCSVTitleRow", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(HoudiniPointCache_eventSetUseCustomCSVTitleRow_Parms), &UHT_STATICS::NewProp_bInUseCustomCSVTitleRow_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bInUseCustomCSVTitleRow,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function SetUseCustomCSVTitleRow Property Definitions ****************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UHoudiniPointCache, nullptr, "SetUseCustomCSVTitleRow", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::HoudiniPointCache_eventSetUseCustomCSVTitleRow_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::HoudiniPointCache_eventSetUseCustomCSVTitleRow_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UHoudiniPointCache_SetUseCustomCSVTitleRow(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UHoudiniPointCache::execSetUseCustomCSVTitleRow)
{
	P_GET_UBOOL(Z_Param_bInUseCustomCSVTitleRow);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetUseCustomCSVTitleRow(Z_Param_bInUseCustomCSVTitleRow);
	P_NATIVE_END;
}
// ********** End Class UHoudiniPointCache Function SetUseCustomCSVTitleRow ************************

// ********** Begin Class UHoudiniPointCache *******************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UHoudiniPointCache_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "HoudiniPointCache.h" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FileName_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "//-----------------------------------------------------------------------------------------\n//  MEMBER VARIABLES\n//-----------------------------------------------------------------------------------------\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "MEMBER VARIABLES" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NumberOfSamples_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The number of values stored in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The number of values stored in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NumberOfAttributes_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The number of attributes stored in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The number of attributes stored in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NumberOfPoints_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The number of unique points found in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The number of unique points found in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NumberOfFrames_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The number of frames imported into the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The number of frames imported into the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FirstFrame_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The first frame of the exported frame range\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The first frame of the exported frame range" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LastFrame_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The last frame of the exported frame range\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The last frame of the exported frame range" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MinSampleTime_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The minimum sample time value, in seconds, in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The minimum sample time value, in seconds, in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxSampleTime_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The maximum sample time value, in seconds, in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The maximum sample time value, in seconds, in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SourceCSVTitleRow_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The source title row for CSV files, describing the content of each column and used to locate specific values in the point cache.\n// Editing this will trigger a re-import of the point cache.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The source title row for CSV files, describing the content of each column and used to locate specific values in the point cache.\nEditing this will trigger a re-import of the point cache." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttributeArray_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The final attribute names used by the asset after parsing.\n// Packed vector values are expanded, so additional attributes (.0, .1, ... or .x, .y, .z) might have been inserted.\n// Use the indexes in this array to access your data.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The final attribute names used by the asset after parsing.\nPacked vector values are expanded, so additional attributes (.0, .1, ... or .x, .y, .z) might have been inserted.\nUse the indexes in this array to access your data." },
#endif
	};
#if WITH_EDITORONLY_DATA
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AssetImportData_MetaData[] = {
		{ "Category", "ImportSettings" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Importing data and options used for this asset */" },
#endif
		{ "EditInline", "true" },
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Importing data and options used for this asset" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RawDataCompressed_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Raw data of the source file so that we can export it again.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Raw data of the source file so that we can export it again." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RawDataFormatID_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Compression scheme used to compress raw \n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Compression scheme used to compress raw" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RawDataUncompressedSize_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Size of data when uncompressed\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Size of data when uncompressed" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RawDataCompressionMethod_MetaData[] = {
		{ "Category", "Houdini Point Cache Properties" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Compression scheme used to compress raw \n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Compression scheme used to compress raw" },
#endif
	};
#endif // WITH_EDITORONLY_DATA
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bLargeFile_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Indicates that we're using a large file\n// Source Data is stored in BulkRawData\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Indicates that we're using a large file\nSource Data is stored in BulkRawData" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FloatSampleData_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Array containing all the sample data converted to floats\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Array containing all the sample data converted to floats" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SpawnTimes_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Array containing the spawn times for each point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Array containing the spawn times for each point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LifeValues_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Array containing all the life values for each point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Array containing all the life values for each point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointTypes_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Array containing all the type values for each point in the point cache\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Array containing all the type values for each point in the point cache" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SpecialAttributeIndexes_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Array containing the column indexes of the special attributes\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Array containing the column indexes of the special attributes" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PointValueIndexes_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Sample indexes for each point\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Sample indexes for each point" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_UseCustomCSVTitleRow_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** For CSV source files, whether to use a custom title row. */" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "For CSV source files, whether to use a custom title row." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FileType_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// The type of source file, such as CSV or JSON.\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniPointCache.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The type of source file, such as CSV or JSON." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UHoudiniPointCache constinit property declarations ***********************
	static const UECodeGen_Private::FStrPropertyParams NewProp_FileName;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_NumberOfSamples;
	static const UECodeGen_Private::FIntPropertyParams NewProp_NumberOfAttributes;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_NumberOfPoints;
	static const UECodeGen_Private::FIntPropertyParams NewProp_NumberOfFrames;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_FirstFrame;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_LastFrame;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_MinSampleTime;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_MaxSampleTime;
	static const UECodeGen_Private::FStrPropertyParams NewProp_SourceCSVTitleRow;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AttributeArray_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_AttributeArray;
#if WITH_EDITORONLY_DATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_AssetImportData;
	static const UECodeGen_Private::FBytePropertyParams NewProp_RawDataCompressed_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_RawDataCompressed;
	static const UECodeGen_Private::FNamePropertyParams NewProp_RawDataFormatID;
	static const UECodeGen_Private::FUInt64PropertyParams NewProp_RawDataUncompressedSize;
	static const UECodeGen_Private::FNamePropertyParams NewProp_RawDataCompressionMethod;
#endif // WITH_EDITORONLY_DATA
	static void NewProp_bLargeFile_SetBit(void* Obj)
	{
		((UHoudiniPointCache*)Obj)->bLargeFile = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bLargeFile;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_FloatSampleData_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_FloatSampleData;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_SpawnTimes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_SpawnTimes;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_LifeValues_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_LifeValues;
	static const UECodeGen_Private::FIntPropertyParams NewProp_PointTypes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_PointTypes;
	static const UECodeGen_Private::FIntPropertyParams NewProp_SpecialAttributeIndexes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_SpecialAttributeIndexes;
	static const UECodeGen_Private::FStructPropertyParams NewProp_PointValueIndexes_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_PointValueIndexes;
	static void NewProp_UseCustomCSVTitleRow_SetBit(void* Obj)
	{
		((UHoudiniPointCache*)Obj)->UseCustomCSVTitleRow = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_UseCustomCSVTitleRow;
	static const UECodeGen_Private::FBytePropertyParams NewProp_FileType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_FileType;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UHoudiniPointCache constinit property declarations *************************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("GetAttributeIndexFromString"), .Pointer = &UHoudiniPointCache::execGetAttributeIndexFromString },
		{ .NameUTF8 = UTF8TEXT("GetAttributeIndexInArrayFromString"), .Pointer = &UHoudiniPointCache::execGetAttributeIndexInArrayFromString },
		{ .NameUTF8 = UTF8TEXT("GetColorValue"), .Pointer = &UHoudiniPointCache::execGetColorValue },
		{ .NameUTF8 = UTF8TEXT("GetFloatValue"), .Pointer = &UHoudiniPointCache::execGetFloatValue },
		{ .NameUTF8 = UTF8TEXT("GetFloatValueForString"), .Pointer = &UHoudiniPointCache::execGetFloatValueForString },
		{ .NameUTF8 = UTF8TEXT("GetImpulseValue"), .Pointer = &UHoudiniPointCache::execGetImpulseValue },
		{ .NameUTF8 = UTF8TEXT("GetLastPointIDToSpawnAtTime"), .Pointer = &UHoudiniPointCache::execGetLastPointIDToSpawnAtTime },
		{ .NameUTF8 = UTF8TEXT("GetLastSampleIndexAtTime"), .Pointer = &UHoudiniPointCache::execGetLastSampleIndexAtTime },
		{ .NameUTF8 = UTF8TEXT("GetNormalValue"), .Pointer = &UHoudiniPointCache::execGetNormalValue },
		{ .NameUTF8 = UTF8TEXT("GetNumberOfAttributes"), .Pointer = &UHoudiniPointCache::execGetNumberOfAttributes },
		{ .NameUTF8 = UTF8TEXT("GetNumberOfPoints"), .Pointer = &UHoudiniPointCache::execGetNumberOfPoints },
		{ .NameUTF8 = UTF8TEXT("GetNumberOfSamples"), .Pointer = &UHoudiniPointCache::execGetNumberOfSamples },
		{ .NameUTF8 = UTF8TEXT("GetPointFloatValueAtTime"), .Pointer = &UHoudiniPointCache::execGetPointFloatValueAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointIDsToSpawnAtTime"), .Pointer = &UHoudiniPointCache::execGetPointIDsToSpawnAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointInt32ValueAtTime"), .Pointer = &UHoudiniPointCache::execGetPointInt32ValueAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointLife"), .Pointer = &UHoudiniPointCache::execGetPointLife },
		{ .NameUTF8 = UTF8TEXT("GetPointLifeAtTime"), .Pointer = &UHoudiniPointCache::execGetPointLifeAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointPositionAtTime"), .Pointer = &UHoudiniPointCache::execGetPointPositionAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointQuatValueAtTime"), .Pointer = &UHoudiniPointCache::execGetPointQuatValueAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointQuatValueAtTimeForString"), .Pointer = &UHoudiniPointCache::execGetPointQuatValueAtTimeForString },
		{ .NameUTF8 = UTF8TEXT("GetPointType"), .Pointer = &UHoudiniPointCache::execGetPointType },
		{ .NameUTF8 = UTF8TEXT("GetPointValueAtTime"), .Pointer = &UHoudiniPointCache::execGetPointValueAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointValueAtTimeForString"), .Pointer = &UHoudiniPointCache::execGetPointValueAtTimeForString },
		{ .NameUTF8 = UTF8TEXT("GetPointVector4ValueAtTime"), .Pointer = &UHoudiniPointCache::execGetPointVector4ValueAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointVector4ValueAtTimeForString"), .Pointer = &UHoudiniPointCache::execGetPointVector4ValueAtTimeForString },
		{ .NameUTF8 = UTF8TEXT("GetPointVectorValueAtTime"), .Pointer = &UHoudiniPointCache::execGetPointVectorValueAtTime },
		{ .NameUTF8 = UTF8TEXT("GetPointVectorValueAtTimeForString"), .Pointer = &UHoudiniPointCache::execGetPointVectorValueAtTimeForString },
		{ .NameUTF8 = UTF8TEXT("GetPositionValue"), .Pointer = &UHoudiniPointCache::execGetPositionValue },
		{ .NameUTF8 = UTF8TEXT("GetQuatValue"), .Pointer = &UHoudiniPointCache::execGetQuatValue },
		{ .NameUTF8 = UTF8TEXT("GetQuatValueForString"), .Pointer = &UHoudiniPointCache::execGetQuatValueForString },
		{ .NameUTF8 = UTF8TEXT("GetSampleIndexesForPointAtTime"), .Pointer = &UHoudiniPointCache::execGetSampleIndexesForPointAtTime },
		{ .NameUTF8 = UTF8TEXT("GetTimeValue"), .Pointer = &UHoudiniPointCache::execGetTimeValue },
		{ .NameUTF8 = UTF8TEXT("GetUseCustomCSVTitleRow"), .Pointer = &UHoudiniPointCache::execGetUseCustomCSVTitleRow },
		{ .NameUTF8 = UTF8TEXT("GetVector4Value"), .Pointer = &UHoudiniPointCache::execGetVector4Value },
		{ .NameUTF8 = UTF8TEXT("GetVector4ValueForString"), .Pointer = &UHoudiniPointCache::execGetVector4ValueForString },
		{ .NameUTF8 = UTF8TEXT("GetVectorValue"), .Pointer = &UHoudiniPointCache::execGetVectorValue },
		{ .NameUTF8 = UTF8TEXT("GetVectorValueForString"), .Pointer = &UHoudiniPointCache::execGetVectorValueForString },
		{ .NameUTF8 = UTF8TEXT("GetVelocityValue"), .Pointer = &UHoudiniPointCache::execGetVelocityValue },
		{ .NameUTF8 = UTF8TEXT("SetUseCustomCSVTitleRow"), .Pointer = &UHoudiniPointCache::execSetUseCustomCSVTitleRow },
	};
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetAttributeIndexFromString, "GetAttributeIndexFromString" }, // 7d836acffe02cf512338093c91c3d649681b82af
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetAttributeIndexInArrayFromString, "GetAttributeIndexInArrayFromString" }, // 019e7dff29a3e0f04549e57dc40973a71a69ff40
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetColorValue, "GetColorValue" }, // b1a4422ad194d67db7cc460f7f5daafc97c6388d
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetFloatValue, "GetFloatValue" }, // 901caab6da3a94020518b054a89ffce5f485229b
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetFloatValueForString, "GetFloatValueForString" }, // 4fce55e2dfacd6669149530cd04ec4d7fef5c798
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetImpulseValue, "GetImpulseValue" }, // 9086c34621ec420cc420147c82fb3634f3becf5e
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetLastPointIDToSpawnAtTime, "GetLastPointIDToSpawnAtTime" }, // 99bcd74d869c804e66ec4f664ca185e13f45512d
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetLastSampleIndexAtTime, "GetLastSampleIndexAtTime" }, // 68268514eeafd2dce539ffc51ab570111cfba3c4
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetNormalValue, "GetNormalValue" }, // d281777d112772f3b810082f79109943442eb184
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfAttributes, "GetNumberOfAttributes" }, // 66dea8f9a42c611e0b648f1315c6a0d80ddf9ed7
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfPoints, "GetNumberOfPoints" }, // 64da66caa3a5e3d5f8b088df0ca144f3503d953a
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetNumberOfSamples, "GetNumberOfSamples" }, // 610f67f45438979a3e05baed4237dd15e785c7d0
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointFloatValueAtTime, "GetPointFloatValueAtTime" }, // 92ff625b91cf7f32f5f971ddea3cebdf4ea196e9
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointIDsToSpawnAtTime, "GetPointIDsToSpawnAtTime" }, // 0170bb929703fbc6ee17e31aaf3ad4f1e5780060
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointInt32ValueAtTime, "GetPointInt32ValueAtTime" }, // f6d980c97539abe4ea54de9be27a78ddd3e4492a
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointLife, "GetPointLife" }, // 1dd8914b8ba927ece3238a491bddddc9e4a8271a
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointLifeAtTime, "GetPointLifeAtTime" }, // 879f30a0d3a1ff9eda1ff7590c99ecad8a2df5ab
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointPositionAtTime, "GetPointPositionAtTime" }, // 7be9bb6fd5c3c57f9e5eea62c00150b436978129
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointQuatValueAtTime, "GetPointQuatValueAtTime" }, // ce3877448f0d1d8233c57741bfa5ee4cece49c5d
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointQuatValueAtTimeForString, "GetPointQuatValueAtTimeForString" }, // 3f0c946a25e6f659d92ad3c04cc977e8224b6707
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointType, "GetPointType" }, // e1772c260260e55ac87924c6fcb521602c2df126
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointValueAtTime, "GetPointValueAtTime" }, // 53ca9820d4dddacecc97c85954d4e38218cd5097
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointValueAtTimeForString, "GetPointValueAtTimeForString" }, // 38030bb974109df974921ff33b4cc033073fc453
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointVector4ValueAtTime, "GetPointVector4ValueAtTime" }, // 32eb7ab740417001a7d7a8552f323b13de8c85fb
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointVector4ValueAtTimeForString, "GetPointVector4ValueAtTimeForString" }, // e716ed145c05c6fb74bcc485bec759d7247e4cb2
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointVectorValueAtTime, "GetPointVectorValueAtTime" }, // 87ae52d72c98ea8348fdc2b88599691d7a23b4c8
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPointVectorValueAtTimeForString, "GetPointVectorValueAtTimeForString" }, // a6315e7d33437a170464d0f93d9d1705ccc414dd
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetPositionValue, "GetPositionValue" }, // 5785a9b6f712398fc43a1145f850ab955a92f427
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetQuatValue, "GetQuatValue" }, // 70f8bac080e79e69a2090e18da387ebeaca3053f
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetQuatValueForString, "GetQuatValueForString" }, // 93786d33d9d27576d6e797fa011d2fe4a73db241
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetSampleIndexesForPointAtTime, "GetSampleIndexesForPointAtTime" }, // 1786097415cb30d0f9d74e1ffa1bfe794db628f5
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetTimeValue, "GetTimeValue" }, // 15deef365611027b800fb4ba6e1e3f1cea4a8cab
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetUseCustomCSVTitleRow, "GetUseCustomCSVTitleRow" }, // 6467f7e0d611083e29ca5e069bc3570725409746
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetVector4Value, "GetVector4Value" }, // 4f2d14b331566bcef982017a0569b25e5dadb589
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetVector4ValueForString, "GetVector4ValueForString" }, // 3f80f4b669ad90e47dd14dee0ecf039673a25e07
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetVectorValue, "GetVectorValue" }, // 86ae5c919efa2ecafaf17d2e741cc6749ff50150
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetVectorValueForString, "GetVectorValueForString" }, // cebe72d5bdce0572d023406321727dbe321c71ae
		{ &Z_Construct_UFunction_UHoudiniPointCache_GetVelocityValue, "GetVelocityValue" }, // 3c3c2dd1f3c3389943e6be59b21332a573017c62
		{ &Z_Construct_UFunction_UHoudiniPointCache_SetUseCustomCSVTitleRow, "SetUseCustomCSVTitleRow" }, // 6cdb89d25684e7205c6f32bdb8768295266c1e5f
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniPointCache>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UHoudiniPointCache Property Definitions **********************************
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_FileName = { "FileName", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, FileName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FileName_MetaData), NewProp_FileName_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_NumberOfSamples = { "NumberOfSamples", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, NumberOfSamples), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NumberOfSamples_MetaData), NewProp_NumberOfSamples_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_NumberOfAttributes = { "NumberOfAttributes", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, NumberOfAttributes), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NumberOfAttributes_MetaData), NewProp_NumberOfAttributes_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_NumberOfPoints = { "NumberOfPoints", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, NumberOfPoints), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NumberOfPoints_MetaData), NewProp_NumberOfPoints_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_NumberOfFrames = { "NumberOfFrames", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, NumberOfFrames), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NumberOfFrames_MetaData), NewProp_NumberOfFrames_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_FirstFrame = { "FirstFrame", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, FirstFrame), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FirstFrame_MetaData), NewProp_FirstFrame_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_LastFrame = { "LastFrame", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, LastFrame), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LastFrame_MetaData), NewProp_LastFrame_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_MinSampleTime = { "MinSampleTime", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, MinSampleTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MinSampleTime_MetaData), NewProp_MinSampleTime_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_MaxSampleTime = { "MaxSampleTime", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, MaxSampleTime), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxSampleTime_MetaData), NewProp_MaxSampleTime_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_SourceCSVTitleRow = { "SourceCSVTitleRow", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, SourceCSVTitleRow), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SourceCSVTitleRow_MetaData), NewProp_SourceCSVTitleRow_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_AttributeArray_Inner = { "AttributeArray", nullptr, (EPropertyFlags)0x0000000000020000, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_AttributeArray = { "AttributeArray", nullptr, (EPropertyFlags)0x0010000000020001, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, AttributeArray), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttributeArray_MetaData), NewProp_AttributeArray_MetaData) };
#if WITH_EDITORONLY_DATA
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_AssetImportData = { "AssetImportData", nullptr, (EPropertyFlags)0x0116000800080009, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, AssetImportData), Z_Construct_UClass_UAssetImportData, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AssetImportData_MetaData), NewProp_AssetImportData_MetaData) };
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_RawDataCompressed_Inner = { "RawDataCompressed", nullptr, (EPropertyFlags)0x0000000800000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_RawDataCompressed = { "RawDataCompressed", nullptr, (EPropertyFlags)0x0010000800000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, RawDataCompressed), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RawDataCompressed_MetaData), NewProp_RawDataCompressed_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_RawDataFormatID = { "RawDataFormatID", nullptr, (EPropertyFlags)0x0010000800020001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, RawDataFormatID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RawDataFormatID_MetaData), NewProp_RawDataFormatID_MetaData) };
const UECodeGen_Private::FUInt64PropertyParams UHT_STATICS::NewProp_RawDataUncompressedSize = { "RawDataUncompressedSize", nullptr, (EPropertyFlags)0x0010000800020001, UECodeGen_Private::EPropertyGenFlags::UInt64, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, RawDataUncompressedSize), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RawDataUncompressedSize_MetaData), NewProp_RawDataUncompressedSize_MetaData) };
const UECodeGen_Private::FNamePropertyParams UHT_STATICS::NewProp_RawDataCompressionMethod = { "RawDataCompressionMethod", nullptr, (EPropertyFlags)0x0010000800020001, UECodeGen_Private::EPropertyGenFlags::Name, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, RawDataCompressionMethod), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RawDataCompressionMethod_MetaData), NewProp_RawDataCompressionMethod_MetaData) };
#endif // WITH_EDITORONLY_DATA
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_bLargeFile = { "bLargeFile", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UHoudiniPointCache), &UHT_STATICS::NewProp_bLargeFile_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bLargeFile_MetaData), NewProp_bLargeFile_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_FloatSampleData_Inner = { "FloatSampleData", nullptr, (EPropertyFlags)0x0000000020000000, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_FloatSampleData = { "FloatSampleData", nullptr, (EPropertyFlags)0x0040000020000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, FloatSampleData_DEPRECATED), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FloatSampleData_MetaData), NewProp_FloatSampleData_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_SpawnTimes_Inner = { "SpawnTimes", nullptr, (EPropertyFlags)0x0000000020000000, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_SpawnTimes = { "SpawnTimes", nullptr, (EPropertyFlags)0x0040000020000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, SpawnTimes_DEPRECATED), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SpawnTimes_MetaData), NewProp_SpawnTimes_MetaData) };
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_LifeValues_Inner = { "LifeValues", nullptr, (EPropertyFlags)0x0000000020000000, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_LifeValues = { "LifeValues", nullptr, (EPropertyFlags)0x0040000020000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, LifeValues_DEPRECATED), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LifeValues_MetaData), NewProp_LifeValues_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_PointTypes_Inner = { "PointTypes", nullptr, (EPropertyFlags)0x0000000020000000, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_PointTypes = { "PointTypes", nullptr, (EPropertyFlags)0x0040000020000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, PointTypes_DEPRECATED), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointTypes_MetaData), NewProp_PointTypes_MetaData) };
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_SpecialAttributeIndexes_Inner = { "SpecialAttributeIndexes", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_SpecialAttributeIndexes = { "SpecialAttributeIndexes", nullptr, (EPropertyFlags)0x0040000000000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, SpecialAttributeIndexes), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SpecialAttributeIndexes_MetaData), NewProp_SpecialAttributeIndexes_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_PointValueIndexes_Inner = { "PointValueIndexes", nullptr, (EPropertyFlags)0x0000000020000000, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FPointIndexes, METADATA_PARAMS(0, nullptr) }; // 4893df7d871e891d832fcd44b992fa3996736bb1
const UECodeGen_Private::FArrayPropertyParams UHT_STATICS::NewProp_PointValueIndexes = { "PointValueIndexes", nullptr, (EPropertyFlags)0x0040000020000000, UECodeGen_Private::EPropertyGenFlags::Array, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, PointValueIndexes_DEPRECATED), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PointValueIndexes_MetaData), NewProp_PointValueIndexes_MetaData) }; // 4893df7d871e891d832fcd44b992fa3996736bb1
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_UseCustomCSVTitleRow = { "UseCustomCSVTitleRow", nullptr, (EPropertyFlags)0x0040000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(UHoudiniPointCache), &UHT_STATICS::NewProp_UseCustomCSVTitleRow_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_UseCustomCSVTitleRow_MetaData), NewProp_UseCustomCSVTitleRow_MetaData) };
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_FileType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_FileType = { "FileType", nullptr, (EPropertyFlags)0x0040000000000000, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniPointCache, FileType), Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FileType_MetaData), NewProp_FileType_MetaData) }; // 58ac6af02b610898e2bba3a0d96d772c47710258
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FileName,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NumberOfSamples,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NumberOfAttributes,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NumberOfPoints,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_NumberOfFrames,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FirstFrame,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LastFrame,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MinSampleTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MaxSampleTime,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SourceCSVTitleRow,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeArray_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AttributeArray,
#if WITH_EDITORONLY_DATA
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_AssetImportData,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RawDataCompressed_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RawDataCompressed,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RawDataFormatID,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RawDataUncompressedSize,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_RawDataCompressionMethod,
#endif // WITH_EDITORONLY_DATA
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_bLargeFile,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FloatSampleData_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FloatSampleData,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SpawnTimes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SpawnTimes,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LifeValues_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_LifeValues,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointTypes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointTypes,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SpecialAttributeIndexes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SpecialAttributeIndexes,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointValueIndexes_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PointValueIndexes,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_UseCustomCSVTitleRow,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FileType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_FileType,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UHoudiniPointCache Property Definitions ************************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UObject,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_HoudiniNiagara,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UHoudiniPointCache,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x009000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static void UHoudiniPointCache_StaticRegisterNativesUHoudiniPointCache()
{
	UClass* Class = UHoudiniPointCache::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, 		MakeConstArrayView(UHT_STATICS::Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_UHoudiniPointCache;
UClass* Z_Construct_UClass_UHoudiniPointCache(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UHoudiniPointCache;
		if (!Z_Registration_Info_UClass_UHoudiniPointCache.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("HoudiniPointCache"),
				Z_Registration_Info_UClass_UHoudiniPointCache.InnerSingleton,
				UHoudiniPointCache_StaticRegisterNativesUHoudiniPointCache,
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
		return Z_Registration_Info_UClass_UHoudiniPointCache.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UHoudiniPointCache.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniPointCache.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UHoudiniPointCache.OuterSingleton;
}
#undef UHT_STATICS
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UHoudiniPointCache);
UHoudiniPointCache::~UHoudiniPointCache() {}
IMPLEMENT_FSTRUCTUREDARCHIVE_SERIALIZER(UHoudiniPointCache)
// ********** End Class UHoudiniPointCache *********************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_cygwin_home_prisms_builder_new_NightlyHEADCMake_dev_engine_unreal_5_8_Engine_Plugins_FX_HoudiniNiagara_Source_HoudiniNiagara_Public_HoudiniPointCache_h__Script_HoudiniNiagara_Statics
struct UHT_STATICS
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ Z_Construct_UEnum_HoudiniNiagara_EHoudiniAttributes, TEXT("EHoudiniAttributes"), &ZRIE_EHoudiniAttributes, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3804894630U) },
		{ Z_Construct_UEnum_HoudiniNiagara_EHoudiniPointCacheFileType, TEXT("EHoudiniPointCacheFileType"), &ZRIE_EHoudiniPointCacheFileType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1487694576U) },
	};
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FPointIndexes, Z_Construct_UScriptStruct_FPointIndexes_Statics::NewStructOps, TEXT("PointIndexes"),&Z_Registration_Info_UScriptStruct_FPointIndexes, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FPointIndexes), 1217650557U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniPointCache, TEXT("UHoudiniPointCache"), &Z_Registration_Info_UClass_UHoudiniPointCache, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniPointCache), 414309168U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_cygwin_home_prisms_builder_new_NightlyHEADCMake_dev_engine_unreal_5_8_Engine_Plugins_FX_HoudiniNiagara_Source_HoudiniNiagara_Public_HoudiniPointCache_h__Script_HoudiniNiagara_08c47a49f8c4654efa1d3448c1e42702fa3459ab{
	TEXT("/Script/HoudiniNiagara"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	UHT_STATICS::EnumInfo, UE_ARRAY_COUNT(UHT_STATICS::EnumInfo),
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
