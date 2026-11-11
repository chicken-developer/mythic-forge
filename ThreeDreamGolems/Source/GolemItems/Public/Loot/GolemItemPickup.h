#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Models/GolemItemModels.h"
#include "GolemItemPickup.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UGolemRPGComponent;

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemPickupView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Loot")
	FGuid PickupId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Loot")
	FGolemItemInstance Item;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Loot")
	bool bClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Loot")
	FGuid ClaimRequestId;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemPickupChanged);

/** A real replicated world pickup. The shared RPG coordinator alone may reserve and consume it. */
UCLASS()
class GOLEMITEMS_API AGolemItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AGolemItemPickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializePickup(const FGuid& InPickupId, const FGolemItemInstance& InItem);
	FGuid GetPickupId() const;
	FGolemPickupView GetView() const;
	bool IsClaimed() const;
	FOnGolemPickupChanged OnPickupChanged;

private:
	friend class UGolemRPGComponent;
	FGolemResult ReserveClaim(const FGuid& RequestId, FGuid& OutToken);
	FGolemResult AbortClaim(const FGuid& Token);
	void CommitClaim(const FGuid& Token, const FGuid& RequestId);
	void PublishClaim();
	void RefreshPresentation();
	UFUNCTION()
	void OnRep_View();

	UPROPERTY(VisibleAnywhere, Category = "Golem|Items|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Items|Loot")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(ReplicatedUsing = OnRep_View)
	FGolemPickupView View;
	FGuid ReservedToken;
};
