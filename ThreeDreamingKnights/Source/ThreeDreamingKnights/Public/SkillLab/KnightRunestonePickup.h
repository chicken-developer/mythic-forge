#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightRunestonePickup.generated.h"

class AKnightCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EKnightRunestonePickupKind : uint8
{
	Healing, Fire, PowerCrystal
};

/** F interaction consumes one authoritative charge; full HP never consumes a healing charge. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRunestonePickup : public AActor
{
	GENERATED_BODY()

public:
	AKnightRunestonePickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ConfigurePickup(EKnightRunestonePickupKind InKind, FGuid InEncounterId);
	FKnightResult TryCollect(AKnightCharacter* Knight, FGuid RequestId);
	int32 GetRemainingCharges() const;
	EKnightRunestonePickupKind GetKind() const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Pickup();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(ReplicatedUsing = OnRep_Pickup)
	EKnightRunestonePickupKind Kind = EKnightRunestonePickupKind::Healing;
	UPROPERTY(ReplicatedUsing = OnRep_Pickup)
	int32 Charges = 1;
	UPROPERTY(Replicated)
	FKnightEntityId PickupId;
	FGuid EncounterId;
	TSet<FGuid> ConsumedRequests;
	bool bCollecting = false;
	int64 Revision = 0;
};
