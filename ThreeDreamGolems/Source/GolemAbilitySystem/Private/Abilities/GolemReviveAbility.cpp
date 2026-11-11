#include "Abilities/GolemReviveAbility.h"

#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

UGolemReviveAbility::UGolemReviveAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Party_Revive));
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Life_Reviving);
}

void UGolemReviveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Target == nullptr && ASC != nullptr)
	{
		Target = ASC->FindAbilityTarget(GolemGameplayTags::Input_Party_Revive);
	}
	UGolemAbilitySystemComponent* TargetASC = IsValid(Target) ? Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true)) : nullptr;
	UGolemLifeComponent* Life = TargetASC != nullptr ? TargetASC->GetLifeComponent() : nullptr;
	FGolemResult Result = Life != nullptr ? Life->TryBeginRevive(GetAvatarActorFromActorInfo(), ReservationToken)
		: FGolemResult::Failure(EGolemError::NotFound, TEXT("ReviveTarget"),
			NSLOCTEXT("GolemGAS", "ReviveNoTarget", "Move close to a downed companion and hold revive."));
	if (!Result.IsSuccess())
	{
		ReportResult(Result);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	TargetLife = Life;
	if (GetWorld() == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		ReportResult(FGolemResult::Failure(EGolemError::InvalidState, TEXT("ReviveCommit"),
			NSLOCTEXT("GolemGAS", "ReviveCommit", "The revive action cannot start in the current state.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ASC->AddGameplayCue(GolemGameplayTags::GameplayCue_Revive);
	bChannelCueActive = true;
	GetWorld()->GetTimerManager().SetTimer(ReviveTimer, this, &ThisClass::CompleteRevive,
		Life->GetReviveDuration(), false);
	ReportResult(FGolemResult::Success());
}

void UGolemReviveAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UGolemReviveAbility::CompleteRevive()
{
	if (!IsActive())
	{
		return;
	}
	const FGolemResult Result = TargetLife.IsValid()
		? TargetLife->TryCompleteRevive(GetAvatarActorFromActorInfo(), ReservationToken)
		: FGolemResult::Failure(EGolemError::Cancelled, TEXT("ReviveTargetLost"),
			NSLOCTEXT("GolemGAS", "ReviveTargetLost", "The revive target is no longer available."));
	ReportResult(Result);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !Result.IsSuccess());
}

void UGolemReviveAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
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
		if (UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem())
		{
			ASC->RemoveGameplayCue(GolemGameplayTags::GameplayCue_Revive);
		}
		bChannelCueActive = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
