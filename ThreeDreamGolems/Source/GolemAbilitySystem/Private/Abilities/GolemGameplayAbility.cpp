#include "Abilities/GolemGameplayAbility.h"

#include "Components/GolemAbilitySystemComponent.h"
#include "Tags/GolemFixtureGameplayTags.h"
#include "Tags/GolemGameplayTags.h"

UGolemGameplayAbility::UGolemGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationRequiredTags.AddTag(GolemGameplayTags::State_Life_Alive);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Downed);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Dead);
	ActivationBlockedTags.AddTag(GolemGameplayTags::State_Fixture_TravelPending);
}

bool UGolemGameplayAbility::ShouldCancelOnInputRelease() const
{
	return true;
}

bool UGolemGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UGolemAbilitySystemComponent* ASC = ActorInfo != nullptr
		? Cast<UGolemAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	return ASC != nullptr
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UGolemGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& InAbilitySystem,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::DoesAbilitySatisfyTagRequirements(InAbilitySystem, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	if (const UGolemAbilitySystemComponent* GolemASC = Cast<UGolemAbilitySystemComponent>(&InAbilitySystem))
	{
		FGameplayTagContainer Required;
		FGameplayTagContainer Blocked;
		GolemASC->GetAdditionalActivationTagRequirements(GetAssetTags(), Required, Blocked);
		return GolemASC->HasAllMatchingGameplayTags(Required) && !GolemASC->HasAnyMatchingGameplayTags(Blocked);
	}
	return false;
}

UGolemAbilitySystemComponent* UGolemGameplayAbility::GetGolemAbilitySystem() const
{
	return Cast<UGolemAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

void UGolemGameplayAbility::ReportResult(const FGolemResult& Result) const
{
	if (UGolemAbilitySystemComponent* AbilitySystem = GetGolemAbilitySystem())
	{
		AbilitySystem->ReportAbilityResult(Result);
	}
}
