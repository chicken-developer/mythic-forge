// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeHoudiniNiagara_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_HoudiniNiagara;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_HoudiniNiagara(ETypeConstructPhase)
	{
		if (!Z_Registration_Info_UPackage__Script_HoudiniNiagara.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/HoudiniNiagara",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000000,
			0xDD7BFC08,
			0x2D8F8765,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_HoudiniNiagara.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_HoudiniNiagara.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_HoudiniNiagara(Z_Construct_UPackage__Script_HoudiniNiagara, TEXT("/Script/HoudiniNiagara"), Z_Registration_Info_UPackage__Script_HoudiniNiagara, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xDD7BFC08, 0x2D8F8765));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
