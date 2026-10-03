#include "Abilities/KnightBlockAbility.h"

#include "Components/KnightCombatComponent.h"
#include "Tags/KnightGameplayTags.h"

UKnightBlockAbility::UKnightBlockAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Combat_Block));
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Combat_Blocking);
}
bool UKnightBlockAbility::ShouldCancelOnInputRelease() const
{
	return true;
}
void UKnightBlockAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	const FKnightResult Result = Combat != nullptr ? Combat->BeginDefense(KnightGameplayTags::Ability_Combat_Block)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("BlockNotConfigured"),
			NSLOCTEXT("KnightCombat", "BlockUnavailable", "Block is unavailable in this area."));
	ReportResult(Result);
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}
void UKnightBlockAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UKnightCombatComponent* Combat = GetCombat())
	{
		Combat->EndDefense(KnightGameplayTags::Ability_Combat_Block);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
