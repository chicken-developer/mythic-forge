#include "Abilities/KnightTraversalAbility.h"

#include "Components/KnightCombatComponent.h"
#include "Tags/KnightGameplayTags.h"
#include "Traversal/KnightTraversalComponent.h"

UKnightTraversalAbility::UKnightTraversalAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_World_Traverse));
}
void UKnightTraversalAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightCombatComponent* Combat = GetCombat();
	AActor* Owner = GetAvatarActorFromActorInfo();
	Traversal = IsValid(Owner) ? Owner->FindComponentByClass<UKnightTraversalComponent>() : nullptr;
	if (Combat == nullptr || Combat->GetProfile() == nullptr || !Traversal.IsValid() ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		ReportResult(FKnightResult::Failure(EKnightError::InvalidState, TEXT("TraversalComposition"),
			NSLOCTEXT("KnightCombat", "TraversalUnavailable", "Traversal is unavailable right now.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	EndedHandle = Traversal->OnTraversalEnded.AddUObject(this, &ThisClass::HandleTraversalEnded);
	const FKnightResult Result = Traversal->TryBeginTraversal(Combat->GetProfile()->GetTraversalMontage(),
		TEXT("KnightTraversal"), TraversalHandle, this);
	ReportResult(Result);
	if (!Result.IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}
void UKnightTraversalAbility::HandleTraversalEnded(FGuid Handle, bool bAborted)
{
	if (Handle == TraversalHandle && IsActive())
	{
		TraversalHandle.Invalidate();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bAborted);
	}
}
void UKnightTraversalAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Traversal.IsValid())
	{
		Traversal->OnTraversalEnded.Remove(EndedHandle);
		if (TraversalHandle.IsValid())
		{
			Traversal->EndTraversal(TraversalHandle, true);
		}
	}
	EndedHandle.Reset();
	TraversalHandle.Invalidate();
	Traversal.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
