#include "PartyArena/GolemArenaCharacter.h"

#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Threat/GolemThreatComponent.h"

AGolemArenaCharacter::AGolemArenaCharacter()
{
	Threat = CreateDefaultSubobject<UGolemThreatComponent>(TEXT("ArenaThreat"));
}

FGolemResult AGolemArenaCharacter::InitializeArenaCombat()
{
	if (!HasAuthority() || !GetAbilityInitializationResult().IsSuccess())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaCombat"),
			NSLOCTEXT("GolemArena", "PawnNotReady", "Arena combat requires an initialized authoritative Golem."));
	}
	const FGolemResult Configure = GetCombatComponent()->Configure(
		FPrimaryAssetId(TEXT("GolemCombatProfile"), TEXT("FoundationCombat")));
	if (!Configure.IsSuccess())
	{
		return Configure;
	}
	return FGolemResult::Success();
}

UGolemThreatComponent* AGolemArenaCharacter::GetThreatComponent() const
{
	return Threat;
}

void AGolemArenaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
