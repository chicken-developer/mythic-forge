#include "Types/GolemStableIds.h"

bool GolemIdentity::IsValidIdentifier(FName Identifier)
{
	if (Identifier.IsNone())
	{
		return false;
	}

	const FString Text = Identifier.ToString();
	for (const TCHAR Character : Text)
	{
		const bool bIsLetter = (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'));
		const bool bIsDigit = Character >= TEXT('0') && Character <= TEXT('9');
		if (!bIsLetter && !bIsDigit && Character != TEXT('_'))
		{
			return false;
		}
	}
	return !Text.IsEmpty();
}

bool GolemIdentity::IsValidDefinitionId(const FPrimaryAssetId& DefinitionId)
{
	return DefinitionId.IsValid()
		&& IsValidIdentifier(DefinitionId.PrimaryAssetType.GetName())
		&& IsValidIdentifier(DefinitionId.PrimaryAssetName);
}

FGolemDefinitionId::FGolemDefinitionId(const FPrimaryAssetId& InValue)
	: Value(InValue)
{
}

bool FGolemDefinitionId::IsValid() const
{
	return GolemIdentity::IsValidDefinitionId(Value);
}

bool FGolemDefinitionId::operator==(const FGolemDefinitionId& Other) const
{
	return Value == Other.Value;
}

FGolemEntityId::FGolemEntityId(const FGuid& InValue)
	: Value(InValue)
{
}

FGolemEntityId FGolemEntityId::Create()
{
	return FGolemEntityId(FGuid::NewGuid());
}

bool FGolemEntityId::IsValid() const
{
	return Value.IsValid();
}

bool FGolemEntityId::operator==(const FGolemEntityId& Other) const
{
	return Value == Other.Value;
}
