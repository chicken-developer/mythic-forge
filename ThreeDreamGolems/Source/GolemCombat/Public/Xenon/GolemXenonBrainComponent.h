#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Xenon/GolemXenonDefinition.h"
#include "GolemXenonBrainComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemVoidHostComponent;

UENUM(BlueprintType)
enum class EGolemXenonAction : uint8
{
	/** No Golem in range, or the brain is switched off. */
	Idle,
	/** Turning towards and walking at the nearest Golem. */
	Chase,
	/** Standing still, showing where the hit will land. */
	Telegraph,
	/** Standing still after the hit. */
	Recover,
	/** Stunned by a filled void. */
	Staggered,
	/** Restored: it has its colour back and no longer fights. */
	Restored
};

DECLARE_MULTICAST_DELEGATE(FOnGolemXenonActionChanged);

/**
 * The whole behaviour of an ordinary Xenon in M0: walk at the nearest Golem, show an attack for
 * its telegraph time, hit everything inside the shown zone, stand still, repeat. A fill stuns it
 * and cancels a telegraphed attack; a restored Xenon stops for good. The host decides; clients
 * get the action, its end time and the zone, enough to draw the warning.
 */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMCOMBAT_API UGolemXenonBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemXenonBrainComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Authority only. Damage of one hit is the ability system's Power times the move's scale. */
	FGolemResult InitializeBrain(UGolemAbilitySystemComponent* InAbilitySystem, UGolemVoidHostComponent* InHost,
		const FGolemXenonBehavior& InBehavior);

	/** A disabled brain stands idle. Tests and scripted moments use it. */
	void SetBrainEnabled(bool bEnabled);
	bool IsBrainEnabled() const { return bBrainEnabled; }

	/** One step of the behaviour at host time Now. The tick calls it. */
	void Step(float DeltaSeconds, double Now);

	EGolemXenonAction GetAction() const { return Action; }
	double GetActionEndServerTime() const { return ActionEnd; }
	float GetZoneRange() const { return ZoneRange; }
	float GetZoneHalfAngleDegrees() const { return ZoneHalfAngle; }
	float GetZoneYaw() const { return ZoneYaw; }
	int32 GetStrikeCount() const { return StrikeCount; }
	AActor* GetTarget() const { return Target.Get(); }

	/** True while Location is inside the zone of an attack this Xenon is telegraphing. */
	bool IsThreatening(const FVector& Location) const;

	/** The telegraphing Xenon whose zone contains Location, if any. */
	static AActor* FindThreat(const UWorld* World, const FVector& Location);

	FOnGolemXenonActionChanged OnActionChanged;

private:
	void SetAction(EGolemXenonAction NewAction, double NewEnd);
	AActor* FindTarget() const;
	void Strike();
	bool IsInsideZone(const FVector& Location) const;

	UFUNCTION()
	void OnRep_Action();

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	EGolemXenonAction Action = EGolemXenonAction::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	double ActionEnd = 0.0;

	/** The shown zone: a wedge of ZoneRange and ZoneHalfAngle around ZoneYaw, fixed when the telegraph starts. */
	UPROPERTY(ReplicatedUsing = OnRep_Action)
	float ZoneRange = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	float ZoneHalfAngle = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	float ZoneYaw = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Xenon")
	FGolemXenonBehavior Behavior;

	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UGolemVoidHostComponent> Host;
	TWeakObjectPtr<AActor> Target;
	int32 MoveIndex = INDEX_NONE;
	int32 StrikeCount = 0;
	bool bInitialized = false;
	bool bBrainEnabled = true;
};
