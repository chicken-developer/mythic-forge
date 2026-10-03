#include "Types/KnightStableIds.h"

bool KnightIdentity::IsValidIdentifier(FName Identifier)
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

bool KnightIdentity::IsValidDefinitionId(const FPrimaryAssetId& DefinitionId)
{
	return DefinitionId.IsValid()
		&& IsValidIdentifier(DefinitionId.PrimaryAssetType.GetName())
		&& IsValidIdentifier(DefinitionId.PrimaryAssetName);
}

FKnightDefinitionId::FKnightDefinitionId(const FPrimaryAssetId& InValue)
	: Value(InValue)
{
}

bool FKnightDefinitionId::IsValid() const
{
	return KnightIdentity::IsValidDefinitionId(Value);
}

bool FKnightDefinitionId::operator==(const FKnightDefinitionId& Other) const
{
	return Value == Other.Value;
}

FKnightEntityId::FKnightEntityId(const FGuid& InValue)
	: Value(InValue)
{
}

FKnightEntityId FKnightEntityId::Create()
{
	return FKnightEntityId(FGuid::NewGuid());
}

bool FKnightEntityId::IsValid() const
{
	return Value.IsValid();
}

bool FKnightEntityId::operator==(const FKnightEntityId& Other) const
{
	return Value == Other.Value;
}
