#include "Skills/GolemSkillRuntimeComponent.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Effects/GolemConsumableEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	FGolemResult EquipmentFailure(EGolemError Error, FName Context)
	{
		return FGolemResult::Failure(Error, Context,
			NSLOCTEXT("GolemEquipment", "Unavailable", "This item action is unavailable in the current state."));
	}
}

FGolemResult UGolemSkillRuntimeComponent::PrepareEquipmentBuild(const FGolemEquipmentCombatBuild& Build)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ResolveASC() == nullptr || !IsAlive()
		|| !Build.Validate().IsSuccess() || HasActiveSkill() || HasDeferredDamage())
	{
		return EquipmentFailure(EGolemError::InvalidState, TEXT("EquipmentCombatPrepare"));
	}
	if (bEquipmentBuildPrepared && !(PreparedEquipmentBuild == Build))
	{
		return EquipmentFailure(EGolemError::Conflict, TEXT("EquipmentAlreadyPrepared"));
	}
	EnsureInitialized();
	PreparedEquipmentBuild = Build;
	bEquipmentBuildPrepared = true;
	return FGolemResult::Success();
}

void UGolemSkillRuntimeComponent::RemoveRelicEffects(FGuid Source)
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
		if (UGolemSkillRuntimeComponent* Runtime = It->FindComponentByClass<UGolemSkillRuntimeComponent>())
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

void UGolemSkillRuntimeComponent::CommitPreparedEquipmentBuild()
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

void UGolemSkillRuntimeComponent::AbortPreparedEquipmentBuild()
{
	bEquipmentBuildPrepared = false;
}

bool UGolemSkillRuntimeComponent::RequiresHammer() const
{
	return State.EquipmentCombatBuild.bEnabled && !State.EquipmentCombatBuild.bHasHammer;
}

float UGolemSkillRuntimeComponent::GetPowerCrystalMultiplier() const
{
	return State.CrystalEnd > ServerTime() ? 1.15f : 1.0f;
}

uint64 UGolemSkillRuntimeComponent::GetItemGeneration() const
{
	return ItemGeneration;
}

bool UGolemSkillRuntimeComponent::CanTriggerRelic(FName Family, FGuid Source, uint64 Generation) const
{
	const UGolemAbilitySystemComponent* ASC = ResolveASC();
	return GetOwner() != nullptr && GetOwner()->HasAuthority() && IsAlive() && ASC != nullptr
		&& !ASC->IsMutationFenced() && State.EquipmentCombatBuild.bEnabled
		&& State.EquipmentCombatBuild.RelicEffectId == Family && Source.IsValid()
		&& State.EquipmentCombatBuild.RelicInstanceId == Source && ItemGeneration == Generation;
}

void UGolemSkillRuntimeComponent::NotifyItemDefenseSuccess(AActor* Source, FGuid ActivationId,
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

void UGolemSkillRuntimeComponent::NotifyPrimaryDirectHeal(AActor* Target, float ActualHealing,
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

void UGolemSkillRuntimeComponent::NotifyConfirmedDirectDamage(AActor* Target, FGuid ActivationId, int32 HitIndex,
	EGolemSkillSchool School, float Damage, AActor* PrimaryTarget, uint64 ExpectedGeneration)
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
	FGolemSkillDamagePacket Packet;
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

FGolemResult UGolemSkillRuntimeComponent::ValidateConsumable(FName EffectId) const
{
	const UGolemAbilitySystemComponent* ASC = ResolveASC();
	const UGolemAttributeSet* Attributes = ResolveAttributes();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ASC == nullptr || Attributes == nullptr
		|| !IsAlive() || !State.EquipmentCombatBuild.bEnabled || State.ConsumableEnd > ServerTime()
		|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered)
		|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Attacking)
		|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Finishing)
		|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Traversing)
		|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving))
	{
		return EquipmentFailure(EGolemError::InvalidState, TEXT("ConsumableState"));
	}
	if ((EffectId == TEXT("Health") && Attributes->GetHealth() < Attributes->GetMaxHealth())
		|| (EffectId == TEXT("Resource") && Attributes->GetRuneEnergy() < Attributes->GetMaxRuneEnergy()))
	{
		return FGolemResult::Success();
	}
	return EquipmentFailure(EGolemError::InvalidState, TEXT("ConsumableNeedsRecovery"));
}

FGolemResult UGolemSkillRuntimeComponent::PrepareConsumable(FName EffectId, const FGolemMutationFence& Fence)
{
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	const FGolemResult Validation = ValidateConsumable(EffectId);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (!ASC->ValidateItemMutationFence(Fence).IsSuccess() || PreparedConsumableSpec.IsValid() || bConsumablePreparing)
	{
		return EquipmentFailure(EGolemError::Conflict, TEXT("ConsumableFence"));
	}
	TGuardValue<bool> PreparingGuard(bConsumablePreparing, true);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGolemConsumableEffect::StaticClass(), 1.0f,
		ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return EquipmentFailure(EGolemError::InvalidState, TEXT("ConsumableSpec"));
	}
	Spec.Data->SetSetByCallerMagnitude(UGolemConsumableEffect::HealthMagnitude,
		EffectId == TEXT("Health") ? ResolveAttributes()->GetMaxHealth() * 0.20f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(UGolemConsumableEffect::ResourceMagnitude,
		EffectId == TEXT("Resource") ? 30.0f : 0.0f);
	const FGolemResult Approval = ASC->PrepareConsumableEffect(Fence, *Spec.Data.Get());
	if (!Approval.IsSuccess())
	{
		return Approval;
	}
	const FGolemResult Revalidation = ValidateConsumable(EffectId);
	if (!Revalidation.IsSuccess())
	{
		return Revalidation;
	}
	PreparedConsumableSpec = Spec;
	PreparedConsumableFence = Fence;
	bConsumableCommitted = false;
	return FGolemResult::Success();
}

void UGolemSkillRuntimeComponent::CommitPreparedConsumable()
{
	if (PreparedConsumableSpec.IsValid() && !bConsumableCommitted)
	{
		State.ConsumableEnd = ServerTime() + 30.0;
		bConsumableCommitted = true;
		PublishState(false);
	}
}

void UGolemSkillRuntimeComponent::AbortPreparedConsumable()
{
	if (!bConsumableCommitted)
	{
		PreparedConsumableSpec = FGameplayEffectSpecHandle();
		PreparedConsumableFence = FGolemMutationFence();
	}
}

void UGolemSkillRuntimeComponent::PublishPreparedConsumable()
{
	if (!bConsumableCommitted || !PreparedConsumableSpec.IsValid())
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = PreparedConsumableSpec;
	const FGolemMutationFence Fence = PreparedConsumableFence;
	PreparedConsumableSpec = FGameplayEffectSpecHandle();
	PreparedConsumableFence = FGolemMutationFence();
	bConsumableCommitted = false;
	ResolveASC()->ExecutePreparedConsumableEffect(Fence, *Spec.Data.Get());
	PublishState();
}
