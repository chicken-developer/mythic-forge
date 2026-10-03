#include "Cues/KnightGameplayCueNotify.h"

#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Tags/KnightGameplayTags.h"

UKnightGameplayCueNotify::UKnightGameplayCueNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsOverride = true;
}

bool UKnightGameplayCueNotify::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::Executed || EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::WhileActive || EventType == EGameplayCueEvent::Removed;
}

void UKnightGameplayCueNotify::HandleGameplayCue(AActor* Target, EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (!IsValid(Target))
	{
		return;
	}
	UKnightAbilitySystemComponent* AbilitySystem = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true));
	const FGameplayTag Tag = Parameters.MatchedTagName.IsValid() ? Parameters.MatchedTagName : Parameters.OriginalTag;
	if (AbilitySystem == nullptr || (Tag != KnightGameplayTags::GameplayCue_Damage
		&& Tag != KnightGameplayTags::GameplayCue_Downed && Tag != KnightGameplayTags::GameplayCue_Revive
		&& Tag != KnightGameplayTags::GameplayCue_Parry && Tag != KnightGameplayTags::GameplayCue_Stagger
		&& Tag != KnightGameplayTags::GameplayCue_Rune))
	{
		return;
	}
	AbilitySystem->RecordGameplayCue(Tag, EventType);
	AbilitySystem->PresentGameplayCue(Tag, EventType);
}
