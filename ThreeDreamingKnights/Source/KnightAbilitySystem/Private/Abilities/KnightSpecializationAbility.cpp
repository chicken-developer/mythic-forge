#include "Abilities/KnightSpecializationAbility.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "TimerManager.h"
#include "Tags/KnightGameplayTags.h"

UKnightSpecializationAbility::UKnightSpecializationAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	ActivationOwnedTags.AddTag(KnightGameplayTags::State_Combat_Attacking);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Finishing);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Traversing);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Reviving);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Blocking);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Parrying);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Dodging);
}

bool UKnightSpecializationAbility::ShouldCancelOnInputRelease() const
{
	return false;
}

void UKnightSpecializationAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem();
	Runtime = ActorInfo->AvatarActor->FindComponentByClass<UKnightSkillRuntimeComponent>();
	const FGameplayAbilitySpec* Spec = ASC != nullptr ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	Slot = Spec != nullptr ? Spec->InputID : 0;
	Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (!Runtime.IsValid() || !Runtime->ValidateSkill(Slot, Target.Get()).IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActivationId = FGuid::NewGuid();
	const FKnightSpecActionDefinition D = FKnightSkillCatalog::GetSkill(Runtime->GetSkillState().Build, Slot);
	const float Haste = ASC->GetKnightAttributes()->GetHaste();
	GetWorld()->GetTimerManager().SetTimer(CommitTimer, this, &ThisClass::CommitAction, D.Windup / (1.0f + Haste), false);
}

void UKnightSpecializationAbility::CommitAction()
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
	const FKnightSpecActionDefinition D = FKnightSkillCatalog::GetSkill(Runtime->GetSkillState().Build, Slot);
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::FinishRecovery, D.Recovery, false);
}

void UKnightSpecializationAbility::FinishRecovery()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UKnightSpecializationAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
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
