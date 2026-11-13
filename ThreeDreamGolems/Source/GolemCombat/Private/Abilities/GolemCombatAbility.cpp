#include "Abilities/GolemCombatAbility.h"

#include "Animation/AnimMontage.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Engine/World.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

UGolemCombatAbility::UGolemCombatAbility()
{
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Combat_Staggered);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Reviving);
}

bool UGolemCombatAbility::ShouldCancelOnInputRelease() const
{
	return false;
}
UGolemCombatComponent* UGolemCombatAbility::GetCombat() const
{
	AActor* Actor = GetAvatarActorFromActorInfo();
	return IsValid(Actor) ? Actor->FindComponentByClass<UGolemCombatComponent>() : nullptr;
}
bool UGolemCombatAbility::StartActionMontage(UAnimMontage* Montage, FName Section, float PlayRate)
{
	UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem();
	return ASC != nullptr && IsValid(Montage) && FMath::IsFinite(PlayRate) && PlayRate > 0.0f &&
		ASC->PlayMontage(this, CurrentActivationInfo, Montage, PlayRate, Section) > 0.0f;
}
void UGolemCombatAbility::SetActionDuration(float Seconds)
{
	if (GetWorld() != nullptr && FMath::IsFinite(Seconds) && Seconds > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ActionTimer, this, &ThisClass::FinishAction, Seconds, false);
	}
}
void UGolemCombatAbility::FinishAction()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
void UGolemCombatAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(ActionTimer);
	}
	if (UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem())
	{
		if (ASC->GetAnimatingAbility() == this)
		{
			ASC->CurrentMontageStop(0.1f);
			ASC->ClearAnimatingAbility(this);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
