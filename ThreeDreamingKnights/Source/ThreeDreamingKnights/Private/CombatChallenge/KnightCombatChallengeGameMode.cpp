#include "CombatChallenge/KnightCombatChallengeGameMode.h"

#include "CombatChallenge/KnightCombatChallengeHUD.h"
#include "CombatChallenge/KnightCombatChallengeState.h"
#include "CombatChallenge/KnightCombatFixture.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Finisher/KnightFinisherComponent.h"
#include "Party/KnightPartyState.h"
#include "Traversal/KnightTraversalComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightCombatChallenge, Log, All);

AKnightCombatChallengeGameMode::AKnightCombatChallengeGameMode()
{
	HUDClass = AKnightCombatChallengeHUD::StaticClass();
}

void AKnightCombatChallengeGameMode::InitGameState()
{
	Super::InitGameState();
	if (!HasAuthority() || !GetInitializationResult().IsSuccess() || GetPartyState() == nullptr)
	{
		CombatInitializationResult = GetInitializationResult();
		return;
	}
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		UKnightCombatComponent* Combat = Entry.Knight->FindComponentByClass<UKnightCombatComponent>();
		if (Combat == nullptr)
		{
			CombatInitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ChallengeCombat"),
				NSLOCTEXT("KnightCombatChallenge", "Composition", "Canonical Knights require their native combat component."));
			break;
		}
		CombatInitializationResult = Combat->Configure(
			FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat")));
		if (!CombatInitializationResult.IsSuccess())
		{
			break;
		}
		CanonicalIds.Add(Entry.KnightId);
	}
	if (!CombatInitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightCombatChallenge, Error, TEXT("Combat challenge initialization failed: %s."),
			*CombatInitializationResult.Message.ToString());
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	ChallengeState = GetWorld()->SpawnActor<AKnightCombatChallengeState>(Parameters);
	if (ChallengeState == nullptr)
	{
		CombatInitializationResult = FKnightResult::Failure(EKnightError::LoadFailed, TEXT("ChallengeState"),
			NSLOCTEXT("KnightCombatChallenge", "StateSpawn", "Combat challenge feedback could not spawn."));
	}
}

void AKnightCombatChallengeGameMode::BeginPlay()
{
	Super::BeginPlay();
	BindChallenge();
}

void AKnightCombatChallengeGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindChallenge();
	Super::EndPlay(EndPlayReason);
}

AKnightCombatChallengeState* AKnightCombatChallengeGameMode::GetChallengeState() const
{
	return ChallengeState;
}

const FKnightResult& AKnightCombatChallengeGameMode::GetCombatInitializationResult() const
{
	return CombatInitializationResult;
}

void AKnightCombatChallengeGameMode::BindChallenge()
{
	if (!HasAuthority() || ChallengeState == nullptr || GetPartyState() == nullptr)
	{
		return;
	}
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FKnightCombatChallengeBinding Binding;
		Binding.Combat = Entry.Knight->FindComponentByClass<UKnightCombatComponent>();
		Binding.Life = Entry.Knight->GetLifeComponent();
		if (Binding.Combat.IsValid())
		{
			Binding.OutcomeHandle = Binding.Combat->OnCombatOutcome.AddUObject(this, &ThisClass::HandleCombatOutcome);
		}
		const TWeakObjectPtr<AKnightCharacter> Knight = Entry.Knight.Get();
		Binding.LifeHandle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this, Knight](const EKnightLifeState Previous, const EKnightLifeState Current)
			{
				HandleKnightLifeChanged(Knight, Previous, Current);
			});
		Bindings.Add(Binding);
	}
	for (TActorIterator<AKnightCombatFixture> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		FKnightCombatChallengeBinding Binding;
		Binding.Combat = Iterator->GetCombatComponent();
		Binding.OutcomeHandle = Binding.Combat->OnCombatOutcome.AddUObject(this, &ThisClass::HandleCombatOutcome);
		Bindings.Add(Binding);
	}
	ChallengeState->CommitSnapshot();
}

void AKnightCombatChallengeGameMode::UnbindChallenge()
{
	for (const FKnightCombatChallengeBinding& Binding : Bindings)
	{
		if (Binding.Combat.IsValid())
		{
			Binding.Combat->OnCombatOutcome.Remove(Binding.OutcomeHandle);
		}
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.LifeHandle);
		}
	}
	Bindings.Reset();
}

void AKnightCombatChallengeGameMode::HandleCombatOutcome(const FKnightCombatOutcome& Outcome)
{
	if (bResettingChallenge || ChallengeState == nullptr || ChallengeState->Snapshot.bFailed
		|| !Outcome.ConfirmationId.IsValid() || RecordedConfirmations.Contains(Outcome.ConfirmationId))
	{
		return;
	}
	const bool bPartyAction = CanonicalIds.Contains(Outcome.SourceId);
	const bool bPartyStagger = Outcome.Outcome == EKnightCombatOutcome::Staggered
		&& CanonicalIds.Contains(Outcome.TargetId) && !bPartyAction;
	if (!bPartyAction && !bPartyStagger)
	{
		return;
	}
	RecordedConfirmations.Add(Outcome.ConfirmationId);
	FKnightCombatChallengeSnapshot& Snapshot = ChallengeState->Snapshot;
	switch (Outcome.Outcome)
	{
	case EKnightCombatOutcome::LightHit:
		++Snapshot.LightHitCount;
		break;
	case EKnightCombatOutcome::HeavyHit:
		++Snapshot.HeavyHitCount;
		break;
	case EKnightCombatOutcome::Combo:
		++Snapshot.ComboCount;
		break;
	case EKnightCombatOutcome::Blocked:
		++Snapshot.BlockCount;
		break;
	case EKnightCombatOutcome::Parried:
		++Snapshot.ParryCount;
		break;
	case EKnightCombatOutcome::Dodged:
		++Snapshot.DodgeCount;
		break;
	case EKnightCombatOutcome::TargetLocked:
		Snapshot.bTargetLocked = true;
		break;
	case EKnightCombatOutcome::Staggered:
		if (bPartyStagger)
		{
			++Snapshot.StaggerCount;
		}
		break;
	case EKnightCombatOutcome::RuneHit:
		++Snapshot.RuneHitCount;
		break;
	case EKnightCombatOutcome::Finisher:
		++Snapshot.FinisherCount;
		break;
	case EKnightCombatOutcome::Traversal:
		++Snapshot.TraversalCount;
		break;
	}
	ChallengeState->CommitSnapshot();
}

void AKnightCombatChallengeGameMode::HandleKnightLifeChanged(const TWeakObjectPtr<AKnightCharacter> Knight,
	const EKnightLifeState Previous, const EKnightLifeState Current)
{
	if (!bResettingChallenge && ChallengeState != nullptr && Knight.IsValid()
		&& Current == EKnightLifeState::Dead && Cast<APlayerController>(Knight->GetController()) != nullptr)
	{
		ChallengeState->Snapshot.bFailed = true;
		ChallengeState->CommitSnapshot();
	}
}

FKnightResult AKnightCombatChallengeGameMode::TryInteract(APlayerController* Player)
{
	if (HasAuthority() && IsValid(Player) && Player->GetWorld() == GetWorld() && GetPartyState() != nullptr
		&& GetPartyState()->GetControlledSlot(Player) != INDEX_NONE && IsValid(Player->GetPawn()))
	{
		AKnightCombatFixture* Closest = nullptr;
		double ClosestDistance = FMath::Square(220.0);
		for (TActorIterator<AKnightCombatFixture> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			const double Distance = FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Iterator->GetActorLocation());
			if (Iterator->GetFixtureKind() == EKnightCombatFixtureKind::ScriptedAttacker && Distance <= ClosestDistance)
			{
				ClosestDistance = Distance;
				Closest = *Iterator;
			}
		}
		if (Closest != nullptr)
		{
			return Closest->TryTelegraphAttack(Player->GetPawn());
		}
	}
	return Super::TryInteract(Player);
}

FKnightResult AKnightCombatChallengeGameMode::TryRestartDrill(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !Player->IsLocalController() || GetPartyState() == nullptr
		|| Player->GetWorld() != GetWorld() || GetPartyState()->GetControlledSlot(Player) == INDEX_NONE)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("ChallengeRetry"),
			NSLOCTEXT("KnightCombatChallenge", "RetryHost", "The host must control a canonical Knight to retry."));
	}
	bResettingChallenge = true;
	ON_SCOPE_EXIT
	{
		bResettingChallenge = false;
	};
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (UKnightTraversalComponent* Traversal = Entry.Knight->FindComponentByClass<UKnightTraversalComponent>();
			Traversal != nullptr && Traversal->HasActiveTraversal())
		{
			Traversal->EndTraversal(Traversal->GetSnapshot().Handle, true);
		}
		if (UKnightFinisherComponent* Finisher = Entry.Knight->FindComponentByClass<UKnightFinisherComponent>();
			Finisher != nullptr && Finisher->HasActiveFinisher())
		{
			Finisher->EndFinisher(Finisher->GetReservation(), false);
		}
		if (UKnightCombatComponent* Combat = Entry.Knight->FindComponentByClass<UKnightCombatComponent>())
		{
			Combat->ClearTransientActions();
		}
	}
	const FKnightResult ParentReset = Super::TryRestartDrill(Player);
	if (!ParentReset.IsSuccess())
	{
		return ParentReset;
	}
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		UKnightCombatComponent* Combat = Entry.Knight->FindComponentByClass<UKnightCombatComponent>();
		const FKnightResult Reset = Combat != nullptr ? Combat->ResetForCheckpoint()
			: FKnightResult::Failure(EKnightError::NotFound, TEXT("CombatReset"), FText::GetEmpty());
		if (!Reset.IsSuccess())
		{
			return Reset;
		}
	}
	for (TActorIterator<AKnightCombatFixture> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const FKnightResult Reset = Iterator->ResetForCheckpoint();
		if (!Reset.IsSuccess())
		{
			return Reset;
		}
	}
	RecordedConfirmations.Reset();
	if (ChallengeState != nullptr)
	{
		const int32 PreviousRevision = ChallengeState->Snapshot.Revision;
		ChallengeState->Snapshot = FKnightCombatChallengeSnapshot();
		ChallengeState->Snapshot.Revision = PreviousRevision;
		ChallengeState->CommitSnapshot();
	}
	UE_LOG(LogKnightCombatChallenge, Display, TEXT("Challenge reset: all GAS resources, fixtures and transient actions restored."));
	return FKnightResult::Success();
}
