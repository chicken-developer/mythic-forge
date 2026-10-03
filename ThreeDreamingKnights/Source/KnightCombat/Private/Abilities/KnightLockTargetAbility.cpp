#include "Abilities/KnightLockTargetAbility.h"

#include "Components/KnightCombatComponent.h"
#include "Tags/KnightGameplayTags.h"

UKnightLockTargetAbility::UKnightLockTargetAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Combat_LockOn));
}
void UKnightLockTargetAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	const FKnightResult Result = Combat != nullptr ? Combat->TryToggleTargetLock(Target)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("LockNotConfigured"),
			NSLOCTEXT("KnightCombat", "LockUnavailable", "Target lock is unavailable in this area."));
	ReportResult(Result);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Result.IsSuccess());
}
