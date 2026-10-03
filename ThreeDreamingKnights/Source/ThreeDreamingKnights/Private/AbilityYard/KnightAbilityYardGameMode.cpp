#include "AbilityYard/KnightAbilityYardGameMode.h"

#include "AbilityYard/KnightAbilityDummy.h"
#include "AbilityYard/KnightAbilityStation.h"
#include "AbilityYard/KnightAbilityYardHUD.h"
#include "AbilityYard/KnightAbilityYardState.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Party/KnightPartyState.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffectTypes.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightAbilityYard, Log, All);

AKnightAbilityYardGameMode::AKnightAbilityYardGameMode()
{
	HUDClass = AKnightAbilityYardHUD::StaticClass();
}

void AKnightAbilityYardGameMode::InitGameState()
{
	Super::InitGameState();
	if (HasAuthority() && GetInitializationResult().IsSuccess() && YardState == nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.Owner = this;
		YardState = GetWorld()->SpawnActor<AKnightAbilityYardState>(Parameters);
		if (YardState == nullptr)
		{
			UE_LOG(LogKnightAbilityYard, Error, TEXT("Ability yard objective state could not spawn."));
		}
	}
}

void AKnightAbilityYardGameMode::BeginPlay()
{
	Super::BeginPlay();
	BindExercise();
}

void AKnightAbilityYardGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindExercise();
	Super::EndPlay(EndPlayReason);
}

AKnightAbilityYardState* AKnightAbilityYardGameMode::GetYardState() const
{
	return YardState;
}

void AKnightAbilityYardGameMode::BindExercise()
{
	if (!HasAuthority() || YardState == nullptr || GetPartyState() == nullptr)
	{
		return;
	}
	for (TActorIterator<AKnightAbilityDummy> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (Dummy != nullptr)
		{
			UE_LOG(LogKnightAbilityYard, Error, TEXT("Ability yard requires exactly one authored dummy."));
			return;
		}
		Dummy = *Iterator;
	}
	if (Dummy == nullptr)
	{
		UE_LOG(LogKnightAbilityYard, Error, TEXT("Ability yard is missing its real GAS dummy."));
		return;
	}
	YardState->Dummy = Dummy;
	YardState->Snapshot.DummyHealth = Dummy->GetKnightAttributeSet()->GetHealth();
	YardState->Snapshot.DummyMaxHealth = Dummy->GetKnightAttributeSet()->GetMaxHealth();
	FKnightAbilityYardBinding DummyBinding;
	DummyBinding.AbilitySystem = Dummy->GetKnightAbilitySystem();
	DummyBinding.HealthHandle = Dummy->GetKnightAbilitySystem()->GetGameplayAttributeValueChangeDelegate(
		UKnightAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleDummyHealthChanged);
	Bindings.Add(DummyBinding);
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		const TWeakObjectPtr<AKnightCharacter> Knight = Entry.Knight.Get();
		FKnightAbilityYardBinding Binding;
		Binding.AbilitySystem = Entry.Knight->GetKnightAbilitySystem();
		Binding.Life = Entry.Knight->GetLifeComponent();
		Binding.HealthHandle = Entry.Knight->GetKnightAbilitySystem()->GetGameplayAttributeValueChangeDelegate(
			UKnightAttributeSet::GetHealthAttribute()).AddWeakLambda(this,
			[this, Knight](const FOnAttributeChangeData& Change)
			{
				HandleKnightHealthChanged(Knight, Change);
			});
		Binding.LifeHandle = Entry.Knight->GetLifeComponent()->OnLifeStateChanged.AddWeakLambda(this,
			[this, Knight](const EKnightLifeState Previous, const EKnightLifeState Current)
			{
				HandleKnightLifeChanged(Knight, Previous, Current);
			});
		Bindings.Add(Binding);
	}
	YardState->CommitSnapshot();
}

void AKnightAbilityYardGameMode::HandleDummyHealthChanged(const FOnAttributeChangeData& Change)
{
	if (bIsResetting || YardState == nullptr || Dummy == nullptr)
	{
		return;
	}
	YardState->Snapshot.DummyHealth = Change.NewValue;
	YardState->Snapshot.DummyMaxHealth = Dummy->GetKnightAttributeSet()->GetMaxHealth();
	YardState->Snapshot.bDummyDamaged |= Change.NewValue < Change.OldValue;
	YardState->CommitSnapshot();
}

void AKnightAbilityYardGameMode::HandleKnightHealthChanged(const TWeakObjectPtr<AKnightCharacter> Knight,
	const FOnAttributeChangeData& Change)
{
	if (bIsResetting || YardState == nullptr || !Knight.IsValid() || Change.NewValue >= Change.OldValue)
	{
		return;
	}
	if (Cast<APlayerController>(Knight->GetController()) != nullptr)
	{
		YardState->Snapshot.bPlayerDamaged = true;
		YardState->CommitSnapshot();
	}
}

void AKnightAbilityYardGameMode::HandleKnightLifeChanged(const TWeakObjectPtr<AKnightCharacter> Knight,
	const EKnightLifeState Previous, const EKnightLifeState Current)
{
	if (bIsResetting || YardState == nullptr || !Knight.IsValid())
	{
		return;
	}
	const FKnightRosterEntry* Entry = GetPartyState()->FindKnight(Knight.Get());
	if (Entry == nullptr)
	{
		return;
	}
	if (Previous == EKnightLifeState::Downed && Current == EKnightLifeState::Alive
		&& YardState->Snapshot.ReviveKnightId == Entry->KnightId)
	{
		YardState->Snapshot.bCompanionRevived = true;
	}
	YardState->CommitSnapshot();
}

FKnightResult AKnightAbilityYardGameMode::TryInteract(APlayerController* Player)
{
	AKnightPartyState* Party = GetPartyState();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "InteractAuthority", "Only the server may activate an exercise station."));
	}
	AKnightCharacter* Knight = Cast<AKnightCharacter>(Player->GetPawn());
	if (Party == nullptr || Party->GetControlledSlot(Player) == INDEX_NONE || Knight == nullptr
		|| YardState == nullptr || Dummy == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "InteractOwner", "Control a canonical Knight to use the exercise stations."));
	}
	AKnightAbilityStation* ClosestStation = nullptr;
	double ClosestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<AKnightAbilityStation> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const double Distance = FVector::DistSquared(Knight->GetActorLocation(), Iterator->GetActorLocation());
		if (Distance < ClosestDistance && Distance <= FMath::Square(Iterator->GetInteractionRange()))
		{
			ClosestDistance = Distance;
			ClosestStation = *Iterator;
		}
	}
	if (ClosestStation == nullptr)
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "InteractRange", "Move closer to a labeled exercise station and press F."));
	}
	AKnightCharacter* AffectedKnight = nullptr;
	const FKnightResult Activation = ClosestStation->TryActivate(Knight, Party, Dummy, AffectedKnight);
	if (Activation.IsSuccess() && ClosestStation->GetStationKind() == EKnightAbilityStationKind::DownCompanion
		&& IsValid(AffectedKnight) && AffectedKnight != Knight && AffectedKnight->GetLifeComponent()->IsDowned())
	{
		const FKnightRosterEntry* Entry = Party->FindKnight(AffectedKnight);
		if (Entry != nullptr)
		{
			YardState->Snapshot.bCompanionDowned = true;
			YardState->Snapshot.ReviveSlot = Entry->Slot;
			YardState->Snapshot.ReviveKnightId = Entry->KnightId;
			YardState->CommitSnapshot();
		}
	}
	return Activation;
}

FKnightResult AKnightAbilityYardGameMode::TryRestartDrill(APlayerController* Player)
{
	AKnightPartyState* Party = GetPartyState();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RestartDrill"),
			NSLOCTEXT("KnightAbilityYard", "RetryAuthority", "Only the host may reset the exercise."));
	}
	if (!Player->IsLocalController() || Party == nullptr || !Party->IsCanonicalRosterReady()
		|| Party->GetControlledSlot(Player) == INDEX_NONE || YardState == nullptr || Dummy == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RestartDrill"),
			NSLOCTEXT("KnightAbilityYard", "RetryOwner", "The host must control a canonical Knight to reset the exercise."));
	}
	bIsResetting = true;
	ON_SCOPE_EXIT
	{
		bIsResetting = false;
	};
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		const FKnightResult Reset = Entry.Knight->GetLifeComponent()->ResetForCheckpoint();
		if (!Reset.IsSuccess())
		{
			return Reset;
		}
	}
	const FKnightResult DummyReset = Dummy->GetLifeComponent()->ResetForCheckpoint();
	if (!DummyReset.IsSuccess())
	{
		return DummyReset;
	}
	const FKnightResult MovementReset = Super::TryRestartDrill(Player);
	if (!MovementReset.IsSuccess())
	{
		return MovementReset;
	}
	const int32 PreviousRevision = YardState->Snapshot.Revision;
	YardState->Snapshot = FKnightAbilityYardSnapshot();
	YardState->Snapshot.Revision = PreviousRevision;
	YardState->Snapshot.DummyHealth = Dummy->GetKnightAttributeSet()->GetHealth();
	YardState->Snapshot.DummyMaxHealth = Dummy->GetKnightAttributeSet()->GetMaxHealth();
	YardState->CommitSnapshot();
	UE_LOG(LogKnightAbilityYard, Display, TEXT("Ability yard reset through GAS; canonical actors and grants preserved."));
	return FKnightResult::Success();
}

void AKnightAbilityYardGameMode::UnbindExercise()
{
	for (const FKnightAbilityYardBinding& Binding : Bindings)
	{
		if (Binding.AbilitySystem.IsValid())
		{
			Binding.AbilitySystem->GetGameplayAttributeValueChangeDelegate(
				UKnightAttributeSet::GetHealthAttribute()).Remove(Binding.HealthHandle);
		}
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.LifeHandle);
		}
	}
	Bindings.Reset();
}
