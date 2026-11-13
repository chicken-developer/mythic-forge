#include "Abilities/GolemHeavyAttackAbility.h"

#include "Tags/GolemGameplayTags.h"

UGolemHeavyAttackAbility::UGolemHeavyAttackAbility()
{
	ActionTag = GolemGameplayTags::Ability_Combat_Heavy;
	SetAssetTags(FGameplayTagContainer(ActionTag));
}
