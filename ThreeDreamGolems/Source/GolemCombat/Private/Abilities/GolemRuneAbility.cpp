#include "Abilities/GolemRuneAbility.h"

#include "Animation/AnimMontage.h"
#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemRuneAbility::UGolemRuneAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Combat_Rune));
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Attacking);
}
void UGolemRuneAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Combat == nullptr || Combat->GetProfile() == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!StartActionMontage(Combat->GetProfile()->GetRuneMontage()))
	{
		ReportResult(FGolemResult::Failure(EGolemError::InvalidState, TEXT("RuneMontage"),
			NSLOCTEXT("GolemCombat", "RuneUnavailable", "The rune action could not start.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FGolemResult Result = Combat->CastRune(Target);
	ReportResult(Result);
	if (!Result.IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SetActionDuration(Combat->GetProfile()->GetRuneMontage()->GetPlayLength());
}
