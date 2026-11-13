#include "Abilities/GolemLightAttackAbility.h"

#include "Tags/GolemGameplayTags.h"

UGolemLightAttackAbility::UGolemLightAttackAbility()
{
	ActionTag = GolemGameplayTags::Ability_Combat_Primary;
	SetAssetTags(FGameplayTagContainer(ActionTag));
}
