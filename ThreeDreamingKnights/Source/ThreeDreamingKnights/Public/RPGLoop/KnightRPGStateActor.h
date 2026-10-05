#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/KnightResult.h"
#include "KnightRPGStateActor.generated.h"

class UKnightInventoryComponent;
class UKnightRPGComponent;

UENUM(BlueprintType)
enum class EKnightRPGLoopStage : uint8
{
	EncounterA,
	RewardAvailable,
	Upgrade,
	EncounterB,
	Complete
};

USTRUCT(BlueprintType)
struct THREEDREAMINGKNIGHTS_API FKnightRPGLoopSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Loop")
	EKnightRPGLoopStage Stage = EKnightRPGLoopStage::EncounterA;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Loop")
	int64 Revision = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Loop")
	int32 RewardPickupCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Loop")
	bool bRewardIssued = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGLoopChanged, const FKnightRPGLoopSnapshot&);

/** Host-owned shared inventory and three per-Knight readmodels have the same replicated actor lifetime. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGStateActor : public AActor
{
	GENERATED_BODY()

public:
	AKnightRPGStateActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UKnightInventoryComponent* GetInventory() const;
	UKnightRPGComponent* GetRPG() const;
	const FKnightRPGLoopSnapshot& GetLoopSnapshot() const;
	FOnKnightRPGLoopChanged OnLoopChanged;

protected:
	virtual void BeginPlay() override;

private:
	friend class AKnightRPGLoopGameMode;
	FKnightResult CommitLoopSnapshot(const FKnightRPGLoopSnapshot& Candidate, int64 ExpectedRevision);

	UFUNCTION()
	void OnRep_LoopSnapshot();

	UPROPERTY(VisibleAnywhere, Category = "Knight|RPG|Domains")
	TObjectPtr<UKnightInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Knight|RPG|Domains")
	TObjectPtr<UKnightRPGComponent> RPG;

	UPROPERTY(ReplicatedUsing = OnRep_LoopSnapshot)
	FKnightRPGLoopSnapshot LoopSnapshot;
};
