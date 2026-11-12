#include "Abilities/GolemTestDamageAbility.h"

#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "Tags/GolemGameplayTags.h"

UGolemTestDamageAbility::UGolemTestDamageAbility()
{
	SetAssetTags(FGameplayTagContainer(GolemGameplayTags::Ability_Test_Pulse));
}

void UGolemTestDamageAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem();
	AActor* Source = GetAvatarActorFromActorInfo();
	AActor* Target = TriggerEventData != nullptr ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	if (Target == nullptr && ASC != nullptr)
	{
		Target = ASC->FindAbilityTarget(GolemGameplayTags::Input_Ability_Test);
	}
	UGolemAbilitySystemComponent* TargetASC = IsValid(Target) ? Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true)) : nullptr;
	FGolemResult Result = FGolemResult::Failure(EGolemError::NotFound, TEXT("PulseTarget"),
		NSLOCTEXT("GolemGAS", "PulseNoEnemy", "Move close to an available enemy target."));
	if (ASC != nullptr && IsValid(Source) && TargetASC != nullptr && Target != Source &&
		Target->GetWorld() == Source->GetWorld() && TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) &&
		TargetASC->GetLifeComponent() != nullptr && !TargetASC->GetLifeComponent()->IsDead())
	{
		if (FVector::DistSquared(Source->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(1000.0f))
		{
			Result = FGolemResult::Failure(EGolemError::OutOfRange, TEXT("PulseRange"),
				NSLOCTEXT("GolemGAS", "PulseRange", "Move closer to the enemy target."));
		}
		else
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemPulseVisibility), false, Source);
			Query.AddIgnoredActor(Target);
			FHitResult Hit;
			if (Source->GetWorld()->LineTraceSingleByChannel(Hit, Source->GetActorLocation(), Target->GetActorLocation(),
				ECC_Visibility, Query))
			{
				Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("PulseLineOfSight"),
					NSLOCTEXT("GolemGAS", "PulseBlocked", "The target is behind an obstacle."));
			}
			else if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
			{
				Result = FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("PulseCommit"),
					NSLOCTEXT("GolemGAS", "PulseCommit", "This action cannot be committed yet."));
			}
			else
			{
				Result = UGolemAbilitySystemComponent::ApplyDamage(Source, Target, ASC->GetGolemAttributes()->GetPower());
			}
		}
	}
	ReportResult(Result);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Result.IsSuccess());
}
