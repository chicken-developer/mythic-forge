#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightCombatDirectorSubsystem.generated.h"

class APawn;
class AController;
class UKnightAbilitySystemComponent;
class UKnightLifeComponent;

USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightAttackLease
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Director")
	FGuid LeaseId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Director")
	FKnightEntityId SourceId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Director")
	FKnightEntityId TargetId;
	bool IsValid() const;
	bool operator==(const FKnightAttackLease& Other) const;
};

UENUM(BlueprintType)
enum class EKnightAttackLeaseReleaseReason : uint8
{
	Completed,
	Expired,
	LifeUnavailable,
	ActorEndPlay,
	ControllerChanged,
	InvalidParticipant,
	WorldEndPlay
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightAttackLeaseReleased,
	const FKnightAttackLease&, EKnightAttackLeaseReleaseReason);

/** Budget arbitration only. StateTree decides when to request a lease; GAS executes the action. */
UCLASS()
class KNIGHTAI_API UKnightCombatDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	FKnightResult TrySetBudgets(int32 InGlobalBudget, int32 InPerTargetBudget);
	FKnightResult TrySetTeamBudgets(int32 InPartyBudget, int32 InHostileBudget, int32 InPerTargetBudget);
	int32 GetHostileBudget() const;
	FKnightResult TryAcquireAttackToken(AActor* Source, AActor* Target, float Duration, FKnightAttackLease& OutLease);
	FKnightResult ReleaseAttackToken(AActor* Source, const FKnightAttackLease& Lease);
	bool IsLeaseActive(const FKnightAttackLease& Lease) const;
	int32 GetActiveLeaseCount() const;
	int32 GetActiveLeaseCountForTarget(const FKnightEntityId& TargetId) const;
	int32 GetGlobalBudget() const;
	int32 GetPerTargetBudget() const;
	bool HasMaintenanceTimer() const;
	FOnKnightAttackLeaseReleased OnAttackLeaseReleased;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	struct FKnightActiveAttackLease
	{
		FKnightAttackLease Capability;
		TWeakObjectPtr<AActor> Source;
		TWeakObjectPtr<AActor> Target;
		TWeakObjectPtr<AController> SourceController;
		double ExpiresAt = 0.0;
	};
	struct FKnightAttackParticipant
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<UKnightLifeComponent> Life;
		FKnightEntityId Id;
		FDelegateHandle LifeHandle;
		int32 References = 0;
	};
	struct FKnightReleasedAttackLease
	{
		FKnightAttackLease Capability;
		TWeakObjectPtr<AActor> Source;
	};

	FKnightResult ValidateAuthority() const;
	FKnightResult ValidateActor(AActor* Actor, UKnightAbilitySystemComponent*& OutAbilitySystem) const;
	bool IsParticipantValid(const TWeakObjectPtr<AActor>& Actor, const FKnightEntityId& Id) const;
	bool IsActiveEntryValid(const FKnightActiveAttackLease& Entry) const;
	void RegisterParticipant(AActor* Actor, UKnightAbilitySystemComponent* AbilitySystem);
	void UnregisterParticipant(const TWeakObjectPtr<AActor>& Actor);
	void ReleaseLeaseInternal(const FGuid& LeaseId, EKnightAttackLeaseReleaseReason Reason, bool bNotify = true);
	void ReleaseActorLeases(AActor* Actor, EKnightAttackLeaseReleaseReason Reason);
	void MaintainLeases();
	void UpdateMaintenanceTimer();
	void ShutdownLeases();
	UFUNCTION()
	void HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* Previous, AController* Current);

	TMap<FGuid, FKnightActiveAttackLease> ActiveLeases;
	TMap<TWeakObjectPtr<AActor>, FKnightAttackParticipant> Participants;
	TMap<FKnightEntityId, FKnightReleasedAttackLease> LastReleasedLeases;
	FTimerHandle MaintenanceTimer;
	int32 GlobalBudget = 2;
	int32 PerTargetBudget = 1;
	int32 PartyBudget = 64;
	int32 HostileBudget = 64;
	bool bReady = false;
};
