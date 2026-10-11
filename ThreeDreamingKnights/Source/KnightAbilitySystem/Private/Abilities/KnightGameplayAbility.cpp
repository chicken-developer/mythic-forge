#include "Abilities/KnightGameplayAbility.h"

#include "Components/KnightAbilitySystemComponent.h"
#include "Tags/KnightFixtureGameplayTags.h"
#include "Tags/KnightGameplayTags.h"

UKnightGameplayAbility::UKnightGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationRequiredTags.AddTag(KnightGameplayTags::State_Life_Alive);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Downed);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Dead);
	ActivationBlockedTags.AddTag(KnightGameplayTags::State_Fixture_TravelPending);
}

bool UKnightGameplayAbility::ShouldCancelOnInputRelease() const
{
	return true;
}

bool UKnightGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UKnightAbilitySystemComponent* ASC = ActorInfo != nullptr
		? Cast<UKnightAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	return ASC != nullptr && !ASC->IsMutationFenced()
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UKnightGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& InAbilitySystem,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (const UKnightAbilitySystemComponent* KnightASC = Cast<UKnightAbilitySystemComponent>(&InAbilitySystem);
		KnightASC != nullptr && KnightASC->IsMutationFenced())
	{
		return false;
	}
	if (!Super::DoesAbilitySatisfyTagRequirements(InAbilitySystem, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	if (const UKnightAbilitySystemComponent* KnightASC = Cast<UKnightAbilitySystemComponent>(&InAbilitySystem))
	{
		FGameplayTagContainer Required;
		FGameplayTagContainer Blocked;
		KnightASC->GetAdditionalActivationTagRequirements(GetAssetTags(), Required, Blocked);
		return KnightASC->HasAllMatchingGameplayTags(Required) && !KnightASC->HasAnyMatchingGameplayTags(Blocked);
	}
	return false;
}

UKnightAbilitySystemComponent* UKnightGameplayAbility::GetKnightAbilitySystem() const
{
	return Cast<UKnightAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

void UKnightGameplayAbility::ReportResult(const FKnightResult& Result) const
{
	if (UKnightAbilitySystemComponent* AbilitySystem = GetKnightAbilitySystem())
	{
		AbilitySystem->ReportAbilityResult(Result);
	}
}
