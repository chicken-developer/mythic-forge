#include "Abilities/KnightHeavyAttackAbility.h"

#include "Tags/KnightGameplayTags.h"

UKnightHeavyAttackAbility::UKnightHeavyAttackAbility()
{
	ActionTag = KnightGameplayTags::Ability_Combat_Heavy;
	SetAssetTags(FGameplayTagContainer(ActionTag));
}
