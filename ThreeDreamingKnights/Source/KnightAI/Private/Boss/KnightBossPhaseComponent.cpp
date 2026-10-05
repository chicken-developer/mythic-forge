#include "Boss/KnightBossPhaseComponent.h"

#include "AbilitySystemGlobals.h"
#include "Assets/KnightAssetManager.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Context/KnightAIResult.h"
#include "Definitions/KnightAIActionSetDefinition.h"
#include "Definitions/KnightBossPhaseDefinition.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightBossPhase, Log, All);

UKnightBossPhaseComponent::UKnightBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKnightBossPhaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightBossPhaseComponent, DefinitionId);
	DOREPLIFETIME(UKnightBossPhaseComponent, PhaseIndex);
	DOREPLIFETIME(UKnightBossPhaseComponent, PhaseKey);
	DOREPLIFETIME(UKnightBossPhaseComponent, PhaseActionSetId);
}

FKnightResult UKnightBossPhaseComponent::Configure(const FPrimaryAssetId& InDefinitionId)
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return KnightAIFailure(EKnightError::ShuttingDown, TEXT("ConfigureBossPhases"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightAIFailure(EKnightError::NotAuthority, TEXT("ConfigureBossPhases"));
	}
	if (DefinitionId == InDefinitionId && IsValid(Definition))
	{
		return FKnightResult::Success();
	}
	if (PhaseIndex != INDEX_NONE)
	{
		return KnightAIFailure(EKnightError::Conflict, TEXT("BossDefinitionAlreadyActive"));
	}
	UKnightAbilitySystemComponent* ASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), true));
	if (ASC == nullptr || ASC->GetOwner() != GetOwner() || ASC->GetAvatarActor() != GetOwner() ||
		!ASC->GetEntityId().IsValid() || ASC->GetKnightAttributes() == nullptr || ASC->GetLifeComponent() == nullptr)
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("BossPhaseGAS"));
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Loaded = nullptr;
	const FKnightResult Resolve = Manager != nullptr
		? Manager->ResolveDefinition(InDefinitionId, UKnightBossPhaseDefinition::StaticClass(), Loaded)
		: KnightAIFailure(EKnightError::InvalidState, TEXT("BossPhaseAssetManager"));
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	UKnightBossPhaseDefinition* Candidate = Cast<UKnightBossPhaseDefinition>(Loaded);
	const FKnightResult Validation = Candidate->ValidateBossPhases();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	for (const FKnightBossPhaseEntry& Phase : Candidate->GetPhases())
	{
		UKnightDefinition* ActionData = nullptr;
		const FKnightResult ActionResolve = Manager->ResolveDefinition(Phase.ActionSetId,
			UKnightAIActionSetDefinition::StaticClass(), ActionData);
		if (!ActionResolve.IsSuccess())
		{
			return ActionResolve;
		}
		const FKnightResult ActionValidation = CastChecked<UKnightAIActionSetDefinition>(ActionData)->ValidateActionSet();
		if (!ActionValidation.IsSuccess())
		{
			return ActionValidation;
		}
	}
	Definition = Candidate;
	DefinitionId = InDefinitionId;
	AbilitySystem = ASC;
	if (!HealthHandle.IsValid())
	{
		HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetHealthAttribute()).
			AddUObject(this, &UKnightBossPhaseComponent::HandleHealthChanged);
		MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetMaxHealthAttribute()).
			AddUObject(this, &UKnightBossPhaseComponent::HandleHealthChanged);
	}
	GetOwner()->ForceNetUpdate();
	return FKnightResult::Success();
}

FKnightResult UKnightBossPhaseComponent::BindContext(UKnightAIContextComponent* InContext)
{
	if (!IsValid(InContext) || InContext->GetControlledPawn() != GetOwner() ||
		InContext->GetAbilitySystem() != AbilitySystem.Get() || !IsValid(Definition))
	{
		return KnightAIFailure(EKnightError::NotOwned, TEXT("BindBossPhaseContext"));
	}
	for (const FKnightBossPhaseEntry& Phase : Definition->GetPhases())
	{
		const FKnightResult Validation = InContext->ValidateActionSet(Phase.ActionSetId);
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
	}
	Context = InContext;
	if (PhaseIndex == INDEX_NONE)
	{
		LastPhaseResult = TryApplyPhase(0);
		if (!LastPhaseResult.IsSuccess())
		{
			Context.Reset();
			return LastPhaseResult;
		}
	}
	else
	{
		LastPhaseResult = TryApplyPhase(PhaseIndex);
		if (!LastPhaseResult.IsSuccess())
		{
			return LastPhaseResult;
		}
	}
	return RefreshPhase();
}

FKnightResult UKnightBossPhaseComponent::InitializePhases(const FPrimaryAssetId& InDefinitionId,
	UKnightAIContextComponent* InContext)
{
	const FKnightResult Result = Configure(InDefinitionId);
	return Result.IsSuccess() ? BindContext(InContext) : Result;
}

FKnightResult UKnightBossPhaseComponent::ResetForCheckpoint()
{
	LastPhaseResult = TryApplyPhase(0);
	return LastPhaseResult;
}

FKnightResult UKnightBossPhaseComponent::RefreshPhase()
{
	if (bApplyingPhase || bEndingPlay || !Context.IsValid() || !AbilitySystem.IsValid() ||
		!IsValid(Definition) || PhaseIndex == INDEX_NONE || !GetOwner()->HasAuthority())
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("RefreshBossPhase"));
	}
	if (!AbilitySystem->GetLifeComponent()->IsAlive())
	{
		Context->CancelOwnedAction();
		return FKnightResult::Success();
	}
	const UKnightAttributeSet* Attributes = AbilitySystem->GetKnightAttributes();
	const float Fraction = Attributes->GetHealth() / Attributes->GetMaxHealth();
	if (!FMath::IsFinite(Fraction))
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("BossHealthFraction"));
	}
	int32 DesiredPhase = PhaseIndex;
	for (int32 Index = PhaseIndex + 1; Index < Definition->GetPhases().Num(); ++Index)
	{
		if (Fraction <= Definition->GetPhases()[Index].HealthFraction)
		{
			DesiredPhase = Index;
		}
	}
	LastPhaseResult = DesiredPhase != PhaseIndex ? TryApplyPhase(DesiredPhase) : FKnightResult::Success();
	return LastPhaseResult;
}

FKnightResult UKnightBossPhaseComponent::TryApplyPhase(int32 InPhaseIndex)
{
	if (bEndingPlay || bApplyingPhase || GetWorld() == nullptr || GetWorld()->bIsTearingDown ||
		!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !Context.IsValid() ||
		Context->GetControlledPawn() != GetOwner() || !IsValid(Definition) ||
		!Definition->GetPhases().IsValidIndex(InPhaseIndex))
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("ApplyBossPhase"));
	}
	const FKnightBossPhaseEntry& Phase = Definition->GetPhases()[InPhaseIndex];
	const FKnightResult Validate = Context->ValidateActionSet(Phase.ActionSetId);
	if (!Validate.IsSuccess())
	{
		return Validate;
	}
	TGuardValue<bool> ApplyingGuard(bApplyingPhase, true);
	const FPrimaryAssetId PreviousActionSet = Context->GetActionSetId();
	const float PreviousInterval = Context->GetBaseDecisionInterval();
	Context->CancelOwnedAction();
	FKnightResult Apply = Context->SetActionSet(Phase.ActionSetId);
	if (Apply.IsSuccess())
	{
		Apply = Context->SetDecisionInterval(Phase.DecisionInterval);
	}
	if (!Apply.IsSuccess())
	{
		if (PreviousActionSet.IsValid() && Context.IsValid())
		{
			const FKnightResult RollbackSet = Context->SetActionSet(PreviousActionSet);
			const FKnightResult RollbackInterval = Context->SetDecisionInterval(PreviousInterval);
			if (!RollbackSet.IsSuccess() || !RollbackInterval.IsSuccess())
			{
				UE_LOG(LogKnightBossPhase, Error, TEXT("Boss phase rollback failed for %s: %s / %s."),
					*DefinitionId.ToString(), *RollbackSet.Context.ToString(), *RollbackInterval.Context.ToString());
			}
		}
		return Apply;
	}
	PhaseIndex = InPhaseIndex;
	PhaseKey = Phase.PhaseKey;
	PhaseActionSetId = Phase.ActionSetId;
	GetOwner()->ForceNetUpdate();
	OnPhaseChanged.Broadcast(GetPhaseSnapshot());
	return FKnightResult::Success();
}

int32 UKnightBossPhaseComponent::GetPhaseIndex() const
{
	return PhaseIndex;
}

FPrimaryAssetId UKnightBossPhaseComponent::GetDefinitionId() const
{
	return DefinitionId;
}

FKnightBossPhaseSnapshot UKnightBossPhaseComponent::GetPhaseSnapshot() const
{
	FKnightBossPhaseSnapshot Snapshot;
	Snapshot.DefinitionId = DefinitionId;
	Snapshot.PhaseIndex = PhaseIndex;
	Snapshot.PhaseKey = PhaseKey;
	Snapshot.ActionSetId = PhaseActionSetId;
	return Snapshot;
}

const FKnightResult& UKnightBossPhaseComponent::GetLastPhaseResult() const
{
	return LastPhaseResult;
}

void UKnightBossPhaseComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	if (Context.IsValid() && !bEndingPlay && !bApplyingPhase)
	{
		const FKnightResult Result = RefreshPhase();
		if (!Result.IsSuccess())
		{
			UE_LOG(LogKnightBossPhase, Warning, TEXT("Boss %s phase update failed: %s."),
				*DefinitionId.ToString(), *Result.Context.ToString());
		}
	}
}

void UKnightBossPhaseComponent::OnRep_PhaseIndex()
{
	if (!bEndingPlay)
	{
		OnPhaseChanged.Broadcast(GetPhaseSnapshot());
	}
}

void UKnightBossPhaseComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	}
	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	Context.Reset();
	AbilitySystem.Reset();
	Super::EndPlay(EndPlayReason);
}
