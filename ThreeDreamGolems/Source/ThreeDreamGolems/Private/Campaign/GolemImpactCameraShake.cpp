#include "Campaign/GolemImpactCameraShake.h"

#include "Campaign/GolemImpactShakePattern.h"

UGolemImpactCameraShake::UGolemImpactCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UGolemImpactShakePattern>(TEXT("ImpactPattern")));
}
