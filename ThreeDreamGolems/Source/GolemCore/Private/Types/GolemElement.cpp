#include "Types/GolemElement.h"

#include "Tags/GolemGameplayTags.h"

FGameplayTag GolemElements::ToTag(const EGolemElement Element)
{
	switch (Element)
	{
	case EGolemElement::Earth:
		return GolemGameplayTags::Element_Earth;
	case EGolemElement::Sky:
		return GolemGameplayTags::Element_Sky;
	default:
		return GolemGameplayTags::Element_Sea;
	}
}

FLinearColor GolemElements::DisplayColor(const uint8 InMask)
{
	switch (InMask & AllMask)
	{
	case 0x1:
		return FLinearColor(0.9f, 0.1f, 0.08f);
	case 0x2:
		return FLinearColor(0.95f, 0.8f, 0.1f);
	case 0x4:
		return FLinearColor(0.08f, 0.25f, 0.95f);
	case 0x3:
		return FLinearColor(0.95f, 0.45f, 0.05f);
	case 0x6:
		return FLinearColor(0.1f, 0.8f, 0.25f);
	case 0x5:
		return FLinearColor(0.6f, 0.15f, 0.85f);
	case 0x7:
		return FLinearColor::White;
	default:
		return FLinearColor(0.22f, 0.22f, 0.24f);
	}
}

FString GolemElements::SymbolText(const uint8 InMask)
{
	FString Symbols;
	if ((InMask & Mask(EGolemElement::Earth)) != 0)
	{
		Symbols += TEXT("^");
	}
	if ((InMask & Mask(EGolemElement::Sky)) != 0)
	{
		Symbols += TEXT("O");
	}
	if ((InMask & Mask(EGolemElement::Sea)) != 0)
	{
		Symbols += TEXT("~");
	}
	return Symbols;
}

bool GolemElements::FromTag(const FGameplayTag& Tag, EGolemElement& OutElement)
{
	for (const EGolemElement Element : { EGolemElement::Earth, EGolemElement::Sky, EGolemElement::Sea })
	{
		if (Tag == ToTag(Element))
		{
			OutElement = Element;
			return true;
		}
	}
	return false;
}

bool GolemElements::FromOwnedTags(const FGameplayTagContainer& Tags, EGolemElement& OutElement)
{
	int32 Found = 0;
	for (const EGolemElement Element : { EGolemElement::Earth, EGolemElement::Sky, EGolemElement::Sea })
	{
		if (Tags.HasTagExact(ToTag(Element)))
		{
			OutElement = Element;
			++Found;
		}
	}
	return Found == 1;
}
