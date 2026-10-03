#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/KnightResult.h"
#include "KnightAbilityStation.generated.h"

class AKnightAbilityDummy;
class AKnightCharacter;
class AKnightPartyState;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EKnightAbilityStationKind : uint8
{
	DamagePlayer,
	DownCompanion
};

/** Explicit exercise interactions apply server GAS damage through the same execution as abilities. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightAbilityStation : public AActor
{
	GENERATED_BODY()

public:
	AKnightAbilityStation();
	virtual void OnConstruction(const FTransform& Transform) override;
	EKnightAbilityStationKind GetStationKind() const;
	float GetInteractionRange() const;
	void SetStationKind(EKnightAbilityStationKind InKind);
	FKnightResult TryActivate(AKnightCharacter* InstigatorKnight, AKnightPartyState* Party,
		AKnightAbilityDummy* DamageSource, AKnightCharacter*& OutAffectedKnight);

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditInstanceOnly, Category = "Knight|AbilityYard")
	EKnightAbilityStationKind StationKind = EKnightAbilityStationKind::DamagePlayer;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|AbilityYard", meta = (Units = "cm", ClampMin = "50"))
	float InteractionRange = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|AbilityYard", meta = (ClampMin = "1"))
	float TestDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|AbilityYard", meta = (Units = "cm", ClampMin = "100"))
	float CompanionRange = 700.0f;
};
