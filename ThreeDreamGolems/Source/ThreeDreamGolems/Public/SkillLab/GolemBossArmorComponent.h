#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/GolemArmorResponse.h"
#include "Types/GolemStableIds.h"
#include "GolemBossArmorComponent.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGolemBossArmorState : uint8
{
	Intact, Brittle, Broken
};

DECLARE_MULTICAST_DELEGATE(FOnGolemBossArmorChanged);

/** One tagged sandbox armor zone; durability is separate from HP and poise. */
UCLASS(ClassGroup = Golem)
class THREEDREAMGOLEMS_API UGolemBossArmorComponent : public UActorComponent, public IGolemArmorResponse
{
	GENERATED_BODY()

public:
	UGolemBossArmorComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void EnableArmorZone(FGuid InEncounterId);
	bool MakeBrittle();
	void ResetArmor(FGuid InEncounterId);
	virtual void ResolveArmorHit(const FHitResult& Hit, const FGuid& ActivationId, int32 HitIndex,
		bool bHeavy, float RawDamage, float& OutDamage) override;
	EGolemBossArmorState GetArmorState() const;
	float GetDurability() const;
	float GetStateEndServerTime() const;
	UStaticMeshComponent* GetArmorZone() const;
	FOnGolemBossArmorChanged OnArmorChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void ExpireArmorState();
	void PublishArmorChange();
	UFUNCTION()
	void OnRep_Armor();
	UPROPERTY(ReplicatedUsing = OnRep_Armor)
	EGolemBossArmorState ArmorState = EGolemBossArmorState::Intact;
	UPROPERTY(ReplicatedUsing = OnRep_Armor)
	float Durability = 100.0f;
	UPROPERTY(ReplicatedUsing = OnRep_Armor)
	float EndServerTime = 0.0f;
	UPROPERTY(ReplicatedUsing = OnRep_Armor)
	bool bEnabled = false;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ArmorZone;
	FGuid EncounterId;
	FTimerHandle ExpiryTimer;
	TSet<FString> AppliedHits;
	int64 Revision = 0;
};
