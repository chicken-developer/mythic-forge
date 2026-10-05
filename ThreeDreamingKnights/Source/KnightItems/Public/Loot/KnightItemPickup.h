#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Models/KnightItemModels.h"
#include "KnightItemPickup.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UKnightRPGComponent;

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightPickupView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Loot")
	FGuid PickupId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Loot")
	FKnightItemInstance Item;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Loot")
	bool bClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Loot")
	FGuid ClaimRequestId;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightPickupChanged);

/** A real replicated world pickup. The shared RPG coordinator alone may reserve and consume it. */
UCLASS()
class KNIGHTITEMS_API AKnightItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AKnightItemPickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializePickup(const FGuid& InPickupId, const FKnightItemInstance& InItem);
	FGuid GetPickupId() const;
	FKnightPickupView GetView() const;
	bool IsClaimed() const;
	FOnKnightPickupChanged OnPickupChanged;

private:
	friend class UKnightRPGComponent;
	FKnightResult ReserveClaim(const FGuid& RequestId, FGuid& OutToken);
	FKnightResult AbortClaim(const FGuid& Token);
	void CommitClaim(const FGuid& Token, const FGuid& RequestId);
	void PublishClaim();
	void RefreshPresentation();
	UFUNCTION()
	void OnRep_View();

	UPROPERTY(VisibleAnywhere, Category = "Knight|Items|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Items|Loot")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(ReplicatedUsing = OnRep_View)
	FKnightPickupView View;
	FGuid ReservedToken;
};
