#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GolemResult.h"
#include "GolemCameraModeComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;
class APlayerController;

USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemCameraModeSettings
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Camera")
	float FieldOfView = 65.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Camera")
	float ArmLength = 260.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Camera")
	FVector SocketOffset = FVector(0.0f, 65.0f, 65.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Camera")
	int32 Priority = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Camera")
	bool bLockMovementInput = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Camera")
	bool bLockLookInput = false;
	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;
};

USTRUCT()
struct GOLEMCOMBAT_API FGolemCameraModeEntry
{
	GENERATED_BODY()
	UPROPERTY()
	FGuid Handle;
	UPROPERTY()
	FGolemCameraModeSettings Settings;
};

/** Source-owned camera layers replicate to the owning pawn and balance only their own input locks. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMCOMBAT_API UGolemCameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemCameraModeComponent();
	FGolemResult Configure(USpringArmComponent* InBoom, UCameraComponent* InCamera);
	FGuid PushMode(const FGolemCameraModeSettings& Settings);
	FGolemResult PopMode(const FGuid& Handle);
	void ClearModes();
	void HandleControllerChanged();
	int32 GetModeCount() const;
	bool HasMode(const FGuid& Handle) const;
	void SetLocalMotionPreferences(float ShakeScale, bool bReduceMotion);
	float GetLocalFeedbackScale() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void OnRep_Modes();
	void ApplyTopMode();
	void ReleaseInputLocks();
	const FGolemCameraModeEntry* FindTopMode() const;
	UPROPERTY(ReplicatedUsing = OnRep_Modes)
	TArray<FGolemCameraModeEntry> Modes;
	UPROPERTY(Transient)
	TWeakObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> Camera;
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> LockedController;
	float BaseFieldOfView = 90.0f;
	float BaseArmLength = 420.0f;
	FVector BaseSocketOffset = FVector::ZeroVector;
	bool bOwnMoveLock = false;
	bool bOwnLookLock = false;
	bool bConfigured = false;
	bool bEndingPlay = false;
	bool bReducedMotion = false;
	bool bBaseCameraLag = false;
	bool bBaseRotationLag = false;
	float LocalFeedbackScale = 1.0f;
};
