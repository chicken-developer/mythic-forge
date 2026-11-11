#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemRunestonePickup.generated.h"

class AGolemCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EGolemRunestonePickupKind : uint8
{
	Healing, Fire, PowerCrystal
};

/** F interaction consumes one authoritative charge; full HP never consumes a healing charge. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRunestonePickup : public AActor
{
	GENERATED_BODY()

public:
	AGolemRunestonePickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ConfigurePickup(EGolemRunestonePickupKind InKind, FGuid InEncounterId);
	FGolemResult TryCollect(AGolemCharacter* Golem, FGuid RequestId);
	int32 GetRemainingCharges() const;
	EGolemRunestonePickupKind GetKind() const;

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
	EGolemRunestonePickupKind Kind = EGolemRunestonePickupKind::Healing;
	UPROPERTY(ReplicatedUsing = OnRep_Pickup)
	int32 Charges = 1;
	UPROPERTY(Replicated)
	FGolemEntityId PickupId;
	FGuid EncounterId;
	TSet<FGuid> ConsumedRequests;
	bool bCollecting = false;
	int64 Revision = 0;
};
