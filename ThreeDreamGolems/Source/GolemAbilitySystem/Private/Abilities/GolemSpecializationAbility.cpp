#include "Abilities/GolemSpecializationAbility.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "TimerManager.h"
#include "Tags/GolemGameplayTags.h"

UGolemSpecializationAbility::UGolemSpecializationAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Attacking);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Combat_Finishing);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Combat_Traversing);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Reviving);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Combat_Blocking);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Combat_Parrying);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Combat_Dodging);
}

bool UGolemSpecializationAbility::ShouldCancelOnInputRelease() const
{
	return false;
}

void UGolemSpecializationAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem();
	Runtime = ActorInfo->AvatarActor->FindComponentByClass<UGolemSkillRuntimeComponent>();
	const FGameplayAbilitySpec* Spec = ASC != nullptr ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	Slot = Spec != nullptr ? Spec->InputID : 0;
	Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (!Runtime.IsValid() || !Runtime->ValidateSkill(Slot, Target.Get()).IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActivationId = FGuid::NewGuid();
	const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(Runtime->GetSkillState().Build, Slot);
	const float Haste = ASC->GetGolemAttributes()->GetHaste();
	GetWorld()->GetTimerManager().SetTimer(CommitTimer, this, &ThisClass::CommitAction, D.Windup / (1.0f + Haste), false);
}

void UGolemSpecializationAbility::CommitAction()
{
	if (!Runtime.IsValid() || !Runtime->ValidateSkill(Slot, Target.Get()).IsSuccess())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!IsActive() || !Runtime.IsValid())
	{
		return;
	}
	Runtime->CommitSkill(Slot, Target.Get(), ActivationId);
	if (!IsActive() || !Runtime.IsValid())
	{
		return;
	}
	const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(Runtime->GetSkillState().Build, Slot);
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::FinishRecovery, D.Recovery, false);
}

void UGolemSpecializationAbility::FinishRecovery()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGolemSpecializationAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(CommitTimer);
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	}
	Runtime.Reset();
	Target.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
