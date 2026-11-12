#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GolemElement.generated.h"

/** The three elements. Each Golem carries exactly one; a void asks for one, two or all three. */
UENUM(BlueprintType)
enum class EGolemElement : uint8
{
	Earth,
	Sky,
	Sea
};

/** Element sets travel as a 3-bit mask: bit 0 Earth, bit 1 Sky, bit 2 Sea. */
namespace GolemElements
{
	constexpr uint8 AllMask = 0x7;

	constexpr uint8 Mask(const EGolemElement Element)
	{
		return static_cast<uint8>(1u << static_cast<uint8>(Element));
	}

	constexpr uint8 Mask(const EGolemElement First, const EGolemElement Second)
	{
		return Mask(First) | Mask(Second);
	}

	constexpr bool IsValidMask(const uint8 InMask)
	{
		return InMask != 0 && (InMask & ~AllMask) == 0;
	}

	constexpr int32 Count(const uint8 InMask)
	{
		return ((InMask >> 0) & 1) + ((InMask >> 1) & 1) + ((InMask >> 2) & 1);
	}

	GOLEMCORE_API FGameplayTag ToTag(EGolemElement Element);

	/** False when the tag is not one of Element.Earth, Element.Sky, Element.Sea. */
	GOLEMCORE_API bool FromTag(const FGameplayTag& Tag, EGolemElement& OutElement);

	/** Finds the single element tag an actor owns. False when it owns none or more than one. */
	GOLEMCORE_API bool FromOwnedTags(const FGameplayTagContainer& Tags, EGolemElement& OutElement);
}
