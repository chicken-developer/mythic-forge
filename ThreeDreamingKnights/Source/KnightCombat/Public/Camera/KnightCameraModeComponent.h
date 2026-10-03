#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/KnightResult.h"
#include "KnightCameraModeComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;
class APlayerController;

USTRUCT(BlueprintType)
struct KNIGHTCOMBAT_API FKnightCameraModeSettings
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Camera")
	float FieldOfView = 65.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Camera")
	float ArmLength = 260.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Camera")
	FVector SocketOffset = FVector(0.0f, 65.0f, 65.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Camera")
	int32 Priority = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Camera")
	bool bLockMovementInput = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Camera")
	bool bLockLookInput = false;
	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;
};

USTRUCT()
struct KNIGHTCOMBAT_API FKnightCameraModeEntry
{
	GENERATED_BODY()
	UPROPERTY()
	FGuid Handle;
	UPROPERTY()
	FKnightCameraModeSettings Settings;
};

/** Source-owned camera layers replicate to the owning pawn and balance only their own input locks. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTCOMBAT_API UKnightCameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightCameraModeComponent();
	FKnightResult Configure(USpringArmComponent* InBoom, UCameraComponent* InCamera);
	FGuid PushMode(const FKnightCameraModeSettings& Settings);
	FKnightResult PopMode(const FGuid& Handle);
	void ClearModes();
	void HandleControllerChanged();
	int32 GetModeCount() const;
	bool HasMode(const FGuid& Handle) const;
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
	const FKnightCameraModeEntry* FindTopMode() const;
	UPROPERTY(ReplicatedUsing = OnRep_Modes)
	TArray<FKnightCameraModeEntry> Modes;
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
};
