#include "Abilities/KnightDodgeAbility.h"

#include "Components/KnightCombatComponent.h"
#include "Tags/KnightGameplayTags.h"

UKnightDodgeAbility::UKnightDodgeAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Combat_Dodge));
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Combat_Dodging);
}
void UKnightDodgeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	const FKnightResult Result = Combat != nullptr ? Combat->BeginDodge()
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("DodgeNotConfigured"),
			NSLOCTEXT("KnightCombat", "DodgeUnavailable", "Dodge is unavailable in this area."));
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!StartActionMontage(Combat->GetProfile()->GetDodgeMontage()))
	{
		ReportResult(Result.IsSuccess() ? FKnightResult::Failure(EKnightError::InvalidState, TEXT("DodgeMontage"),
			NSLOCTEXT("KnightCombat", "DodgeAnimationUnavailable", "The dodge animation could not start.")) : Result);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SetActionDuration(Combat->GetProfile()->GetTuning().DodgeDuration);
	ReportResult(FKnightResult::Success());
}
void UKnightDodgeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UKnightCombatComponent* Combat = GetCombat())
	{
		Combat->EndDodge();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
