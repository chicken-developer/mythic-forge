#pragma once

#include "CoreMinimal.h"
#include "Character/GolemCharacter.h"
#include "GameplayEffectTypes.h"
#include "GolemArenaCharacter.generated.h"

class UGolemThreatComponent;

UCLASS()
class THREEDREAMGOLEMS_API AGolemArenaCharacter : public AGolemCharacter
{
	GENERATED_BODY()

public:
	AGolemArenaCharacter();
	FGolemResult InitializeArenaCombat();
	UGolemThreatComponent* GetThreatComponent() const;
	FActiveGameplayEffectHandle GetRecoveryEffectHandle() const;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Threat")
	TObjectPtr<UGolemThreatComponent> Threat;
	FActiveGameplayEffectHandle RecoveryEffect;
};
