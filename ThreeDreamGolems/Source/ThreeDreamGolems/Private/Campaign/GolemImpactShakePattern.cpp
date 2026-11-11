#include "Campaign/GolemImpactShakePattern.h"

namespace
{
	constexpr float ShakeDuration = 0.13f;
}

void UGolemImpactShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(ShakeDuration);
}

void UGolemImpactShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.0f;
}

void UGolemImpactShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,
	FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float Envelope = FMath::Square(FMath::Clamp(1.0f - Elapsed / ShakeDuration, 0.0f, 1.0f));
	OutResult.Rotation.Pitch = 0.25f * Envelope * FMath::Sin(Elapsed * 100.0f);
}

bool UGolemImpactShakePattern::IsFinishedImpl() const
{
	return Elapsed >= ShakeDuration;
}

void UGolemImpactShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	Elapsed = ShakeDuration;
}
