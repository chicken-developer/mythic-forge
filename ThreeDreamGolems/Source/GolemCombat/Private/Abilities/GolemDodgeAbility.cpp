#include "Abilities/GolemDodgeAbility.h"

#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemDodgeAbility::UGolemDodgeAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Combat_Dodge));
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Dodging);
}
void UGolemDodgeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	const FGolemResult Result = Combat != nullptr ? Combat->BeginDodge()
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("DodgeNotConfigured"),
			NSLOCTEXT("GolemCombat", "DodgeUnavailable", "Dodge is unavailable in this area."));
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!StartActionMontage(Combat->GetProfile()->GetDodgeMontage()))
	{
		ReportResult(Result.IsSuccess() ? FGolemResult::Failure(EGolemError::InvalidState, TEXT("DodgeMontage"),
			NSLOCTEXT("GolemCombat", "DodgeAnimationUnavailable", "The dodge animation could not start.")) : Result);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SetActionDuration(Combat->GetProfile()->GetTuning().DodgeDuration);
	ReportResult(FGolemResult::Success());
}
void UGolemDodgeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UGolemCombatComponent* Combat = GetCombat())
	{
		Combat->EndDodge();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
