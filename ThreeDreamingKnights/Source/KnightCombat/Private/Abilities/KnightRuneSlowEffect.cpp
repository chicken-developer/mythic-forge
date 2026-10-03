#include "Abilities/KnightRuneSlowEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/KnightGameplayTags.h"

UKnightRuneSlowEffect::UKnightRuneSlowEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(3.0f);
	FInheritedTagContainer Tags;
	Tags.AddTag(KnightGameplayTags::State_Combat_RuneSlowed);
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("KnightRuneTargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}
