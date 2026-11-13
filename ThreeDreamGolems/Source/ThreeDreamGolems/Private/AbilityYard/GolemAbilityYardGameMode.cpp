#include "AbilityYard/GolemAbilityYardGameMode.h"

#include "AbilityYard/GolemAbilityDummy.h"
#include "AbilityYard/GolemAbilityStation.h"
#include "AbilityYard/GolemAbilityYardHUD.h"
#include "AbilityYard/GolemAbilityYardState.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Party/GolemPartyState.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffectTypes.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemAbilityYard, Log, All);

AGolemAbilityYardGameMode::AGolemAbilityYardGameMode()
{
	HUDClass = AGolemAbilityYardHUD::StaticClass();
}

void AGolemAbilityYardGameMode::InitGameState()
{
	Super::InitGameState();
	if (HasAuthority() && GetInitializationResult().IsSuccess() && YardState == nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.Owner = this;
		YardState = GetWorld()->SpawnActor<AGolemAbilityYardState>(Parameters);
		if (YardState == nullptr)
		{
			UE_LOG(LogGolemAbilityYard, Error, TEXT("Ability yard objective state could not spawn."));
		}
	}
}

void AGolemAbilityYardGameMode::BeginPlay()
{
	Super::BeginPlay();
	BindExercise();
}

void AGolemAbilityYardGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindExercise();
	Super::EndPlay(EndPlayReason);
}

AGolemAbilityYardState* AGolemAbilityYardGameMode::GetYardState() const
{
	return YardState;
}

void AGolemAbilityYardGameMode::BindExercise()
{
	if (!HasAuthority() || YardState == nullptr || GetPartyState() == nullptr)
	{
		return;
	}
	for (TActorIterator<AGolemAbilityDummy> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (Dummy != nullptr)
		{
			UE_LOG(LogGolemAbilityYard, Error, TEXT("Ability yard requires exactly one authored dummy."));
			return;
		}
		Dummy = *Iterator;
	}
	if (Dummy == nullptr)
	{
		UE_LOG(LogGolemAbilityYard, Error, TEXT("Ability yard is missing its real GAS dummy."));
		return;
	}
	YardState->Dummy = Dummy;
	YardState->Snapshot.DummyHealth = Dummy->GetGolemAttributeSet()->GetHealth();
	YardState->Snapshot.DummyMaxHealth = Dummy->GetGolemAttributeSet()->GetMaxHealth();
	FGolemAbilityYardBinding DummyBinding;
	DummyBinding.AbilitySystem = Dummy->GetGolemAbilitySystem();
	DummyBinding.HealthHandle = Dummy->GetGolemAbilitySystem()->GetGameplayAttributeValueChangeDelegate(
		UGolemAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleDummyHealthChanged);
	Bindings.Add(DummyBinding);
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		const TWeakObjectPtr<AGolemCharacter> Golem = Entry.Golem.Get();
		FGolemAbilityYardBinding Binding;
		Binding.AbilitySystem = Entry.Golem->GetGolemAbilitySystem();
		Binding.Life = Entry.Golem->GetLifeComponent();
		Binding.HealthHandle = Entry.Golem->GetGolemAbilitySystem()->GetGameplayAttributeValueChangeDelegate(
			UGolemAttributeSet::GetHealthAttribute()).AddWeakLambda(this,
			[this, Golem](const FOnAttributeChangeData& Change)
			{
				HandleGolemHealthChanged(Golem, Change);
			});
		Binding.LifeHandle = Entry.Golem->GetLifeComponent()->OnLifeStateChanged.AddWeakLambda(this,
			[this, Golem](const EGolemLifeState Previous, const EGolemLifeState Current)
			{
				HandleGolemLifeChanged(Golem, Previous, Current);
			});
		Bindings.Add(Binding);
	}
	YardState->CommitSnapshot();
}

void AGolemAbilityYardGameMode::HandleDummyHealthChanged(const FOnAttributeChangeData& Change)
{
	if (bIsResetting || YardState == nullptr || Dummy == nullptr)
	{
		return;
	}
	YardState->Snapshot.DummyHealth = Change.NewValue;
	YardState->Snapshot.DummyMaxHealth = Dummy->GetGolemAttributeSet()->GetMaxHealth();
	YardState->Snapshot.bDummyDamaged |= Change.NewValue < Change.OldValue;
	YardState->CommitSnapshot();
}

void AGolemAbilityYardGameMode::HandleGolemHealthChanged(const TWeakObjectPtr<AGolemCharacter> Golem,
	const FOnAttributeChangeData& Change)
{
	if (bIsResetting || YardState == nullptr || !Golem.IsValid() || Change.NewValue >= Change.OldValue)
	{
		return;
	}
	if (Cast<APlayerController>(Golem->GetController()) != nullptr)
	{
		YardState->Snapshot.bPlayerDamaged = true;
		YardState->CommitSnapshot();
	}
}

void AGolemAbilityYardGameMode::HandleGolemLifeChanged(const TWeakObjectPtr<AGolemCharacter> Golem,
	const EGolemLifeState Previous, const EGolemLifeState Current)
{
	if (bIsResetting || YardState == nullptr || !Golem.IsValid())
	{
		return;
	}
	const FGolemRosterEntry* Entry = GetPartyState()->FindGolem(Golem.Get());
	if (Entry == nullptr)
	{
		return;
	}
	if (Previous == EGolemLifeState::Downed && Current == EGolemLifeState::Alive
		&& YardState->Snapshot.ReviveGolemId == Entry->GolemId)
	{
		YardState->Snapshot.bCompanionRevived = true;
	}
	YardState->CommitSnapshot();
}

FGolemResult AGolemAbilityYardGameMode::TryInteract(APlayerController* Player)
{
	AGolemPartyState* Party = GetPartyState();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "InteractAuthority", "Only the server may activate an exercise station."));
	}
	AGolemCharacter* Golem = Cast<AGolemCharacter>(Player->GetPawn());
	if (Party == nullptr || Party->GetControlledSlot(Player) == INDEX_NONE || Golem == nullptr
		|| YardState == nullptr || Dummy == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "InteractOwner", "Control a canonical Golem to use the exercise stations."));
	}
	AGolemAbilityStation* ClosestStation = nullptr;
	double ClosestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<AGolemAbilityStation> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const double Distance = FVector::DistSquared(Golem->GetActorLocation(), Iterator->GetActorLocation());
		if (Distance < ClosestDistance && Distance <= FMath::Square(Iterator->GetInteractionRange()))
		{
			ClosestDistance = Distance;
			ClosestStation = *Iterator;
		}
	}
	if (ClosestStation == nullptr)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "InteractRange", "Move closer to a labeled exercise station and press F."));
	}
	AGolemCharacter* AffectedGolem = nullptr;
	const FGolemResult Activation = ClosestStation->TryActivate(Golem, Party, Dummy, AffectedGolem);
	if (Activation.IsSuccess() && ClosestStation->GetStationKind() == EGolemAbilityStationKind::DownCompanion
		&& IsValid(AffectedGolem) && AffectedGolem != Golem && AffectedGolem->GetLifeComponent()->IsDowned())
	{
		const FGolemRosterEntry* Entry = Party->FindGolem(AffectedGolem);
		if (Entry != nullptr)
		{
			YardState->Snapshot.bCompanionDowned = true;
			YardState->Snapshot.ReviveSlot = Entry->Slot;
			YardState->Snapshot.ReviveGolemId = Entry->GolemId;
			YardState->CommitSnapshot();
		}
	}
	return Activation;
}

FGolemResult AGolemAbilityYardGameMode::TryRestartDrill(APlayerController* Player)
{
	AGolemPartyState* Party = GetPartyState();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RestartDrill"),
			NSLOCTEXT("GolemAbilityYard", "RetryAuthority", "Only the host may reset the exercise."));
	}
	if (!Player->IsLocalController() || Party == nullptr || !Party->IsCanonicalRosterReady()
		|| Party->GetControlledSlot(Player) == INDEX_NONE || YardState == nullptr || Dummy == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RestartDrill"),
			NSLOCTEXT("GolemAbilityYard", "RetryOwner", "The host must control a canonical Golem to reset the exercise."));
	}
	bIsResetting = true;
	ON_SCOPE_EXIT
	{
		bIsResetting = false;
	};
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		const FGolemResult Reset = Entry.Golem->GetLifeComponent()->ResetForCheckpoint();
		if (!Reset.IsSuccess())
		{
			return Reset;
		}
	}
	const FGolemResult DummyReset = Dummy->GetLifeComponent()->ResetForCheckpoint();
	if (!DummyReset.IsSuccess())
	{
		return DummyReset;
	}
	const FGolemResult MovementReset = Super::TryRestartDrill(Player);
	if (!MovementReset.IsSuccess())
	{
		return MovementReset;
	}
	const int32 PreviousRevision = YardState->Snapshot.Revision;
	YardState->Snapshot = FGolemAbilityYardSnapshot();
	YardState->Snapshot.Revision = PreviousRevision;
	YardState->Snapshot.DummyHealth = Dummy->GetGolemAttributeSet()->GetHealth();
	YardState->Snapshot.DummyMaxHealth = Dummy->GetGolemAttributeSet()->GetMaxHealth();
	YardState->CommitSnapshot();
	UE_LOG(LogGolemAbilityYard, Display, TEXT("Ability yard reset through GAS; canonical actors and grants preserved."));
	return FGolemResult::Success();
}

void AGolemAbilityYardGameMode::UnbindExercise()
{
	for (const FGolemAbilityYardBinding& Binding : Bindings)
	{
		if (Binding.AbilitySystem.IsValid())
		{
			Binding.AbilitySystem->GetGameplayAttributeValueChangeDelegate(
				UGolemAttributeSet::GetHealthAttribute()).Remove(Binding.HealthHandle);
		}
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.LifeHandle);
		}
	}
	Bindings.Reset();
}
