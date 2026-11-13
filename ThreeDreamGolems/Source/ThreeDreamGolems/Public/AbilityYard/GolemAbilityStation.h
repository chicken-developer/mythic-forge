#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GolemResult.h"
#include "GolemAbilityStation.generated.h"

class AGolemAbilityDummy;
class AGolemCharacter;
class AGolemPartyState;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EGolemAbilityStationKind : uint8
{
	DamagePlayer,
	DownCompanion
};

/** Explicit exercise interactions apply server GAS damage through the same execution as abilities. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemAbilityStation : public AActor
{
	GENERATED_BODY()

public:
	AGolemAbilityStation();
	virtual void OnConstruction(const FTransform& Transform) override;
	EGolemAbilityStationKind GetStationKind() const;
	float GetInteractionRange() const;
	void SetStationKind(EGolemAbilityStationKind InKind);
	FGolemResult TryActivate(AGolemCharacter* InstigatorGolem, AGolemPartyState* Party,
		AGolemAbilityDummy* DamageSource, AGolemCharacter*& OutAffectedGolem);

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditInstanceOnly, Category = "Golem|AbilityYard")
	EGolemAbilityStationKind StationKind = EGolemAbilityStationKind::DamagePlayer;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|AbilityYard", meta = (Units = "cm", ClampMin = "50"))
	float InteractionRange = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|AbilityYard", meta = (ClampMin = "1"))
	float TestDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|AbilityYard", meta = (Units = "cm", ClampMin = "100"))
	float CompanionRange = 700.0f;
};
