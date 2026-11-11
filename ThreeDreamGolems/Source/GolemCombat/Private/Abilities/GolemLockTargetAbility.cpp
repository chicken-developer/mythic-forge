#include "Abilities/GolemLockTargetAbility.h"

#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemLockTargetAbility::UGolemLockTargetAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Combat_LockOn));
}
void UGolemLockTargetAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	const FGolemResult Result = Combat != nullptr ? Combat->TryToggleTargetLock(Target)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("LockNotConfigured"),
			NSLOCTEXT("GolemCombat", "LockUnavailable", "Target lock is unavailable in this area."));
	ReportResult(Result);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Result.IsSuccess());
}
