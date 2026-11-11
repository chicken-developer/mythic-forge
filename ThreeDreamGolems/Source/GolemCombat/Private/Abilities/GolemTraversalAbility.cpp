#include "Abilities/GolemTraversalAbility.h"

#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"
#include "Traversal/GolemTraversalComponent.h"

UGolemTraversalAbility::UGolemTraversalAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_World_Traverse));
}
void UGolemTraversalAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	AActor* Owner = GetAvatarActorFromActorInfo();
	Traversal = IsValid(Owner) ? Owner->FindComponentByClass<UGolemTraversalComponent>() : nullptr;
	if (Combat == nullptr || Combat->GetProfile() == nullptr || !Traversal.IsValid() ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		ReportResult(FGolemResult::Failure(EGolemError::InvalidState, TEXT("TraversalComposition"),
			NSLOCTEXT("GolemCombat", "TraversalUnavailable", "Traversal is unavailable right now.")));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	EndedHandle = Traversal->OnTraversalEnded.AddUObject(this, &ThisClass::HandleTraversalEnded);
	const FGolemResult Result = Traversal->TryBeginTraversal(Combat->GetProfile()->GetTraversalMontage(),
		TEXT("KnightTraversal"), TraversalHandle, this);
	ReportResult(Result);
	if (!Result.IsSuccess())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}
void UGolemTraversalAbility::HandleTraversalEnded(FGuid Handle, bool bAborted)
{
	if (Handle == TraversalHandle && IsActive())
	{
		TraversalHandle.Invalidate();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bAborted);
	}
}
void UGolemTraversalAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
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
