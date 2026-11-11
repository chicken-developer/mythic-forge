#include "Abilities/GolemBlockAbility.h"

#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemBlockAbility::UGolemBlockAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Combat_Block));
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Blocking);
}
bool UGolemBlockAbility::ShouldCancelOnInputRelease() const
{
	return true;
}
void UGolemBlockAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	const FGolemResult Result = Combat != nullptr ? Combat->BeginDefense(GolemGameplayTags::Ability_Combat_Block)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("BlockNotConfigured"),
			NSLOCTEXT("GolemCombat", "BlockUnavailable", "Block is unavailable in this area."));
	ReportResult(Result);
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}
void UGolemBlockAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UGolemCombatComponent* Combat = GetCombat())
	{
		Combat->EndDefense(GolemGameplayTags::Ability_Combat_Block);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
