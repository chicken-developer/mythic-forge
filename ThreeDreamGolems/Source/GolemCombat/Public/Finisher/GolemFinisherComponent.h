#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemFinisherComponent.generated.h"

class ACharacter;
class AController;
class UAnimMontage;
class UGameplayAbility;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemFinisherEnded, FGuid, bool);

/** A target grants one exclusive reservation; both GAS montages and all transient state share that GUID. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMCOMBAT_API UGolemFinisherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemFinisherComponent();
	FGolemResult TryBeginFinisher(ACharacter* Target, UAnimMontage* SourceMontage,
		UAnimMontage* TargetMontage, FGuid& OutReservation, UGameplayAbility* SourceAbility = nullptr);
	FGolemResult EndFinisher(const FGuid& Reservation, bool bComplete);
	void PrepareForEndPlay(bool bInWorldEnding);
	bool HasActiveFinisher() const;
	bool IsReserved() const;
	FGuid GetReservation() const;
	float GetActiveDuration() const;
	FOnGolemFinisherEnded OnFinisherEnded;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void RestoreMovement();
	void AbortReservation();
	UPROPERTY(Replicated)
	FGuid ReservationId;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemFinisherComponent> ReservedSource;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemFinisherComponent> TargetComponent;
	UPROPERTY(Transient)
	TWeakObjectPtr<AController> StartingController;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Finisher", meta = (ClampMin = "50", Units = "cm"))
	float MaximumRange = 220.0f;
	FGolemEntityId SourceIdentity;
	FGolemEntityId TargetIdentity;
	FGuid CameraHandle;
	float StartedAt = 0.0f;
	float ActiveDuration = 0.0f;
	uint8 PreviousMovementMode = 0;
	uint8 PreviousCustomMode = 0;
	bool bOwnFinishingTag = false;
	bool bEndingPlay = false;
	bool bWorldEnding = false;
};
