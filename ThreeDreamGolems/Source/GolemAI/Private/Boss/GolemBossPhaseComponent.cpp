#include "Boss/GolemBossPhaseComponent.h"

#include "AbilitySystemGlobals.h"
#include "Assets/GolemAssetManager.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Context/GolemAIResult.h"
#include "Definitions/GolemAIActionSetDefinition.h"
#include "Definitions/GolemBossPhaseDefinition.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemBossPhase, Log, All);

UGolemBossPhaseComponent::UGolemBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGolemBossPhaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemBossPhaseComponent, DefinitionId);
	DOREPLIFETIME(UGolemBossPhaseComponent, PhaseIndex);
	DOREPLIFETIME(UGolemBossPhaseComponent, PhaseKey);
	DOREPLIFETIME(UGolemBossPhaseComponent, PhaseActionSetId);
}

FGolemResult UGolemBossPhaseComponent::Configure(const FPrimaryAssetId& InDefinitionId)
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return GolemAIFailure(EGolemError::ShuttingDown, TEXT("ConfigureBossPhases"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemAIFailure(EGolemError::NotAuthority, TEXT("ConfigureBossPhases"));
	}
	if (DefinitionId == InDefinitionId && IsValid(Definition))
	{
		return FGolemResult::Success();
	}
	if (PhaseIndex != INDEX_NONE)
	{
		return GolemAIFailure(EGolemError::Conflict, TEXT("BossDefinitionAlreadyActive"));
	}
	UGolemAbilitySystemComponent* ASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), true));
	if (ASC == nullptr || ASC->GetOwner() != GetOwner() || ASC->GetAvatarActor() != GetOwner() ||
		!ASC->GetEntityId().IsValid() || ASC->GetGolemAttributes() == nullptr || ASC->GetLifeComponent() == nullptr)
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("BossPhaseGAS"));
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Loaded = nullptr;
	const FGolemResult Resolve = Manager != nullptr
		? Manager->ResolveDefinition(InDefinitionId, UGolemBossPhaseDefinition::StaticClass(), Loaded)
		: GolemAIFailure(EGolemError::InvalidState, TEXT("BossPhaseAssetManager"));
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	UGolemBossPhaseDefinition* Candidate = Cast<UGolemBossPhaseDefinition>(Loaded);
	const FGolemResult Validation = Candidate->ValidateBossPhases();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	for (const FGolemBossPhaseEntry& Phase : Candidate->GetPhases())
	{
		UGolemDefinition* ActionData = nullptr;
		const FGolemResult ActionResolve = Manager->ResolveDefinition(Phase.ActionSetId,
			UGolemAIActionSetDefinition::StaticClass(), ActionData);
		if (!ActionResolve.IsSuccess())
		{
			return ActionResolve;
		}
		const FGolemResult ActionValidation = CastChecked<UGolemAIActionSetDefinition>(ActionData)->ValidateActionSet();
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
		HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetHealthAttribute()).
			AddUObject(this, &UGolemBossPhaseComponent::HandleHealthChanged);
		MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetMaxHealthAttribute()).
			AddUObject(this, &UGolemBossPhaseComponent::HandleHealthChanged);
	}
	GetOwner()->ForceNetUpdate();
	return FGolemResult::Success();
}

FGolemResult UGolemBossPhaseComponent::BindContext(UGolemAIContextComponent* InContext)
{
	if (!IsValid(InContext) || InContext->GetControlledPawn() != GetOwner() ||
		InContext->GetAbilitySystem() != AbilitySystem.Get() || !IsValid(Definition))
	{
		return GolemAIFailure(EGolemError::NotOwned, TEXT("BindBossPhaseContext"));
	}
	for (const FGolemBossPhaseEntry& Phase : Definition->GetPhases())
	{
		const FGolemResult Validation = InContext->ValidateActionSet(Phase.ActionSetId);
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

FGolemResult UGolemBossPhaseComponent::InitializePhases(const FPrimaryAssetId& InDefinitionId,
	UGolemAIContextComponent* InContext)
{
	const FGolemResult Result = Configure(InDefinitionId);
	return Result.IsSuccess() ? BindContext(InContext) : Result;
}

FGolemResult UGolemBossPhaseComponent::ResetForCheckpoint()
{
	LastPhaseResult = TryApplyPhase(0);
	return LastPhaseResult;
}

FGolemResult UGolemBossPhaseComponent::RefreshPhase()
{
	if (bApplyingPhase || bEndingPlay || !Context.IsValid() || !AbilitySystem.IsValid() ||
		!IsValid(Definition) || PhaseIndex == INDEX_NONE || !GetOwner()->HasAuthority())
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("RefreshBossPhase"));
	}
	if (!AbilitySystem->GetLifeComponent()->IsAlive())
	{
		Context->CancelOwnedAction();
		return FGolemResult::Success();
	}
	const UGolemAttributeSet* Attributes = AbilitySystem->GetGolemAttributes();
	const float Fraction = Attributes->GetHealth() / Attributes->GetMaxHealth();
	if (!FMath::IsFinite(Fraction))
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("BossHealthFraction"));
	}
	int32 DesiredPhase = PhaseIndex;
	for (int32 Index = PhaseIndex + 1; Index < Definition->GetPhases().Num(); ++Index)
	{
		if (Fraction <= Definition->GetPhases()[Index].HealthFraction)
		{
			DesiredPhase = Index;
		}
	}
	LastPhaseResult = DesiredPhase != PhaseIndex ? TryApplyPhase(DesiredPhase) : FGolemResult::Success();
	return LastPhaseResult;
}

FGolemResult UGolemBossPhaseComponent::TryApplyPhase(int32 InPhaseIndex)
{
	if (bEndingPlay || bApplyingPhase || GetWorld() == nullptr || GetWorld()->bIsTearingDown ||
		!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !Context.IsValid() ||
		Context->GetControlledPawn() != GetOwner() || !IsValid(Definition) ||
		!Definition->GetPhases().IsValidIndex(InPhaseIndex))
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("ApplyBossPhase"));
	}
	const FGolemBossPhaseEntry& Phase = Definition->GetPhases()[InPhaseIndex];
	const FGolemResult Validate = Context->ValidateActionSet(Phase.ActionSetId);
	if (!Validate.IsSuccess())
	{
		return Validate;
	}
	TGuardValue<bool> ApplyingGuard(bApplyingPhase, true);
	const FPrimaryAssetId PreviousActionSet = Context->GetActionSetId();
	const float PreviousInterval = Context->GetBaseDecisionInterval();
	Context->CancelOwnedAction();
	FGolemResult Apply = Context->SetActionSet(Phase.ActionSetId);
	if (Apply.IsSuccess())
	{
		Apply = Context->SetDecisionInterval(Phase.DecisionInterval);
	}
	if (!Apply.IsSuccess())
	{
		if (PreviousActionSet.IsValid() && Context.IsValid())
		{
			const FGolemResult RollbackSet = Context->SetActionSet(PreviousActionSet);
			const FGolemResult RollbackInterval = Context->SetDecisionInterval(PreviousInterval);
			if (!RollbackSet.IsSuccess() || !RollbackInterval.IsSuccess())
			{
				UE_LOG(LogGolemBossPhase, Error, TEXT("Boss phase rollback failed for %s: %s / %s."),
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
	return FGolemResult::Success();
}

int32 UGolemBossPhaseComponent::GetPhaseIndex() const
{
	return PhaseIndex;
}

FPrimaryAssetId UGolemBossPhaseComponent::GetDefinitionId() const
{
	return DefinitionId;
}

FGolemBossPhaseSnapshot UGolemBossPhaseComponent::GetPhaseSnapshot() const
{
	FGolemBossPhaseSnapshot Snapshot;
	Snapshot.DefinitionId = DefinitionId;
	Snapshot.PhaseIndex = PhaseIndex;
	Snapshot.PhaseKey = PhaseKey;
	Snapshot.ActionSetId = PhaseActionSetId;
	return Snapshot;
}

const FGolemResult& UGolemBossPhaseComponent::GetLastPhaseResult() const
{
	return LastPhaseResult;
}

void UGolemBossPhaseComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	if (Context.IsValid() && !bEndingPlay && !bApplyingPhase)
	{
		const FGolemResult Result = RefreshPhase();
		if (!Result.IsSuccess())
		{
			UE_LOG(LogGolemBossPhase, Warning, TEXT("Boss %s phase update failed: %s."),
				*DefinitionId.ToString(), *Result.Context.ToString());
		}
	}
}

void UGolemBossPhaseComponent::OnRep_PhaseIndex()
{
	if (!bEndingPlay)
	{
		OnPhaseChanged.Broadcast(GetPhaseSnapshot());
	}
}

void UGolemBossPhaseComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	}
	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	Context.Reset();
	AbilitySystem.Reset();
	Super::EndPlay(EndPlayReason);
}
