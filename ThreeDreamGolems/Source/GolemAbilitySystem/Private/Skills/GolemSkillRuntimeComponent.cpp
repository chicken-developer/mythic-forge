#include "Skills/GolemSkillRuntimeComponent.h"

#include "Abilities/GolemSpecializationAbility.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Damage/GolemTauntResponse.h"
#include "Effects/GolemDamageEffect.h"
#include "Effects/GolemPowerCrystalEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemSkillLabEvent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

namespace
{
	FGolemResult SkillFailure(EGolemError Error, FName Context)
	{
		return FGolemResult::Failure(Error, Context,
			NSLOCTEXT("GolemSkills", "Unavailable", "This skill cannot be used with the current target, resources or cooldown."));
	}
	UGolemAbilitySystemComponent* ActorASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	UGolemSkillRuntimeComponent* TargetRuntime(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}
		UGolemSkillRuntimeComponent* Runtime = Actor->FindComponentByClass<UGolemSkillRuntimeComponent>();
		if (Runtime == nullptr && Actor->HasAuthority() && ActorASC(Actor) != nullptr)
		{
			Runtime = NewObject<UGolemSkillRuntimeComponent>(Actor, TEXT("SkillStatusRuntime"));
			Actor->AddInstanceComponent(Runtime);
			Runtime->RegisterComponent();
		}
		return Runtime;
	}
	bool ApplyInstant(UGolemAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float Amount)
	{
		if (ASC == nullptr || !FMath::IsFinite(Amount))
		{
			return false;
		}
		UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage());
		GE->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = GE->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(Amount);
		return ASC->ApplyGameplayEffectSpecToSelf(FGameplayEffectSpec(GE, ASC->MakeEffectContext(), 1.0f)).WasSuccessfullyApplied();
	}
}

UGolemSkillRuntimeComponent::UGolemSkillRuntimeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	State.CooldownEnds.Init(0.0, 4);
}

void UGolemSkillRuntimeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemSkillRuntimeComponent, State);
}

void UGolemSkillRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();
}

void UGolemSkillRuntimeComponent::EnsureInitialized()
{
	if (bInitialized || GetOwner() == nullptr || !GetOwner()->HasAuthority() || ResolveASC() == nullptr
		|| ResolveASC()->GetLifeComponent() == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	bInitialized = true;
	LifeHandle = ResolveASC()->GetLifeComponent()->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleLifeChanged);
	MaxHealthHandle = ResolveASC()->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	GetWorld()->GetTimerManager().SetTimer(RuntimeTimer, this, &ThisClass::AdvanceRuntime, 0.1f, true);
}

void UGolemSkillRuntimeComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(RuntimeTimer);
		for (FTimerHandle& Timer : ProjectileTimers)
		{
			GetWorld()->GetTimerManager().ClearTimer(Timer);
		}
	}
	if (UGolemAbilitySystemComponent* ASC = ResolveASC())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
		if (ASC->GetLifeComponent() != nullptr)
		{
			ASC->GetLifeComponent()->OnLifeStateChanged.Remove(LifeHandle);
		}
		if (GetOwner()->HasAuthority())
		{
			for (FGameplayAbilitySpecHandle Handle : AbilityHandles)
			{
				ASC->ClearAbility(Handle);
			}
			ASC->RemoveActiveGameplayEffect(CrystalEffect);
		}
	}
	Super::EndPlay(EndPlayReason);
}

UGolemAbilitySystemComponent* UGolemSkillRuntimeComponent::ResolveASC() const
{
	return ActorASC(GetOwner());
}

UGolemAttributeSet* UGolemSkillRuntimeComponent::ResolveAttributes() const
{
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	return ASC != nullptr ? ASC->GetGolemAttributes() : nullptr;
}

bool UGolemSkillRuntimeComponent::IsAlive() const
{
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	return ASC != nullptr && ASC->GetLifeComponent() != nullptr && ASC->GetLifeComponent()->IsAlive();
}

double UGolemSkillRuntimeComponent::ServerTime() const
{
	const AGameStateBase* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0;
}

FGolemResult UGolemSkillRuntimeComponent::ConfigureBuild(const FGolemSkillBuild& Build)
{
	const FGolemResult Result = PrepareBuild(Build);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	CommitPreparedBuild();
	PublishPreparedBuild();
	return FGolemResult::Success();
}

FGolemResult UGolemSkillRuntimeComponent::PrepareBuild(const FGolemSkillBuild& Build)
{
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ASC == nullptr)
	{
		return SkillFailure(EGolemError::NotAuthority, TEXT("ConfigureSkills"));
	}
	if (!FGolemSkillCatalog::ValidateDescriptor(Build).IsSuccess() || HasActiveSkill()
		|| (State.Build.SpecId != Build.SpecId && HasDeferredDamage())
		|| ProjectileTimers.ContainsByPredicate([this](const FTimerHandle& Timer)
		{
			return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(Timer);
		}))
	{
		return SkillFailure(EGolemError::Conflict, TEXT("SkillDebtOrSpec"));
	}
	EnsureInitialized();
	if (bBuildPrepared)
	{
		return PreparedBuild == Build ? FGolemResult::Success() : SkillFailure(EGolemError::Conflict, TEXT("SkillBuildPrepared"));
	}
	if (!Build.SpecId.IsNone() && AbilityHandles.IsEmpty())
	{
		for (int32 Slot = 1; Slot <= 4; ++Slot)
		{
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(
				FGameplayAbilitySpec(UGolemSpecializationAbility::StaticClass(), 1, Slot, this));
			AbilityHandles.Add(Handle);
			bPreparedCreatedGrants = true;
			if (!Handle.IsValid() || ASC->FindAbilitySpecFromHandle(Handle) == nullptr)
			{
				AbortPreparedBuild();
				return SkillFailure(EGolemError::InvalidState, TEXT("SkillGrantRejected"));
			}
		}
	}
	PreparedBuild = Build;
	bBuildPrepared = true;
	return FGolemResult::Success();
}

void UGolemSkillRuntimeComponent::CommitPreparedBuild()
{
	if (!bBuildPrepared)
	{
		return;
	}
	const FGolemSkillBuild Build = PreparedBuild;
	if (State.Build.SpecId != Build.SpecId)
	{
		State.Stacks = 0;
		State.StackEnd = 0.0;
		State.BuffEnd = 0.0;
		State.WaterReadyEnd = 0.0;
		State.IceReadyEnd = 0.0;
		Periodics.RemoveAll([](const FPeriodicStream& Stream)
		{
			return !Stream.ItemSource.IsValid();
		});
		// Remove only pools cast by this source; shields from other Golems remain intact.
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (UGolemSkillRuntimeComponent* Runtime = It->FindComponentByClass<UGolemSkillRuntimeComponent>())
			{
				const int32 Removed = Runtime->Shields.RemoveAll([this](const FShieldPool& Pool)
				{
					return Pool.Caster.Get() == GetOwner() && !Pool.ItemSource.IsValid();
				});
				if (Removed > 0)
				{
					Runtime->PublishState(false);
					ChangedShieldOwners.Add(Runtime);
				}
			}
		}
	}
	State.Build = Build;
	bBuildPrepared = false;
	bPreparedCreatedGrants = false;
	PublishState(false);
}

void UGolemSkillRuntimeComponent::AbortPreparedBuild()
{
	if (bPreparedCreatedGrants && ResolveASC() != nullptr)
	{
		for (FGameplayAbilitySpecHandle Handle : AbilityHandles)
		{
			ResolveASC()->ClearAbility(Handle);
		}
		AbilityHandles.Reset();
	}
	bBuildPrepared = false;
	bPreparedCreatedGrants = false;
}

void UGolemSkillRuntimeComponent::PublishPreparedBuild(bool bNotifySubscribers)
{
	if (State.Build.SpecId.IsNone() && ResolveASC() != nullptr)
	{
		for (FGameplayAbilitySpecHandle Handle : AbilityHandles)
		{
			ResolveASC()->ClearAbility(Handle);
		}
		AbilityHandles.Reset();
	}
	const TArray<TWeakObjectPtr<UGolemSkillRuntimeComponent>> Owners = MoveTemp(ChangedShieldOwners);
	ChangedShieldOwners.Reset();
	for (const TWeakObjectPtr<UGolemSkillRuntimeComponent>& Owner : Owners)
	{
		if (Owner.IsValid() && Owner.Get() != this)
		{
			Owner->PublishState(bNotifySubscribers);
		}
	}
	PublishState(bNotifySubscribers);
}

bool UGolemSkillRuntimeComponent::HasUnsnapshottedEffects() const
{
	const double Now = ServerTime();
	return HasActiveSkill() || HasDeferredDamage() || Shields.ContainsByPredicate([Now](const FShieldPool& Pool)
	{
		return Pool.Amount > 0.0f && Pool.End > Now;
	}) || Periodics.ContainsByPredicate([Now](const FPeriodicStream& Stream)
	{
		return Stream.End > Now;
	})
		|| (CrystalEffect.IsValid() && CrystalEnd > Now)
		|| ProjectileTimers.ContainsByPredicate([this](const FTimerHandle& Timer)
		{
			return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(Timer);
		});
}

bool UGolemSkillRuntimeComponent::HasActiveSkill() const
{
	const UGolemAbilitySystemComponent* ASC = ResolveASC();
	if (ASC == nullptr)
	{
		return false;
	}
	for (FGameplayAbilitySpecHandle Handle : AbilityHandles)
	{
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		if (Spec != nullptr && Spec->IsActive())
		{
			return true;
		}
	}
	return false;
}

bool UGolemSkillRuntimeComponent::IsFriendly(AActor* Target) const
{
	const UGolemAbilitySystemComponent* Source = ResolveASC();
	const UGolemAbilitySystemComponent* Other = ActorASC(Target);
	return Source != nullptr && Other != nullptr && Source->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)
		== Other->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem);
}

bool UGolemSkillRuntimeComponent::HasLineOfSight(AActor* Target) const
{
	if (Target == GetOwner())
	{
		return true;
	}
	if (!IsValid(Target) || GetWorld() == nullptr)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemSkillLOS), false, GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GetOwner()->GetActorLocation(),
		Target->GetActorLocation(), ECC_Visibility, Query);
	return !bHit || Hit.GetActor() == Target;
}

FGolemResult UGolemSkillRuntimeComponent::ValidateSkill(int32 Slot, AActor* Target) const
{
	if (RequiresHammer())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RequiresHammer"),
			NSLOCTEXT("KnightEquipment", "RequiresHammer", "Equip a hammer to use this action."));
	}
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	const UGolemAttributeSet* A = ResolveAttributes();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ASC == nullptr || A == nullptr || !IsAlive()
		|| State.Build.SpecId.IsNone() || Slot < 1 || Slot > 4 || ASC->IsMutationFenced()
		|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered))
	{
		return SkillFailure(EGolemError::InvalidState, TEXT("SkillState"));
	}
	const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(State.Build, Slot);
	float Cost = D.Cost;
	if (State.Build.SpecId == TEXT("Venus") && State.Stacks == 3 && State.StackEnd > ServerTime()
		&& Slot > 1 && Slot < 4)
	{
		Cost *= 0.5f;
	}
	if (State.Build.SpecId == TEXT("Pluto") && Slot == 1 && State.WaterReadyEnd > ServerTime())
	{
		Cost *= 0.75f;
	}
	if (State.CooldownEnds.IsValidIndex(Slot - 1) && State.CooldownEnds[Slot - 1] > ServerTime())
	{
		return SkillFailure(EGolemError::Conflict, TEXT("SkillCooldown"));
	}
	if (A->GetRuneEnergy() < Cost)
	{
		return SkillFailure(EGolemError::InsufficientResources, TEXT("SkillResource"));
	}
	if (Slot == 4 || (State.Build.SpecId == TEXT("Jupiter") && Slot == 3)
		|| (State.Build.SpecId == TEXT("Pluto") && Slot == 3))
	{
		return FGolemResult::Success();
	}
	AActor* ResolvedTarget = Target;
	if (D.bFriendly && ResolvedTarget == nullptr)
	{
		ResolvedTarget = GetOwner();
	}
	UGolemAbilitySystemComponent* TargetASC = ActorASC(ResolvedTarget);
	if (TargetASC == nullptr || TargetASC->GetLifeComponent() == nullptr || !TargetASC->GetLifeComponent()->IsAlive()
		|| D.bFriendly != IsFriendly(ResolvedTarget))
	{
		return SkillFailure(EGolemError::InvalidState, TEXT("SkillTarget"));
	}
	if (FVector::DistSquared(GetOwner()->GetActorLocation(), ResolvedTarget->GetActorLocation()) > FMath::Square(D.Range)
		|| !HasLineOfSight(ResolvedTarget))
	{
		return SkillFailure(EGolemError::OutOfRange, TEXT("SkillRangeLOS"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemSkillRuntimeComponent::RequestSkill(int32 Slot, AActor* Target)
{
	const FGolemResult Validation = ValidateSkill(Slot, Target);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			return SkillFailure(EGolemError::Conflict, TEXT("SkillActionRunning"));
		}
	}
	if (!AbilityHandles.IsValidIndex(Slot - 1))
	{
		return SkillFailure(EGolemError::NotFound, TEXT("SkillGrant"));
	}
	return ASC->ActivateSpecializationHandle(AbilityHandles[Slot - 1], Target);
}

FGolemSkillRuntimeState UGolemSkillRuntimeComponent::GetSkillState() const
{
	return State;
}

FGolemSkillBuild UGolemSkillRuntimeComponent::GetBuild() const
{
	return State.Build;
}

FGolemSkillRuntimeSnapshot UGolemSkillRuntimeComponent::CaptureSnapshot() const
{
	FGolemSkillRuntimeSnapshot Snapshot;
	const double Now = ServerTime();
	for (double End : State.CooldownEnds)
	{
		Snapshot.CooldownRemaining.Add(FMath::Max(0.0, End - Now));
	}
	Snapshot.Resource = ResolveAttributes() != nullptr ? ResolveAttributes()->GetRuneEnergy() : 0.0f;
	Snapshot.Stacks = State.Stacks;
	Snapshot.StackRemaining = FMath::Max(0.0, State.StackEnd - Now);
	Snapshot.BuffRemaining = FMath::Max(0.0, State.BuffEnd - Now);
	Snapshot.WaterReadyRemaining = FMath::Max(0.0, State.WaterReadyEnd - Now);
	Snapshot.IceReadyRemaining = FMath::Max(0.0, State.IceReadyEnd - Now);
	Snapshot.AftershockRemaining = FMath::Max(0.0, AftershockEnd - Now);
	const float StackInterval = State.Build.SpecId == TEXT("Saturn") ? 2.0f : State.Build.SpecId == TEXT("Uranus") ? 1.0f : 0.0f;
	Snapshot.StackGainRemaining = FMath::Max(0.0, LastStack + StackInterval - Now);
	Snapshot.ResourceRegenDelayRemaining = FMath::Max(0.0, LastSpend + 1.0 - Now);
	Snapshot.BulwarkRemaining = FMath::Max(0.0, State.BulwarkEnd - Now);
	Snapshot.LifeseedRemaining = FMath::Max(0.0, State.LifeseedEnd - Now);
	Snapshot.StarfallRemaining = FMath::Max(0.0, State.StarfallEnd - Now);
	Snapshot.StarfallCounterRemaining = FMath::Max(0.0, State.StarfallCounterEnd - Now);
	Snapshot.ConsumableRemaining = FMath::Max(0.0, State.ConsumableEnd - Now);
	Snapshot.StarfallCount = State.StarfallCounterEnd > Now ? State.StarfallCount : 0;
	return Snapshot;
}

FGolemResult UGolemSkillRuntimeComponent::ValidateSnapshot(const FGolemSkillRuntimeSnapshot& Snapshot)
{
	if (Snapshot.CooldownRemaining.Num() != 4 || !FMath::IsFinite(Snapshot.Resource) || Snapshot.Resource < 0.0f
		|| Snapshot.Resource > 1000000.0f
		|| Snapshot.Stacks < 0 || Snapshot.Stacks > 5 || Snapshot.StarfallCount < 0 || Snapshot.StarfallCount > 2
		|| Snapshot.BulwarkRemaining > 12.0f || Snapshot.LifeseedRemaining > 10.0f
		|| Snapshot.StarfallRemaining > 8.0f || Snapshot.StarfallCounterRemaining > 10.0f
		|| Snapshot.ConsumableRemaining > 30.0f)
	{
		return SkillFailure(EGolemError::InvalidData, TEXT("SkillSnapshot"));
	}
	const float Durations[] { Snapshot.StackRemaining, Snapshot.BuffRemaining,
		Snapshot.WaterReadyRemaining, Snapshot.IceReadyRemaining, Snapshot.AftershockRemaining,
		Snapshot.StackGainRemaining, Snapshot.ResourceRegenDelayRemaining, Snapshot.BulwarkRemaining,
		Snapshot.LifeseedRemaining, Snapshot.StarfallRemaining, Snapshot.StarfallCounterRemaining, Snapshot.ConsumableRemaining };
	for (float Duration : Durations)
	{
		if (!FMath::IsFinite(Duration) || Duration < 0.0f || Duration > 60.0f)
		{
			return SkillFailure(EGolemError::InvalidData, TEXT("SkillSnapshotDuration"));
		}
	}
	for (float Remaining : Snapshot.CooldownRemaining)
	{
		if (!FMath::IsFinite(Remaining) || Remaining < 0.0f || Remaining > 120.0f)
		{
			return SkillFailure(EGolemError::InvalidData, TEXT("SkillSnapshotCooldown"));
		}
	}
	return FGolemResult::Success();
}

FGolemResult UGolemSkillRuntimeComponent::RestoreSnapshot(const FGolemSkillRuntimeSnapshot& Snapshot, bool bNotifySubscribers)
{
	const FGolemResult Validation = ValidateSnapshot(Snapshot);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ResolveAttributes() == nullptr)
	{
		return SkillFailure(EGolemError::NotAuthority, TEXT("SkillSnapshotAuthority"));
	}
	const double Now = ServerTime();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		State.CooldownEnds[Index] = Now + Snapshot.CooldownRemaining[Index];
	}
	State.Stacks = Snapshot.Stacks;
	State.StackEnd = Now + Snapshot.StackRemaining;
	State.BuffEnd = Now + Snapshot.BuffRemaining;
	State.WaterReadyEnd = Now + Snapshot.WaterReadyRemaining;
	State.IceReadyEnd = Now + Snapshot.IceReadyRemaining;
	AftershockEnd = Now + Snapshot.AftershockRemaining;
	const float StackInterval = State.Build.SpecId == TEXT("Saturn") ? 2.0f : State.Build.SpecId == TEXT("Uranus") ? 1.0f : 0.0f;
	LastStack = Now - StackInterval + Snapshot.StackGainRemaining;
	LastSpend = Now - 1.0 + Snapshot.ResourceRegenDelayRemaining;
	State.BulwarkEnd = Now + Snapshot.BulwarkRemaining;
	State.LifeseedEnd = Now + Snapshot.LifeseedRemaining;
	State.StarfallEnd = Now + Snapshot.StarfallRemaining;
	State.StarfallCounterEnd = Now + Snapshot.StarfallCounterRemaining;
	State.ConsumableEnd = Now + Snapshot.ConsumableRemaining;
	State.StarfallCount = Snapshot.StarfallCount;
	ResolveASC()->SetNumericAttributeBase(UGolemAttributeSet::GetRuneEnergyAttribute(),
		FMath::Min(Snapshot.Resource, ResolveAttributes()->GetMaxRuneEnergy()));
	PublishState(bNotifySubscribers);
	return FGolemResult::Success();
}

float UGolemSkillRuntimeComponent::GetDeferredDamage() const
{
	return State.DeferredDamage;
}

bool UGolemSkillRuntimeComponent::HasDeferredDamage() const
{
	return State.DeferredDamage > KINDA_SMALL_NUMBER;
}

bool UGolemSkillRuntimeComponent::IsSkillAvailable(int32 Slot) const
{
	return Slot >= 1 && Slot <= 4 && !State.Build.SpecId.IsNone()
		&& State.CooldownEnds.IsValidIndex(Slot - 1) && State.CooldownEnds[Slot - 1] <= ServerTime();
}

void UGolemSkillRuntimeComponent::SetEncounterId(FGuid InEncounterId)
{
	EncounterId = InEncounterId;
}

FGameplayAbilitySpecHandle UGolemSkillRuntimeComponent::GetSkillHandle(int32 Slot) const
{
	return AbilityHandles.IsValidIndex(Slot - 1) ? AbilityHandles[Slot - 1] : FGameplayAbilitySpecHandle();
}

TArray<AActor*> UGolemSkillRuntimeComponent::FindTargets(const FVector& Center, float Radius,
	bool bFriendly, AActor* Exclude) const
{
	TArray<AActor*> Targets;
	if (GetWorld() == nullptr)
	{
		return Targets;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		UGolemAbilitySystemComponent* ASC = ActorASC(Actor);
		if (Actor != Exclude && ASC != nullptr && ASC->GetLifeComponent() != nullptr
			&& ASC->GetLifeComponent()->IsAlive() && IsFriendly(Actor) == bFriendly
			&& FVector::DistSquared(Center, Actor->GetActorLocation()) <= FMath::Square(Radius))
		{
			Targets.Add(Actor);
		}
	}
	Targets.Sort([Center](const AActor& Left, const AActor& Right)
	{
		const float L = FVector::DistSquared(Center, Left.GetActorLocation());
		const float R = FVector::DistSquared(Center, Right.GetActorLocation());
		return L == R ? Left.GetName() < Right.GetName() : L < R;
	});
	return Targets;
}

AActor* UGolemSkillRuntimeComponent::SelectLowestHealthAlly(float Radius, AActor* Around, AActor* Exclude) const
{
	const FVector Center = IsValid(Around) ? Around->GetActorLocation() : GetOwner()->GetActorLocation();
	AActor* Best = nullptr;
	float Missing = 0.0f;
	for (AActor* Ally : FindTargets(Center, Radius, true, Exclude))
	{
		const UGolemAttributeSet* A = ActorASC(Ally)->GetGolemAttributes();
		const float Candidate = A != nullptr ? A->GetMaxHealth() - A->GetHealth() : 0.0f;
		if (Candidate > Missing && HasLineOfSight(Ally))
		{
			Missing = Candidate;
			Best = Ally;
		}
	}
	return Best;
}

void UGolemSkillRuntimeComponent::AddStack(int32 Count, int32 Cap, float Duration, float Interval)
{
	const double Now = ServerTime();
	if (Now - LastStack < Interval)
	{
		return;
	}
	State.Stacks = FMath::Clamp(State.Stacks + Count, 0, Cap);
	State.StackEnd = Now + Duration;
	LastStack = Now;
	PublishState();
}

void UGolemSkillRuntimeComponent::AddShield(AActor* Caster, FName Source, float Amount, float Duration,
	float SourceCap, FGuid ItemSource)
{
	const UGolemAttributeSet* A = ResolveAttributes();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || A == nullptr || !IsAlive() || Amount <= 0.0f)
	{
		return;
	}
	const double Now = ServerTime();
	Shields.RemoveAll([Now](const FShieldPool& Pool)
	{
		return Pool.End <= Now || Pool.Amount <= 0.0f;
	});
	FShieldPool* Pool = Shields.FindByPredicate([Caster, Source](const FShieldPool& Entry)
	{
		return Entry.Source == Source && Entry.Caster.Get() == Caster;
	});
	if (Pool == nullptr)
	{
		Pool = &Shields.AddDefaulted_GetRef();
		Pool->Source = Source;
		Pool->Caster = Caster;
	}
	float Other = 0.0f;
	for (const FShieldPool& Entry : Shields)
	{
		if (&Entry != Pool)
		{
			Other += Entry.Amount;
		}
	}
	const float RemainingCap = FMath::Max(0.0f, A->GetMaxHealth() * 0.35f - Other);
	Pool->Amount = FMath::Min(FMath::Max(Pool->Amount, FMath::Min(Amount, A->GetMaxHealth() * SourceCap)), RemainingCap);
	Pool->CapFraction = SourceCap;
	Pool->End = Now + Duration;
	Pool->ItemSource = ItemSource;
	PublishState();
}

float UGolemSkillRuntimeComponent::ApplyHeal(AActor* Target, float Magnitude, bool bDirect, bool bCanProcShield)
{
	UGolemAbilitySystemComponent* ASC = ActorASC(Target);
	if (ASC == nullptr || ASC->GetGolemAttributes() == nullptr || ASC->GetLifeComponent() == nullptr
		|| !ASC->GetLifeComponent()->IsAlive() || !Target->HasAuthority() || !IsFriendly(Target))
	{
		return 0.0f;
	}
	UGolemAttributeSet* A = ASC->GetGolemAttributes();
	const float Before = A->GetHealth();
	const bool bBuff = State.BuffEnd > ServerTime();
	if (State.Build.SpecId == TEXT("Terra") && bBuff && Before < A->GetMaxHealth() * 0.40f)
	{
		Magnitude *= 1.25f;
	}
	ApplyInstant(ASC, UGolemAttributeSet::GetHealthAttribute(), Magnitude);
	const float Actual = A->GetHealth() - Before;
	if (bCanProcShield && bBuff)
	{
		if (State.Build.SpecId == TEXT("Venus") && bDirect && Actual > 0.0f)
		{
			TargetRuntime(Target)->AddShield(GetOwner(), TEXT("Venus.A4"), Actual * 0.25f, 5.0f, 0.15f);
		}
		if (State.Build.SpecId == TEXT("Terra"))
		{
			TargetRuntime(Target)->AddShield(GetOwner(), TEXT("Terra.A4"), FMath::Max(0.0f, Magnitude - Actual) * 0.30f, 5.0f, 0.15f);
		}
	}
	return Actual;
}

FGolemResult UGolemSkillRuntimeComponent::ApplyExternalHeal(float Magnitude)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ResolveAttributes() == nullptr || !IsAlive()
		|| !FMath::IsFinite(Magnitude) || Magnitude <= 0.0f || ResolveASC()->IsMutationFenced())
	{
		return SkillFailure(EGolemError::InvalidState, TEXT("ExternalHeal"));
	}
	return ApplyInstant(ResolveASC(), UGolemAttributeSet::GetHealthAttribute(), Magnitude)
		? FGolemResult::Success() : SkillFailure(EGolemError::InvalidState, TEXT("ExternalHealGE"));
}

void UGolemSkillRuntimeComponent::AddPeriodic(FName Key, AActor* Target, float Magnitude,
	float Duration, bool bDamage, bool bMercury)
{
	FPeriodicStream* Stream = Periodics.FindByPredicate([Key, Target](const FPeriodicStream& Entry)
	{
		return Entry.Key == Key && Entry.Target.Get() == Target;
	});
	if (Stream == nullptr)
	{
		Stream = &Periodics.AddDefaulted_GetRef();
		Stream->Key = Key;
		Stream->Target = Target;
		Stream->NextTick = ServerTime() + 1.0;
	}
	Stream->Magnitude = Magnitude;
	Stream->End = ServerTime() + Duration;
	Stream->bDamage = bDamage;
	Stream->bMercury = bMercury;
}

void UGolemSkillRuntimeComponent::AddGroundPeriodic(AActor* Target, const FGolemSpecActionDefinition& D, float Magnitude)
{
	FPeriodicStream* Stream = Periodics.FindByPredicate([](const FPeriodicStream& Entry)
	{
		return Entry.Key == TEXT("Terra.A2");
	});
	if (Stream == nullptr)
	{
		Stream = &Periodics.AddDefaulted_GetRef();
		Stream->NextTick = ServerTime() + 1.0;
	}
	Stream->Key = TEXT("Terra.A2");
	Stream->Target = Target;
	Stream->Ground = Target->GetActorLocation();
	Stream->Magnitude = Magnitude;
	Stream->Radius = D.Radius;
	Stream->End = ServerTime() + D.Duration;
	Stream->bGround = true;
	Stream->bMobile = State.Build.SkillRune == TEXT("Skill.Mobile");
}

void UGolemSkillRuntimeComponent::NotifyDefenseSuccess()
{
	if (State.Build.SpecId == TEXT("Uranus") && IsAlive())
	{
		AddStack(1, 3, 8.0f, 1.0f);
	}
}

void UGolemSkillRuntimeComponent::NotifyHeavyHit(AActor* Target)
{
	if (State.Build.SpecId == TEXT("Saturn") && IsAlive())
	{
		AddStack(1, 4, 8.0f, 2.0f);
	}
	if (State.Build.WeaponRune != TEXT("Weapon.Aftershock") || ServerTime() < AftershockEnd || !IsValid(Target))
	{
		return;
	}
	AftershockEnd = ServerTime() + 4.0;
	FGolemSkillDamagePacket P;
	P.RawDamage = ResolveAttributes()->GetAttackPower() * 0.30f * (1.0f + ResolveAttributes()->GetVersatility());
	P.PoiseDamage = 5.0f;
	P.bDirect = false;
	P.ActivationId = FGuid::NewGuid();
	TArray<AActor*> Enemies = FindTargets(Target->GetActorLocation(), 200.0f, false);
	for (int32 Index = 0; Index < FMath::Min(3, Enemies.Num()); ++Index)
	{
		P.HitIndex = Index;
		ApplySkillDamage(GetOwner(), Enemies[Index], P);
	}
}

float UGolemSkillRuntimeComponent::GetIncomingReduction(AActor* Source) const
{
	float Reduction = 0.0f;
	const double Now = ServerTime();
	if (State.Build.SpecId == TEXT("Saturn") && State.StackEnd > Now)
	{
		Reduction += State.Stacks * 0.03f;
	}
	if (State.Build.SpecId == TEXT("Neptune") && State.BuffEnd > Now)
	{
		Reduction += 0.15f;
	}
	if (GetWorld() != nullptr && IsValid(Source))
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			const UGolemSkillRuntimeComponent* R = It->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (R == nullptr || R->State.Build.SpecId != TEXT("Uranus") || R->State.BuffEnd <= Now || !R->IsAlive()
				|| !IsFriendly(*It))
			{
				continue;
			}
			const FVector Relative = GetOwner()->GetActorLocation() - It->GetActorLocation();
			const FVector Incoming = (Source->GetActorLocation() - It->GetActorLocation()).GetSafeNormal2D();
			const bool bBehind = It->GetActorLocation().Equals(GetOwner()->GetActorLocation(), 1.0f)
				|| FVector::DotProduct(-It->GetActorForwardVector(), Relative.GetSafeNormal2D()) >= FMath::Cos(FMath::DegreesToRadians(30.0f));
			if (Relative.SizeSquared() <= FMath::Square(600.0f) && bBehind
				&& FVector::DotProduct(It->GetActorForwardVector(), Incoming) >= 0.0f)
			{
				Reduction += 0.25f;
				break;
			}
		}
	}
	return Reduction;
}

float UGolemSkillRuntimeComponent::GetPoiseScale() const
{
	return State.Build.SpecId == TEXT("Saturn") && State.BuffEnd > ServerTime() ? 0.5f : 1.0f;
}

float UGolemSkillRuntimeComponent::ResolveDamagePools(float Damage, bool bEligibleDeferred)
{
	PublishState(false);
	if (State.Build.SpecId == TEXT("Neptune") && bEligibleDeferred && IsAlive())
	{
		FDebtTranche& Debt = Debts.AddDefaulted_GetRef();
		Debt.PerTick = Damage * 0.25f / 5.0f;
		Debt.NextTick = ServerTime() + 1.0;
		Damage *= 0.75f;
	}
	const double Now = ServerTime();
	Shields.RemoveAll([Now](const FShieldPool& Pool)
	{
		return Pool.End <= Now || Pool.Amount <= 0.0f;
	});
	for (FShieldPool& Pool : Shields)
	{
		const float Absorbed = FMath::Min(Pool.Amount, Damage);
		Pool.Amount -= Absorbed;
		Damage -= Absorbed;
	}
	PublishState();
	return Damage;
}

void UGolemSkillRuntimeComponent::AdvanceRuntime()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ResolveAttributes() == nullptr || ResolveASC()->IsMutationFenced())
	{
		return;
	}
	const double Now = ServerTime();
	if (State.StackEnd <= Now)
	{
		State.Stacks = 0;
	}
	ProjectileTimers.RemoveAll([this](const FTimerHandle& Timer)
	{
		return !GetWorld()->GetTimerManager().IsTimerActive(Timer);
	});
	if (State.Build.SpecId == TEXT("Saturn") && State.BuffEnd > Now)
	{
		State.Stacks = 4;
		State.StackEnd = Now + 8.0;
	}
	if (State.ChilledEnd <= Now)
	{
		State.Chilled = 0;
	}
	if (IsAlive())
	{
		if (!State.Build.SpecId.IsNone())
		{
			ApplyInstant(ResolveASC(), UGolemAttributeSet::GetStaminaAttribute(), 2.5f);
		}
		if (!State.Build.SpecId.IsNone() && Now >= LastSpend + 1.0)
		{
			ApplyInstant(ResolveASC(), UGolemAttributeSet::GetRuneEnergyAttribute(), 1.0f);
		}
		float DeferredTick = 0.0f;
		for (FDebtTranche& Debt : Debts)
		{
			while (Debt.Ticks > 0 && Debt.NextTick <= Now + 0.001)
			{
				DeferredTick += Debt.PerTick;
				--Debt.Ticks;
				Debt.NextTick += 1.0;
			}
		}
		Debts.RemoveAll([](const FDebtTranche& Debt)
		{
			return Debt.Ticks <= 0;
		});
		if (DeferredTick > 0.0f)
		{
			const float Before = ResolveAttributes()->GetHealth();
			ApplyInstant(ResolveASC(), UGolemAttributeSet::GetHealthAttribute(), -DeferredTick);
			ResolveASC()->PublishDamageConfirmation(GetOwner(), Before - ResolveAttributes()->GetHealth());
		}
		// Copy due streams because damage/life events may clear the source collection synchronously.
		TArray<FPeriodicStream> Due;
		for (FPeriodicStream& Stream : Periodics)
		{
			while (Stream.NextTick <= Now + 0.001 && Stream.NextTick <= Stream.End + 0.001)
			{
				Due.Add(Stream);
				Stream.NextTick += 1.0;
			}
		}
		Periodics.RemoveAll([Now](const FPeriodicStream& Stream)
		{
			return Stream.End < Now || !Stream.Target.IsValid();
		});
		for (const FPeriodicStream& Stream : Due)
		{
			if (!IsAlive())
			{
				break;
			}
			AActor* Target = Stream.Target.Get();
			if (!IsValid(Target))
			{
				continue;
			}
			if (Stream.ItemSource.IsValid())
			{
				if (CanTriggerRelic(TEXT("Lifeseed"), Stream.ItemSource, Stream.ItemGeneration))
				{
					TargetRuntime(Target)->ApplyExternalHeal(Stream.Magnitude);
				}
				continue;
			}
			if (Stream.bDamage)
			{
				FGolemSkillDamagePacket P;
				P.RawDamage = Stream.Magnitude;
				P.School = EGolemSkillSchool::Fire;
				P.bDirect = false;
				P.bCanDefend = false;
				P.ActivationId = FGuid::NewGuid();
				ApplySkillDamage(GetOwner(), Target, P);
			}
			else if (Stream.bGround)
			{
				const FVector Center = Stream.bMobile ? Target->GetActorLocation() : Stream.Ground;
				TArray<AActor*> Allies = FindTargets(Center, Stream.Radius, true);
				for (int32 Index = 0; Index < FMath::Min(3, Allies.Num()); ++Index)
				{
					ApplyHeal(Allies[Index], Stream.Magnitude, false);
				}
			}
			else
			{
				ApplyHeal(Target, Stream.Magnitude, false);
				if (Stream.bMercury && State.BuffEnd > Now)
				{
					const float Radius = State.Build.SpecChoice == TEXT("B") ? 800.0f : 600.0f;
					AActor* Secondary = SelectLowestHealthAlly(Radius, Target, Target);
					if (Secondary != nullptr)
					{
						ApplyHeal(Secondary, Stream.Magnitude * 0.30f, false, false);
					}
				}
			}
		}
	}
	Shields.RemoveAll([Now](const FShieldPool& Pool)
	{
		return Pool.End <= Now || Pool.Amount <= 0.0f;
	});
	if (CrystalEffect.IsValid() && CrystalEnd <= Now)
	{
		ResolveASC()->RemoveActiveGameplayEffect(CrystalEffect);
		CrystalEffect.Invalidate();
	}
	for (TMap<FGuid, double>::TIterator It = SeenItemActivations.CreateIterator(); It; ++It)
	{
		if (It.Value() <= Now)
		{
			It.RemoveCurrent();
		}
	}
	if (State.StarfallCounterEnd <= Now)
	{
		State.StarfallCount = 0;
	}
	PublishState();
}

void UGolemSkillRuntimeComponent::PublishState(bool bNotify)
{
	State.CrystalEnd = CrystalEnd;
	State.Shield = 0.0f;
	const float Cap = ResolveAttributes() != nullptr ? ResolveAttributes()->GetMaxHealth() * 0.35f : 0.0f;
	for (FShieldPool& Pool : Shields)
	{
		const float SourceCap = ResolveAttributes() != nullptr ? ResolveAttributes()->GetMaxHealth() * Pool.CapFraction : 0.0f;
		Pool.Amount = FMath::Min3(Pool.Amount, FMath::Max(0.0f, Cap - State.Shield), SourceCap);
		State.Shield += Pool.Amount;
	}
	State.DeferredDamage = 0.0f;
	for (const FDebtTranche& Debt : Debts)
	{
		State.DeferredDamage += Debt.PerTick * Debt.Ticks;
	}
	++State.Revision;
	if (!bNotify)
	{
		return;
	}
	OnSkillStateChanged.Broadcast(State);
	if (GetOwner() != nullptr)
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UGolemSkillRuntimeComponent::OnRep_State()
{
	OnSkillStateChanged.Broadcast(State);
}

void UGolemSkillRuntimeComponent::HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	if (Current != EGolemLifeState::Alive)
	{
		++ItemGeneration;
		State.StarfallCount = 0;
		State.StarfallCounterEnd = 0.0;
		Debts.Reset();
		Shields.Reset();
		Periodics.Reset();
		State.Stacks = 0;
		State.StackEnd = 0.0;
		State.BuffEnd = 0.0;
		PublishState();
	}
}

void UGolemSkillRuntimeComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Change)
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		PublishState(ResolveASC() != nullptr && !ResolveASC()->IsMutationFenced());
	}
}

void UGolemSkillRuntimeComponent::ResetForCheckpoint()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	Debts.Reset();
	Shields.Reset();
	Periodics.Reset();
	++ItemGeneration;
	State.StarfallCount = 0;
	State.StarfallCounterEnd = 0.0;
	SeenItemActivations.Reset();
	State.Stacks = 0;
	State.StackEnd = State.BuffEnd = State.ChilledEnd = State.WaterReadyEnd = State.IceReadyEnd = 0.0;
	State.Chilled = 0;
	State.CooldownEnds.Init(0.0, 4);
	AftershockEnd = ShatterEnd = CrystalEnd = 0.0;
	if (ResolveASC() != nullptr)
	{
		ResolveASC()->RemoveActiveGameplayEffect(CrystalEffect);
	}
	CrystalEffect.Invalidate();
	for (FTimerHandle& Timer : ProjectileTimers)
	{
		GetWorld()->GetTimerManager().ClearTimer(Timer);
	}
	ProjectileTimers.Reset();
	PublishState();
}

FGolemResult UGolemSkillRuntimeComponent::AddPowerCrystalBuff()
{
	UGolemAbilitySystemComponent* ASC = ResolveASC();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ASC == nullptr || !IsAlive())
	{
		return SkillFailure(EGolemError::InvalidState, TEXT("CrystalBuff"));
	}
	EnsureInitialized();
	if (ASC->GetActiveGameplayEffect(CrystalEffect) == nullptr)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			UGolemPowerCrystalEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return SkillFailure(EGolemError::InvalidState, TEXT("CrystalGE"));
		}
		Spec.Data->SetSetByCallerMagnitude(UGolemPowerCrystalEffect::PowerMultiplierMagnitude, 1.15f);
		CrystalEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!CrystalEffect.IsValid())
		{
			return SkillFailure(EGolemError::InvalidState, TEXT("CrystalGE"));
		}
	}
	CrystalEnd = ServerTime() + 10.0;
	PublishState();
	return FGolemResult::Success();
}

FGolemResult UGolemSkillRuntimeComponent::ApplySkillDamage(AActor* Source, AActor* Target,
	const FGolemSkillDamagePacket& Packet, const FHitResult* Hit)
{
	UGolemAbilitySystemComponent* SourceASC = ActorASC(Source);
	UGolemAbilitySystemComponent* TargetASC = ActorASC(Target);
	if (SourceASC == nullptr || TargetASC == nullptr || !Source->HasAuthority() || !Target->HasAuthority()
		|| Source->GetWorld() != Target->GetWorld()
		|| SourceASC->IsMutationFenced() || TargetASC->IsMutationFenced() || !FMath::IsFinite(Packet.RawDamage)
		|| Packet.RawDamage <= 0.0f || Packet.RawDamage > 1000000.0f)
	{
		return SkillFailure(EGolemError::InvalidData, TEXT("SkillDamage"));
	}
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	if (Packet.PrimaryTarget.IsValid())
	{
		Context.AddActors({ Packet.PrimaryTarget.Get() });
	}
	FHitResult Trace;
	if (Hit == nullptr)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GolemSkillHit), false, Source);
		Source->GetWorld()->LineTraceSingleByChannel(Trace, Source->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
		if (Trace.GetActor() == Target)
		{
			Context.AddHitResult(Trace, true);
		}
	}
	else
	{
		Context.AddHitResult(*Hit, true);
	}
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGolemDamageEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return SkillFailure(EGolemError::InvalidState, TEXT("SkillDamageSpec"));
	}
	Spec.Data->SetSetByCallerMagnitude(GolemGameplayTags::Data_Damage, Packet.RawDamage);
	Spec.Data->SetSetByCallerMagnitude(GolemGameplayTags::Data_DamagePoise, Packet.PoiseDamage);
	Spec.Data->SetSetByCallerMagnitude(GolemGameplayTags::Data_DamageCanDefend, Packet.bCanDefend ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillSeed")), 1.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillSchool")), static_cast<float>(Packet.School));
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillFrostPenetration")), Packet.FrostPenetration);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillDeferred")), Packet.bEligibleDeferred ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillHeavy")), Packet.bHeavy ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillDirect")), Packet.bDirect ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("ItemProc")), Packet.bItemProc ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillPrimary")), Packet.bPrimaryTarget ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("ItemEligibleActivation")), Packet.bEligibleItemActivation ? 1.0f : 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("SkillHitIndex")), Packet.HitIndex);
	// A float represents each 16-bit half exactly, unlike serializing a uint32 into one float.
	const uint32 Parts[] { Packet.ActivationId.A, Packet.ActivationId.B, Packet.ActivationId.C, Packet.ActivationId.D };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Spec.Data->SetSetByCallerMagnitude(FName(*FString::Printf(TEXT("SkillActivation%dLow"), Index)), Parts[Index] & 0xffff);
		Spec.Data->SetSetByCallerMagnitude(FName(*FString::Printf(TEXT("SkillActivation%dHigh"), Index)), Parts[Index] >> 16);
	}
	return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied()
		? FGolemResult::Success() : SkillFailure(EGolemError::InvalidState, TEXT("SkillDamageRejected"));
}

void UGolemSkillRuntimeComponent::CommitSkill(int32 Slot, AActor* Target, FGuid ActivationId)
{
	const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(State.Build, Slot);
	UGolemAttributeSet* A = ResolveAttributes();
	const FName S = State.Build.SpecId;
	const double Now = ServerTime();
	AActor* ResolvedTarget = D.bFriendly && Target == nullptr ? GetOwner() : Target;
	if (State.StackEnd <= Now)
	{
		State.Stacks = 0;
	}
	float Cost = D.Cost;
	const bool bBuffAtCommit = State.BuffEnd > Now;
	const bool bEmpoweredHeal = (S == TEXT("Venus") || S == TEXT("Terra")) && State.Stacks == 3
		&& (S == TEXT("Venus") ? Slot == 2 || Slot == 3 : Slot == 3);
	const bool bEmpoweredIce = S == TEXT("Pluto") && Slot != 1 && Slot != 4 && State.IceReadyEnd > Now;
	int32 ConsumedStacks = 0;
	if (bEmpoweredHeal)
	{
		State.Stacks = 0;
		if (S == TEXT("Venus"))
		{
			Cost *= 0.5f;
		}
	}
	if ((S == TEXT("Uranus") && Slot == 3) || (S == TEXT("Jupiter") && Slot == 2)
		|| (S == TEXT("Mars") && Slot == 2))
	{
		ConsumedStacks = State.Stacks;
		State.Stacks = 0;
	}
	if (S == TEXT("Pluto") && Slot == 1 && State.WaterReadyEnd > Now)
	{
		Cost *= 0.75f;
		State.WaterReadyEnd = 0.0;
	}
	if (bEmpoweredIce)
	{
		State.IceReadyEnd = 0.0;
	}
	ApplyInstant(ResolveASC(), UGolemAttributeSet::GetRuneEnergyAttribute(), -Cost);
	if (Cost > 0.0f)
	{
		LastSpend = Now;
	}
	State.CooldownEnds[Slot - 1] = Now + D.Cooldown / (1.0f + A->GetCooldownRecovery());
	if (Slot == 4)
	{
		State.BuffEnd = Now + D.Duration;
		if (S == TEXT("Neptune"))
		{
			const float Outstanding = GetDeferredDamage();
			const float Cleared = FMath::Min(Outstanding * 0.5f, A->GetMaxHealth() * 0.20f);
			const float Scale = Outstanding > 0.0f ? (Outstanding - Cleared) / Outstanding : 1.0f;
			for (FDebtTranche& Debt : Debts)
			{
				Debt.PerTick *= Scale;
			}
		}
		if (S == TEXT("Saturn"))
		{
			State.Stacks = 4;
			State.StackEnd = Now + 8.0;
		}
	}
	else if (D.bProjectile && IsValid(ResolvedTarget))
	{
		const float Flight = FVector::Dist(GetOwner()->GetActorLocation(), ResolvedTarget->GetActorLocation()) / 2000.0f;
		const TWeakObjectPtr<AActor> WeakTarget(ResolvedTarget);
		FTimerHandle FlightTimer;
		GetWorld()->GetTimerManager().SetTimer(FlightTimer, FTimerDelegate::CreateWeakLambda(this,
			[this, Slot, WeakTarget, ActivationId, Power = A->GetAttackPower(), Versatility = A->GetVersatility(),
				CritChance = A->GetCritChance(), CritMultiplier = A->GetCritMultiplier(), Mastery = A->GetMasteryBonus(),
				ConsumedStacks, bEmpoweredHeal, bEmpoweredIce, bBuffAtCommit]()
		{
			if (IsAlive() && WeakTarget.IsValid() && HasLineOfSight(WeakTarget.Get()))
			{
				ExecuteSkill(Slot, WeakTarget.Get(), ActivationId, Power, Versatility, CritChance,
					CritMultiplier, Mastery, ConsumedStacks, bEmpoweredHeal, bEmpoweredIce, bBuffAtCommit);
			}
		}), FMath::Max(0.01f, Flight), false);
		ProjectileTimers.Add(FlightTimer);
	}
	else
	{
		ExecuteSkill(Slot, ResolvedTarget, ActivationId, A->GetAttackPower(), A->GetVersatility(),
			A->GetCritChance(), A->GetCritMultiplier(), A->GetMasteryBonus(), ConsumedStacks,
			bEmpoweredHeal, bEmpoweredIce, bBuffAtCommit);
	}
	if (S == TEXT("Mars") && Slot == 2 && ConsumedStacks > 0 && State.BuffEnd > Now)
	{
		AddStack(2, 5, 10.0f);
	}
	PublishState();
	if (UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		FGolemSkillLabEvent Event;
		Event.ConfirmationId = ActivationId;
		Event.RequestId = ActivationId;
		Event.Revision = State.Revision;
		Event.SourceId = ResolveASC()->GetEntityId();
		Event.TargetId = ActorASC(ResolvedTarget) != nullptr ? ActorASC(ResolvedTarget)->GetEntityId() : Event.SourceId;
		Event.EncounterId = EncounterId;
		Event.EventName = TEXT("AbilityCommitted");
		Event.SubjectId = D.AbilityId;
		Bus->PublishConfirmed(Event);
	}
}

void UGolemSkillRuntimeComponent::ExecuteSkill(int32 Slot, AActor* Target, FGuid ActivationId,
	float PowerSnapshot, float VersatilitySnapshot, float CritChanceSnapshot, float CritMultiplierSnapshot,
	float MasterySnapshot, int32 ConsumedStacks, bool bEmpoweredHeal, bool bEmpoweredIce, bool bBuffSnapshot)
{
	const FName S = State.Build.SpecId;
	const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(State.Build, Slot);
	const bool bBuff = bBuffSnapshot;
	const float Mastery = MasterySnapshot;
	if (Slot == 4)
	{
		return;
	}
	if (S == TEXT("Jupiter") && Slot == 3)
	{
		const FVector Start = GetOwner()->GetActorLocation();
		FHitResult Hit;
		GetOwner()->SetActorLocation(Start + GetOwner()->GetActorForwardVector() * 400.0f, true, &Hit);
		return;
	}
	float Magnitude = D.Coefficient * PowerSnapshot;
	if (D.Role == EGolemSkillRole::Tank && Slot == 3)
	{
		Magnitude *= (1.0f + Mastery);
		if (S == TEXT("Uranus"))
		{
			Magnitude *= 1.0f + ConsumedStacks * 0.10f;
		}
		AActor* ShieldTarget = S == TEXT("Uranus") && IsValid(Target) ? Target : GetOwner();
		TargetRuntime(ShieldTarget)->AddShield(GetOwner(), D.AbilityId, Magnitude, D.Duration);
		if (S == TEXT("Saturn"))
		{
			AddStack(1, 4, 8.0f, 2.0f);
		}
		return;
	}
	Magnitude *= 1.0f + VersatilitySnapshot;
	if (D.bFriendly)
	{
		if (Target == nullptr)
		{
			Target = GetOwner();
		}
		if (S == TEXT("Mercury"))
		{
			TSet<AActor*> HotTargets;
			for (const FPeriodicStream& Stream : Periodics)
			{
				if (Stream.bMercury && Stream.End > ServerTime() && Stream.Target.IsValid())
				{
					HotTargets.Add(Stream.Target.Get());
				}
			}
			if (Slot == 2)
			{
				HotTargets.Add(Target);
			}
			Magnitude *= 1.0f + FMath::Min(3, HotTargets.Num()) * 0.04f;
			if (Slot == 2)
			{
				AddPeriodic(D.AbilityId, Target, Magnitude * (1.0f + Mastery), D.Duration, false, true);
				return;
			}
		}
		if (S == TEXT("Terra") && Slot == 2)
		{
			AddGroundPeriodic(Target, D, Magnitude * (1.0f + Mastery));
			return;
		}
		if (S == TEXT("Venus") && Slot == 2)
		{
			Magnitude *= 1.0f + Mastery;
		}
		const float BaseMagnitude = Magnitude;
		if (bEmpoweredHeal && S == TEXT("Venus"))
		{
			Magnitude *= 1.30f;
		}
		TArray<AActor*> Allies = D.Radius > 0.0f ? FindTargets(Target->GetActorLocation(), D.Radius, true) : TArray<AActor*> { Target };
		float Actual = 0.0f;
		const FGuid RelicSource = State.EquipmentCombatBuild.RelicInstanceId;
		const uint64 RelicGeneration = ItemGeneration;
		for (int32 Index = 0; Index < FMath::Min(3, Allies.Num()); ++Index)
		{
			const float PrimaryHealing = ApplyHeal(Allies[Index], Magnitude, true);
			Actual += PrimaryHealing;
			if (Allies[Index] == Target)
			{
				NotifyPrimaryDirectHeal(Target, PrimaryHealing, RelicSource, RelicGeneration);
			}
		}
		if (Actual > 0.0f && !bEmpoweredHeal && (S == TEXT("Venus") || (S == TEXT("Terra") && Slot == 3)))
		{
			AddStack(1, 3, 10.0f);
		}
		if (S == TEXT("Terra") && Slot == 3 && bEmpoweredHeal)
		{
			AddPeriodic(TEXT("Terra.Passive"), Target, BaseMagnitude * 0.30f / 6.0f, 6.0f);
		}
		if (State.Build.SkillRune == TEXT("Skill.Propagation") && ((S == TEXT("Venus") && Slot == 2)
			|| ((S == TEXT("Mercury") || S == TEXT("Terra")) && Slot == 3)))
		{
			AActor* Secondary = SelectLowestHealthAlly(600.0f, Target, Target);
			if (Secondary != nullptr)
			{
				ApplyHeal(Secondary, Magnitude * 0.30f, true, false);
			}
		}
		return;
	}
	TArray<AActor*> Enemies;
	if (S == TEXT("Pluto") && Slot == 3)
	{
		Enemies = FindTargets(GetOwner()->GetActorLocation(), D.Radius, false);
	}
	else if (S == TEXT("Mars") && Slot == 3)
	{
		for (AActor* Enemy : FindTargets(GetOwner()->GetActorLocation(), 800.0f, false))
		{
			const FVector Relative = GetOwner()->GetActorTransform().InverseTransformPosition(Enemy->GetActorLocation());
			if (Relative.X >= 0.0f && Relative.X <= 800.0f && FMath::Abs(Relative.Y) <= D.Radius)
			{
				Enemies.Add(Enemy);
			}
		}
	}
	else if (Slot == 2 && (D.Role == EGolemSkillRole::Tank || S == TEXT("Mars"))
		&& State.Build.SkillRune != TEXT("Skill.Convergence"))
	{
		const float HalfAngle = S == TEXT("Uranus") || S == TEXT("Mars") ? 35.0f : 45.0f;
		const FVector Forward = IsValid(Target) ? (Target->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D()
			: GetOwner()->GetActorForwardVector();
		for (AActor* Enemy : FindTargets(GetOwner()->GetActorLocation(), D.Range, false))
		{
			if (FVector::DotProduct(Forward, (Enemy->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D())
				>= FMath::Cos(FMath::DegreesToRadians(HalfAngle)))
			{
				Enemies.Add(Enemy);
			}
		}
	}
	else if (IsValid(Target))
	{
		Enemies.Add(Target);
	}
	float OffensiveBonus = 0.0f;
	if (S == TEXT("Jupiter") && Slot == 2)
	{
		Magnitude *= 1.0f + Mastery;
		OffensiveBonus += ConsumedStacks * 0.06f;
	}
	if (bEmpoweredIce)
	{
		OffensiveBonus += 0.20f;
	}
	Magnitude *= 1.0f + FMath::Min(0.50f, OffensiveBonus);
	FRandomStream Critical(static_cast<int32>(ActivationId.A ^ ActivationId.D));
	const float RawBaseDamage = D.Coefficient * PowerSnapshot;
	const bool bCrit = Critical.FRand() < CritChanceSnapshot;
	const float DirectMagnitude = Magnitude * (bCrit ? CritMultiplierSnapshot : 1.0f);
	bool bAnyHit = false;
	for (int32 Index = 0; Index < FMath::Min(3, Enemies.Num()); ++Index)
	{
		AActor* Enemy = Enemies[Index];
		if (!HasLineOfSight(Enemy))
		{
			continue;
		}
		FGolemSkillDamagePacket P;
		P.RawDamage = DirectMagnitude;
		P.PoiseDamage = D.PoiseDamage;
		P.School = D.School;
		P.FrostPenetration = S == TEXT("Pluto") && Slot != 1 && bBuff ? 0.20f : 0.0f;
		P.ActivationId = ActivationId;
		P.HitIndex = Index;
		P.bPrimaryTarget = Enemy == (IsValid(Target) ? Target : Enemies[0]);
		P.bEligibleItemActivation = Slot >= 1 && Slot <= 3;
		P.PrimaryTarget = IsValid(Target) ? Target : Enemies[0];
		if (!ApplySkillDamage(GetOwner(), Enemy, P).IsSuccess())
		{
			continue;
		}
		bAnyHit = true;
		if (Slot == 2 && D.Role == EGolemSkillRole::Tank)
		{
			TInlineComponentArray<UActorComponent*> Components(Enemy);
			for (UActorComponent* Component : Components)
			{
				if (IGolemTauntResponse* Response = Cast<IGolemTauntResponse>(Component))
				{
					Response->ApplySkillTaunt(GetOwner(), 3.0f);
					break;
				}
			}
		}
		UGolemSkillRuntimeComponent* Status = TargetRuntime(Enemy);
		if (S == TEXT("Pluto") && Status != nullptr)
		{
			if (Slot == 2 && Status->State.Chilled >= 3 && Status->State.ChilledEnd > ServerTime()
				&& Status->ShatterEnd <= ServerTime())
			{
				Status->State.Chilled = 0;
				Status->ShatterEnd = ServerTime() + 4.0;
				P.RawDamage = 0.60f * PowerSnapshot * (1.0f + Mastery) * (bBuff ? 1.25f : 1.0f)
					* (1.0f + VersatilitySnapshot);
				P.PoiseDamage = 20.0f;
				P.bDirect = false;
				P.bCanDefend = false;
				P.FrostPenetration = 0.0f;
				P.HitIndex += 100;
				ApplySkillDamage(GetOwner(), Enemy, P);
			}
			else if (Slot == 1 || Slot == 3)
			{
				Status->State.Chilled = FMath::Min(3, Status->State.Chilled + (Slot == 3 ? 2 : 1));
				Status->State.ChilledEnd = ServerTime() + 6.0;
			}
			Status->PublishState();
		}
		if (S == TEXT("Mars") && Slot == 2 && ConsumedStacks > 0)
		{
			const float Burn = RawBaseDamage * 0.08f * ConsumedStacks * (1.0f + Mastery)
				* (1.0f + VersatilitySnapshot) / 6.0f;
			AddPeriodic(TEXT("Mars.Burn"), Enemy, Burn, 6.0f, true);
		}
	}
	if (bAnyHit && S == TEXT("Jupiter"))
	{
		if (Slot == 1)
		{
			AddStack(bBuff ? 2 : 1, 5, 10.0f);
		}
		if (Slot == 2 && bBuff && IsValid(Target))
		{
			TArray<AActor*> Secondary = FindTargets(Target->GetActorLocation(), 600.0f, false, Target);
			const int32 Cap = State.Build.SpecChoice == TEXT("B") ? 3 : 2;
			for (int32 Index = 0; Index < FMath::Min(Cap, Secondary.Num()); ++Index)
			{
				FGolemSkillDamagePacket P;
				P.RawDamage = DirectMagnitude * 0.30f;
				P.School = EGolemSkillSchool::Lightning;
				P.bDirect = false;
				P.ActivationId = ActivationId;
				P.HitIndex = 200 + Index;
				ApplySkillDamage(GetOwner(), Secondary[Index], P);
			}
		}
	}
	if (bAnyHit && S == TEXT("Pluto"))
	{
		if (Slot == 1)
		{
			State.IceReadyEnd = ServerTime() + 6.0;
		}
		else
		{
			State.WaterReadyEnd = ServerTime() + 6.0;
		}
	}
	if (bAnyHit && S == TEXT("Mars") && (Slot == 1 || Slot == 3))
	{
		AddStack(bBuff ? 2 : 1, 5, 10.0f);
	}
	PublishState();
}
