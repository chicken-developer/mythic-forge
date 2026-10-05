#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Events/KnightEventSubsystem.h"
#include "Types/KnightStableIds.h"
#include "KnightThreatComponent.generated.h"

class UKnightAbilitySystemComponent;
class UKnightLifeComponent;
struct FKnightDamageConfirmedEvent;
enum class EKnightLifeState : uint8;

struct KNIGHTAI_API FKnightThreatSnapshot
{
	FKnightEntityId TargetId;
	TWeakObjectPtr<AActor> Target;
	float Threat = 0.0f;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightThreatChanged);

/** Authority-owned awareness and confirmed damage threat. Read queries never purge or alter stored scores. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTAI_API UKnightThreatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightThreatComponent();
	FKnightResult InitializeThreat();
	/** Units: threat/s, threat/damage, maximum threat. Tuning changes require an empty table. */
	FKnightResult TrySetThreatTuning(float InDecayPerSecond, float InDamageScale, float InMaxThreat = 10000.0f);
	FKnightResult AddThreat(AActor* Target, float Amount);
	FKnightResult ObserveTarget(AActor* Target, float Awareness = 5.0f);
	FKnightResult ForgetTarget(AActor* Target);
	FKnightResult ClearThreat();
	AActor* GetHighestThreatTarget() const;
	float GetThreat(const FKnightEntityId& TargetId) const;
	int32 GetThreatCount() const;
	int32 GetTrackedEntryCount() const;
	TArray<FKnightThreatSnapshot> GetThreatSnapshot() const;
	bool HasMaintenanceTimer() const;
	FOnKnightThreatChanged OnThreatChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	struct FKnightThreatEntry
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<UKnightLifeComponent> Life;
		FDelegateHandle LifeHandle;
		float Amount = 0.0f;
		double UpdatedAt = 0.0;
	};

	FKnightResult ValidateOwner() const;
	FKnightResult ValidateTarget(AActor* Target, UKnightAbilitySystemComponent*& OutAbilitySystem) const;
	bool IsEntryValid(const FKnightEntityId& Id, const FKnightThreatEntry& Entry) const;
	float CalculateThreat(const FKnightThreatEntry& Entry) const;
	void HandleDamageConfirmed(const FKnightDamageConfirmedEvent& Event);
	void HandleOwnerLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleTargetUnavailable(const FKnightEntityId& Id);
	void MaintainThreat();
	void RemoveEntry(const FKnightEntityId& Id);
	void ResetEntries(bool bNotify);
	void UpdateMaintenanceTimer();
	UFUNCTION()
	void HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason);

	TMap<FKnightEntityId, FKnightThreatEntry> Entries;
	TWeakObjectPtr<UKnightAbilitySystemComponent> OwnerAbilitySystem;
	TWeakObjectPtr<UKnightLifeComponent> OwnerLife;
	FKnightEntityId OwnerId;
	FKnightEventSubscription DamageSubscription;
	FDelegateHandle OwnerLifeHandle;
	FTimerHandle MaintenanceTimer;
	float DecayPerSecond = 2.0f;
	float DamageScale = 1.0f;
	float MaxThreat = 10000.0f;
	bool bEndingPlay = false;
};
