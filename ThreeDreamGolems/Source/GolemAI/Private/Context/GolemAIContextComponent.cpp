#include "Context/GolemAIContextComponent.h"
#include "Skills/GolemSkillRuntimeComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Assets/GolemAssetManager.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Context/GolemAIResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystem.h"
#include "Tags/GolemGameplayTags.h"
#include "Threat/GolemThreatComponent.h"

namespace GolemAIContext
{
	UGolemAbilitySystemComponent* ResolveASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}

	bool IsHostile(const UGolemAbilitySystemComponent* Source, const UGolemAbilitySystemComponent* Other)
	{
		return Source != nullptr && Other != nullptr &&
			((Source->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) &&
				Other->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)) ||
			(Source->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
				Other->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy)));
	}

	bool ComesBefore(const FGolemEntityId& Left, const FGolemEntityId& Right)
	{
		const FGuid A = Left.Value;
		const FGuid B = Right.Value;
		if (A.A != B.A)
		{
			return A.A < B.A;
		}
		if (A.B != B.B)
		{
			return A.B < B.B;
		}
		if (A.C != B.C)
		{
			return A.C < B.C;
		}
		return A.D < B.D;
	}
}

UGolemAIContextComponent::UGolemAIContextComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FGolemResult UGolemAIContextComponent::ValidateContext() const
{
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	const UWorld* World = GetWorld();
	if (bEndingPlay || bClearingContext || World == nullptr || World->bIsTearingDown)
	{
		return GolemAIFailure(EGolemError::ShuttingDown, TEXT("AIContextLifecycle"));
	}
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return GolemAIFailure(EGolemError::NotAuthority, TEXT("AIContextAuthority"));
	}
	if (!ControlledPawn.IsValid() || Controller->GetPawn() != ControlledPawn.Get() ||
		ControlledPawn->GetController() != Controller || ControlledPawn->GetWorld() != World ||
		ControlledPawn->IsActorBeingDestroyed() || !AbilitySystem.IsValid() || !Life.IsValid() ||
		AbilitySystem->GetOwner() != ControlledPawn.Get() || AbilitySystem->GetAvatarActor() != ControlledPawn.Get() ||
		AbilitySystem->GetEntityId() != PawnId || !PawnId.IsValid())
	{
		return GolemAIFailure(EGolemError::NotOwned, TEXT("AIControlledPawn"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::InitializeControlledPawn(APawn* InPawn)
{
	AAIController* Controller = Cast<AAIController>(GetOwner());
	UWorld* World = GetWorld();
	if (bEndingPlay || World == nullptr || World->bIsTearingDown)
	{
		return GolemAIFailure(EGolemError::ShuttingDown, TEXT("InitializeAIContext"));
	}
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return GolemAIFailure(EGolemError::NotAuthority, TEXT("InitializeAIContext"));
	}
	UGolemAbilitySystemComponent* ASC = GolemAIContext::ResolveASC(InPawn);
	if (!IsValid(InPawn) || InPawn->GetWorld() != World || Controller->GetPawn() != InPawn ||
		InPawn->GetController() != Controller || ASC == nullptr || ASC->GetOwner() != InPawn ||
		ASC->GetAvatarActor() != InPawn || !ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr ||
		ASC->GetGolemAttributes() == nullptr)
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("InitializeAIPawnGAS"));
	}
	if (ControlledPawn.Get() == InPawn && AbilitySystem.Get() == ASC && PawnId == ASC->GetEntityId())
	{
		return FGolemResult::Success();
	}
	ClearControlledPawn();
	ControlledPawn = InPawn;
	AbilitySystem = ASC;
	Life = ASC->GetLifeComponent();
	Threat = InPawn->FindComponentByClass<UGolemThreatComponent>();
	PawnId = ASC->GetEntityId();
	Director = World->GetSubsystem<UGolemCombatDirectorSubsystem>();
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UGolemAIContextComponent::HandleAbilityEnded);
	LifeHandle = Life->OnLifeStateChanged.AddUObject(this, &UGolemAIContextComponent::HandlePawnLifeChanged);
	if (Director.IsValid())
	{
		LeaseHandle = Director->OnAttackLeaseReleased.AddUObject(this, &UGolemAIContextComponent::HandleLeaseReleased);
	}
	InPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this,
		&UGolemAIContextComponent::HandlePawnControllerChanged);
	InPawn->OnEndPlay.AddUniqueDynamic(this, &UGolemAIContextComponent::HandleObservedEndPlay);
	HoldLocation = InPawn->GetActorLocation();
	OnDecisionChanged.Broadcast();
	return FGolemResult::Success();
}

void UGolemAIContextComponent::ClearControlledPawn()
{
	if (bClearingContext)
	{
		return;
	}
	TGuardValue<bool> ClearingGuard(bClearingContext, true);
	++OwnershipGeneration;
	CancelOwnedAction();
	const TArray<int32> Requests = ActiveEQSRequests.Array();
	for (int32 Request : Requests)
	{
		if (UEnvQueryManager* Queries = UEnvQueryManager::GetCurrent(GetWorld()))
		{
			Queries->AbortQuery(Request);
		}
		FinishEQSRequest(Request, true);
	}
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	if (Life.IsValid())
	{
		Life->OnLifeStateChanged.Remove(LifeHandle);
	}
	if (Director.IsValid())
	{
		Director->OnAttackLeaseReleased.Remove(LeaseHandle);
	}
	if (ControlledPawn.IsValid())
	{
		ControlledPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this,
			&UGolemAIContextComponent::HandlePawnControllerChanged);
		ControlledPawn->OnEndPlay.RemoveDynamic(this, &UGolemAIContextComponent::HandleObservedEndPlay);
	}
	AbilityEndedHandle.Reset();
	LifeHandle.Reset();
	LeaseHandle.Reset();
	ControlledPawn.Reset();
	AbilitySystem.Reset();
	Life.Reset();
	Threat.Reset();
	Director.Reset();
	PawnId = FGolemEntityId();
	Target.Reset();
	TargetId = FGolemEntityId();
	FocusTarget.Reset();
	FormationLeader.Reset();
	PerceivedActors.Reset();
	CooldownEnds.Reset();
	ActionSet = nullptr;
	ActionSetId = FPrimaryAssetId();
	Command = EGolemCompanionCommand::Follow;
	bTargetFriendlyDowned = false;
	if (!bEndingPlay)
	{
		OnDecisionChanged.Broadcast();
	}
}

FGolemResult UGolemAIContextComponent::ValidateActionSet(const FPrimaryAssetId& InActionSetId) const
{
	const FGolemResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess())
	{
		return ContextResult;
	}
	if (!GolemIdentity::IsValidDefinitionId(InActionSetId) ||
		InActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAIActionSet")))
	{
		return GolemAIFailure(EGolemError::InvalidId, TEXT("AIActionSetId"));
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Loaded = nullptr;
	const FGolemResult Resolve = Manager != nullptr
		? Manager->ResolveDefinition(InActionSetId, UGolemAIActionSetDefinition::StaticClass(), Loaded)
		: GolemAIFailure(EGolemError::InvalidState, TEXT("AIAssetManager"));
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	const UGolemAIActionSetDefinition* Set = Cast<UGolemAIActionSetDefinition>(Loaded);
	const FGolemResult Validate = Set->ValidateActionSet();
	if (!Validate.IsSuccess())
	{
		return Validate;
	}
	for (const FGolemAIActionEntry& Entry : Set->GetActions())
	{
		FGameplayAbilitySpecHandle Spec;
		FGameplayTag Input;
		const FGolemResult Granted = ResolveActionSpec(Entry.ActionTag, Spec, Input);
		if (!Granted.IsSuccess())
		{
			return Granted;
		}
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::SetActionSet(const FPrimaryAssetId& InActionSetId)
{
	const FGolemResult Validate = ValidateActionSet(InActionSetId);
	if (!Validate.IsSuccess())
	{
		return Validate;
	}
	if (ActionSetId == InActionSetId && IsValid(ActionSet))
	{
		return FGolemResult::Success();
	}
	UGolemDefinition* Loaded = nullptr;
	const FGolemResult Resolve = UGolemAssetManager::GetGolemAssetManager()->ResolveDefinition(InActionSetId,
		UGolemAIActionSetDefinition::StaticClass(), Loaded);
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	CancelOwnedAction();
	const FGolemResult StillOwned = ValidateContext();
	if (!StillOwned.IsSuccess())
	{
		return StillOwned;
	}
	ActionSet = Cast<UGolemAIActionSetDefinition>(Loaded);
	ActionSetId = InActionSetId;
	// Stable keys retain actual selection delays when a boss changes its choice set.
	OnDecisionChanged.Broadcast();
	return FGolemResult::Success();
}

FPrimaryAssetId UGolemAIContextComponent::GetActionSetId() const
{
	return ActionSetId;
}

APawn* UGolemAIContextComponent::GetControlledPawn() const
{
	return ControlledPawn.Get();
}

UGolemAbilitySystemComponent* UGolemAIContextComponent::GetAbilitySystem() const
{
	return AbilitySystem.Get();
}

FGolemResult UGolemAIContextComponent::ValidateTarget(AActor* InTarget, bool bFriendlyDowned) const
{
	const FGolemResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess())
	{
		return ContextResult;
	}
	UGolemAbilitySystemComponent* TargetASC = GolemAIContext::ResolveASC(InTarget);
	if (!IsValid(InTarget) || InTarget == ControlledPawn.Get() || InTarget->GetWorld() != GetWorld() ||
		InTarget->IsActorBeingDestroyed() || !InTarget->HasAuthority() || TargetASC == nullptr ||
		TargetASC->GetOwner() != InTarget || TargetASC->GetAvatarActor() != InTarget ||
		!TargetASC->GetEntityId().IsValid() || TargetASC->GetEntityId() == PawnId ||
		TargetASC->GetLifeComponent() == nullptr)
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AITargetIdentity"));
	}
	const bool bEligible = bFriendlyDowned
		? AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
			TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
			TargetASC->GetLifeComponent()->IsDowned()
		: GolemAIContext::IsHostile(AbilitySystem.Get(), TargetASC) && TargetASC->GetLifeComponent()->IsAlive();
	return bEligible ? FGolemResult::Success()
		: GolemAIFailure(EGolemError::InvalidState, TEXT("AITargetRoleOrLife"));
}

FGolemResult UGolemAIContextComponent::SetValidatedTarget(AActor* InTarget, bool bFriendlyDowned)
{
	const FGolemResult Result = ValidateTarget(InTarget, bFriendlyDowned);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemAbilitySystemComponent* TargetASC = GolemAIContext::ResolveASC(InTarget);
	if (Target.Get() == InTarget && TargetId == TargetASC->GetEntityId() &&
		bTargetFriendlyDowned == bFriendlyDowned)
	{
		return FGolemResult::Success();
	}
	CancelOwnedAction();
	if (!ValidateTarget(InTarget, bFriendlyDowned).IsSuccess())
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("AITargetChangedDuringCancel"));
	}
	Target = InTarget;
	TargetId = TargetASC->GetEntityId();
	bTargetFriendlyDowned = bFriendlyDowned;
	OnDecisionChanged.Broadcast();
	return FGolemResult::Success();
}

void UGolemAIContextComponent::ClearTarget()
{
	CancelOwnedAction();
	Target.Reset();
	TargetId = FGolemEntityId();
	bTargetFriendlyDowned = false;
	if (!bEndingPlay)
	{
		OnDecisionChanged.Broadcast();
	}
}

FGolemResult UGolemAIContextComponent::SetCompanionCommand(EGolemCompanionCommand InCommand,
	AActor* InFocusTarget)
{
	const FGolemResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) ||
		static_cast<uint8>(InCommand) > static_cast<uint8>(EGolemCompanionCommand::Regroup))
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AICompanionCommand"));
	}
	if (InCommand == EGolemCompanionCommand::Focus)
	{
		const FGolemResult TargetResult = ValidateTarget(InFocusTarget, false);
		if (!TargetResult.IsSuccess())
		{
			return TargetResult;
		}
	}
	CancelOwnedAction();
	if (!ValidateContext().IsSuccess())
	{
		return GolemAIFailure(EGolemError::NotOwned, TEXT("AICommandChangedDuringCancel"));
	}
	Command = InCommand;
	FocusTarget = InCommand == EGolemCompanionCommand::Focus ? InFocusTarget : nullptr;
	HoldLocation = ControlledPawn->GetActorLocation();
	if (InCommand == EGolemCompanionCommand::Regroup)
	{
		ClearTarget();
	}
	RefreshDecisionContext();
	OnDecisionChanged.Broadcast();
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::SetFormationLeader(AActor* Leader, const FVector& LocalOffset,
	float Acceptance)
{
	const FGolemResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (LocalOffset.ContainsNaN() || LocalOffset.SizeSquared() > FMath::Square(10000.0) ||
		!FMath::IsFinite(Acceptance) || Acceptance < 1.0f || Acceptance > 2000.0f ||
		(Leader != nullptr && (!IsValid(Leader) || Leader == ControlledPawn.Get() || Leader->GetWorld() != GetWorld())))
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIFormationLeader"));
	}
	FormationLeader = Leader;
	FormationOffset = LocalOffset;
	FormationAcceptance = Acceptance;
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::SetPerceivedActor(AActor* Actor, bool bSensed)
{
	const FGolemResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemAbilitySystemComponent* ActorASC = GolemAIContext::ResolveASC(Actor);
	if (ActorASC == nullptr || Actor->GetWorld() != GetWorld() || !ActorASC->GetEntityId().IsValid())
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIPerceptionActor"));
	}
	if (!bSensed)
	{
		PerceivedActors.Remove(ActorASC->GetEntityId());
		return FGolemResult::Success();
	}
	const FGolemResult TargetResult = ValidateTarget(Actor, false);
	if (!TargetResult.IsSuccess())
	{
		return TargetResult;
	}
	if (PerceivedActors.Num() >= 64 && !PerceivedActors.Contains(ActorASC->GetEntityId()))
	{
		return GolemAIFailure(EGolemError::CapacityExceeded, TEXT("AIPerceptionCapacity"));
	}
	const TWeakObjectPtr<AActor>* Existing = PerceivedActors.Find(ActorASC->GetEntityId());
	if (Existing != nullptr && Existing->IsValid() && Existing->Get() != Actor)
	{
		return GolemAIFailure(EGolemError::DuplicateId, TEXT("AIPerceptionIdentity"));
	}
	PerceivedActors.Add(ActorASC->GetEntityId(), Actor);
	if (Threat.IsValid())
	{
		return Threat->ObserveTarget(Actor);
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::SetDecisionInterval(float InInterval)
{
	const FGolemResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FMath::IsFinite(InInterval) || InInterval < 0.05f || InInterval > 2.0f)
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIDecisionInterval"));
	}
	DecisionInterval = InInterval;
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::SetAttackLeaseDuration(float InDuration)
{
	if (!FMath::IsFinite(InDuration) || InDuration < 0.1f || InDuration > 60.0f)
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIAttackLeaseDuration"));
	}
	const FGolemResult Result = ValidateContext();
	if (Result.IsSuccess())
	{
		AttackLeaseDuration = InDuration;
	}
	return Result;
}

float UGolemAIContextComponent::GetBaseDecisionInterval() const
{
	return DecisionInterval;
}

FGolemResult UGolemAIContextComponent::SetScheduledDecisionInterval(float InInterval)
{
	const FGolemResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FMath::IsFinite(InInterval) || InInterval < 0.05f || InInterval > 2.0f)
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIScheduledDecisionInterval"));
	}
	ScheduledDecisionInterval = InInterval;
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::SetRepositionQuery(UEnvQuery* InQuery)
{
	if (!IsValid(InQuery) || InQuery->GetOptions().IsEmpty())
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIRepositionQuery"));
	}
	const FGolemResult Result = ValidateContext();
	if (Result.IsSuccess())
	{
		RepositionQuery = InQuery;
	}
	return Result;
}

UEnvQuery* UGolemAIContextComponent::GetRepositionQuery() const
{
	return RepositionQuery;
}

FGolemAIDecisionSnapshot UGolemAIContextComponent::GetDecisionSnapshot() const
{
	FGolemAIDecisionSnapshot Snapshot;
	Snapshot.PawnId = PawnId;
	Snapshot.ActionSetId = ActionSetId;
	Snapshot.Command = Command;
	Snapshot.DecisionInterval = FMath::Max(DecisionInterval, ScheduledDecisionInterval);
	Snapshot.FormationAcceptance = FormationAcceptance;
	if (!ValidateContext().IsSuccess())
	{
		return Snapshot;
	}
	Snapshot.bAlive = Life->IsAlive();
	Snapshot.bStaggered = AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered);
	Snapshot.Stamina = AbilitySystem->GetGolemAttributes()->GetStamina();
	Snapshot.RuneEnergy = AbilitySystem->GetGolemAttributes()->GetRuneEnergy();
	Snapshot.bActionRunning = ActiveActionToken.IsValid();
	Snapshot.ActiveActionToken = ActiveActionToken;
	Snapshot.ActiveActionTag = ActiveActionTag;
	Snapshot.bHoldPosition = Command == EGolemCompanionCommand::Hold;
	if (FormationLeader.IsValid() && !FormationLeader->IsActorBeingDestroyed())
	{
		Snapshot.FormationGoal = FormationLeader->GetActorLocation() +
			FRotator(0.0, FormationLeader->GetActorRotation().Yaw, 0.0).RotateVector(FormationOffset);
		Snapshot.bHasFormationGoal = !Snapshot.FormationGoal.ContainsNaN();
		if (Snapshot.bHasFormationGoal && !Snapshot.bHoldPosition)
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			if (Navigation != nullptr)
			{
				const FVector LeaderPosition = FormationLeader->GetActorLocation();
				const FVector DesiredOffset = Snapshot.FormationGoal - LeaderPosition;
				FNavLocation LeaderOnNavigation;
				const bool bHasLeaderNavigation = Navigation->ProjectPointToNavigation(LeaderPosition, LeaderOnNavigation,
					FVector(150.0, 150.0, 250.0), &ControlledPawn->GetNavAgentPropertiesRef());
				if (bHasLeaderNavigation)
				{
					Snapshot.FormationGoal = LeaderOnNavigation.Location;
				}
				// Contract wide formations on the leader's side of walls and narrow bridges.
				// Movement still requires a complete path from the companion; this never relocates a pawn.
				for (const double Fraction : {1.0, 0.5, 0.25, 0.0})
				{
					if (!bHasLeaderNavigation)
					{
						break;
					}
					FNavLocation Projected;
					FVector Obstruction;
					if (Navigation->ProjectPointToNavigation(LeaderPosition + DesiredOffset * Fraction, Projected,
						FVector(150.0, 150.0, 250.0), &ControlledPawn->GetNavAgentPropertiesRef())
						&& !UNavigationSystemV1::NavigationRaycast(ControlledPawn.Get(), LeaderOnNavigation.Location,
							Projected.Location, Obstruction, {}, ControlledPawn->GetController()))
					{
						Snapshot.FormationGoal = Projected.Location;
						break;
					}
				}
			}
		}
	}
	if (Snapshot.bHoldPosition)
	{
		Snapshot.FormationGoal = HoldLocation;
		Snapshot.bHasFormationGoal = true;
	}
	Snapshot.bNeedsRegroup = Command == EGolemCompanionCommand::Regroup && Snapshot.bHasFormationGoal &&
		FVector::DistSquared2D(ControlledPawn->GetActorLocation(), Snapshot.FormationGoal) >
			FMath::Square(FormationAcceptance);
	if (Target.IsValid() && ValidateTarget(Target.Get(), bTargetFriendlyDowned).IsSuccess() &&
		GolemAIContext::ResolveASC(Target.Get())->GetEntityId() == TargetId)
	{
		Snapshot.Target = Target;
		Snapshot.TargetId = TargetId;
		Snapshot.bHasTarget = true;
		Snapshot.bTargetFriendlyDowned = bTargetFriendlyDowned;
		Snapshot.TargetDistance = FVector::Distance(ControlledPawn->GetActorLocation(), Target->GetActorLocation());
	}
	if (Snapshot.bAlive && !Snapshot.bStaggered && Snapshot.bHasTarget && !Snapshot.bTargetFriendlyDowned &&
		!Snapshot.bNeedsRegroup && !Snapshot.bActionRunning && IsValid(ActionSet))
	{
		FGolemAIActionEntry Selection;
		Snapshot.bCanSelectAction = ActionSet->TrySelectAction(MakeSelectionContext(), Selection).IsSuccess();
		if (HasSpecialization())
		{
			int32 Slot = 0;
			AActor* SkillTarget = nullptr;
			Snapshot.bCanSelectAction = SelectSpecializationAction(Slot, SkillTarget).IsSuccess();
		}
	}
	return Snapshot;
}

void UGolemAIContextComponent::RefreshDecisionContext()
{
	if (!ValidateContext().IsSuccess() || !Life->IsAlive())
	{
		return;
	}
	if (Target.IsValid() && ValidateTarget(Target.Get(), bTargetFriendlyDowned).IsSuccess() &&
		GolemAIContext::ResolveASC(Target.Get())->GetEntityId() == TargetId && ActiveActionToken.IsValid())
	{
		return;
	}
	if (TargetId.IsValid() && (!Target.IsValid() || !ValidateTarget(Target.Get(), bTargetFriendlyDowned).IsSuccess()))
	{
		ClearTarget();
	}
	// Sensing only: the StateTree's priority states choose revive, regroup, attack, movement or waiting.
	AActor* Downed = nullptr;
	double BestDownedDistance = FMath::Square(2200.0);
	FGolemEntityId BestDownedId;
	if (AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem))
	{
		for (TActorIterator<APawn> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			APawn* Candidate = *Iterator;
			if (!ValidateTarget(Candidate, true).IsSuccess())
			{
				continue;
			}
			UGolemAbilitySystemComponent* CandidateASC = GolemAIContext::ResolveASC(Candidate);
			UGolemLifeComponent* CandidateLife = CandidateASC->GetLifeComponent();
			if (CandidateLife->IsReviveInProgress() && CandidateLife->GetActiveReviver() != ControlledPawn.Get())
			{
				continue;
			}
			const double Distance = FVector::DistSquared(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
			if ((Command == EGolemCompanionCommand::Hold &&
				Distance > FMath::Square(CandidateLife->GetReviveRange())) || Distance > BestDownedDistance)
			{
				continue;
			}
			if (Distance < BestDownedDistance || !BestDownedId.IsValid() ||
				GolemAIContext::ComesBefore(CandidateASC->GetEntityId(), BestDownedId))
			{
				Downed = Candidate;
				BestDownedDistance = Distance;
				BestDownedId = CandidateASC->GetEntityId();
			}
		}
	}
	if (Downed != nullptr)
	{
		SetValidatedTarget(Downed, true);
		return;
	}
	if (GetDecisionSnapshot().bNeedsRegroup)
	{
		ClearTarget();
		return;
	}
	if (Command == EGolemCompanionCommand::Focus && ValidateTarget(FocusTarget.Get(), false).IsSuccess())
	{
		SetValidatedTarget(FocusTarget.Get());
		return;
	}
	AActor* Best = Threat.IsValid() ? Threat->GetHighestThreatTarget() : nullptr;
	if (Best != nullptr && !ValidateTarget(Best, false).IsSuccess())
	{
		Best = nullptr;
	}
	double BestDistance = Best != nullptr
		? FVector::DistSquared(ControlledPawn->GetActorLocation(), Best->GetActorLocation()) : TNumericLimits<double>::Max();
	FGolemEntityId BestId = Best != nullptr ? GolemAIContext::ResolveASC(Best)->GetEntityId() : FGolemEntityId();
	if (Best == nullptr)
	{
		for (auto Iterator = PerceivedActors.CreateIterator(); Iterator; ++Iterator)
		{
			AActor* Candidate = Iterator.Value().Get();
			if (!ValidateTarget(Candidate, false).IsSuccess() ||
				GolemAIContext::ResolveASC(Candidate)->GetEntityId() != Iterator.Key())
			{
				Iterator.RemoveCurrent();
				continue;
			}
			const double Distance = FVector::DistSquared(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
			if (Distance < BestDistance || (Distance == BestDistance &&
				(!BestId.IsValid() || GolemAIContext::ComesBefore(Iterator.Key(), BestId))))
			{
				Best = Candidate;
				BestId = Iterator.Key();
				BestDistance = Distance;
			}
		}
	}
	if (Best != nullptr && (Command != EGolemCompanionCommand::Hold || BestDistance <= FMath::Square(500.0)))
	{
		SetValidatedTarget(Best);
	}
	else if (TargetId.IsValid())
	{
		ClearTarget();
	}
}

FGolemAIActionSelectionContext UGolemAIContextComponent::MakeSelectionContext() const
{
	FGolemAIActionSelectionContext Selection;
	if (!AbilitySystem.IsValid() || !ControlledPawn.IsValid() || !Target.IsValid())
	{
		return Selection;
	}
	Selection.Distance = FVector::Distance(ControlledPawn->GetActorLocation(), Target->GetActorLocation());
	Selection.Stamina = AbilitySystem->GetGolemAttributes()->GetStamina();
	Selection.RuneEnergy = AbilitySystem->GetGolemAttributes()->GetRuneEnergy();
	AbilitySystem->GetOwnedGameplayTags(Selection.StateTags);
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability != nullptr)
		{
			Selection.OwnedAbilityActionTags.AppendTags(Spec.Ability->GetAssetTags());
		}
	}
	const double Time = GetWorld()->GetTimeSeconds();
	for (const TPair<FName, double>& Entry : CooldownEnds)
	{
		Selection.RemainingCooldowns.Add(Entry.Key, static_cast<float>(FMath::Max(0.0, Entry.Value - Time)));
	}
	return Selection;
}

FGolemResult UGolemAIContextComponent::TrySelectAction(FGolemAIActionEntry& OutAction) const
{
	OutAction = FGolemAIActionEntry();
	const FGolemResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!Life->IsAlive() || ActiveActionToken.IsValid() || bTargetFriendlyDowned ||
		!ValidateTarget(Target.Get(), false).IsSuccess() || !IsValid(ActionSet) ||
		GetDecisionSnapshot().bNeedsRegroup)
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("AISelectActionState"));
	}
	return ActionSet->TrySelectAction(MakeSelectionContext(), OutAction);
}

FGolemResult UGolemAIContextComponent::ResolveActionSpec(FGameplayTag ActionTag,
	FGameplayAbilitySpecHandle& OutSpec, FGameplayTag& OutInput) const
{
	OutSpec = FGameplayAbilitySpecHandle();
	OutInput = FGameplayTag();
	if (!AbilitySystem.IsValid() || !ActionTag.IsValid())
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIActionSpec"));
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability == nullptr || !Spec.Ability->GetAssetTags().HasTagExact(ActionTag))
		{
			continue;
		}
		if (OutSpec.IsValid())
		{
			return GolemAIFailure(EGolemError::Conflict, TEXT("AIActionAmbiguousGrant"));
		}
		for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
		{
			if (Tag.ToString().StartsWith(TEXT("Input.")))
			{
				if (OutInput.IsValid())
				{
					return GolemAIFailure(EGolemError::Conflict, TEXT("AIActionAmbiguousInput"));
				}
				OutInput = Tag;
			}
		}
		OutSpec = Spec.Handle;
	}
	if (!OutSpec.IsValid() || !OutInput.IsValid())
	{
		return GolemAIFailure(EGolemError::NotFound, TEXT("AIActionNotGranted"));
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Handle != OutSpec && Spec.GetDynamicSpecSourceTags().HasTagExact(OutInput))
		{
			return GolemAIFailure(EGolemError::Conflict, TEXT("AIActionInputSharedByDifferentSpec"));
		}
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::TryBeginAction(const FGolemAIActionEntry& Action,
	bool bUseAttackLease, FGuid& OutActionToken)
{
	OutActionToken.Invalidate();
	FGolemAIActionEntry Selected;
	const FGolemResult Result = TrySelectAction(Selected);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Action.ActionKey != Selected.ActionKey || Action.ActionTag != Selected.ActionTag)
	{
		return GolemAIFailure(EGolemError::Conflict, TEXT("AIActionSelectionChanged"));
	}
	return BeginActionSpec(Selected.ActionTag, Selected.ActionKey, Selected.CooldownSeconds,
		bUseAttackLease, OutActionToken);
}

bool UGolemAIContextComponent::HasSpecialization() const
{
	const UGolemSkillRuntimeComponent* Runtime = ControlledPawn.IsValid()
		? ControlledPawn->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
	return Runtime != nullptr && !Runtime->GetBuild().SpecId.IsNone();
}

FGolemResult UGolemAIContextComponent::SelectSpecializationAction(int32& OutSlot, AActor*& OutTarget) const
{
	OutSlot = 0;
	OutTarget = nullptr;
	const UGolemSkillRuntimeComponent* Runtime = ControlledPawn.IsValid()
		? ControlledPawn->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
	if (Runtime == nullptr || Runtime->GetBuild().SpecId.IsNone())
	{
		return GolemAIFailure(EGolemError::NotFound, TEXT("AISpecialization"));
	}
	const FGolemSkillRuntimeState State = Runtime->GetSkillState();
	const EGolemSkillRole Role = FGolemSkillCatalog::Role(State.Build.SpecId);
	TArray<int32> Priority;
	AActor* Ally = Runtime->SelectLowestHealthAlly(1200.0f);
	if (Role == EGolemSkillRole::Healer && Ally != nullptr)
	{
		Priority = { 4, 2, 3, 1 };
	}
	else if (Role == EGolemSkillRole::Tank)
	{
		Priority = State.Shield < 20.0f ? TArray<int32> { 4, 3, 2, 1 } : TArray<int32> { 4, 2, 1, 3 };
	}
	else if (State.Build.SpecId == TEXT("Pluto"))
	{
		const UGolemSkillRuntimeComponent* EnemyRuntime = Target.IsValid()
			? Target->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
		const bool bCanShatter = EnemyRuntime != nullptr && EnemyRuntime->GetSkillState().Chilled >= 3;
		Priority = bCanShatter ? TArray<int32> { 4, 2, 3, 1 } : TArray<int32> { 4, 3, 1, 2 };
	}
	else
	{
		Priority = State.Stacks >= 2 ? TArray<int32> { 4, 2, 1, 3 } : TArray<int32> { 4, 1, 3, 2 };
	}
	for (int32 Slot : Priority)
	{
		const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(State.Build, Slot);
		AActor* Candidate = D.bFriendly ? Ally != nullptr ? Ally : ControlledPawn.Get() : Target.Get();
		if (Role == EGolemSkillRole::Healer && Ally == nullptr && Slot != 1)
		{
			continue;
		}
		if (Runtime->ValidateSkill(Slot, Candidate).IsSuccess())
		{
			OutSlot = Slot;
			OutTarget = Candidate;
			return FGolemResult::Success();
		}
	}
	return GolemAIFailure(EGolemError::Conflict, TEXT("AISkillCooldownOrRange"));
}

FGolemResult UGolemAIContextComponent::TryBeginSpecializationAction(bool bUseAttackLease, FGuid& OutActionToken)
{
	OutActionToken.Invalidate();
	const FGolemResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess() || !Life->IsAlive() || ActiveActionToken.IsValid() || bStartingAction)
	{
		return ContextResult.IsSuccess() ? GolemAIFailure(EGolemError::Conflict, TEXT("AISkillRunning")) : ContextResult;
	}
	int32 Slot = 0;
	AActor* SkillTarget = nullptr;
	const FGolemResult Selection = SelectSpecializationAction(Slot, SkillTarget);
	if (!Selection.IsSuccess())
	{
		return Selection;
	}
	UGolemSkillRuntimeComponent* Runtime = ControlledPawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
	const FGolemSpecActionDefinition D = FGolemSkillCatalog::GetSkill(Runtime->GetBuild(), Slot);
	const uint64 StartingGeneration = OwnershipGeneration;
	const TWeakObjectPtr<UGolemAbilitySystemComponent> StartingASC = AbilitySystem;
	const TWeakObjectPtr<APawn> StartingPawn = ControlledPawn;
	const TWeakObjectPtr<UGolemCombatDirectorSubsystem> StartingDirector = Director;
	const FGolemEntityId StartingPawnId = PawnId;
	const TWeakObjectPtr<UGolemSkillRuntimeComponent> StartingRuntime = Runtime;
	TGuardValue<bool> StartingGuard(bStartingAction, true);
	FGolemAttackLease Lease;
	bool bLeaseTransferred = false;
	ON_SCOPE_EXIT
	{
		if (!bLeaseTransferred && Lease.IsValid() && StartingDirector.IsValid() && StartingPawn.IsValid())
		{
			StartingDirector->ReleaseAttackToken(StartingPawn.Get(), Lease);
		}
	};
	if (bUseAttackLease && !D.bFriendly && Slot != 4)
	{
		if (!Director.IsValid())
		{
			return GolemAIFailure(EGolemError::InvalidState, TEXT("AISkillDirector"));
		}
		const FGolemResult LeaseResult = StartingDirector->TryAcquireAttackToken(
			StartingPawn.Get(), SkillTarget, AttackLeaseDuration, Lease);
		if (!LeaseResult.IsSuccess())
		{
			return LeaseResult;
		}
	}
	if (OwnershipGeneration != StartingGeneration || ControlledPawn != StartingPawn || AbilitySystem != StartingASC
		|| PawnId != StartingPawnId || !StartingRuntime.IsValid() || !ValidateContext().IsSuccess())
	{
		return GolemAIFailure(EGolemError::Cancelled, TEXT("AISkillOwnershipChangedBeforeActivation"));
	}
	ActiveActionToken = FGuid::NewGuid();
	OutActionToken = ActiveActionToken;
	const FGuid StartedToken = ActiveActionToken;
	ActiveSpec = StartingRuntime->GetSkillHandle(Slot);
	ActiveLease = Lease;
	bLeaseTransferred = true;
	LastActionToken.Invalidate();
	LastActionStatus = EGolemAIActionStatus::Unknown;
	const FGolemResult Result = StartingRuntime->RequestSkill(Slot, SkillTarget);
	if (OwnershipGeneration != StartingGeneration || ControlledPawn != StartingPawn || AbilitySystem != StartingASC
		|| PawnId != StartingPawnId || !ValidateContext().IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EGolemAIActionStatus::Cancelled, true);
		}
		return GolemAIFailure(EGolemError::Cancelled, TEXT("AISkillOwnershipChangedDuringActivation"));
	}
	if (!Result.IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EGolemAIActionStatus::Failed, true);
		}
		return Result;
	}
	if (LastActionToken == StartedToken && LastActionStatus == EGolemAIActionStatus::Cancelled)
	{
		return GolemAIFailure(EGolemError::Cancelled, TEXT("AISkillCancelledDuringActivation"));
	}
	OnDecisionChanged.Broadcast();
	return FGolemResult::Success();
}

FGolemResult UGolemAIContextComponent::TryBeginRevive(FGuid& OutActionToken)
{
	OutActionToken.Invalidate();
	const FGolemResult Result = ValidateTarget(Target.Get(), true);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!bTargetFriendlyDowned)
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("AIReviveTarget"));
	}
	const FGolemResult Source = GolemAIContext::ResolveASC(Target.Get())->GetLifeComponent()->
		ValidateReviveSource(ControlledPawn.Get());
	if (!Source.IsSuccess())
	{
		return Source;
	}
	return BeginActionSpec(GolemGameplayTags::Ability_Party_Revive, TEXT("Revive"), 0.0f, false, OutActionToken);
}

FGolemResult UGolemAIContextComponent::BeginActionSpec(FGameplayTag ActionTag, FName ActionKey,
	float Cooldown, bool bUseAttackLease, FGuid& OutActionToken)
{
	const FGolemResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess())
	{
		return ContextResult;
	}
	if (!GetWorld()->HasBegunPlay())
	{
		return GolemAIFailure(EGolemError::InvalidState, TEXT("AIActionBeforeBeginPlay"));
	}
	if (!Life->IsAlive() || ActiveActionToken.IsValid() || bStartingAction)
	{
		return GolemAIFailure(EGolemError::Conflict, TEXT("AIActionAlreadyRunning"));
	}
	const uint64 StartingGeneration = OwnershipGeneration;
	const TWeakObjectPtr<UGolemAbilitySystemComponent> StartingASC = AbilitySystem;
	const TWeakObjectPtr<APawn> StartingPawn = ControlledPawn;
	const TWeakObjectPtr<UGolemCombatDirectorSubsystem> StartingDirector = Director;
	const FGolemEntityId StartingPawnId = PawnId;
	TGuardValue<bool> StartingGuard(bStartingAction, true);
	FGolemAttackLease PendingLease;
	bool bLeaseTransferred = false;
	ON_SCOPE_EXIT
	{
		if (!bLeaseTransferred && PendingLease.IsValid() && StartingDirector.IsValid() && StartingPawn.IsValid())
		{
			StartingDirector->ReleaseAttackToken(StartingPawn.Get(), PendingLease);
		}
	};
	FGameplayAbilitySpecHandle SpecHandle;
	FGameplayTag Input;
	const FGolemResult Resolve = ResolveActionSpec(ActionTag, SpecHandle, Input);
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(SpecHandle);
	if (Spec == nullptr || Spec->IsActive())
	{
		return GolemAIFailure(EGolemError::Conflict, TEXT("AIActionSpecInUse"));
	}
	if (bUseAttackLease)
	{
		if (!Director.IsValid())
		{
			return GolemAIFailure(EGolemError::InvalidState, TEXT("AICombatDirectorMissing"));
		}
		const FGolemResult Lease = StartingDirector->TryAcquireAttackToken(StartingPawn.Get(), Target.Get(),
			AttackLeaseDuration, PendingLease);
		if (!Lease.IsSuccess())
		{
			return Lease;
		}
		if (OwnershipGeneration != StartingGeneration || !ValidateContext().IsSuccess())
		{
			return GolemAIFailure(EGolemError::Cancelled, TEXT("AIActionOwnershipChangedBeforeActivation"));
		}
	}
	UGolemCombatComponent* Combat = ControlledPawn->FindComponentByClass<UGolemCombatComponent>();
	if (!bTargetFriendlyDowned && Combat != nullptr)
	{
		const FGolemResult TargetResult = Combat->SetValidatedTarget(Target.Get());
		if (!TargetResult.IsSuccess())
		{
			return TargetResult;
		}
	}
	if (OwnershipGeneration != StartingGeneration || !ValidateContext().IsSuccess())
	{
		return GolemAIFailure(EGolemError::Cancelled, TEXT("AIActionOwnershipChangedBeforeActivation"));
	}
	ActiveActionToken = FGuid::NewGuid();
	ActiveLease = PendingLease;
	bLeaseTransferred = true;
	OutActionToken = ActiveActionToken;
	const FGuid StartedToken = ActiveActionToken;
	ActiveSpec = SpecHandle;
	ActiveInput = Input;
	ActiveActionTag = ActionTag;
	LastActionToken.Invalidate();
	LastActionStatus = EGolemAIActionStatus::Unknown;
	const FGolemResult Activate = StartingASC->RequestAbilityInput(Input, true, Target.Get());
	if (OwnershipGeneration != StartingGeneration || ControlledPawn != StartingPawn ||
		AbilitySystem != StartingASC || PawnId != StartingPawnId || !ValidateContext().IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EGolemAIActionStatus::Cancelled, true);
		}
		return GolemAIFailure(EGolemError::Cancelled, TEXT("AIActionOwnershipChangedDuringActivation"));
	}
	if (!Activate.IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EGolemAIActionStatus::Failed, true);
		}
		else if (LastActionToken == StartedToken)
		{
			LastActionStatus = EGolemAIActionStatus::Failed;
		}
		return Activate;
	}
	if (LastActionToken == StartedToken && LastActionStatus == EGolemAIActionStatus::Cancelled)
	{
		return GolemAIFailure(EGolemError::Cancelled, TEXT("AIActionCancelledDuringActivation"));
	}
	if (Cooldown > 0.0f)
	{
		CooldownEnds.Add(ActionKey, GetWorld()->GetTimeSeconds() + Cooldown);
	}
	if (ActiveActionToken == StartedToken)
	{
		Spec = AbilitySystem->FindAbilitySpecFromHandle(SpecHandle);
		if (Spec == nullptr || !Spec->IsActive())
		{
			FinishOwnedAction(EGolemAIActionStatus::Failed, false);
			return GolemAIFailure(EGolemError::InvalidState, TEXT("AIActionDidNotActivate"));
		}
	}
	OnDecisionChanged.Broadcast();
	return FGolemResult::Success();
}

void UGolemAIContextComponent::FinishOwnedAction(EGolemAIActionStatus Status, bool bCancelSpec)
{
	if (!ActiveActionToken.IsValid())
	{
		return;
	}
	const FGuid FinishedToken = ActiveActionToken;
	const FGameplayAbilitySpecHandle FinishedSpec = ActiveSpec;
	const FGolemAttackLease Lease = ActiveLease;
	const TWeakObjectPtr<UGolemAbilitySystemComponent> FinishedASC = AbilitySystem;
	const TWeakObjectPtr<APawn> FinishedPawn = ControlledPawn;
	const TWeakObjectPtr<UGolemCombatDirectorSubsystem> FinishedDirector = Director;
	const FGolemEntityId FinishedPawnId = PawnId;
	const uint64 FinishedGeneration = OwnershipGeneration;
	ActiveActionToken.Invalidate();
	ActiveSpec = FGameplayAbilitySpecHandle();
	ActiveInput = FGameplayTag();
	ActiveActionTag = FGameplayTag();
	ActiveLease = FGolemAttackLease();
	LastActionToken = FinishedToken;
	LastActionStatus = Status;
	if (FinishedASC.IsValid())
	{
		if (bCancelSpec)
		{
			FinishedASC->CancelAbilityHandle(FinishedSpec);
		}
		// Cancellation may synchronously clear/rebind this controller context; retain only the old ASC capability.
		if (FinishedASC.IsValid() && FinishedASC->GetEntityId() == FinishedPawnId)
		{
			if (FGameplayAbilitySpec* Spec = FinishedASC->FindAbilitySpecFromHandle(FinishedSpec))
			{
				const bool bNewOwnedActivation = ActiveActionToken.IsValid() && ActiveSpec == FinishedSpec;
				if (!bNewOwnedActivation && (OwnershipGeneration == FinishedGeneration || !Spec->IsActive()))
				{
					Spec->InputPressed = false;
					if (FinishedPawn.IsValid() && FinishedASC->GetAvatarActor() == FinishedPawn.Get())
					{
						FinishedASC->AbilitySpecInputReleased(*Spec);
					}
				}
			}
		}
		// Exact spec release remains safe even if another source changed the input binding while the action ended.
	}
	if (Lease.IsValid() && FinishedDirector.IsValid() && FinishedPawn.IsValid())
	{
		FinishedDirector->ReleaseAttackToken(FinishedPawn.Get(), Lease);
	}
	if (!bEndingPlay)
	{
		OnActionFinished.Broadcast(FinishedToken, Status);
		OnDecisionChanged.Broadcast();
	}
}

FGolemResult UGolemAIContextComponent::CancelAction(const FGuid& ActionToken)
{
	if (!ActionToken.IsValid())
	{
		return GolemAIFailure(EGolemError::InvalidId, TEXT("CancelAIAction"));
	}
	if (ActionToken == LastActionToken && !ActiveActionToken.IsValid())
	{
		return FGolemResult::Success();
	}
	if (ActionToken != ActiveActionToken)
	{
		return GolemAIFailure(EGolemError::NotOwned, TEXT("CancelAIActionCapability"));
	}
	FinishOwnedAction(EGolemAIActionStatus::Cancelled, true);
	return FGolemResult::Success();
}

void UGolemAIContextComponent::CancelOwnedAction()
{
	FinishOwnedAction(EGolemAIActionStatus::Cancelled, true);
}

EGolemAIActionStatus UGolemAIContextComponent::GetActionStatus(const FGuid& ActionToken) const
{
	if (ActionToken.IsValid() && ActionToken == ActiveActionToken)
	{
		return EGolemAIActionStatus::Running;
	}
	return ActionToken.IsValid() && ActionToken == LastActionToken ? LastActionStatus : EGolemAIActionStatus::Unknown;
}

FGolemAttackLease UGolemAIContextComponent::GetActiveAttackLease() const
{
	return ActiveLease;
}

TArray<int32> UGolemAIContextComponent::GetActiveEQSRequestIds() const
{
	TArray<int32> Requests = ActiveEQSRequests.Array();
	Requests.Sort();
	return Requests;
}

int32 UGolemAIContextComponent::GetCompletedEQSRequestCount() const
{
	return CompletedEQSRequests;
}

int32 UGolemAIContextComponent::GetAbortedEQSRequestCount() const
{
	return AbortedEQSRequests;
}

void UGolemAIContextComponent::TrackEQSRequest(int32 QueryId)
{
	if (QueryId != INDEX_NONE)
	{
		ActiveEQSRequests.Add(QueryId);
	}
}

void UGolemAIContextComponent::FinishEQSRequest(int32 QueryId, bool bAborted)
{
	if (ActiveEQSRequests.Remove(QueryId) > 0)
	{
		if (bAborted)
		{
			++AbortedEQSRequests;
		}
		else
		{
			++CompletedEQSRequests;
		}
	}
}

void UGolemAIContextComponent::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	if (ActiveActionToken.IsValid() && Data.AbilitySpecHandle == ActiveSpec)
	{
		FinishOwnedAction(Data.bWasCancelled ? EGolemAIActionStatus::Cancelled : EGolemAIActionStatus::Succeeded, false);
	}
}

void UGolemAIContextComponent::HandlePawnLifeChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	if (Current != EGolemLifeState::Alive)
	{
		CancelOwnedAction();
		Target.Reset();
		TargetId = FGolemEntityId();
	}
	if (!bEndingPlay)
	{
		OnDecisionChanged.Broadcast();
	}
}

void UGolemAIContextComponent::HandleLeaseReleased(const FGolemAttackLease& Lease,
	EGolemAttackLeaseReleaseReason Reason)
{
	if (ActiveLease.IsValid() && Lease == ActiveLease)
	{
		ActiveLease = FGolemAttackLease();
		FinishOwnedAction(EGolemAIActionStatus::Cancelled, true);
	}
}

void UGolemAIContextComponent::HandlePawnControllerChanged(APawn* Pawn, AController* Previous,
	AController* Current)
{
	if (Pawn == ControlledPawn.Get() && Current != GetOwner())
	{
		ClearControlledPawn();
	}
}

void UGolemAIContextComponent::HandleObservedEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (Actor == ControlledPawn.Get())
	{
		ClearControlledPawn();
	}
}

void UGolemAIContextComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ClearControlledPawn();
	RepositionQuery = nullptr;
	Super::EndPlay(EndPlayReason);
}
