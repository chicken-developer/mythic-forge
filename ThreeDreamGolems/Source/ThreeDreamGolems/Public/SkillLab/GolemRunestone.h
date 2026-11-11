#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemRunestone.generated.h"

class AGolemCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EGolemRunestoneKind : uint8
{
	Healing, Crystal, Frost
};

UENUM(BlueprintType)
enum class EGolemRunestoneState : uint8
{
	Ready, Channeling, Cooldown
};

DECLARE_MULTICAST_DELEGATE(FOnGolemRunestoneStateChanged);

/** Minimal pre-M6 world interaction adapter; reservations belong to the authority actor. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRunestone : public AActor
{
	GENERATED_BODY()

public:
	AGolemRunestone();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ConfigureFixture(EGolemRunestoneKind InKind, FGuid InEncounterId);
	FGolemResult TryActivate(AGolemCharacter* Golem, FGuid RequestId);
	void ResetFixture(FGuid InEncounterId);
	EGolemRunestoneKind GetKind() const;
	EGolemRunestoneState GetState() const;
	float GetRemainingTime() const;
	float GetStateEndServerTime() const;
	FOnGolemRunestoneStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	bool ValidateChannel() const;
	bool SpawnWorldResult();
	void AdvanceChannel();
	void FinishCooldown();
	UFUNCTION()
	void OnRep_State();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Stone;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_State, Category = "Golem|SkillLab")
	EGolemRunestoneKind Kind = EGolemRunestoneKind::Healing;
	UPROPERTY(ReplicatedUsing = OnRep_State)
	EGolemRunestoneState State = EGolemRunestoneState::Ready;
	UPROPERTY(ReplicatedUsing = OnRep_State)
	float EndServerTime = 0.0f;
	UPROPERTY(Replicated)
	FGolemEntityId FixtureId;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedObjects;
	TWeakObjectPtr<AGolemCharacter> ChannelGolem;
	TWeakObjectPtr<AController> ChannelController;
	FGolemEntityId ChannelIdentity;
	FGuid EncounterId;
	FGuid ChannelRequest;
	TSet<FGuid> CommittedRequests;
	FTimerHandle ChannelTimer;
	FTimerHandle CooldownTimer;
	int64 Revision = 0;
};
