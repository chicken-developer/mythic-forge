#include "Voids/GolemVoidHostComponent.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "Voids/GolemColorVoidComponent.h"

FGolemResult FGolemVoidHostTuning::Validate() const
{
	if (VoidsToFill < 1 || VoidsToFill > 32 || MaxOpenVoids < 1 || MaxOpenVoids > VoidsToFill ||
		!FMath::IsFinite(StaggerSeconds) || StaggerSeconds < 0.0f || StaggerSeconds > 600.0f ||
		!FMath::IsFinite(GrayBodyDamageFraction) || GrayBodyDamageFraction < 0.0f || GrayBodyDamageFraction > 1.0f)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("VoidHostTuning"), FText::GetEmpty());
	}
	return Void.Validate();
}

UGolemVoidHostComponent::UGolemVoidHostComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UGolemVoidHostComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemVoidHostComponent, FilledCount);
	DOREPLIFETIME(UGolemVoidHostComponent, bRestored);
	DOREPLIFETIME(UGolemVoidHostComponent, StaggerEndServerTime);
}

void UGolemVoidHostComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bInitialized && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		AdvanceTo(HostNow());
	}
}

void UGolemVoidHostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem.IsValid() && AbilitySystem->GetLifeComponent() != nullptr)
	{
		AbilitySystem->GetLifeComponent()->OnLifeStateChanged.Remove(LifeHandle);
	}
	LifeHandle.Reset();
	for (UGolemColorVoidComponent* Slot : Slots)
	{
		if (IsValid(Slot))
		{
			Slot->OnVoidFilled.RemoveAll(this);
		}
	}
	if (IsValid(CoreSlot))
	{
		CoreSlot->OnVoidFilled.RemoveAll(this);
	}
	OnHostChanged.Clear();
	OnRestored.Clear();
	Super::EndPlay(EndPlayReason);
}

double UGolemVoidHostComponent::HostNow() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0;
}

FGolemResult UGolemVoidHostComponent::InitializeHost(UGolemAbilitySystemComponent* InAbilitySystem,
	const FGolemVoidHostTuning& InTuning, const TArray<UGolemColorVoidComponent*>& InSlots,
	const TArray<uint8>& InColorPlan, UGolemColorVoidComponent* InCoreSlot)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("InitializeVoidHost"), FText::GetEmpty());
	}
	if (bInitialized)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("VoidHostAlreadyInitialized"), FText::GetEmpty());
	}
	const FGolemResult TuningResult = InTuning.Validate();
	if (!TuningResult.IsSuccess())
	{
		return TuningResult;
	}
	if (InAbilitySystem == nullptr || InAbilitySystem->GetOwner() != GetOwner() || InAbilitySystem->GetLifeComponent() == nullptr ||
		InAbilitySystem->GetGolemAttributes() == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("VoidHostAbilitySystem"), FText::GetEmpty());
	}
	// A slot is reused only after its void is filled, so every simultaneously open void needs its own slot.
	if (InSlots.Num() < InTuning.MaxOpenVoids || InColorPlan.Num() != InTuning.VoidsToFill ||
		(InTuning.bHasWhiteCore && InCoreSlot == nullptr))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("VoidHostSlots"), FText::GetEmpty());
	}
	for (const UGolemColorVoidComponent* Slot : InSlots)
	{
		if (!IsValid(Slot) || Slot->GetOwner() != GetOwner() || Slot == InCoreSlot)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("VoidHostSlotOwner"), FText::GetEmpty());
		}
	}
	for (const uint8 PlanMask : InColorPlan)
	{
		if (!GolemElements::IsValidMask(PlanMask))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("VoidHostColorPlan"), FText::GetEmpty());
		}
	}
	Tuning = InTuning;
	AbilitySystem = InAbilitySystem;
	ColorPlan = InColorPlan;
	Slots.Reset();
	for (UGolemColorVoidComponent* Slot : InSlots)
	{
		Slots.Add(Slot);
		Slot->OnVoidFilled.AddUObject(this, &UGolemVoidHostComponent::HandleVoidFilled);
	}
	CoreSlot = InTuning.bHasWhiteCore ? InCoreSlot : nullptr;
	if (CoreSlot != nullptr)
	{
		const FGolemResult CoreResult = CoreSlot->ConfigureVoid(GolemElements::AllMask, Tuning.Void, true);
		if (!CoreResult.IsSuccess())
		{
			return CoreResult;
		}
		CoreSlot->OnVoidFilled.AddUObject(this, &UGolemVoidHostComponent::HandleVoidFilled);
	}
	LifeHandle = InAbilitySystem->GetLifeComponent()->OnLifeStateChanged.AddUObject(this,
		&UGolemVoidHostComponent::HandleLifeChanged);
	NextPlanIndex = 0;
	LastFilledSlot = INDEX_NONE;
	FilledCount = 0;
	bRestored = false;
	StaggerEndServerTime = 0.0;
	LastTime = HostNow();
	bInitialized = true;
	RevealUpToCapacity();
	OnHostChanged.Broadcast();
	return FGolemResult::Success();
}

bool UGolemVoidHostComponent::IsWhiteCoreRevealed() const
{
	return CoreSlot != nullptr && CoreSlot->GetVoidState() != EGolemVoidState::Hidden;
}

TArray<UGolemColorVoidComponent*> UGolemVoidHostComponent::GetOpenVoids() const
{
	TArray<UGolemColorVoidComponent*> Result;
	for (UGolemColorVoidComponent* Slot : Slots)
	{
		if (IsValid(Slot) && Slot->AcceptsHits())
		{
			Result.Add(Slot);
		}
	}
	if (IsValid(CoreSlot) && CoreSlot->AcceptsHits())
	{
		Result.Add(CoreSlot);
	}
	return Result;
}

int32 UGolemVoidHostComponent::CountActiveSlots() const
{
	int32 Active = 0;
	for (const UGolemColorVoidComponent* Slot : Slots)
	{
		const EGolemVoidState State = Slot->GetVoidState();
		Active += State == EGolemVoidState::Open || State == EGolemVoidState::Primed || State == EGolemVoidState::Dulled ? 1 : 0;
	}
	return Active;
}

void UGolemVoidHostComponent::RevealUpToCapacity()
{
	if (bRestored || IsStaggered())
	{
		return;
	}
	while (NextPlanIndex < ColorPlan.Num() && CountActiveSlots() < Tuning.MaxOpenVoids)
	{
		// Start after the slot that was just filled so the next void appears somewhere else when it can.
		int32 Chosen = INDEX_NONE;
		const int32 Start = LastFilledSlot == INDEX_NONE ? 0 : LastFilledSlot + 1;
		for (int32 Offset = 0; Offset < Slots.Num(); ++Offset)
		{
			const int32 Index = (Start + Offset) % Slots.Num();
			const EGolemVoidState State = Slots[Index]->GetVoidState();
			const bool bFree = State == EGolemVoidState::Hidden || State == EGolemVoidState::Filled;
			if (bFree && (Index != LastFilledSlot || Slots.Num() == 1))
			{
				Chosen = Index;
				break;
			}
		}
		if (Chosen == INDEX_NONE && LastFilledSlot != INDEX_NONE)
		{
			const EGolemVoidState State = Slots[LastFilledSlot]->GetVoidState();
			Chosen = State == EGolemVoidState::Hidden || State == EGolemVoidState::Filled ? LastFilledSlot : INDEX_NONE;
		}
		if (Chosen == INDEX_NONE || !Slots[Chosen]->ConfigureVoid(ColorPlan[NextPlanIndex], Tuning.Void, false).IsSuccess())
		{
			break;
		}
		++NextPlanIndex;
	}
	if (CoreSlot != nullptr && FilledCount >= Tuning.VoidsToFill && CoreSlot->GetVoidState() == EGolemVoidState::Hidden)
	{
		CoreSlot->SetRevealed(true);
	}
}

void UGolemVoidHostComponent::AdvanceTo(const double Now)
{
	if (!bInitialized || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	LastTime = FMath::Max(LastTime, Now);
	for (UGolemColorVoidComponent* Slot : Slots)
	{
		Slot->AdvanceTo(Now);
	}
	if (CoreSlot != nullptr)
	{
		CoreSlot->AdvanceTo(Now);
	}
	if (IsStaggered() && Now >= StaggerEndServerTime)
	{
		EndStagger();
	}
}

void UGolemVoidHostComponent::EndStagger()
{
	StaggerEndServerTime = 0.0;
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 0,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	RevealUpToCapacity();
	OnHostChanged.Broadcast();
}

void UGolemVoidHostComponent::HandleVoidFilled(UGolemColorVoidComponent* Void, AActor* InstigatorActor)
{
	if (!bInitialized || bRestored || !AbilitySystem.IsValid() || AbilitySystem->GetGolemAttributes() == nullptr)
	{
		return;
	}
	const int32 SlotIndex = Slots.IndexOfByKey(Void);
	if (SlotIndex == INDEX_NONE && Void != CoreSlot)
	{
		return;
	}
	++FilledCount;
	if (SlotIndex != INDEX_NONE)
	{
		LastFilledSlot = SlotIndex;
	}
	const UGolemAttributeSet* Attributes = AbilitySystem->GetGolemAttributes();
	const bool bLastFill = FilledCount >= Tuning.TotalFills();
	// Each fill is worth an equal slice of maximum health; the last one always empties what is left.
	const float Slice = Attributes->GetMaxHealth() / static_cast<float>(Tuning.TotalFills());
	const float Damage = bLastFill ? FMath::Max(Slice, Attributes->GetHealth()) : FMath::Min(Slice, Attributes->GetHealth());
	AActor* Source = IsValid(InstigatorActor) ? InstigatorActor : GetOwner();
	if (Damage > 0.0f)
	{
		bRestoring = bLastFill;
		UGolemAbilitySystemComponent::ApplyDamage(Source, GetOwner(), Damage);
		bRestoring = false;
	}
	if (bRestored)
	{
		return;
	}
	if (bLastFill || Attributes->GetHealth() <= 0.0f)
	{
		Restore(InstigatorActor);
		return;
	}
	StaggerEndServerTime = FMath::Max(LastTime, HostNow()) + Tuning.StaggerSeconds;
	AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	if (Tuning.StaggerSeconds <= 0.0f)
	{
		EndStagger();
		return;
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnHostChanged.Broadcast();
}

void UGolemVoidHostComponent::HandleLifeChanged(const EGolemLifeState Previous, const EGolemLifeState Current)
{
	// Grey-body damage can also empty the health bar; the Xenon is restored either way.
	if (Current != EGolemLifeState::Alive && !bRestored && !bRestoring)
	{
		Restore(nullptr);
	}
}

void UGolemVoidHostComponent::Restore(AActor* InstigatorActor)
{
	if (bRestored)
	{
		return;
	}
	bRestored = true;
	StaggerEndServerTime = 0.0;
	for (UGolemColorVoidComponent* Slot : Slots)
	{
		Slot->SetRevealed(false);
	}
	if (CoreSlot != nullptr)
	{
		CoreSlot->SetRevealed(false);
	}
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 0,
			EGameplayTagReplicationState::TagAndCountToAll);
		AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Xenon_Restored, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnHostChanged.Broadcast();
	OnRestored.Broadcast(InstigatorActor);
}

void UGolemVoidHostComponent::ResolveIncomingDamage(AActor* Source, const float InDamage, const float PoiseDamage,
	const bool bCanDefend, float& OutHealthDamage)
{
	OutHealthDamage = !bInitialized ? InDamage : (bRestored ? 0.0f : InDamage * Tuning.GrayBodyDamageFraction);
}

void UGolemVoidHostComponent::OnRep_HostState()
{
	OnHostChanged.Broadcast();
}
