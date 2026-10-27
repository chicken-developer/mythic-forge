#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "KnightImpactShakePattern.generated.h"

/** Short additive feedback; gameplay timing and the camera mode stack are unaffected. */
UCLASS()
class UKnightImpactShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,
		FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	float Elapsed = 0.0f;
};
