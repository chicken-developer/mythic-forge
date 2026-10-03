#include "Abilities/KnightLightAttackAbility.h"

#include "Tags/KnightGameplayTags.h"

UKnightLightAttackAbility::UKnightLightAttackAbility()
{
	ActionTag = KnightGameplayTags::Ability_Combat_Primary;
	SetAssetTags(FGameplayTagContainer(ActionTag));
}
