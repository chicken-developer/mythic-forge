#include "Settings/KnightSettings.h"

UKnightSettings::UKnightSettings()
{
	DefinitionRoots.Add(TEXT("/Game/Knight/Definitions"));
	FoundationDefinition = FPrimaryAssetId(TEXT("KnightDefinition"), TEXT("Foundation"));
}

FName UKnightSettings::GetCategoryName() const
{
	return TEXT("Game");
}
