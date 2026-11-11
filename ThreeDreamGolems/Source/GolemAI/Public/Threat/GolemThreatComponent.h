#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Events/GolemEventSubsystem.h"
#include "Types/GolemStableIds.h"
#include "Damage/GolemTauntResponse.h"
#include "GolemThreatComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemLifeComponent;
struct FGolemDamageConfirmedEvent;
enum class EGolemLifeState : uint8;

struct GOLEMAI_API FGolemThreatSnapshot
{
	FGolemEntityId TargetId;
	TWeakObjectPtr<AActor> Target;
	float Threat = 0.0f;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemThreatChanged);

/** Authority-owned awareness and confirmed damage threat. Read queries never purge or alter stored scores. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMAI_API UGolemThreatComponent : public UActorComponent, public IGolemTauntResponse
{
	GENERATED_BODY()

public:
	UGolemThreatComponent();
	FGolemResult InitializeThreat();
	/** Units: threat/s, threat/damage, maximum threat. Tuning changes require an empty table. */
	FGolemResult TrySetThreatTuning(float InDecayPerSecond, float InDamageScale, float InMaxThreat = 10000.0f);
	FGolemResult AddThreat(AActor* Target, float Amount);
	FGolemResult ObserveTarget(AActor* Target, float Awareness = 5.0f);
	FGolemResult ForgetTarget(AActor* Target);
	FGolemResult ClearThreat();
	AActor* GetHighestThreatTarget() const;
	virtual void ApplySkillTaunt(AActor* Source, float Duration) override;
	float GetThreat(const FGolemEntityId& TargetId) const;
	int32 GetThreatCount() const;
	int32 GetTrackedEntryCount() const;
	TArray<FGolemThreatSnapshot> GetThreatSnapshot() const;
	bool HasMaintenanceTimer() const;
	FOnGolemThreatChanged OnThreatChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	struct FGolemThreatEntry
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<UGolemLifeComponent> Life;
		FDelegateHandle LifeHandle;
		float Amount = 0.0f;
		double UpdatedAt = 0.0;
	};

	FGolemResult ValidateOwner() const;
	FGolemResult ValidateTarget(AActor* Target, UGolemAbilitySystemComponent*& OutAbilitySystem) const;
	bool IsEntryValid(const FGolemEntityId& Id, const FGolemThreatEntry& Entry) const;
	float CalculateThreat(const FGolemThreatEntry& Entry) const;
	void HandleDamageConfirmed(const FGolemDamageConfirmedEvent& Event);
	void HandleOwnerLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleTargetUnavailable(const FGolemEntityId& Id);
	void MaintainThreat();
	void RemoveEntry(const FGolemEntityId& Id);
	void ResetEntries(bool bNotify);
	void UpdateMaintenanceTimer();
	UFUNCTION()
	void HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason);

	TMap<FGolemEntityId, FGolemThreatEntry> Entries;
	TWeakObjectPtr<UGolemAbilitySystemComponent> OwnerAbilitySystem;
	TWeakObjectPtr<UGolemLifeComponent> OwnerLife;
	FGolemEntityId OwnerId;
	FGolemEventSubscription DamageSubscription;
	FDelegateHandle OwnerLifeHandle;
	FTimerHandle MaintenanceTimer;
	float DecayPerSecond = 2.0f;
	float DamageScale = 1.0f;
	float MaxThreat = 10000.0f;
	bool bEndingPlay = false;
	TWeakObjectPtr<AActor> TauntTarget;
	double TauntEnd = 0.0;
};
