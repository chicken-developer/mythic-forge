#include "Perception/KnightAIRolePerceptionComponent.h"

#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Tags/KnightGameplayTags.h"

UKnightAIRolePerceptionComponent::UKnightAIRolePerceptionComponent()
{
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("KnightSight"));
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

FKnightResult UKnightAIRolePerceptionComponent::InitializeRoleSense(APawn* InPawn)
{
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	UKnightAbilitySystemComponent* ASC = IsValid(InPawn)
		? Cast<UKnightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InPawn)) : nullptr;
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("InitializeAISight"),
			NSLOCTEXT("KnightAI", "SightAuthority", "AI sight filtering belongs to the server controller."));
	}
	if (bEndingPlay || !IsValid(InPawn) || InPawn->GetWorld() != GetWorld() || InPawn->GetController() != Controller
		|| ASC == nullptr || !ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InitializeAISight"),
			NSLOCTEXT("KnightAI", "SightPawn", "AI sight requires the owned pawn's initialized GAS identity and life."));
	}
	ClearRoleSense();
	ControlledPawn = InPawn;
	SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	RequestStimuliListenerUpdate();
	UAIPerceptionSystem::RegisterPerceptionStimuliSource(this, UAISense_Sight::StaticClass(), InPawn);
	return FKnightResult::Success();
}

FKnightResult UKnightAIRolePerceptionComponent::ConfigureRoleSense(const FKnightAIArchetypeTuning& Tuning)
{
	const FKnightResult Validation = Tuning.Validate();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || bEndingPlay)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("ConfigureAISight"),
			NSLOCTEXT("KnightAI", "SightTuningAuthority", "Only a live server controller can configure sight."));
	}
	SightConfig->SightRadius = Tuning.SightRadius;
	SightConfig->LoseSightRadius = Tuning.LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = Tuning.SightAngle;
	SightConfig->SetMaxAge(Tuning.StimulusMaxAge);
	ConfigureSense(*SightConfig);
	RequestStimuliListenerUpdate();
	return FKnightResult::Success();
}

void UKnightAIRolePerceptionComponent::ClearRoleSense()
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
		FKnightSensedActor Entry;
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

int32 UKnightAIRolePerceptionComponent::GetEligibleSensedCount() const
{
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AActor>, FKnightSensedActor>& Entry : SensedActors)
	{
		Count += Entry.Key.IsValid() && Entry.Value.bEligible ? 1 : 0;
	}
	return Count;
}

int64 UKnightAIRolePerceptionComponent::GetStimulusUpdateCount() const
{
	return StimulusUpdateCount;
}

void UKnightAIRolePerceptionComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ClearRoleSense();
	OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandleTargetPerception);
	OnRolePerceptionChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

bool UKnightAIRolePerceptionComponent::IsEligibleActor(AActor* Actor) const
{
	if (!ControlledPawn.IsValid() || !IsValid(Actor) || Actor == ControlledPawn.Get()
		|| Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld())
	{
		return false;
	}
	const UKnightAbilitySystemComponent* OwnerASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn.Get()));
	const UKnightAbilitySystemComponent* TargetASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
	if (OwnerASC == nullptr || TargetASC == nullptr || !TargetASC->GetEntityId().IsValid()
		|| TargetASC->GetLifeComponent() == nullptr || TargetASC->GetLifeComponent()->IsDead())
	{
		return false;
	}
	if (OwnerASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight))
	{
		return TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)
			|| (TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) && TargetASC->GetLifeComponent()->IsAlive());
	}
	return OwnerASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy)
		&& TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)
		&& TargetASC->GetLifeComponent()->IsAlive();
}

void UKnightAIRolePerceptionComponent::RefreshTrackedActor(AActor* Actor)
{
	FKnightSensedActor* Entry = SensedActors.Find(Actor);
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

void UKnightAIRolePerceptionComponent::RemoveTrackedActor(AActor* Actor, bool bNotify)
{
	FKnightSensedActor Entry;
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

void UKnightAIRolePerceptionComponent::HandleTargetPerception(AActor* Actor, FAIStimulus Stimulus)
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
	UKnightAbilitySystemComponent* ASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
	if (Actor == ControlledPawn.Get() || Actor->GetWorld() != GetWorld() || ASC == nullptr
		|| !ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr)
	{
		return;
	}
	if (!SensedActors.Contains(Actor))
	{
		FKnightSensedActor Entry;
		Entry.Life = ASC->GetLifeComponent();
		const TWeakObjectPtr<UKnightAIRolePerceptionComponent> WeakThis(this);
		const TWeakObjectPtr<AActor> WeakActor(Actor);
		Entry.LifeHandle = Entry.Life->OnLifeStateChanged.AddLambda(
			[WeakThis, WeakActor](EKnightLifeState Previous, EKnightLifeState Current)
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

void UKnightAIRolePerceptionComponent::HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	RemoveTrackedActor(Actor, true);
}
