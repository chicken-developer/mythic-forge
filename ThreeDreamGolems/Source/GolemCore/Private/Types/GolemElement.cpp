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
