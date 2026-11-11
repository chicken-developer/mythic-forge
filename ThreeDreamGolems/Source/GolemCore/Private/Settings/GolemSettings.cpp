#include "Settings/GolemSettings.h"

UGolemSettings::UGolemSettings()
{
	DefinitionRoots.Add(TEXT("/Game/Golem/Definitions"));
	FoundationDefinition = FPrimaryAssetId(TEXT("GolemDefinition"), TEXT("Foundation"));
}

FName UGolemSettings::GetCategoryName() const
{
	return TEXT("Game");
}
