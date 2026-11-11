#include "Skills/GolemEquipmentCombatTypes.h"

FGolemResult FGolemEquipmentCombatBuild::Validate() const
{
	const bool bKnownRelic = RelicEffectId.IsNone() || RelicEffectId == TEXT("Bulwark")
		|| RelicEffectId == TEXT("Lifeseed") || RelicEffectId == TEXT("Starfall");
	if (!bKnownRelic || (RelicEffectId.IsNone() == RelicInstanceId.IsValid())
		|| (!bEnabled && (bHasHammer || !RelicEffectId.IsNone() || RelicInstanceId.IsValid())))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EquipmentCombatBuild"),
			NSLOCTEXT("GolemEquipment", "InvalidCombatBuild", "Equipment combat state requires a valid item source."));
	}
	return FGolemResult::Success();
}

bool FGolemEquipmentCombatBuild::operator==(const FGolemEquipmentCombatBuild& Other) const
{
	return bEnabled == Other.bEnabled && bHasHammer == Other.bHasHammer
		&& RelicInstanceId == Other.RelicInstanceId && RelicEffectId == Other.RelicEffectId;
}
