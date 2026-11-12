#include "Finisher/GolemFinisherComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Attributes/GolemAttributeSet.h"
#include "BrainComponent.h"
#include "Camera/GolemCameraModeComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	UGolemAbilitySystemComponent* FindGolemASC(AActor* Actor)
	{
		return Cast<UGolemAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
	}
}

UGolemFinisherComponent::UGolemFinisherComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

FGolemResult UGolemFinisherComponent::TryBeginFinisher(ACharacter* Target, UAnimMontage* SourceMontage,
	UAnimMontage* TargetMontage, FGuid& OutReservation, UGameplayAbility* SourceAbility)
{
	OutReservation.Invalidate();
	ACharacter* Source = Cast<ACharacter>(GetOwner());
	UGolemAbilitySystemComponent* SourceASC = FindGolemASC(Source);
	UGolemAbilitySystemComponent* TargetASC = FindGolemASC(Target);
	UGolemFinisherComponent* Victim = IsValid(Target) ? Target->FindComponentByClass<UGolemFinisherComponent>() : nullptr;
	if (Source == nullptr || !Source->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("BeginFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherAuthority", "Only the server can reserve a synchronized finisher."));
	}
	if (bEndingPlay || IsReserved() || !IsValid(Target) || Target == Source || Target->GetWorld() != GetWorld()
		|| Victim == nullptr || Victim->bEndingPlay || Victim->IsReserved() || SourceASC == nullptr || TargetASC == nullptr
		|| !SourceASC->GetEntityId().IsValid() || !TargetASC->GetEntityId().IsValid()
		|| SourceASC->GetLifeComponent() == nullptr || !SourceASC->GetLifeComponent()->IsAlive()
		|| TargetASC->GetLifeComponent() == nullptr || !TargetASC->GetLifeComponent()->IsAlive()
		|| !TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy)
		|| !TargetASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered)
		|| !IsValid(SourceMontage) || !IsValid(TargetMontage) || SourceAbility == nullptr
		|| SourceAbility->GetAvatarActorFromActorInfo() != Source)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("BeginFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherState", "Finisher requires a free living source and staggered unreserved enemy."));
	}
	const UGolemCombatComponent* Combat = Source->FindComponentByClass<UGolemCombatComponent>();
	const FGolemResult TargetValidation = Combat != nullptr
		? Combat->ValidateCombatTarget(Target, MaximumRange)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("BeginFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherCombat", "Finisher source has no configured combat domain."));
	if (!TargetValidation.IsSuccess())
	{
		return TargetValidation;
	}
	if (SourceMontage->GetPlayLength() <= 0.0f || TargetMontage->GetPlayLength() <= 0.0f)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("BeginFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherDuration", "Both finisher montages require a positive duration."));
	}
	ReservationId = FGuid::NewGuid();
	Victim->ReservationId = ReservationId;
	Victim->ReservedSource = this;
	TargetComponent = Victim;
	SourceIdentity = SourceASC->GetEntityId();
	TargetIdentity = TargetASC->GetEntityId();
	StartingController = Source->GetController();
	ActiveMontage = SourceMontage;
	Victim->ActiveMontage = TargetMontage;
	UCharacterMovementComponent* SourceMovement = Source->GetCharacterMovement();
	UCharacterMovementComponent* TargetMovement = Target->GetCharacterMovement();
	PreviousMovementMode = SourceMovement->MovementMode;
	PreviousCustomMode = SourceMovement->CustomMovementMode;
	Victim->PreviousMovementMode = TargetMovement->MovementMode;
	Victim->PreviousCustomMode = TargetMovement->CustomMovementMode;
	SourceMovement->StopMovementImmediately();
	TargetMovement->StopMovementImmediately();
	Source->StopJumping();
	Target->StopJumping();
	Source->ConsumeMovementInputVector();
	Target->ConsumeMovementInputVector();
	SourceMovement->SetMovementMode(MOVE_None);
	TargetMovement->SetMovementMode(MOVE_None);
	const FVector Facing = (Target->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D();
	Source->SetActorRotation(Facing.Rotation());
	Target->SetActorRotation((-Facing).Rotation());
	SourceASC->AddLooseGameplayTag(GolemGameplayTags::State_Combat_Finishing, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	bOwnFinishingTag = true;
	TargetASC->AddLooseGameplayTag(GolemGameplayTags::State_Combat_Finishing, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	Victim->bOwnFinishingTag = true;
	if (UGolemCameraModeComponent* Camera = Source->FindComponentByClass<UGolemCameraModeComponent>())
	{
		FGolemCameraModeSettings Settings;
		Settings.Priority = 100;
		Settings.bLockMovementInput = true;
		Settings.bLockLookInput = true;
		Settings.FocusTarget = Target;
		CameraHandle = Camera->PushMode(Settings);
	}
	StartedAt = GetWorld()->GetTimeSeconds();
	ActiveDuration = SourceASC->PlayMontage(SourceAbility, SourceAbility->GetCurrentActivationInfo(), SourceMontage, 1.0f);
	const float TargetRate = TargetMontage->GetPlayLength() / SourceMontage->GetPlayLength();
	const float TargetDuration = TargetASC->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), TargetMontage, TargetRate);
	Victim->StartedAt = StartedAt;
	Victim->ActiveDuration = ActiveDuration;
	if (ActiveDuration <= 0.0f || TargetDuration <= 0.0f)
	{
		EndFinisher(ReservationId, false);
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("BeginFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherMontage", "Both GAS finisher montages must start; reservation was rolled back."));
	}
	OutReservation = ReservationId;
	SetComponentTickEnabled(true);
	Victim->SetComponentTickEnabled(true);
	Source->ForceNetUpdate();
	Target->ForceNetUpdate();
	return FGolemResult::Success();
}

FGolemResult UGolemFinisherComponent::EndFinisher(const FGuid& Reservation, bool bComplete)
{
	ACharacter* Source = Cast<ACharacter>(GetOwner());
	if (Source == nullptr || !Source->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("EndFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherEndAuthority", "Finisher completion belongs to the server."));
	}
	if (!Reservation.IsValid() || Reservation != ReservationId || ReservedSource.IsValid())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("EndFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherEndHandle", "Only the reservation source can complete this finisher handle."));
	}
	// Callers may pass ReservationId itself; preserve the accepted identity before invalidating either endpoint.
	const FGuid AcceptedReservation = Reservation;
	UGolemFinisherComponent* Victim = TargetComponent.Get();
	ACharacter* Target = Victim != nullptr ? Cast<ACharacter>(Victim->GetOwner()) : nullptr;
	UGolemAbilitySystemComponent* SourceASC = FindGolemASC(Source);
	UGolemAbilitySystemComponent* TargetASC = FindGolemASC(Target);
	const bool bValidCompletion = bComplete && !bEndingPlay && IsValid(Target) && !Target->IsActorBeingDestroyed()
		&& Victim->ReservationId == AcceptedReservation && Victim->ReservedSource.Get() == this
		&& Source->GetController() == StartingController.Get() && SourceASC != nullptr && TargetASC != nullptr
		&& SourceASC->GetEntityId() == SourceIdentity && TargetASC->GetEntityId() == TargetIdentity
		&& SourceASC->GetLifeComponent() != nullptr && SourceASC->GetLifeComponent()->IsAlive()
		&& TargetASC->GetLifeComponent() != nullptr && TargetASC->GetLifeComponent()->IsAlive()
		&& GetWorld()->GetTimeSeconds() - StartedAt >= ActiveDuration - 0.15f
		&& FVector::Dist(Source->GetActorLocation(), Target->GetActorLocation()) <= MaximumRange + 50.0f;
	ReservationId.Invalidate();
	TargetComponent.Reset();
	StartingController.Reset();
	SetComponentTickEnabled(false);
	if (Victim != nullptr && Victim->ReservationId == AcceptedReservation && Victim->ReservedSource.Get() == this)
	{
		Victim->ReservationId.Invalidate();
		Victim->ReservedSource.Reset();
		Victim->SetComponentTickEnabled(false);
		if (TargetASC != nullptr && Victim->bOwnFinishingTag)
		{
			TargetASC->RemoveLooseGameplayTag(GolemGameplayTags::State_Combat_Finishing, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		Victim->bOwnFinishingTag = false;
		if (TargetASC != nullptr && TargetASC->GetCurrentMontage() == Victim->ActiveMontage && !bValidCompletion)
		{
			TargetASC->CurrentMontageStop(0.1f);
		}
		Victim->ActiveMontage = nullptr;
		if (!bWorldEnding)
		{
			Victim->RestoreMovement();
		}
		if (Target != nullptr)
		{
			Target->ForceNetUpdate();
		}
	}
	if (SourceASC != nullptr && SourceASC->GetCurrentMontage() == ActiveMontage && !bValidCompletion)
	{
		SourceASC->CurrentMontageStop(0.1f);
	}
	if (SourceASC != nullptr && bOwnFinishingTag)
	{
		SourceASC->RemoveLooseGameplayTag(GolemGameplayTags::State_Combat_Finishing, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	bOwnFinishingTag = false;
	ActiveMontage = nullptr;
	if (UGolemCameraModeComponent* Camera = Source->FindComponentByClass<UGolemCameraModeComponent>();
		Camera != nullptr && CameraHandle.IsValid())
	{
		Camera->PopMode(CameraHandle);
	}
	CameraHandle.Invalidate();
	RestoreMovement();
	FGolemResult Result = bValidCompletion ? FGolemResult::Success()
		: FGolemResult::Failure(EGolemError::Cancelled, TEXT("EndFinisher"),
			NSLOCTEXT("GolemCombat", "FinisherAborted", "Finisher was interrupted; camera, movement and reservation were released."));
	if (bValidCompletion)
	{
		const UGolemAttributeSet* TargetAttributes = TargetASC->GetGolemAttributes();
		Result = TargetAttributes != nullptr
			? UGolemAbilitySystemComponent::ApplyCombatDamage(Source, Target,
				TargetAttributes->GetMaxHealth() + 1.0f, 0.0f, false)
			: FGolemResult::Failure(EGolemError::InvalidState, TEXT("EndFinisher"),
				NSLOCTEXT("GolemCombat", "FinisherAttributes", "Finisher target has no GAS attributes."));
		if (Result.IsSuccess())
		{
			if (UGolemCombatComponent* Combat = Source->FindComponentByClass<UGolemCombatComponent>())
			{
				Combat->PublishOutcome(EGolemCombatOutcome::Finisher, GolemGameplayTags::Ability_Combat_Finisher, Target);
			}
		}
	}
	SourceIdentity = FGolemEntityId();
	TargetIdentity = FGolemEntityId();
	Source->ForceNetUpdate();
	OnFinisherEnded.Broadcast(AcceptedReservation, bValidCompletion && Result.IsSuccess());
	return Result;
}

bool UGolemFinisherComponent::HasActiveFinisher() const
{
	return ReservationId.IsValid() && SourceIdentity.IsValid();
}

void UGolemFinisherComponent::PrepareForEndPlay(bool bInWorldEnding)
{
	bEndingPlay = true;
	bWorldEnding = bWorldEnding || bInWorldEnding;
	if (bWorldEnding && ReservedSource.IsValid())
	{
		ReservedSource->bWorldEnding = true;
	}
	AbortReservation();
}

bool UGolemFinisherComponent::IsReserved() const
{
	return ReservationId.IsValid();
}

FGuid UGolemFinisherComponent::GetReservation() const
{
	return ReservationId;
}

float UGolemFinisherComponent::GetActiveDuration() const
{
	return ActiveDuration;
}

void UGolemFinisherComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemFinisherComponent, ReservationId);
}

void UGolemFinisherComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	PrepareForEndPlay(EndPlayReason == EEndPlayReason::EndPlayInEditor || EndPlayReason == EEndPlayReason::Quit
		|| EndPlayReason == EEndPlayReason::LevelTransition);
	OnFinisherEnded.Clear();
	Super::EndPlay(EndPlayReason);
}

void UGolemFinisherComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || !Character->HasAuthority() || !IsReserved())
	{
		return;
	}
	UGolemAbilitySystemComponent* ASC = FindGolemASC(Character);
	const UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
	const bool bSourceInterrupted = HasActiveFinisher()
		&& (Character->GetController() != StartingController.Get()
			|| !TargetComponent.IsValid()
			|| (GetWorld()->GetTimeSeconds() - StartedAt < ActiveDuration - 0.15f
				&& (Animation == nullptr || !Animation->Montage_IsPlaying(ActiveMontage))));
	const bool bVictimInterrupted = ReservedSource.IsValid()
		&& GetWorld()->GetTimeSeconds() - StartedAt < ActiveDuration - 0.15f
		&& (Animation == nullptr || !Animation->Montage_IsPlaying(ActiveMontage));
	if (ASC == nullptr || ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
		|| Character->IsActorBeingDestroyed() || bSourceInterrupted || bVictimInterrupted
		|| (ReservedSource.IsValid() && !ReservedSource->HasActiveFinisher()))
	{
		AbortReservation();
	}
	else if (HasActiveFinisher() && GetWorld()->GetTimeSeconds() - StartedAt >= ActiveDuration)
	{
		EndFinisher(ReservationId, true);
	}
}

void UGolemFinisherComponent::RestoreMovement()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UGolemAbilitySystemComponent* ASC = FindGolemASC(Character);
	if (Character == nullptr || bEndingPlay || bWorldEnding || Character->IsActorBeingDestroyed()
		|| GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	const bool bAlive = ASC != nullptr && ASC->GetLifeComponent() != nullptr && ASC->GetLifeComponent()->IsAlive();
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->SetMovementMode(bAlive ? static_cast<EMovementMode>(PreviousMovementMode) : MOVE_None,
		PreviousCustomMode);
	if (AAIController* AI = Cast<AAIController>(Character->GetController()); bAlive && AI != nullptr
		&& AI->GetBrainComponent() != nullptr)
	{
		AI->GetBrainComponent()->RestartLogic();
	}
}

void UGolemFinisherComponent::AbortReservation()
{
	if (UGolemFinisherComponent* Source = ReservedSource.Get())
	{
		Source->EndFinisher(ReservationId, false);
		if (bEndingPlay && ReservationId.IsValid())
		{
			if (UGolemAbilitySystemComponent* ASC = FindGolemASC(GetOwner()); ASC != nullptr && bOwnFinishingTag)
			{
				ASC->RemoveLooseGameplayTag(GolemGameplayTags::State_Combat_Finishing, 1,
					EGameplayTagReplicationState::TagAndCountToAll);
			}
			bOwnFinishingTag = false;
			ReservationId.Invalidate();
			ReservedSource.Reset();
			ActiveMontage = nullptr;
		}
	}
	else if (HasActiveFinisher())
	{
		EndFinisher(ReservationId, false);
	}
}
