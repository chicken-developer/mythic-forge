#include "Abilities/GolemParryAbility.h"

#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemParryAbility::UGolemParryAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Combat_Parry));
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Parrying);
}
void UGolemParryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	const FGolemResult Result = Combat != nullptr ? Combat->BeginDefense(GolemGameplayTags::Ability_Combat_Parry)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("ParryNotConfigured"),
			NSLOCTEXT("GolemCombat", "ParryUnavailable", "Parry is unavailable in this area."));
	ReportResult(Result);
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SetActionDuration(Combat->GetProfile()->GetTuning().ParryWindow);
}
void UGolemParryAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UGolemCombatComponent* Combat = GetCombat())
	{
		Combat->EndDefense(GolemGameplayTags::Ability_Combat_Parry);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
