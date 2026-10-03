#include "Abilities/KnightTestDamageAbility.h"

#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "Tags/KnightGameplayTags.h"

UKnightTestDamageAbility::UKnightTestDamageAbility()
{
	SetAssetTags(FGameplayTagContainer(KnightGameplayTags::Ability_Test_Pulse));
}

void UKnightTestDamageAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem();
	AActor* Source = GetAvatarActorFromActorInfo();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Target == nullptr && ASC != nullptr)
	{
		Target = ASC->FindAbilityTarget(KnightGameplayTags::Input_Ability_Test);
	}
	UKnightAbilitySystemComponent* TargetASC = IsValid(Target) ? Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true)) : nullptr;
	FKnightResult Result = FKnightResult::Failure(EKnightError::NotFound, TEXT("PulseTarget"),
		NSLOCTEXT("KnightGAS", "PulseNoEnemy", "Move close to an available enemy target."));
	if (ASC != nullptr && IsValid(Source) && TargetASC != nullptr && Target != Source &&
		Target->GetWorld() == Source->GetWorld() && TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) &&
		TargetASC->GetLifeComponent() != nullptr && !TargetASC->GetLifeComponent()->IsDead())
	{
		if (FVector::DistSquared(Source->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(1000.0f))
		{
			Result = FKnightResult::Failure(EKnightError::OutOfRange, TEXT("PulseRange"),
				NSLOCTEXT("KnightGAS", "PulseRange", "Move closer to the enemy target."));
		}
		else
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightPulseVisibility), false, Source);
			Query.AddIgnoredActor(Target);
			FHitResult Hit;
			if (Source->GetWorld()->LineTraceSingleByChannel(Hit, Source->GetActorLocation(), Target->GetActorLocation(),
				ECC_Visibility, Query))
			{
				Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("PulseLineOfSight"),
					NSLOCTEXT("KnightGAS", "PulseBlocked", "The target is behind an obstacle."));
			}
			else if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
			{
				Result = FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("PulseCommit"),
					NSLOCTEXT("KnightGAS", "PulseCommit", "This action cannot be committed yet."));
			}
			else
			{
				Result = UKnightAbilitySystemComponent::ApplyDamage(Source, Target, ASC->GetKnightAttributes()->GetAttackPower());
			}
		}
	}
	ReportResult(Result);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Result.IsSuccess());
}
