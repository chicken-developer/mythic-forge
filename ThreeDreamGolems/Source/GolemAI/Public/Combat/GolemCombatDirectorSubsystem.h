#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemCombatDirectorSubsystem.generated.h"

class APawn;
class AController;
class UGolemAbilitySystemComponent;
class UGolemLifeComponent;

USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemAttackLease
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Director")
	FGuid LeaseId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Director")
	FGolemEntityId SourceId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Director")
	FGolemEntityId TargetId;
	bool IsValid() const;
	bool operator==(const FGolemAttackLease& Other) const;
};

UENUM(BlueprintType)
enum class EGolemAttackLeaseReleaseReason : uint8
{
	Completed,
	Expired,
	LifeUnavailable,
	ActorEndPlay,
	ControllerChanged,
	InvalidParticipant,
	WorldEndPlay
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemAttackLeaseReleased,
	const FGolemAttackLease&, EGolemAttackLeaseReleaseReason);

/** Budget arbitration only. StateTree decides when to request a lease; GAS executes the action. */
UCLASS()
class GOLEMAI_API UGolemCombatDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	FGolemResult TrySetBudgets(int32 InGlobalBudget, int32 InPerTargetBudget);
	FGolemResult TrySetTeamBudgets(int32 InPartyBudget, int32 InHostileBudget, int32 InPerTargetBudget);
	int32 GetHostileBudget() const;
	FGolemResult TryAcquireAttackToken(AActor* Source, AActor* Target, float Duration, FGolemAttackLease& OutLease);
	FGolemResult ReleaseAttackToken(AActor* Source, const FGolemAttackLease& Lease);
	bool IsLeaseActive(const FGolemAttackLease& Lease) const;
	int32 GetActiveLeaseCount() const;
	int32 GetActiveLeaseCountForTarget(const FGolemEntityId& TargetId) const;
	int32 GetGlobalBudget() const;
	int32 GetPerTargetBudget() const;
	bool HasMaintenanceTimer() const;
	FOnGolemAttackLeaseReleased OnAttackLeaseReleased;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	struct FGolemActiveAttackLease
	{
		FGolemAttackLease Capability;
		TWeakObjectPtr<AActor> Source;
		TWeakObjectPtr<AActor> Target;
		TWeakObjectPtr<AController> SourceController;
		double ExpiresAt = 0.0;
	};
	struct FGolemAttackParticipant
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<UGolemLifeComponent> Life;
		FGolemEntityId Id;
		FDelegateHandle LifeHandle;
		int32 References = 0;
	};
	struct FGolemReleasedAttackLease
	{
		FGolemAttackLease Capability;
		TWeakObjectPtr<AActor> Source;
	};

	FGolemResult ValidateAuthority() const;
	FGolemResult ValidateActor(AActor* Actor, UGolemAbilitySystemComponent*& OutAbilitySystem) const;
	bool IsParticipantValid(const TWeakObjectPtr<AActor>& Actor, const FGolemEntityId& Id) const;
	bool IsActiveEntryValid(const FGolemActiveAttackLease& Entry) const;
	void RegisterParticipant(AActor* Actor, UGolemAbilitySystemComponent* AbilitySystem);
	void UnregisterParticipant(const TWeakObjectPtr<AActor>& Actor);
	void ReleaseLeaseInternal(const FGuid& LeaseId, EGolemAttackLeaseReleaseReason Reason, bool bNotify = true);
	void ReleaseActorLeases(AActor* Actor, EGolemAttackLeaseReleaseReason Reason);
	void MaintainLeases();
	void UpdateMaintenanceTimer();
	void ShutdownLeases();
	UFUNCTION()
	void HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* Previous, AController* Current);

	TMap<FGuid, FGolemActiveAttackLease> ActiveLeases;
	TMap<TWeakObjectPtr<AActor>, FGolemAttackParticipant> Participants;
	TMap<FGolemEntityId, FGolemReleasedAttackLease> LastReleasedLeases;
	FTimerHandle MaintenanceTimer;
	int32 GlobalBudget = 2;
	int32 PerTargetBudget = 1;
	int32 PartyBudget = 64;
	int32 HostileBudget = 64;
	bool bReady = false;
};
