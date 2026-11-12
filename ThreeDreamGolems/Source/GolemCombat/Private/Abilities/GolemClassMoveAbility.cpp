#include "Abilities/GolemClassMoveAbility.h"

#include "Components/GolemCombatComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemClassMoveAbility::UGolemClassMoveAbility()
{
	// The tag keeps its old name: tag relationships and saved ability sets already refer to it.
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Combat_Block));
	ActivationOwnedTags.AddTag(GolemGameplayTags::State_Combat_Blocking);
}

bool UGolemClassMoveAbility::ShouldCancelOnInputRelease() const
{
	return bHeldMove;
}

void UGolemClassMoveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemCombatComponent* Combat = GetCombat();
	EGolemClassMove Move = EGolemClassMove::Guard;
	float Duration = 0.0f;
	const FGolemResult Result = Combat != nullptr ? Combat->BeginClassMove(Move, Duration)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("ClassMoveNotConfigured"),
			NSLOCTEXT("GolemCombat", "ClassMoveUnavailable", "The class move is unavailable in this area."));
	ReportResult(Result);
	if (!Result.IsSuccess() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bHeldMove = Move == EGolemClassMove::Guard;
	// A Guard with no time limit lasts until release; everything else ends by itself.
	SetActionDuration(Duration);
}

void UGolemClassMoveAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UGolemCombatComponent* Combat = GetCombat())
	{
		Combat->EndClassMove(bWasCancelled);
	}
	bHeldMove = true;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
