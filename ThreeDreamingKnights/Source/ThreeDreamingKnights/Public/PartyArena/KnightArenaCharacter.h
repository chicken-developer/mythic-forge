#pragma once

#include "CoreMinimal.h"
#include "Character/KnightCharacter.h"
#include "GameplayEffectTypes.h"
#include "KnightArenaCharacter.generated.h"

class UKnightThreatComponent;

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightArenaCharacter : public AKnightCharacter
{
	GENERATED_BODY()

public:
	AKnightArenaCharacter();
	FKnightResult InitializeArenaCombat();
	UKnightThreatComponent* GetThreatComponent() const;
	FActiveGameplayEffectHandle GetRecoveryEffectHandle() const;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Threat")
	TObjectPtr<UKnightThreatComponent> Threat;
	FActiveGameplayEffectHandle RecoveryEffect;
};
