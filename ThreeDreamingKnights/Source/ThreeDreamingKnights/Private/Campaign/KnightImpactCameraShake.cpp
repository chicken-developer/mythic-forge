#include "Campaign/KnightImpactCameraShake.h"

#include "Campaign/KnightImpactShakePattern.h"

UKnightImpactCameraShake::UKnightImpactCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UKnightImpactShakePattern>(TEXT("ImpactPattern")));
}
