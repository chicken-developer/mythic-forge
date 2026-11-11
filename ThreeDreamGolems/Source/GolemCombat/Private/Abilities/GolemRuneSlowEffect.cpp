#include "Abilities/GolemRuneSlowEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/GolemGameplayTags.h"

UGolemRuneSlowEffect::UGolemRuneSlowEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(3.0f);
	FInheritedTagContainer Tags;
	Tags.AddTag(GolemGameplayTags::State_Combat_RuneSlowed);
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GolemRuneTargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}
