#include "Abilities/KnightAttackAbility.h"

#include "Animation/AnimInstance.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Engine/World.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

UKnightAttackAbility::UKnightAttackAbility()
{
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Combat_Attacking);
}

void UKnightAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	UAnimMontage* Montage = nullptr;
	FName Section;
	const FKnightResult Result = Combat != nullptr ? Combat->BeginAttack(ActionTag, this, Montage, Section)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("CombatNotConfigured"),
			NSLOCTEXT("KnightCombat", "CombatNotConfigured", "Combat is not available in this area."));
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!StartActionMontage(Montage, Section))
	{
		ReportResult(Result.IsSuccess() ? FKnightResult::Failure(EKnightError::InvalidState, TEXT("AttackMontage"),
			NSLOCTEXT("KnightCombat", "AttackAnimationUnavailable", "The attack animation could not start.")) : Result);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(ActionTimer, this, &ThisClass::CheckAttackMontage, 0.03f, true);
	ReportResult(FKnightResult::Success());
}
void UKnightAttackAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if (UKnightCombatComponent* Combat = GetCombat())
	{
		Combat->BufferAttackInput();
	}
}
void UKnightAttackAbility::CheckAttackMontage()
{
	UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem();
	const UAnimInstance* Animation = CurrentActorInfo != nullptr ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (ASC == nullptr || Animation == nullptr || ASC->GetAnimatingAbility() != this ||
		!Animation->Montage_IsPlaying(ASC->GetCurrentMontage()))
	{
		FinishAction();
	}
}
void UKnightAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UKnightCombatComponent* Combat = GetCombat())
	{
		Combat->EndAttack(this);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
