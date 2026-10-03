#include "Abilities/KnightFinisherAbility.h"

#include "Components/KnightCombatComponent.h"
#include "Finisher/KnightFinisherComponent.h"
#include "GameFramework/Character.h"
#include "Tags/KnightGameplayTags.h"

UKnightFinisherAbility::UKnightFinisherAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Combat_Finisher));
}
void UKnightFinisherAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	AActor* Owner = GetAvatarActorFromActorInfo();
	Finisher = IsValid(Owner) ? Owner->FindComponentByClass<UKnightFinisherComponent>() : nullptr;
	if (Combat == nullptr || Combat->GetProfile() == nullptr || !Finisher.IsValid() ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		ReportResult(FKnightResult::Failure(EKnightError::InvalidState, TEXT("FinisherComposition"),
			NSLOCTEXT("KnightCombat", "FinisherUnavailable", "Finisher is unavailable right now.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Target == nullptr)
	{
		Target = Combat->FindCombatTarget(220.0f, true);
	}
	EndedHandle = Finisher->OnFinisherEnded.AddUObject(this, &ThisClass::HandleFinisherEnded);
	const FKnightResult Result = Finisher->TryBeginFinisher(Cast<ACharacter>(Target),
		Combat->GetProfile()->GetFinisherSourceMontage(), Combat->GetProfile()->GetFinisherTargetMontage(),
		Reservation, this);
	ReportResult(Result);
	if (!Result.IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}
void UKnightFinisherAbility::HandleFinisherEnded(FGuid Handle, bool bCompleted)
{
	if (Handle == Reservation && IsActive())
	{
		Reservation.Invalidate();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bCompleted);
	}
}
void UKnightFinisherAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Finisher.IsValid())
	{
		Finisher->OnFinisherEnded.Remove(EndedHandle);
		if (Reservation.IsValid())
		{
			Finisher->EndFinisher(Reservation, false);
		}
	}
	EndedHandle.Reset();
	Reservation.Invalidate();
	Finisher.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
