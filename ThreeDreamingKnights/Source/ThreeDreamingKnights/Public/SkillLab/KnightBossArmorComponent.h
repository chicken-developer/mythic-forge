#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/KnightArmorResponse.h"
#include "Types/KnightStableIds.h"
#include "KnightBossArmorComponent.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EKnightBossArmorState : uint8
{
	Intact, Brittle, Broken
};

DECLARE_MULTICAST_DELEGATE(FOnKnightBossArmorChanged);

/** One tagged sandbox armor zone; durability is separate from HP and poise. */
UCLASS(ClassGroup = Knight)
class THREEDREAMINGKNIGHTS_API UKnightBossArmorComponent : public UActorComponent, public IKnightArmorResponse
{
	GENERATED_BODY()

public:
	UKnightBossArmorComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void EnableArmorZone(FGuid InEncounterId);
	bool MakeBrittle();
	void ResetArmor(FGuid InEncounterId);
	virtual void ResolveArmorHit(const FHitResult& Hit, const FGuid& ActivationId, int32 HitIndex,
		bool bHeavy, float RawDamage, float& OutDamage) override;
	EKnightBossArmorState GetArmorState() const;
	float GetDurability() const;
	float GetStateEndServerTime() const;
	UStaticMeshComponent* GetArmorZone() const;
	FOnKnightBossArmorChanged OnArmorChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void ExpireArmorState();
	void PublishArmorChange();
	UFUNCTION()
	void OnRep_Armor();
	UPROPERTY(ReplicatedUsing = OnRep_Armor)
	EKnightBossArmorState ArmorState = EKnightBossArmorState::Intact;
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
