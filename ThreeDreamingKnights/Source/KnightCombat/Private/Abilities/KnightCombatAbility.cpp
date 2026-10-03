#include "Abilities/KnightCombatAbility.h"

#include "Animation/AnimMontage.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Engine/World.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

UKnightCombatAbility::UKnightCombatAbility()
{
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Staggered);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Finishing);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Traversing);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Reviving);
}

bool UKnightCombatAbility::ShouldCancelOnInputRelease() const
{
	return false;
}
UKnightCombatComponent* UKnightCombatAbility::GetCombat() const
{
	AActor* Actor = GetAvatarActorFromActorInfo();
	return IsValid(Actor) ? Actor->FindComponentByClass<UKnightCombatComponent>() : nullptr;
}
bool UKnightCombatAbility::StartActionMontage(UAnimMontage* Montage, FName Section)
{
	UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem();
	return ASC != nullptr && IsValid(Montage) &&
		ASC->PlayMontage(this, CurrentActivationInfo, Montage, 1.0f, Section) > 0.0f;
}
void UKnightCombatAbility::SetActionDuration(float Seconds)
{
	if (GetWorld() != nullptr && FMath::IsFinite(Seconds) && Seconds > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ActionTimer, this, &ThisClass::FinishAction, Seconds, false);
	}
}
void UKnightCombatAbility::FinishAction()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
void UKnightCombatAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(ActionTimer);
	}
	if (UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem())
	{
		if (ASC->GetAnimatingAbility() == this)
		{
			ASC->CurrentMontageStop(0.1f);
			ASC->ClearAnimatingAbility(this);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
