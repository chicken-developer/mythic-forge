#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemFrozenSurface.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Temporary collision with a replicated warning and authority safe ejection before thaw. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemFrozenSurface : public AActor
{
	GENERATED_BODY()

public:
	AGolemFrozenSurface();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void FreezeSurface();
	void ResetSurface();
	bool IsFrozen() const;
	bool IsThawWarning() const;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void WarnThaw();
	void ThawSurface();
	UFUNCTION()
	void OnRep_Surface();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(ReplicatedUsing = OnRep_Surface)
	bool bFrozen = false;
	UPROPERTY(ReplicatedUsing = OnRep_Surface)
	bool bWarning = false;
	FTimerHandle WarningTimer;
	FTimerHandle ThawTimer;
};
