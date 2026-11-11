#include "Settings/GolemSettings.h"

UGolemSettings::UGolemSettings()
{
	DefinitionRoots.Add(TEXT("/Game/Knight/Definitions"));
	FoundationDefinition = FPrimaryAssetId(TEXT("KnightDefinition"), TEXT("Foundation"));
}

FName UGolemSettings::GetCategoryName() const
{
	return TEXT("Game");
}
