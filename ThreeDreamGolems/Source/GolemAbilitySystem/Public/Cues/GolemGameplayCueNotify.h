#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GolemGameplayCueNotify.generated.h"

/** Native runtime cue-set handler supports executed bursts and persistent late-join state. */
UCLASS(NotBlueprintable)
class GOLEMABILITYSYSTEM_API UGolemGameplayCueNotify : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGolemGameplayCueNotify(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual void HandleGameplayCue(AActor* Target, EGameplayCueEvent::Type EventType,
		const FGameplayCueParameters& Parameters) override;
};
