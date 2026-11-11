#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GolemResult.h"
#include "GolemRPGStateActor.generated.h"

class UGolemInventoryComponent;
class UGolemRPGComponent;

UENUM(BlueprintType)
enum class EGolemRPGLoopStage : uint8
{
	EncounterA,
	RewardAvailable,
	Upgrade,
	EncounterB,
	Complete
};

USTRUCT(BlueprintType)
struct THREEDREAMGOLEMS_API FGolemRPGLoopSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Loop")
	EGolemRPGLoopStage Stage = EGolemRPGLoopStage::EncounterA;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Loop")
	int64 Revision = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Loop")
	int32 RewardPickupCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Loop")
	bool bRewardIssued = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGLoopChanged, const FGolemRPGLoopSnapshot&);

/** Host-owned shared inventory and three per-Golem readmodels have the same replicated actor lifetime. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGStateActor : public AActor
{
	GENERATED_BODY()

public:
	AGolemRPGStateActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UGolemInventoryComponent* GetInventory() const;
	UGolemRPGComponent* GetRPG() const;
	const FGolemRPGLoopSnapshot& GetLoopSnapshot() const;
	FOnGolemRPGLoopChanged OnLoopChanged;

protected:
	virtual void BeginPlay() override;

private:
	friend class AGolemRPGLoopGameMode;
	FGolemResult CommitLoopSnapshot(const FGolemRPGLoopSnapshot& Candidate, int64 ExpectedRevision);

	UFUNCTION()
	void OnRep_LoopSnapshot();

	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Domains")
	TObjectPtr<UGolemInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Domains")
	TObjectPtr<UGolemRPGComponent> RPG;

	UPROPERTY(ReplicatedUsing = OnRep_LoopSnapshot)
	FGolemRPGLoopSnapshot LoopSnapshot;
};
