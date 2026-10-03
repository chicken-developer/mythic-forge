#include "Abilities/KnightReviveAbility.h"

#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

UKnightReviveAbility::UKnightReviveAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Party_Revive));
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Life_Reviving);
}

void UKnightReviveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Target == nullptr && ASC != nullptr)
	{
		Target = ASC->FindAbilityTarget(KnightGameplayTags::Input_Party_Revive);
	}
	UKnightAbilitySystemComponent* TargetASC = IsValid(Target) ? Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true)) : nullptr;
	UKnightLifeComponent* Life = TargetASC != nullptr ? TargetASC->GetLifeComponent() : nullptr;
	FKnightResult Result = Life != nullptr ? Life->TryBeginRevive(GetAvatarActorFromActorInfo(), ReservationToken)
		: FKnightResult::Failure(EKnightError::NotFound, TEXT("ReviveTarget"),
			NSLOCTEXT("KnightGAS", "ReviveNoTarget", "Move close to a downed companion and hold revive."));
	if (!Result.IsSuccess())
	{
		ReportResult(Result);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	TargetLife = Life;
	if (GetWorld() == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		ReportResult(FKnightResult::Failure(EKnightError::InvalidState, TEXT("ReviveCommit"),
			NSLOCTEXT("KnightGAS", "ReviveCommit", "The revive action cannot start in the current state.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ASC->AddGameplayCue(KnightGameplayTags::GameplayCue_Revive);
	bChannelCueActive = true;
	GetWorld()->GetTimerManager().SetTimer(ReviveTimer, this, &ThisClass::CompleteRevive,
		Life->GetReviveDuration(), false);
	ReportResult(FKnightResult::Success());
}

void UKnightReviveAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UKnightReviveAbility::CompleteRevive()
{
	if (!IsActive())
	{
		return;
	}
	const FKnightResult Result = TargetLife.IsValid()
		? TargetLife->TryCompleteRevive(GetAvatarActorFromActorInfo(), ReservationToken)
		: FKnightResult::Failure(EKnightError::Cancelled, TEXT("ReviveTargetLost"),
			NSLOCTEXT("KnightGAS", "ReviveTargetLost", "The revive target is no longer available."));
	ReportResult(Result);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !Result.IsSuccess());
}

void UKnightReviveAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(ReviveTimer);
	}
	if (TargetLife.IsValid() && ReservationToken.IsValid())
	{
		TargetLife->CancelRevive(GetAvatarActorFromActorInfo(), ReservationToken);
	}
	TargetLife.Reset();
	ReservationToken.Invalidate();
	if (bChannelCueActive)
	{
		if (UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem())
		{
			ASC->RemoveGameplayCue(KnightGameplayTags::GameplayCue_Revive);
		}
		bChannelCueActive = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
