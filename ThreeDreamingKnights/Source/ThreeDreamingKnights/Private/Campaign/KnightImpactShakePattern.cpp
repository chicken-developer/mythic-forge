#include "Campaign/KnightImpactShakePattern.h"

namespace
{
	constexpr float Duration = 0.13f;
}

void UKnightImpactShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UKnightImpactShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.0f;
}

void UKnightImpactShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,
	FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float Envelope = FMath::Square(FMath::Clamp(1.0f - Elapsed / Duration, 0.0f, 1.0f));
	OutResult.Rotation.Pitch = 0.25f * Envelope * FMath::Sin(Elapsed * 100.0f);
}

bool UKnightImpactShakePattern::IsFinishedImpl() const
{
	return Elapsed >= Duration;
}

void UKnightImpactShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	Elapsed = Duration;
}
