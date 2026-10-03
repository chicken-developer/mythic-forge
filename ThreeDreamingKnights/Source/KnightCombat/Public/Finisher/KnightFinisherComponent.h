#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightFinisherComponent.generated.h"

class ACharacter;
class AController;
class UAnimMontage;
class UGameplayAbility;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightFinisherEnded, FGuid, bool);

/** A target grants one exclusive reservation; both GAS montages and all transient state share that GUID. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTCOMBAT_API UKnightFinisherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightFinisherComponent();
	FKnightResult TryBeginFinisher(ACharacter* Target, UAnimMontage* SourceMontage,
		UAnimMontage* TargetMontage, FGuid& OutReservation, UGameplayAbility* SourceAbility = nullptr);
	FKnightResult EndFinisher(const FGuid& Reservation, bool bComplete);
	void PrepareForEndPlay(bool bInWorldEnding);
	bool HasActiveFinisher() const;
	bool IsReserved() const;
	FGuid GetReservation() const;
	float GetActiveDuration() const;
	FOnKnightFinisherEnded OnFinisherEnded;
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
	TWeakObjectPtr<UKnightFinisherComponent> ReservedSource;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightFinisherComponent> TargetComponent;
	UPROPERTY(Transient)
	TWeakObjectPtr<AController> StartingController;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Finisher", meta = (ClampMin = "50", Units = "cm"))
	float MaximumRange = 220.0f;
	FKnightEntityId SourceIdentity;
	FKnightEntityId TargetIdentity;
	FGuid CameraHandle;
	float StartedAt = 0.0f;
	float ActiveDuration = 0.0f;
	uint8 PreviousMovementMode = 0;
	uint8 PreviousCustomMode = 0;
	bool bOwnFinishingTag = false;
	bool bEndingPlay = false;
	bool bWorldEnding = false;
};
