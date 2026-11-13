#include "Cues/GolemGameplayCueNotify.h"

#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Tags/GolemGameplayTags.h"

UGolemGameplayCueNotify::UGolemGameplayCueNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsOverride = true;
}

bool UGolemGameplayCueNotify::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::Executed || EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::WhileActive || EventType == EGameplayCueEvent::Removed;
}

void UGolemGameplayCueNotify::HandleGameplayCue(AActor* Target, EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (!IsValid(Target))
	{
		return;
	}
	UGolemAbilitySystemComponent* AbilitySystem = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true));
	const FGameplayTag Tag = Parameters.MatchedTagName.IsValid() ? Parameters.MatchedTagName : Parameters.OriginalTag;
	if (AbilitySystem == nullptr || (Tag != GolemGameplayTags::GameplayCue_Damage
		&& Tag != GolemGameplayTags::GameplayCue_Downed && Tag != GolemGameplayTags::GameplayCue_Revive))
	{
		return;
	}
	AbilitySystem->RecordGameplayCue(Tag, EventType);
	AbilitySystem->PresentGameplayCue(Tag, EventType);
}
