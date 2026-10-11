#include "Context/KnightAIContextComponent.h"
#include "Skills/KnightSkillRuntimeComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Assets/KnightAssetManager.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Context/KnightAIResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"
#include "Tags/KnightGameplayTags.h"
#include "Threat/KnightThreatComponent.h"

namespace KnightAIContext
{
	UKnightAbilitySystemComponent* ResolveASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UKnightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}

	bool IsHostile(const UKnightAbilitySystemComponent* Source, const UKnightAbilitySystemComponent* Other)
	{
		return Source != nullptr && Other != nullptr &&
			((Source->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) &&
				Other->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)) ||
			(Source->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
				Other->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy)));
	}

	bool ComesBefore(const FKnightEntityId& Left, const FKnightEntityId& Right)
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

UKnightAIContextComponent::UKnightAIContextComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FKnightResult UKnightAIContextComponent::ValidateContext() const
{
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	const UWorld* World = GetWorld();
	if (bEndingPlay || bClearingContext || World == nullptr || World->bIsTearingDown)
	{
		return KnightAIFailure(EKnightError::ShuttingDown, TEXT("AIContextLifecycle"));
	}
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return KnightAIFailure(EKnightError::NotAuthority, TEXT("AIContextAuthority"));
	}
	if (!ControlledPawn.IsValid() || Controller->GetPawn() != ControlledPawn.Get() ||
		ControlledPawn->GetController() != Controller || ControlledPawn->GetWorld() != World ||
		ControlledPawn->IsActorBeingDestroyed() || !AbilitySystem.IsValid() || !Life.IsValid() ||
		AbilitySystem->GetOwner() != ControlledPawn.Get() || AbilitySystem->GetAvatarActor() != ControlledPawn.Get() ||
		AbilitySystem->GetEntityId() != PawnId || !PawnId.IsValid())
	{
		return KnightAIFailure(EKnightError::NotOwned, TEXT("AIControlledPawn"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::InitializeControlledPawn(APawn* InPawn)
{
	AAIController* Controller = Cast<AAIController>(GetOwner());
	UWorld* World = GetWorld();
	if (bEndingPlay || World == nullptr || World->bIsTearingDown)
	{
		return KnightAIFailure(EKnightError::ShuttingDown, TEXT("InitializeAIContext"));
	}
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return KnightAIFailure(EKnightError::NotAuthority, TEXT("InitializeAIContext"));
	}
	UKnightAbilitySystemComponent* ASC = KnightAIContext::ResolveASC(InPawn);
	if (!IsValid(InPawn) || InPawn->GetWorld() != World || Controller->GetPawn() != InPawn ||
		InPawn->GetController() != Controller || ASC == nullptr || ASC->GetOwner() != InPawn ||
		ASC->GetAvatarActor() != InPawn || !ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr ||
		ASC->GetKnightAttributes() == nullptr)
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("InitializeAIPawnGAS"));
	}
	if (ControlledPawn.Get() == InPawn && AbilitySystem.Get() == ASC && PawnId == ASC->GetEntityId())
	{
		return FKnightResult::Success();
	}
	ClearControlledPawn();
	ControlledPawn = InPawn;
	AbilitySystem = ASC;
	Life = ASC->GetLifeComponent();
	Threat = InPawn->FindComponentByClass<UKnightThreatComponent>();
	PawnId = ASC->GetEntityId();
	Director = World->GetSubsystem<UKnightCombatDirectorSubsystem>();
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UKnightAIContextComponent::HandleAbilityEnded);
	LifeHandle = Life->OnLifeStateChanged.AddUObject(this, &UKnightAIContextComponent::HandlePawnLifeChanged);
	if (Director.IsValid())
	{
		LeaseHandle = Director->OnAttackLeaseReleased.AddUObject(this, &UKnightAIContextComponent::HandleLeaseReleased);
	}
	InPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this,
		&UKnightAIContextComponent::HandlePawnControllerChanged);
	InPawn->OnEndPlay.AddUniqueDynamic(this, &UKnightAIContextComponent::HandleObservedEndPlay);
	HoldLocation = InPawn->GetActorLocation();
	OnDecisionChanged.Broadcast();
	return FKnightResult::Success();
}

void UKnightAIContextComponent::ClearControlledPawn()
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
			&UKnightAIContextComponent::HandlePawnControllerChanged);
		ControlledPawn->OnEndPlay.RemoveDynamic(this, &UKnightAIContextComponent::HandleObservedEndPlay);
	}
	AbilityEndedHandle.Reset();
	LifeHandle.Reset();
	LeaseHandle.Reset();
	ControlledPawn.Reset();
	AbilitySystem.Reset();
	Life.Reset();
	Threat.Reset();
	Director.Reset();
	PawnId = FKnightEntityId();
	Target.Reset();
	TargetId = FKnightEntityId();
	FocusTarget.Reset();
	FormationLeader.Reset();
	PerceivedActors.Reset();
	CooldownEnds.Reset();
	ActionSet = nullptr;
	ActionSetId = FPrimaryAssetId();
	Command = EKnightCompanionCommand::Follow;
	bTargetFriendlyDowned = false;
	if (!bEndingPlay)
	{
		OnDecisionChanged.Broadcast();
	}
}

FKnightResult UKnightAIContextComponent::ValidateActionSet(const FPrimaryAssetId& InActionSetId) const
{
	const FKnightResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess())
	{
		return ContextResult;
	}
	if (!KnightIdentity::IsValidDefinitionId(InActionSetId) ||
		InActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAIActionSet")))
	{
		return KnightAIFailure(EKnightError::InvalidId, TEXT("AIActionSetId"));
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Loaded = nullptr;
	const FKnightResult Resolve = Manager != nullptr
		? Manager->ResolveDefinition(InActionSetId, UKnightAIActionSetDefinition::StaticClass(), Loaded)
		: KnightAIFailure(EKnightError::InvalidState, TEXT("AIAssetManager"));
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	const UKnightAIActionSetDefinition* Set = Cast<UKnightAIActionSetDefinition>(Loaded);
	const FKnightResult Validate = Set->ValidateActionSet();
	if (!Validate.IsSuccess())
	{
		return Validate;
	}
	for (const FKnightAIActionEntry& Entry : Set->GetActions())
	{
		FGameplayAbilitySpecHandle Spec;
		FGameplayTag Input;
		const FKnightResult Granted = ResolveActionSpec(Entry.ActionTag, Spec, Input);
		if (!Granted.IsSuccess())
		{
			return Granted;
		}
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::SetActionSet(const FPrimaryAssetId& InActionSetId)
{
	const FKnightResult Validate = ValidateActionSet(InActionSetId);
	if (!Validate.IsSuccess())
	{
		return Validate;
	}
	if (ActionSetId == InActionSetId && IsValid(ActionSet))
	{
		return FKnightResult::Success();
	}
	UKnightDefinition* Loaded = nullptr;
	const FKnightResult Resolve = UKnightAssetManager::GetKnightAssetManager()->ResolveDefinition(InActionSetId,
		UKnightAIActionSetDefinition::StaticClass(), Loaded);
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	CancelOwnedAction();
	const FKnightResult StillOwned = ValidateContext();
	if (!StillOwned.IsSuccess())
	{
		return StillOwned;
	}
	ActionSet = Cast<UKnightAIActionSetDefinition>(Loaded);
	ActionSetId = InActionSetId;
	// Stable keys retain actual selection delays when a boss changes its choice set.
	OnDecisionChanged.Broadcast();
	return FKnightResult::Success();
}

FPrimaryAssetId UKnightAIContextComponent::GetActionSetId() const
{
	return ActionSetId;
}

APawn* UKnightAIContextComponent::GetControlledPawn() const
{
	return ControlledPawn.Get();
}

UKnightAbilitySystemComponent* UKnightAIContextComponent::GetAbilitySystem() const
{
	return AbilitySystem.Get();
}

FKnightResult UKnightAIContextComponent::ValidateTarget(AActor* InTarget, bool bFriendlyDowned) const
{
	const FKnightResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess())
	{
		return ContextResult;
	}
	UKnightAbilitySystemComponent* TargetASC = KnightAIContext::ResolveASC(InTarget);
	if (!IsValid(InTarget) || InTarget == ControlledPawn.Get() || InTarget->GetWorld() != GetWorld() ||
		InTarget->IsActorBeingDestroyed() || !InTarget->HasAuthority() || TargetASC == nullptr ||
		TargetASC->GetOwner() != InTarget || TargetASC->GetAvatarActor() != InTarget ||
		!TargetASC->GetEntityId().IsValid() || TargetASC->GetEntityId() == PawnId ||
		TargetASC->GetLifeComponent() == nullptr)
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AITargetIdentity"));
	}
	const bool bEligible = bFriendlyDowned
		? AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
			TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
			TargetASC->GetLifeComponent()->IsDowned()
		: KnightAIContext::IsHostile(AbilitySystem.Get(), TargetASC) && TargetASC->GetLifeComponent()->IsAlive();
	return bEligible ? FKnightResult::Success()
		: KnightAIFailure(EKnightError::InvalidState, TEXT("AITargetRoleOrLife"));
}

FKnightResult UKnightAIContextComponent::SetValidatedTarget(AActor* InTarget, bool bFriendlyDowned)
{
	const FKnightResult Result = ValidateTarget(InTarget, bFriendlyDowned);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightAbilitySystemComponent* TargetASC = KnightAIContext::ResolveASC(InTarget);
	if (Target.Get() == InTarget && TargetId == TargetASC->GetEntityId() &&
		bTargetFriendlyDowned == bFriendlyDowned)
	{
		return FKnightResult::Success();
	}
	CancelOwnedAction();
	if (!ValidateTarget(InTarget, bFriendlyDowned).IsSuccess())
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("AITargetChangedDuringCancel"));
	}
	Target = InTarget;
	TargetId = TargetASC->GetEntityId();
	bTargetFriendlyDowned = bFriendlyDowned;
	OnDecisionChanged.Broadcast();
	return FKnightResult::Success();
}

void UKnightAIContextComponent::ClearTarget()
{
	CancelOwnedAction();
	Target.Reset();
	TargetId = FKnightEntityId();
	bTargetFriendlyDowned = false;
	if (!bEndingPlay)
	{
		OnDecisionChanged.Broadcast();
	}
}

FKnightResult UKnightAIContextComponent::SetCompanionCommand(EKnightCompanionCommand InCommand,
	AActor* InFocusTarget)
{
	const FKnightResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) ||
		static_cast<uint8>(InCommand) > static_cast<uint8>(EKnightCompanionCommand::Regroup))
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AICompanionCommand"));
	}
	if (InCommand == EKnightCompanionCommand::Focus)
	{
		const FKnightResult TargetResult = ValidateTarget(InFocusTarget, false);
		if (!TargetResult.IsSuccess())
		{
			return TargetResult;
		}
	}
	CancelOwnedAction();
	if (!ValidateContext().IsSuccess())
	{
		return KnightAIFailure(EKnightError::NotOwned, TEXT("AICommandChangedDuringCancel"));
	}
	Command = InCommand;
	FocusTarget = InCommand == EKnightCompanionCommand::Focus ? InFocusTarget : nullptr;
	HoldLocation = ControlledPawn->GetActorLocation();
	if (InCommand == EKnightCompanionCommand::Regroup)
	{
		ClearTarget();
	}
	RefreshDecisionContext();
	OnDecisionChanged.Broadcast();
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::SetFormationLeader(AActor* Leader, const FVector& LocalOffset,
	float Acceptance)
{
	const FKnightResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (LocalOffset.ContainsNaN() || LocalOffset.SizeSquared() > FMath::Square(10000.0) ||
		!FMath::IsFinite(Acceptance) || Acceptance < 1.0f || Acceptance > 2000.0f ||
		(Leader != nullptr && (!IsValid(Leader) || Leader == ControlledPawn.Get() || Leader->GetWorld() != GetWorld())))
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIFormationLeader"));
	}
	FormationLeader = Leader;
	FormationOffset = LocalOffset;
	FormationAcceptance = Acceptance;
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::SetPerceivedActor(AActor* Actor, bool bSensed)
{
	const FKnightResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightAbilitySystemComponent* ActorASC = KnightAIContext::ResolveASC(Actor);
	if (ActorASC == nullptr || Actor->GetWorld() != GetWorld() || !ActorASC->GetEntityId().IsValid())
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIPerceptionActor"));
	}
	if (!bSensed)
	{
		PerceivedActors.Remove(ActorASC->GetEntityId());
		return FKnightResult::Success();
	}
	const FKnightResult TargetResult = ValidateTarget(Actor, false);
	if (!TargetResult.IsSuccess())
	{
		return TargetResult;
	}
	if (PerceivedActors.Num() >= 64 && !PerceivedActors.Contains(ActorASC->GetEntityId()))
	{
		return KnightAIFailure(EKnightError::CapacityExceeded, TEXT("AIPerceptionCapacity"));
	}
	const TWeakObjectPtr<AActor>* Existing = PerceivedActors.Find(ActorASC->GetEntityId());
	if (Existing != nullptr && Existing->IsValid() && Existing->Get() != Actor)
	{
		return KnightAIFailure(EKnightError::DuplicateId, TEXT("AIPerceptionIdentity"));
	}
	PerceivedActors.Add(ActorASC->GetEntityId(), Actor);
	if (Threat.IsValid())
	{
		return Threat->ObserveTarget(Actor);
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::SetDecisionInterval(float InInterval)
{
	const FKnightResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FMath::IsFinite(InInterval) || InInterval < 0.05f || InInterval > 2.0f)
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIDecisionInterval"));
	}
	DecisionInterval = InInterval;
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::SetAttackLeaseDuration(float InDuration)
{
	if (!FMath::IsFinite(InDuration) || InDuration < 0.1f || InDuration > 60.0f)
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIAttackLeaseDuration"));
	}
	const FKnightResult Result = ValidateContext();
	if (Result.IsSuccess())
	{
		AttackLeaseDuration = InDuration;
	}
	return Result;
}

float UKnightAIContextComponent::GetBaseDecisionInterval() const
{
	return DecisionInterval;
}

FKnightResult UKnightAIContextComponent::SetScheduledDecisionInterval(float InInterval)
{
	const FKnightResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FMath::IsFinite(InInterval) || InInterval < 0.05f || InInterval > 2.0f)
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIScheduledDecisionInterval"));
	}
	ScheduledDecisionInterval = InInterval;
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::SetRepositionQuery(UEnvQuery* InQuery)
{
	if (!IsValid(InQuery) || InQuery->GetOptions().IsEmpty())
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIRepositionQuery"));
	}
	const FKnightResult Result = ValidateContext();
	if (Result.IsSuccess())
	{
		RepositionQuery = InQuery;
	}
	return Result;
}

UEnvQuery* UKnightAIContextComponent::GetRepositionQuery() const
{
	return RepositionQuery;
}

FKnightAIDecisionSnapshot UKnightAIContextComponent::GetDecisionSnapshot() const
{
	FKnightAIDecisionSnapshot Snapshot;
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
	Snapshot.bStaggered = AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Staggered);
	Snapshot.Stamina = AbilitySystem->GetKnightAttributes()->GetStamina();
	Snapshot.RuneEnergy = AbilitySystem->GetKnightAttributes()->GetRuneEnergy();
	Snapshot.bActionRunning = ActiveActionToken.IsValid();
	Snapshot.ActiveActionToken = ActiveActionToken;
	Snapshot.ActiveActionTag = ActiveActionTag;
	Snapshot.bHoldPosition = Command == EKnightCompanionCommand::Hold;
	if (FormationLeader.IsValid() && !FormationLeader->IsActorBeingDestroyed())
	{
		Snapshot.FormationGoal = FormationLeader->GetActorLocation() +
			FRotator(0.0, FormationLeader->GetActorRotation().Yaw, 0.0).RotateVector(FormationOffset);
		Snapshot.bHasFormationGoal = !Snapshot.FormationGoal.ContainsNaN();
	}
	if (Snapshot.bHoldPosition)
	{
		Snapshot.FormationGoal = HoldLocation;
		Snapshot.bHasFormationGoal = true;
	}
	Snapshot.bNeedsRegroup = Command == EKnightCompanionCommand::Regroup && Snapshot.bHasFormationGoal &&
		FVector::DistSquared2D(ControlledPawn->GetActorLocation(), Snapshot.FormationGoal) >
			FMath::Square(FormationAcceptance);
	if (Target.IsValid() && ValidateTarget(Target.Get(), bTargetFriendlyDowned).IsSuccess() &&
		KnightAIContext::ResolveASC(Target.Get())->GetEntityId() == TargetId)
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
		FKnightAIActionEntry Selection;
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

void UKnightAIContextComponent::RefreshDecisionContext()
{
	if (!ValidateContext().IsSuccess() || !Life->IsAlive())
	{
		return;
	}
	if (Target.IsValid() && ValidateTarget(Target.Get(), bTargetFriendlyDowned).IsSuccess() &&
		KnightAIContext::ResolveASC(Target.Get())->GetEntityId() == TargetId && ActiveActionToken.IsValid())
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
	FKnightEntityId BestDownedId;
	if (AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight))
	{
		for (TActorIterator<APawn> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			APawn* Candidate = *Iterator;
			if (!ValidateTarget(Candidate, true).IsSuccess())
			{
				continue;
			}
			UKnightAbilitySystemComponent* CandidateASC = KnightAIContext::ResolveASC(Candidate);
			UKnightLifeComponent* CandidateLife = CandidateASC->GetLifeComponent();
			if (CandidateLife->IsReviveInProgress() && CandidateLife->GetActiveReviver() != ControlledPawn.Get())
			{
				continue;
			}
			const double Distance = FVector::DistSquared(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
			if ((Command == EKnightCompanionCommand::Hold &&
				Distance > FMath::Square(CandidateLife->GetReviveRange())) || Distance > BestDownedDistance)
			{
				continue;
			}
			if (Distance < BestDownedDistance || !BestDownedId.IsValid() ||
				KnightAIContext::ComesBefore(CandidateASC->GetEntityId(), BestDownedId))
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
	if (Command == EKnightCompanionCommand::Focus && ValidateTarget(FocusTarget.Get(), false).IsSuccess())
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
	FKnightEntityId BestId = Best != nullptr ? KnightAIContext::ResolveASC(Best)->GetEntityId() : FKnightEntityId();
	if (Best == nullptr)
	{
		for (auto Iterator = PerceivedActors.CreateIterator(); Iterator; ++Iterator)
		{
			AActor* Candidate = Iterator.Value().Get();
			if (!ValidateTarget(Candidate, false).IsSuccess() ||
				KnightAIContext::ResolveASC(Candidate)->GetEntityId() != Iterator.Key())
			{
				Iterator.RemoveCurrent();
				continue;
			}
			const double Distance = FVector::DistSquared(ControlledPawn->GetActorLocation(), Candidate->GetActorLocation());
			if (Distance < BestDistance || (Distance == BestDistance &&
				(!BestId.IsValid() || KnightAIContext::ComesBefore(Iterator.Key(), BestId))))
			{
				Best = Candidate;
				BestId = Iterator.Key();
				BestDistance = Distance;
			}
		}
	}
	if (Best != nullptr && (Command != EKnightCompanionCommand::Hold || BestDistance <= FMath::Square(500.0)))
	{
		SetValidatedTarget(Best);
	}
	else if (TargetId.IsValid())
	{
		ClearTarget();
	}
}

FKnightAIActionSelectionContext UKnightAIContextComponent::MakeSelectionContext() const
{
	FKnightAIActionSelectionContext Selection;
	if (!AbilitySystem.IsValid() || !ControlledPawn.IsValid() || !Target.IsValid())
	{
		return Selection;
	}
	Selection.Distance = FVector::Distance(ControlledPawn->GetActorLocation(), Target->GetActorLocation());
	Selection.Stamina = AbilitySystem->GetKnightAttributes()->GetStamina();
	Selection.RuneEnergy = AbilitySystem->GetKnightAttributes()->GetRuneEnergy();
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

FKnightResult UKnightAIContextComponent::TrySelectAction(FKnightAIActionEntry& OutAction) const
{
	OutAction = FKnightAIActionEntry();
	const FKnightResult Result = ValidateContext();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!Life->IsAlive() || ActiveActionToken.IsValid() || bTargetFriendlyDowned ||
		!ValidateTarget(Target.Get(), false).IsSuccess() || !IsValid(ActionSet) ||
		GetDecisionSnapshot().bNeedsRegroup)
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("AISelectActionState"));
	}
	return ActionSet->TrySelectAction(MakeSelectionContext(), OutAction);
}

FKnightResult UKnightAIContextComponent::ResolveActionSpec(FGameplayTag ActionTag,
	FGameplayAbilitySpecHandle& OutSpec, FGameplayTag& OutInput) const
{
	OutSpec = FGameplayAbilitySpecHandle();
	OutInput = FGameplayTag();
	if (!AbilitySystem.IsValid() || !ActionTag.IsValid())
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIActionSpec"));
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability == nullptr || !Spec.Ability->GetAssetTags().HasTagExact(ActionTag))
		{
			continue;
		}
		if (OutSpec.IsValid())
		{
			return KnightAIFailure(EKnightError::Conflict, TEXT("AIActionAmbiguousGrant"));
		}
		for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
		{
			if (Tag.ToString().StartsWith(TEXT("Input.")))
			{
				if (OutInput.IsValid())
				{
					return KnightAIFailure(EKnightError::Conflict, TEXT("AIActionAmbiguousInput"));
				}
				OutInput = Tag;
			}
		}
		OutSpec = Spec.Handle;
	}
	if (!OutSpec.IsValid() || !OutInput.IsValid())
	{
		return KnightAIFailure(EKnightError::NotFound, TEXT("AIActionNotGranted"));
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Handle != OutSpec && Spec.GetDynamicSpecSourceTags().HasTagExact(OutInput))
		{
			return KnightAIFailure(EKnightError::Conflict, TEXT("AIActionInputSharedByDifferentSpec"));
		}
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::TryBeginAction(const FKnightAIActionEntry& Action,
	bool bUseAttackLease, FGuid& OutActionToken)
{
	OutActionToken.Invalidate();
	FKnightAIActionEntry Selected;
	const FKnightResult Result = TrySelectAction(Selected);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Action.ActionKey != Selected.ActionKey || Action.ActionTag != Selected.ActionTag)
	{
		return KnightAIFailure(EKnightError::Conflict, TEXT("AIActionSelectionChanged"));
	}
	return BeginActionSpec(Selected.ActionTag, Selected.ActionKey, Selected.CooldownSeconds,
		bUseAttackLease, OutActionToken);
}

bool UKnightAIContextComponent::HasSpecialization() const
{
	const UKnightSkillRuntimeComponent* Runtime = ControlledPawn.IsValid()
		? ControlledPawn->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
	return Runtime != nullptr && !Runtime->GetBuild().SpecId.IsNone();
}

FKnightResult UKnightAIContextComponent::SelectSpecializationAction(int32& OutSlot, AActor*& OutTarget) const
{
	OutSlot = 0;
	OutTarget = nullptr;
	const UKnightSkillRuntimeComponent* Runtime = ControlledPawn.IsValid()
		? ControlledPawn->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
	if (Runtime == nullptr || Runtime->GetBuild().SpecId.IsNone())
	{
		return KnightAIFailure(EKnightError::NotFound, TEXT("AISpecialization"));
	}
	const FKnightSkillRuntimeState State = Runtime->GetSkillState();
	const EKnightSkillRole Role = FKnightSkillCatalog::Role(State.Build.SpecId);
	TArray<int32> Priority;
	AActor* Ally = Runtime->SelectLowestHealthAlly(1200.0f);
	if (Role == EKnightSkillRole::Healer && Ally != nullptr)
	{
		Priority = { 4, 2, 3, 1 };
	}
	else if (Role == EKnightSkillRole::Tank)
	{
		Priority = State.Shield < 20.0f ? TArray<int32> { 4, 3, 2, 1 } : TArray<int32> { 4, 2, 1, 3 };
	}
	else if (State.Build.SpecId == TEXT("Pluto"))
	{
		const UKnightSkillRuntimeComponent* EnemyRuntime = Target.IsValid()
			? Target->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
		const bool bCanShatter = EnemyRuntime != nullptr && EnemyRuntime->GetSkillState().Chilled >= 3;
		Priority = bCanShatter ? TArray<int32> { 4, 2, 3, 1 } : TArray<int32> { 4, 3, 1, 2 };
	}
	else
	{
		Priority = State.Stacks >= 2 ? TArray<int32> { 4, 2, 1, 3 } : TArray<int32> { 4, 1, 3, 2 };
	}
	for (int32 Slot : Priority)
	{
		const FKnightSpecActionDefinition D = FKnightSkillCatalog::GetSkill(State.Build, Slot);
		AActor* Candidate = D.bFriendly ? Ally != nullptr ? Ally : ControlledPawn.Get() : Target.Get();
		if (Role == EKnightSkillRole::Healer && Ally == nullptr && Slot != 1)
		{
			continue;
		}
		if (Runtime->ValidateSkill(Slot, Candidate).IsSuccess())
		{
			OutSlot = Slot;
			OutTarget = Candidate;
			return FKnightResult::Success();
		}
	}
	return KnightAIFailure(EKnightError::Conflict, TEXT("AISkillCooldownOrRange"));
}

FKnightResult UKnightAIContextComponent::TryBeginSpecializationAction(bool bUseAttackLease, FGuid& OutActionToken)
{
	OutActionToken.Invalidate();
	const FKnightResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess() || !Life->IsAlive() || ActiveActionToken.IsValid() || bStartingAction)
	{
		return ContextResult.IsSuccess() ? KnightAIFailure(EKnightError::Conflict, TEXT("AISkillRunning")) : ContextResult;
	}
	int32 Slot = 0;
	AActor* SkillTarget = nullptr;
	const FKnightResult Selection = SelectSpecializationAction(Slot, SkillTarget);
	if (!Selection.IsSuccess())
	{
		return Selection;
	}
	UKnightSkillRuntimeComponent* Runtime = ControlledPawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	const FKnightSpecActionDefinition D = FKnightSkillCatalog::GetSkill(Runtime->GetBuild(), Slot);
	const uint64 StartingGeneration = OwnershipGeneration;
	const TWeakObjectPtr<UKnightAbilitySystemComponent> StartingASC = AbilitySystem;
	const TWeakObjectPtr<APawn> StartingPawn = ControlledPawn;
	const TWeakObjectPtr<UKnightCombatDirectorSubsystem> StartingDirector = Director;
	const FKnightEntityId StartingPawnId = PawnId;
	const TWeakObjectPtr<UKnightSkillRuntimeComponent> StartingRuntime = Runtime;
	TGuardValue<bool> StartingGuard(bStartingAction, true);
	FKnightAttackLease Lease;
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
			return KnightAIFailure(EKnightError::InvalidState, TEXT("AISkillDirector"));
		}
		const FKnightResult LeaseResult = StartingDirector->TryAcquireAttackToken(
			StartingPawn.Get(), SkillTarget, AttackLeaseDuration, Lease);
		if (!LeaseResult.IsSuccess())
		{
			return LeaseResult;
		}
	}
	if (OwnershipGeneration != StartingGeneration || ControlledPawn != StartingPawn || AbilitySystem != StartingASC
		|| PawnId != StartingPawnId || !StartingRuntime.IsValid() || !ValidateContext().IsSuccess())
	{
		return KnightAIFailure(EKnightError::Cancelled, TEXT("AISkillOwnershipChangedBeforeActivation"));
	}
	ActiveActionToken = FGuid::NewGuid();
	OutActionToken = ActiveActionToken;
	const FGuid StartedToken = ActiveActionToken;
	ActiveSpec = StartingRuntime->GetSkillHandle(Slot);
	ActiveLease = Lease;
	bLeaseTransferred = true;
	LastActionToken.Invalidate();
	LastActionStatus = EKnightAIActionStatus::Unknown;
	const FKnightResult Result = StartingRuntime->RequestSkill(Slot, SkillTarget);
	if (OwnershipGeneration != StartingGeneration || ControlledPawn != StartingPawn || AbilitySystem != StartingASC
		|| PawnId != StartingPawnId || !ValidateContext().IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EKnightAIActionStatus::Cancelled, true);
		}
		return KnightAIFailure(EKnightError::Cancelled, TEXT("AISkillOwnershipChangedDuringActivation"));
	}
	if (!Result.IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EKnightAIActionStatus::Failed, true);
		}
		return Result;
	}
	if (LastActionToken == StartedToken && LastActionStatus == EKnightAIActionStatus::Cancelled)
	{
		return KnightAIFailure(EKnightError::Cancelled, TEXT("AISkillCancelledDuringActivation"));
	}
	OnDecisionChanged.Broadcast();
	return FKnightResult::Success();
}

FKnightResult UKnightAIContextComponent::TryBeginRevive(FGuid& OutActionToken)
{
	OutActionToken.Invalidate();
	const FKnightResult Result = ValidateTarget(Target.Get(), true);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!bTargetFriendlyDowned)
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("AIReviveTarget"));
	}
	const FKnightResult Source = KnightAIContext::ResolveASC(Target.Get())->GetLifeComponent()->
		ValidateReviveSource(ControlledPawn.Get());
	if (!Source.IsSuccess())
	{
		return Source;
	}
	return BeginActionSpec(KnightGameplayTags::Ability_Party_Revive, TEXT("Revive"), 0.0f, false, OutActionToken);
}

FKnightResult UKnightAIContextComponent::BeginActionSpec(FGameplayTag ActionTag, FName ActionKey,
	float Cooldown, bool bUseAttackLease, FGuid& OutActionToken)
{
	const FKnightResult ContextResult = ValidateContext();
	if (!ContextResult.IsSuccess())
	{
		return ContextResult;
	}
	if (!GetWorld()->HasBegunPlay())
	{
		return KnightAIFailure(EKnightError::InvalidState, TEXT("AIActionBeforeBeginPlay"));
	}
	if (!Life->IsAlive() || ActiveActionToken.IsValid() || bStartingAction)
	{
		return KnightAIFailure(EKnightError::Conflict, TEXT("AIActionAlreadyRunning"));
	}
	const uint64 StartingGeneration = OwnershipGeneration;
	const TWeakObjectPtr<UKnightAbilitySystemComponent> StartingASC = AbilitySystem;
	const TWeakObjectPtr<APawn> StartingPawn = ControlledPawn;
	const TWeakObjectPtr<UKnightCombatDirectorSubsystem> StartingDirector = Director;
	const FKnightEntityId StartingPawnId = PawnId;
	TGuardValue<bool> StartingGuard(bStartingAction, true);
	FKnightAttackLease PendingLease;
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
	const FKnightResult Resolve = ResolveActionSpec(ActionTag, SpecHandle, Input);
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(SpecHandle);
	if (Spec == nullptr || Spec->IsActive())
	{
		return KnightAIFailure(EKnightError::Conflict, TEXT("AIActionSpecInUse"));
	}
	if (bUseAttackLease)
	{
		if (!Director.IsValid())
		{
			return KnightAIFailure(EKnightError::InvalidState, TEXT("AICombatDirectorMissing"));
		}
		const FKnightResult Lease = StartingDirector->TryAcquireAttackToken(StartingPawn.Get(), Target.Get(),
			AttackLeaseDuration, PendingLease);
		if (!Lease.IsSuccess())
		{
			return Lease;
		}
		if (OwnershipGeneration != StartingGeneration || !ValidateContext().IsSuccess())
		{
			return KnightAIFailure(EKnightError::Cancelled, TEXT("AIActionOwnershipChangedBeforeActivation"));
		}
	}
	UKnightCombatComponent* Combat = ControlledPawn->FindComponentByClass<UKnightCombatComponent>();
	if (!bTargetFriendlyDowned && Combat != nullptr)
	{
		const FKnightResult TargetResult = Combat->SetValidatedTarget(Target.Get());
		if (!TargetResult.IsSuccess())
		{
			return TargetResult;
		}
	}
	if (OwnershipGeneration != StartingGeneration || !ValidateContext().IsSuccess())
	{
		return KnightAIFailure(EKnightError::Cancelled, TEXT("AIActionOwnershipChangedBeforeActivation"));
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
	LastActionStatus = EKnightAIActionStatus::Unknown;
	const FKnightResult Activate = StartingASC->RequestAbilityInput(Input, true, Target.Get());
	if (OwnershipGeneration != StartingGeneration || ControlledPawn != StartingPawn ||
		AbilitySystem != StartingASC || PawnId != StartingPawnId || !ValidateContext().IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EKnightAIActionStatus::Cancelled, true);
		}
		return KnightAIFailure(EKnightError::Cancelled, TEXT("AIActionOwnershipChangedDuringActivation"));
	}
	if (!Activate.IsSuccess())
	{
		if (ActiveActionToken == StartedToken)
		{
			FinishOwnedAction(EKnightAIActionStatus::Failed, true);
		}
		else if (LastActionToken == StartedToken)
		{
			LastActionStatus = EKnightAIActionStatus::Failed;
		}
		return Activate;
	}
	if (LastActionToken == StartedToken && LastActionStatus == EKnightAIActionStatus::Cancelled)
	{
		return KnightAIFailure(EKnightError::Cancelled, TEXT("AIActionCancelledDuringActivation"));
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
			FinishOwnedAction(EKnightAIActionStatus::Failed, false);
			return KnightAIFailure(EKnightError::InvalidState, TEXT("AIActionDidNotActivate"));
		}
	}
	OnDecisionChanged.Broadcast();
	return FKnightResult::Success();
}

void UKnightAIContextComponent::FinishOwnedAction(EKnightAIActionStatus Status, bool bCancelSpec)
{
	if (!ActiveActionToken.IsValid())
	{
		return;
	}
	const FGuid FinishedToken = ActiveActionToken;
	const FGameplayAbilitySpecHandle FinishedSpec = ActiveSpec;
	const FKnightAttackLease Lease = ActiveLease;
	const TWeakObjectPtr<UKnightAbilitySystemComponent> FinishedASC = AbilitySystem;
	const TWeakObjectPtr<APawn> FinishedPawn = ControlledPawn;
	const TWeakObjectPtr<UKnightCombatDirectorSubsystem> FinishedDirector = Director;
	const FKnightEntityId FinishedPawnId = PawnId;
	const uint64 FinishedGeneration = OwnershipGeneration;
	ActiveActionToken.Invalidate();
	ActiveSpec = FGameplayAbilitySpecHandle();
	ActiveInput = FGameplayTag();
	ActiveActionTag = FGameplayTag();
	ActiveLease = FKnightAttackLease();
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

FKnightResult UKnightAIContextComponent::CancelAction(const FGuid& ActionToken)
{
	if (!ActionToken.IsValid())
	{
		return KnightAIFailure(EKnightError::InvalidId, TEXT("CancelAIAction"));
	}
	if (ActionToken == LastActionToken && !ActiveActionToken.IsValid())
	{
		return FKnightResult::Success();
	}
	if (ActionToken != ActiveActionToken)
	{
		return KnightAIFailure(EKnightError::NotOwned, TEXT("CancelAIActionCapability"));
	}
	FinishOwnedAction(EKnightAIActionStatus::Cancelled, true);
	return FKnightResult::Success();
}

void UKnightAIContextComponent::CancelOwnedAction()
{
	FinishOwnedAction(EKnightAIActionStatus::Cancelled, true);
}

EKnightAIActionStatus UKnightAIContextComponent::GetActionStatus(const FGuid& ActionToken) const
{
	if (ActionToken.IsValid() && ActionToken == ActiveActionToken)
	{
		return EKnightAIActionStatus::Running;
	}
	return ActionToken.IsValid() && ActionToken == LastActionToken ? LastActionStatus : EKnightAIActionStatus::Unknown;
}

FKnightAttackLease UKnightAIContextComponent::GetActiveAttackLease() const
{
	return ActiveLease;
}

TArray<int32> UKnightAIContextComponent::GetActiveEQSRequestIds() const
{
	TArray<int32> Requests = ActiveEQSRequests.Array();
	Requests.Sort();
	return Requests;
}

int32 UKnightAIContextComponent::GetCompletedEQSRequestCount() const
{
	return CompletedEQSRequests;
}

int32 UKnightAIContextComponent::GetAbortedEQSRequestCount() const
{
	return AbortedEQSRequests;
}

void UKnightAIContextComponent::TrackEQSRequest(int32 QueryId)
{
	if (QueryId != INDEX_NONE)
	{
		ActiveEQSRequests.Add(QueryId);
	}
}

void UKnightAIContextComponent::FinishEQSRequest(int32 QueryId, bool bAborted)
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

void UKnightAIContextComponent::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	if (ActiveActionToken.IsValid() && Data.AbilitySpecHandle == ActiveSpec)
	{
		FinishOwnedAction(Data.bWasCancelled ? EKnightAIActionStatus::Cancelled : EKnightAIActionStatus::Succeeded, false);
	}
}

void UKnightAIContextComponent::HandlePawnLifeChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	if (Current != EKnightLifeState::Alive)
	{
		CancelOwnedAction();
		Target.Reset();
		TargetId = FKnightEntityId();
	}
	if (!bEndingPlay)
	{
		OnDecisionChanged.Broadcast();
	}
}

void UKnightAIContextComponent::HandleLeaseReleased(const FKnightAttackLease& Lease,
	EKnightAttackLeaseReleaseReason Reason)
{
	if (ActiveLease.IsValid() && Lease == ActiveLease)
	{
		ActiveLease = FKnightAttackLease();
		FinishOwnedAction(EKnightAIActionStatus::Cancelled, true);
	}
}

void UKnightAIContextComponent::HandlePawnControllerChanged(APawn* Pawn, AController* Previous,
	AController* Current)
{
	if (Pawn == ControlledPawn.Get() && Current != GetOwner())
	{
		ClearControlledPawn();
	}
}

void UKnightAIContextComponent::HandleObservedEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (Actor == ControlledPawn.Get())
	{
		ClearControlledPawn();
	}
}

void UKnightAIContextComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ClearControlledPawn();
	RepositionQuery = nullptr;
	Super::EndPlay(EndPlayReason);
}
