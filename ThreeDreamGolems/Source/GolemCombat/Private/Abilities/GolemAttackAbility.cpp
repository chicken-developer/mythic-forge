#include "Abilities/GolemAttackAbility.h"

#include "Animation/AnimInstance.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Engine/World.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

UGolemAttackAbility::UGolemAttackAbility()
{
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Attacking);
}

void UGolemAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	UAnimMontage* Montage = nullptr;
	FName Section;
	float PlayRate = 1.0f;
	const FGolemResult Result = Combat != nullptr ? Combat->BeginAttack(ActionTag, this, Montage, Section, PlayRate)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("CombatNotConfigured"),
			NSLOCTEXT("GolemCombat", "CombatNotConfigured", "Combat is not available in this area."));
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo) ||
		!StartActionMontage(Montage, Section, PlayRate))
	{
		ReportResult(Result.IsSuccess() ? FGolemResult::Failure(EGolemError::InvalidState, TEXT("AttackMontage"),
			NSLOCTEXT("GolemCombat", "AttackAnimationUnavailable", "The attack animation could not start.")) : Result);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(ActionTimer, this, &ThisClass::CheckAttackMontage, 0.03f, true);
	ReportResult(FGolemResult::Success());
}
void UGolemAttackAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if (UGolemCombatComponent* Combat = GetCombat())
	{
		Combat->BufferAttackInput();
	}
}
void UGolemAttackAbility::CheckAttackMontage()
{
	UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem();
	const UAnimInstance* Animation = CurrentActorInfo != nullptr ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (ASC == nullptr || Animation == nullptr || ASC->GetAnimatingAbility() != this ||
		!Animation->Montage_IsPlaying(ASC->GetCurrentMontage()))
	{
		FinishAction();
	}
}
void UGolemAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UGolemCombatComponent* Combat = GetCombat())
	{
		Combat->EndAttack(this);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
