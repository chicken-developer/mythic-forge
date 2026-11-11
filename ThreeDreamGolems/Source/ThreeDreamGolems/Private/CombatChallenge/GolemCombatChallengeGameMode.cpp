#include "CombatChallenge/GolemCombatChallengeGameMode.h"

#include "CombatChallenge/GolemCombatChallengeHUD.h"
#include "CombatChallenge/GolemCombatChallengeState.h"
#include "CombatChallenge/GolemCombatFixture.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Finisher/GolemFinisherComponent.h"
#include "Party/GolemPartyState.h"
#include "Traversal/GolemTraversalComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemCombatChallenge, Log, All);

AGolemCombatChallengeGameMode::AGolemCombatChallengeGameMode()
{
	HUDClass = AGolemCombatChallengeHUD::StaticClass();
}

void AGolemCombatChallengeGameMode::InitGameState()
{
	Super::InitGameState();
	if (!HasAuthority() || !GetInitializationResult().IsSuccess() || GetPartyState() == nullptr)
	{
		CombatInitializationResult = GetInitializationResult();
		return;
	}
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		UGolemCombatComponent* Combat = Entry.Golem->FindComponentByClass<UGolemCombatComponent>();
		if (Combat == nullptr)
		{
			CombatInitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("ChallengeCombat"),
				NSLOCTEXT("GolemCombatChallenge", "Composition", "Canonical Golems require their native combat component."));
			break;
		}
		CombatInitializationResult = Combat->Configure(
			FPrimaryAssetId(TEXT("GolemCombatProfile"), TEXT("FoundationCombat")));
		if (!CombatInitializationResult.IsSuccess())
		{
			break;
		}
		CanonicalIds.Add(Entry.GolemId);
	}
	if (!CombatInitializationResult.IsSuccess())
	{
		UE_LOG(LogGolemCombatChallenge, Error, TEXT("Combat challenge initialization failed: %s."),
			*CombatInitializationResult.Message.ToString());
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	ChallengeState = GetWorld()->SpawnActor<AGolemCombatChallengeState>(Parameters);
	if (ChallengeState == nullptr)
	{
		CombatInitializationResult = FGolemResult::Failure(EGolemError::LoadFailed, TEXT("ChallengeState"),
			NSLOCTEXT("GolemCombatChallenge", "StateSpawn", "Combat challenge feedback could not spawn."));
	}
}

void AGolemCombatChallengeGameMode::BeginPlay()
{
	Super::BeginPlay();
	BindChallenge();
}

void AGolemCombatChallengeGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindChallenge();
	Super::EndPlay(EndPlayReason);
}

AGolemCombatChallengeState* AGolemCombatChallengeGameMode::GetChallengeState() const
{
	return ChallengeState;
}

const FGolemResult& AGolemCombatChallengeGameMode::GetCombatInitializationResult() const
{
	return CombatInitializationResult;
}

void AGolemCombatChallengeGameMode::BindChallenge()
{
	if (!HasAuthority() || ChallengeState == nullptr || GetPartyState() == nullptr)
	{
		return;
	}
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FGolemCombatChallengeBinding Binding;
		Binding.Combat = Entry.Golem->FindComponentByClass<UGolemCombatComponent>();
		Binding.Life = Entry.Golem->GetLifeComponent();
		if (Binding.Combat.IsValid())
		{
			Binding.OutcomeHandle = Binding.Combat->OnCombatOutcome.AddUObject(this, &ThisClass::HandleCombatOutcome);
		}
		const TWeakObjectPtr<AGolemCharacter> Golem = Entry.Golem.Get();
		Binding.LifeHandle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this, Golem](const EGolemLifeState Previous, const EGolemLifeState Current)
			{
				HandleGolemLifeChanged(Golem, Previous, Current);
			});
		Bindings.Add(Binding);
	}
	for (TActorIterator<AGolemCombatFixture> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		FGolemCombatChallengeBinding Binding;
		Binding.Combat = Iterator->GetCombatComponent();
		Binding.OutcomeHandle = Binding.Combat->OnCombatOutcome.AddUObject(this, &ThisClass::HandleCombatOutcome);
		Bindings.Add(Binding);
	}
	ChallengeState->CommitSnapshot();
}

void AGolemCombatChallengeGameMode::UnbindChallenge()
{
	for (const FGolemCombatChallengeBinding& Binding : Bindings)
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

void AGolemCombatChallengeGameMode::HandleCombatOutcome(const FGolemCombatOutcome& Outcome)
{
	if (bResettingChallenge || ChallengeState == nullptr || ChallengeState->Snapshot.bFailed
		|| !Outcome.ConfirmationId.IsValid() || RecordedConfirmations.Contains(Outcome.ConfirmationId))
	{
		return;
	}
	const bool bPartyAction = CanonicalIds.Contains(Outcome.SourceId);
	const bool bPartyStagger = Outcome.Outcome == EGolemCombatOutcome::Staggered
		&& CanonicalIds.Contains(Outcome.TargetId) && !bPartyAction;
	if (!bPartyAction && !bPartyStagger)
	{
		return;
	}
	RecordedConfirmations.Add(Outcome.ConfirmationId);
	FGolemCombatChallengeSnapshot& Snapshot = ChallengeState->Snapshot;
	switch (Outcome.Outcome)
	{
	case EGolemCombatOutcome::LightHit:
		++Snapshot.LightHitCount;
		break;
	case EGolemCombatOutcome::HeavyHit:
		++Snapshot.HeavyHitCount;
		break;
	case EGolemCombatOutcome::Combo:
		++Snapshot.ComboCount;
		break;
	case EGolemCombatOutcome::Blocked:
		++Snapshot.BlockCount;
		break;
	case EGolemCombatOutcome::Parried:
		++Snapshot.ParryCount;
		break;
	case EGolemCombatOutcome::Dodged:
		++Snapshot.DodgeCount;
		break;
	case EGolemCombatOutcome::TargetLocked:
		Snapshot.bTargetLocked = true;
		break;
	case EGolemCombatOutcome::Staggered:
		if (bPartyStagger)
		{
			++Snapshot.StaggerCount;
		}
		break;
	case EGolemCombatOutcome::RuneHit:
		++Snapshot.RuneHitCount;
		break;
	case EGolemCombatOutcome::Finisher:
		++Snapshot.FinisherCount;
		break;
	case EGolemCombatOutcome::Traversal:
		++Snapshot.TraversalCount;
		break;
	}
	ChallengeState->CommitSnapshot();
}

void AGolemCombatChallengeGameMode::HandleGolemLifeChanged(const TWeakObjectPtr<AGolemCharacter> Golem,
	const EGolemLifeState Previous, const EGolemLifeState Current)
{
	if (!bResettingChallenge && ChallengeState != nullptr && Golem.IsValid()
		&& Current == EGolemLifeState::Dead && Cast<APlayerController>(Golem->GetController()) != nullptr)
	{
		ChallengeState->Snapshot.bFailed = true;
		ChallengeState->CommitSnapshot();
	}
}

FGolemResult AGolemCombatChallengeGameMode::TryInteract(APlayerController* Player)
{
	if (HasAuthority() && IsValid(Player) && Player->GetWorld() == GetWorld() && GetPartyState() != nullptr
		&& GetPartyState()->GetControlledSlot(Player) != INDEX_NONE && IsValid(Player->GetPawn()))
	{
		AGolemCombatFixture* Closest = nullptr;
		double ClosestDistance = FMath::Square(220.0);
		for (TActorIterator<AGolemCombatFixture> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			const double Distance = FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Iterator->GetActorLocation());
			if (Iterator->GetFixtureKind() == EGolemCombatFixtureKind::ScriptedAttacker && Distance <= ClosestDistance)
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

FGolemResult AGolemCombatChallengeGameMode::TryRestartDrill(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !Player->IsLocalController() || GetPartyState() == nullptr
		|| Player->GetWorld() != GetWorld() || GetPartyState()->GetControlledSlot(Player) == INDEX_NONE)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("ChallengeRetry"),
			NSLOCTEXT("GolemCombatChallenge", "RetryHost", "The host must control a canonical Golem to retry."));
	}
	bResettingChallenge = true;
	ON_SCOPE_EXIT
	{
		bResettingChallenge = false;
	};
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (UGolemTraversalComponent* Traversal = Entry.Golem->FindComponentByClass<UGolemTraversalComponent>();
			Traversal != nullptr && Traversal->HasActiveTraversal())
		{
			Traversal->EndTraversal(Traversal->GetSnapshot().Handle, true);
		}
		if (UGolemFinisherComponent* Finisher = Entry.Golem->FindComponentByClass<UGolemFinisherComponent>();
			Finisher != nullptr && Finisher->HasActiveFinisher())
		{
			Finisher->EndFinisher(Finisher->GetReservation(), false);
		}
		if (UGolemCombatComponent* Combat = Entry.Golem->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->ClearTransientActions();
		}
	}
	const FGolemResult ParentReset = Super::TryRestartDrill(Player);
	if (!ParentReset.IsSuccess())
	{
		return ParentReset;
	}
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		UGolemCombatComponent* Combat = Entry.Golem->FindComponentByClass<UGolemCombatComponent>();
		const FGolemResult Reset = Combat != nullptr ? Combat->ResetForCheckpoint()
			: FGolemResult::Failure(EGolemError::NotFound, TEXT("CombatReset"), FText::GetEmpty());
		if (!Reset.IsSuccess())
		{
			return Reset;
		}
	}
	for (TActorIterator<AGolemCombatFixture> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const FGolemResult Reset = Iterator->ResetForCheckpoint();
		if (!Reset.IsSuccess())
		{
			return Reset;
		}
	}
	RecordedConfirmations.Reset();
	if (ChallengeState != nullptr)
	{
		const int32 PreviousRevision = ChallengeState->Snapshot.Revision;
		ChallengeState->Snapshot = FGolemCombatChallengeSnapshot();
		ChallengeState->Snapshot.Revision = PreviousRevision;
		ChallengeState->CommitSnapshot();
	}
	UE_LOG(LogGolemCombatChallenge, Display, TEXT("Challenge reset: all GAS resources, fixtures and transient actions restored."));
	return FGolemResult::Success();
}
