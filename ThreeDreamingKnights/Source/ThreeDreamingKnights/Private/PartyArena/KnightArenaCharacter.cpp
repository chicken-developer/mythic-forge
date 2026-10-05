#include "PartyArena/KnightArenaCharacter.h"

#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Effects/KnightArenaResourceRecoveryEffect.h"
#include "Threat/KnightThreatComponent.h"

AKnightArenaCharacter::AKnightArenaCharacter()
{
	Threat = CreateDefaultSubobject<UKnightThreatComponent>(TEXT("ArenaThreat"));
}

FKnightResult AKnightArenaCharacter::InitializeArenaCombat()
{
	if (!HasAuthority() || !GetAbilityInitializationResult().IsSuccess())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaCombat"),
			NSLOCTEXT("KnightArena", "PawnNotReady", "Arena combat requires an initialized authoritative Knight."));
	}
	const FKnightResult Configure = GetCombatComponent()->Configure(
		FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat")));
	if (!Configure.IsSuccess())
	{
		return Configure;
	}
	if (!RecoveryEffect.IsValid())
	{
		UKnightAbilitySystemComponent* ASC = GetKnightAbilitySystem();
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			UKnightArenaResourceRecoveryEffect::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ArenaRecovery"), FText::GetEmpty());
		}
		RecoveryEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!RecoveryEffect.IsValid())
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaRecovery"),
				NSLOCTEXT("KnightArena", "RecoveryRejected", "The arena resource recovery effect was rejected."));
		}
	}
	return FKnightResult::Success();
}

UKnightThreatComponent* AKnightArenaCharacter::GetThreatComponent() const
{
	return Threat;
}

FActiveGameplayEffectHandle AKnightArenaCharacter::GetRecoveryEffectHandle() const
{
	return RecoveryEffect;
}

void AKnightArenaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && RecoveryEffect.IsValid())
	{
		GetKnightAbilitySystem()->RemoveActiveGameplayEffect(RecoveryEffect);
		RecoveryEffect.Invalidate();
	}
	Super::EndPlay(EndPlayReason);
}
