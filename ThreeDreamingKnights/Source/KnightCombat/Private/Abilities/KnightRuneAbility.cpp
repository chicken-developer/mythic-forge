#include "Abilities/KnightRuneAbility.h"

#include "Animation/AnimMontage.h"
#include "Components/KnightCombatComponent.h"
#include "Tags/KnightGameplayTags.h"

UKnightRuneAbility::UKnightRuneAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Combat_Rune));
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Combat_Attacking);
}
void UKnightRuneAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Combat == nullptr || Combat->GetProfile() == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!StartActionMontage(Combat->GetProfile()->GetRuneMontage()))
	{
		ReportResult(FKnightResult::Failure(EKnightError::InvalidState, TEXT("RuneMontage"),
			NSLOCTEXT("KnightCombat", "RuneUnavailable", "The rune action could not start.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FKnightResult Result = Combat->CastRune(Target);
	ReportResult(Result);
	if (!Result.IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SetActionDuration(Combat->GetProfile()->GetRuneMontage()->GetPlayLength());
}
