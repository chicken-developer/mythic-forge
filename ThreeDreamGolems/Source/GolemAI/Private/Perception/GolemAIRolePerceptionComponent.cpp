#include "Perception/GolemAIRolePerceptionComponent.h"

#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Tags/GolemGameplayTags.h"

UGolemAIRolePerceptionComponent::UGolemAIRolePerceptionComponent()
{
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("GolemSight"));
	SightConfig->SightRadius = 1800.0f;
	SightConfig->LoseSightRadius = 2200.0f;
	SightConfig->PeripheralVisionAngleDegrees = 100.0f;
	SightConfig->SetMaxAge(5.0f);
	// Controllers keep UE's neutral default team; explicit role validation happens after real stimuli.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	ConfigureSense(*SightConfig);
	SetDominantSense(UAISense_Sight::StaticClass());
	OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::HandleTargetPerception);
}

FGolemResult UGolemAIRolePerceptionComponent::InitializeRoleSense(APawn* InPawn)
{
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	UGolemAbilitySystemComponent* ASC = IsValid(InPawn)
		? Cast<UGolemAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InPawn)) : nullptr;
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("InitializeAISight"),
			NSLOCTEXT("GolemAI", "SightAuthority", "AI sight filtering belongs to the server controller."));
	}
	if (bEndingPlay || !IsValid(InPawn) || InPawn->GetWorld() != GetWorld() || InPawn->GetController() != Controller
		|| ASC == nullptr || !ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InitializeAISight"),
			NSLOCTEXT("GolemAI", "SightPawn", "AI sight requires the owned pawn's initialized GAS identity and life."));
	}
	ClearRoleSense();
	ControlledPawn = InPawn;
	SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	RequestStimuliListenerUpdate();
	UAIPerceptionSystem::RegisterPerceptionStimuliSource(this, UAISense_Sight::StaticClass(), InPawn);
	return FGolemResult::Success();
}

FGolemResult UGolemAIRolePerceptionComponent::ConfigureRoleSense(const FGolemAIArchetypeTuning& Tuning)
{
	const FGolemResult Validation = Tuning.Validate();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || bEndingPlay)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("ConfigureAISight"),
			NSLOCTEXT("GolemAI", "SightTuningAuthority", "Only a live server controller can configure sight."));
	}
	SightConfig->SightRadius = Tuning.SightRadius;
	SightConfig->LoseSightRadius = Tuning.LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = Tuning.SightAngle;
	SightConfig->SetMaxAge(Tuning.StimulusMaxAge);
	ConfigureSense(*SightConfig);
	RequestStimuliListenerUpdate();
	return FGolemResult::Success();
}

void UGolemAIRolePerceptionComponent::ClearRoleSense()
{
	ControlledPawn.Reset();
	if (IsRegistered())
	{
		SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	}
	TArray<TWeakObjectPtr<AActor>> Actors;
	SensedActors.GetKeys(Actors);
	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		FGolemSensedActor Entry;
		if (SensedActors.RemoveAndCopyValue(Actor, Entry))
		{
			if (Entry.Life.IsValid())
			{
				Entry.Life->OnLifeStateChanged.Remove(Entry.LifeHandle);
			}
			if (Actor.IsValid())
			{
				Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleTargetEndPlay);
				if (Entry.bEligible)
				{
					OnRolePerceptionChanged.Broadcast(Actor.Get(), false);
				}
			}
		}
	}
	SensedActors.Empty();
	ForgetAll();
}

int32 UGolemAIRolePerceptionComponent::GetEligibleSensedCount() const
{
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AActor>, FGolemSensedActor>& Entry : SensedActors)
	{
		Count += Entry.Key.IsValid() && Entry.Value.bEligible ? 1 : 0;
	}
	return Count;
}

int64 UGolemAIRolePerceptionComponent::GetStimulusUpdateCount() const
{
	return StimulusUpdateCount;
}

void UGolemAIRolePerceptionComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ClearRoleSense();
	OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandleTargetPerception);
	OnRolePerceptionChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

bool UGolemAIRolePerceptionComponent::IsEligibleActor(AActor* Actor) const
{
	if (!ControlledPawn.IsValid() || !IsValid(Actor) || Actor == ControlledPawn.Get()
		|| Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld())
	{
		return false;
	}
	const UGolemAbilitySystemComponent* OwnerASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn.Get()));
	const UGolemAbilitySystemComponent* TargetASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
	if (OwnerASC == nullptr || TargetASC == nullptr || !TargetASC->GetEntityId().IsValid()
		|| TargetASC->GetLifeComponent() == nullptr || TargetASC->GetLifeComponent()->IsDead())
	{
		return false;
	}
	if (OwnerASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem))
	{
		return TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)
			|| (TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) && TargetASC->GetLifeComponent()->IsAlive());
	}
	return OwnerASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy)
		&& TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)
		&& TargetASC->GetLifeComponent()->IsAlive();
}

void UGolemAIRolePerceptionComponent::RefreshTrackedActor(AActor* Actor)
{
	FGolemSensedActor* Entry = SensedActors.Find(Actor);
	if (Entry == nullptr)
	{
		return;
	}
	const bool bEligible = IsEligibleActor(Actor);
	if (bEligible != Entry->bEligible)
	{
		Entry->bEligible = bEligible;
		OnRolePerceptionChanged.Broadcast(Actor, bEligible);
	}
}

void UGolemAIRolePerceptionComponent::RemoveTrackedActor(AActor* Actor, bool bNotify)
{
	FGolemSensedActor Entry;
	if (!SensedActors.RemoveAndCopyValue(Actor, Entry))
	{
		return;
	}
	if (Entry.Life.IsValid())
	{
		Entry.Life->OnLifeStateChanged.Remove(Entry.LifeHandle);
	}
	if (Actor != nullptr)
	{
		Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleTargetEndPlay);
	}
	if (bNotify && Entry.bEligible && Actor != nullptr)
	{
		OnRolePerceptionChanged.Broadcast(Actor, false);
	}
}

void UGolemAIRolePerceptionComponent::HandleTargetPerception(AActor* Actor, FAIStimulus Stimulus)
{
	if (bEndingPlay || !ControlledPawn.IsValid() || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	++StimulusUpdateCount;
	if (!Stimulus.WasSuccessfullySensed() || !IsValid(Actor))
	{
		RemoveTrackedActor(Actor, true);
		return;
	}
	UGolemAbilitySystemComponent* ASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
	if (Actor == ControlledPawn.Get() || Actor->GetWorld() != GetWorld() || ASC == nullptr
		|| !ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr)
	{
		return;
	}
	if (!SensedActors.Contains(Actor))
	{
		FGolemSensedActor Entry;
		Entry.Life = ASC->GetLifeComponent();
		const TWeakObjectPtr<UGolemAIRolePerceptionComponent> WeakThis(this);
		const TWeakObjectPtr<AActor> WeakActor(Actor);
		Entry.LifeHandle = Entry.Life->OnLifeStateChanged.AddLambda(
			[WeakThis, WeakActor](EGolemLifeState Previous, EGolemLifeState Current)
			{
				if (WeakThis.IsValid() && WeakActor.IsValid())
				{
					WeakThis->RefreshTrackedActor(WeakActor.Get());
				}
			});
		Actor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleTargetEndPlay);
		SensedActors.Add(Actor, Entry);
	}
	RefreshTrackedActor(Actor);
}

void UGolemAIRolePerceptionComponent::HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	RemoveTrackedActor(Actor, true);
}
