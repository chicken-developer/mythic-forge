#include "Abilities/KnightParryAbility.h"

#include "Components/KnightCombatComponent.h"
#include "Tags/KnightGameplayTags.h"

UKnightParryAbility::UKnightParryAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Combat_Parry));
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Combat_Parrying);
}
void UKnightParryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	const FKnightResult Result = Combat != nullptr ? Combat->BeginDefense(KnightGameplayTags::Ability_Combat_Parry)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("ParryNotConfigured"),
			NSLOCTEXT("KnightCombat", "ParryUnavailable", "Parry is unavailable in this area."));
	ReportResult(Result);
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SetActionDuration(Combat->GetProfile()->GetTuning().ParryWindow);
}
void UKnightParryAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UKnightCombatComponent* Combat = GetCombat())
	{
		Combat->EndDefense(KnightGameplayTags::Ability_Combat_Parry);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
