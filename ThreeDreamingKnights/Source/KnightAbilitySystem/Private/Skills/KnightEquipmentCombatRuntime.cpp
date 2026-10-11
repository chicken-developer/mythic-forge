#include "Skills/KnightSkillRuntimeComponent.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Effects/KnightConsumableEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tags/KnightGameplayTags.h"

namespace
{
	FKnightResult EquipmentFailure(EKnightError Error, FName Context)
	{
		return FKnightResult::Failure(Error, Context,
			NSLOCTEXT("KnightEquipment", "Unavailable", "This item action is unavailable in the current state."));
	}
}

FKnightResult UKnightSkillRuntimeComponent::PrepareEquipmentBuild(const FKnightEquipmentCombatBuild& Build)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ResolveASC() == nullptr || !IsAlive()
		|| !Build.Validate().IsSuccess() || HasActiveSkill() || HasDeferredDamage())
	{
		return EquipmentFailure(EKnightError::InvalidState, TEXT("EquipmentCombatPrepare"));
	}
	if (bEquipmentBuildPrepared && !(PreparedEquipmentBuild == Build))
	{
		return EquipmentFailure(EKnightError::Conflict, TEXT("EquipmentAlreadyPrepared"));
	}
	EnsureInitialized();
	PreparedEquipmentBuild = Build;
	bEquipmentBuildPrepared = true;
	return FKnightResult::Success();
}

void UKnightSkillRuntimeComponent::RemoveRelicEffects(FGuid Source)
{
	if (!Source.IsValid())
	{
		return;
	}
	Periodics.RemoveAll([Source](const FPeriodicStream& Stream)
	{
		return Stream.ItemSource == Source;
	});
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (UKnightSkillRuntimeComponent* Runtime = It->FindComponentByClass<UKnightSkillRuntimeComponent>())
		{
			const int32 Removed = Runtime->Shields.RemoveAll([this, Source](const FShieldPool& Pool)
			{
				return Pool.ItemSource == Source && Pool.Caster.Get() == GetOwner();
			});
			if (Removed > 0)
			{
				Runtime->PublishState(false);
				ChangedShieldOwners.AddUnique(Runtime);
			}
		}
	}
}

void UKnightSkillRuntimeComponent::CommitPreparedEquipmentBuild()
{
	if (!bEquipmentBuildPrepared)
	{
		return;
	}
	if (!(State.EquipmentCombatBuild == PreparedEquipmentBuild))
	{
		if (State.EquipmentCombatBuild.RelicInstanceId != PreparedEquipmentBuild.RelicInstanceId
			|| State.EquipmentCombatBuild.RelicEffectId != PreparedEquipmentBuild.RelicEffectId
			|| State.EquipmentCombatBuild.bEnabled != PreparedEquipmentBuild.bEnabled)
		{
			++ItemGeneration;
			RemoveRelicEffects(State.EquipmentCombatBuild.RelicInstanceId);
			State.StarfallCount = 0;
			State.StarfallCounterEnd = 0.0;
		}
		State.EquipmentCombatBuild = PreparedEquipmentBuild;
	}
	bEquipmentBuildPrepared = false;
	PublishState(false);
}

void UKnightSkillRuntimeComponent::AbortPreparedEquipmentBuild()
{
	bEquipmentBuildPrepared = false;
}

bool UKnightSkillRuntimeComponent::RequiresHammer() const
{
	return State.EquipmentCombatBuild.bEnabled && !State.EquipmentCombatBuild.bHasHammer;
}

float UKnightSkillRuntimeComponent::GetPowerCrystalMultiplier() const
{
	return State.CrystalEnd > ServerTime() ? 1.15f : 1.0f;
}

uint64 UKnightSkillRuntimeComponent::GetItemGeneration() const
{
	return ItemGeneration;
}

bool UKnightSkillRuntimeComponent::CanTriggerRelic(FName Family, FGuid Source, uint64 Generation) const
{
	const UKnightAbilitySystemComponent* ASC = ResolveASC();
	return GetOwner() != nullptr && GetOwner()->HasAuthority() && IsAlive() && ASC != nullptr
		&& !ASC->IsMutationFenced() && State.EquipmentCombatBuild.bEnabled
		&& State.EquipmentCombatBuild.RelicEffectId == Family && Source.IsValid()
		&& State.EquipmentCombatBuild.RelicInstanceId == Source && ItemGeneration == Generation;
}

void UKnightSkillRuntimeComponent::NotifyItemDefenseSuccess(AActor* Source, FGuid ActivationId,
	bool bDirect, uint64 ExpectedGeneration)
{
	const FGuid ItemSource = State.EquipmentCombatBuild.RelicInstanceId;
	if (!bDirect || !ActivationId.IsValid() || !IsValid(Source) || IsFriendly(Source)
		|| !CanTriggerRelic(TEXT("Bulwark"), ItemSource, ExpectedGeneration) || State.BulwarkEnd > ServerTime())
	{
		return;
	}
	State.BulwarkEnd = ServerTime() + 12.0;
	AddShield(GetOwner(), FName(*ItemSource.ToString()), ResolveAttributes()->GetAttackPower() * 0.60f,
		5.0f, 0.35f, ItemSource);
}

void UKnightSkillRuntimeComponent::NotifyPrimaryDirectHeal(AActor* Target, float ActualHealing,
	FGuid ExpectedSource, uint64 ExpectedGeneration)
{
	if (ActualHealing <= 0.0f || !CanTriggerRelic(TEXT("Lifeseed"), ExpectedSource, ExpectedGeneration)
		|| State.LifeseedEnd > ServerTime())
	{
		return;
	}
	const float Magnitude = ResolveAttributes()->GetAttackPower() * 0.15f * (1.0f + ResolveAttributes()->GetVersatility());
	State.LifeseedEnd = ServerTime() + 10.0;
	AddPeriodic(FName(*ExpectedSource.ToString()), Target, Magnitude, 3.0f);
	FPeriodicStream* Stream = Periodics.FindByPredicate([ExpectedSource, Target](const FPeriodicStream& Entry)
	{
		return Entry.Key == FName(*ExpectedSource.ToString()) && Entry.Target.Get() == Target;
	});
	if (Stream != nullptr)
	{
		Stream->ItemSource = ExpectedSource;
		Stream->ItemGeneration = ExpectedGeneration;
	}
	PublishState();
}

void UKnightSkillRuntimeComponent::NotifyConfirmedDirectDamage(AActor* Target, FGuid ActivationId, int32 HitIndex,
	EKnightSkillSchool School, float Damage, AActor* PrimaryTarget, uint64 ExpectedGeneration)
{
	const FGuid ItemSource = State.EquipmentCombatBuild.RelicInstanceId;
	if (Damage <= 0.0f || !ActivationId.IsValid() || HitIndex < 0 || !IsValid(Target)
		|| IsFriendly(Target) || !CanTriggerRelic(TEXT("Starfall"), ItemSource, ExpectedGeneration)
		|| State.StarfallEnd > ServerTime() || SeenItemActivations.Contains(ActivationId))
	{
		return;
	}
	const double Now = ServerTime();
	SeenItemActivations.Add(ActivationId, Now + 10.0);
	if (State.StarfallCounterEnd <= Now)
	{
		State.StarfallCount = 0;
	}
	State.StarfallCounterEnd = Now + 10.0;
	++State.StarfallCount;
	if (State.StarfallCount < 3)
	{
		PublishState();
		return;
	}
	State.StarfallCount = 0;
	State.StarfallEnd = Now + 8.0;
	FKnightSkillDamagePacket Packet;
	Packet.RawDamage = ResolveAttributes()->GetAttackPower() * 0.40f * (1.0f + ResolveAttributes()->GetVersatility());
	Packet.School = School;
	Packet.bDirect = false;
	Packet.bCanDefend = false;
	Packet.bItemProc = true;
	Packet.ActivationId = ActivationId;
	Packet.HitIndex = 1000 + HitIndex;
	AActor* ProcTarget = IsValid(PrimaryTarget) && !IsFriendly(PrimaryTarget) ? PrimaryTarget : Target;
	ApplySkillDamage(GetOwner(), ProcTarget, Packet);
	PublishState();
}

FKnightResult UKnightSkillRuntimeComponent::ValidateConsumable(FName EffectId) const
{
	const UKnightAbilitySystemComponent* ASC = ResolveASC();
	const UKnightAttributeSet* Attributes = ResolveAttributes();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ASC == nullptr || Attributes == nullptr
		|| !IsAlive() || !State.EquipmentCombatBuild.bEnabled || State.ConsumableEnd > ServerTime()
		|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Staggered)
		|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Attacking)
		|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
		|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing)
		|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving))
	{
		return EquipmentFailure(EKnightError::InvalidState, TEXT("ConsumableState"));
	}
	if ((EffectId == TEXT("Health") && Attributes->GetHealth() < Attributes->GetMaxHealth())
		|| (EffectId == TEXT("Resource") && Attributes->GetRuneEnergy() < Attributes->GetMaxRuneEnergy()))
	{
		return FKnightResult::Success();
	}
	return EquipmentFailure(EKnightError::InvalidState, TEXT("ConsumableNeedsRecovery"));
}

FKnightResult UKnightSkillRuntimeComponent::PrepareConsumable(FName EffectId, const FKnightMutationFence& Fence)
{
	UKnightAbilitySystemComponent* ASC = ResolveASC();
	const FKnightResult Validation = ValidateConsumable(EffectId);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (!ASC->ValidateItemMutationFence(Fence).IsSuccess() || PreparedConsumableSpec.IsValid() || bConsumablePreparing)
	{
		return EquipmentFailure(EKnightError::Conflict, TEXT("ConsumableFence"));
	}
	TGuardValue<bool> PreparingGuard(bConsumablePreparing, true);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UKnightConsumableEffect::StaticClass(), 1.0f,
		ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return EquipmentFailure(EKnightError::InvalidState, TEXT("ConsumableSpec"));
	}
	Spec.Data->SetSetByCallerMagnitude(UKnightConsumableEffect::HealthMagnitude,
		EffectId == TEXT("Health") ? ResolveAttributes()->GetMaxHealth() * 0.20f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(UKnightConsumableEffect::ResourceMagnitude,
		EffectId == TEXT("Resource") ? 30.0f : 0.0f);
	const FKnightResult Approval = ASC->PrepareConsumableEffect(Fence, *Spec.Data.Get());
	if (!Approval.IsSuccess())
	{
		return Approval;
	}
	const FKnightResult Revalidation = ValidateConsumable(EffectId);
	if (!Revalidation.IsSuccess())
	{
		return Revalidation;
	}
	PreparedConsumableSpec = Spec;
	PreparedConsumableFence = Fence;
	bConsumableCommitted = false;
	return FKnightResult::Success();
}

void UKnightSkillRuntimeComponent::CommitPreparedConsumable()
{
	if (PreparedConsumableSpec.IsValid() && !bConsumableCommitted)
	{
		State.ConsumableEnd = ServerTime() + 30.0;
		bConsumableCommitted = true;
		PublishState(false);
	}
}

void UKnightSkillRuntimeComponent::AbortPreparedConsumable()
{
	if (!bConsumableCommitted)
	{
		PreparedConsumableSpec = FGameplayEffectSpecHandle();
		PreparedConsumableFence = FKnightMutationFence();
	}
}

void UKnightSkillRuntimeComponent::PublishPreparedConsumable()
{
	if (!bConsumableCommitted || !PreparedConsumableSpec.IsValid())
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = PreparedConsumableSpec;
	const FKnightMutationFence Fence = PreparedConsumableFence;
	PreparedConsumableSpec = FGameplayEffectSpecHandle();
	PreparedConsumableFence = FKnightMutationFence();
	bConsumableCommitted = false;
	ResolveASC()->ExecutePreparedConsumableEffect(Fence, *Spec.Data.Get());
	PublishState();
}
