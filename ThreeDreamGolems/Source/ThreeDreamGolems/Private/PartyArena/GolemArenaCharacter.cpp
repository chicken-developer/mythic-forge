#include "PartyArena/GolemArenaCharacter.h"

#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Effects/GolemArenaResourceRecoveryEffect.h"
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
	if (!RecoveryEffect.IsValid())
	{
		UGolemAbilitySystemComponent* ASC = GetGolemAbilitySystem();
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			UGolemArenaResourceRecoveryEffect::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ArenaRecovery"), FText::GetEmpty());
		}
		RecoveryEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!RecoveryEffect.IsValid())
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaRecovery"),
				NSLOCTEXT("GolemArena", "RecoveryRejected", "The arena resource recovery effect was rejected."));
		}
	}
	return FGolemResult::Success();
}

UGolemThreatComponent* AGolemArenaCharacter::GetThreatComponent() const
{
	return Threat;
}

FActiveGameplayEffectHandle AGolemArenaCharacter::GetRecoveryEffectHandle() const
{
	return RecoveryEffect;
}

void AGolemArenaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && RecoveryEffect.IsValid())
	{
		GetGolemAbilitySystem()->RemoveActiveGameplayEffect(RecoveryEffect);
		RecoveryEffect.Invalidate();
	}
	Super::EndPlay(EndPlayReason);
}
