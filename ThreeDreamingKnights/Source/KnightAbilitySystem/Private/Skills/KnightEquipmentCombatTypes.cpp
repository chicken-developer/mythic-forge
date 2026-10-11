#include "Skills/KnightEquipmentCombatTypes.h"

FKnightResult FKnightEquipmentCombatBuild::Validate() const
{
	const bool bKnownRelic = RelicEffectId.IsNone() || RelicEffectId == TEXT("Bulwark")
		|| RelicEffectId == TEXT("Lifeseed") || RelicEffectId == TEXT("Starfall");
	if (!bKnownRelic || (RelicEffectId.IsNone() == RelicInstanceId.IsValid())
		|| (!bEnabled && (bHasHammer || !RelicEffectId.IsNone() || RelicInstanceId.IsValid())))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("EquipmentCombatBuild"),
			NSLOCTEXT("KnightEquipment", "InvalidCombatBuild", "Equipment combat state requires a valid item source."));
	}
	return FKnightResult::Success();
}

bool FKnightEquipmentCombatBuild::operator==(const FKnightEquipmentCombatBuild& Other) const
{
	return bEnabled == Other.bEnabled && bHasHammer == Other.bHasHammer
		&& RelicInstanceId == Other.RelicInstanceId && RelicEffectId == Other.RelicEffectId;
}
