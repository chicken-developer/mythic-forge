#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightRunestone.generated.h"

class AKnightCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EKnightRunestoneKind : uint8
{
	Healing, Crystal, Frost
};

UENUM(BlueprintType)
enum class EKnightRunestoneState : uint8
{
	Ready, Channeling, Cooldown
};

DECLARE_MULTICAST_DELEGATE(FOnKnightRunestoneStateChanged);

/** Minimal pre-M6 world interaction adapter; reservations belong to the authority actor. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRunestone : public AActor
{
	GENERATED_BODY()

public:
	AKnightRunestone();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ConfigureFixture(EKnightRunestoneKind InKind, FGuid InEncounterId);
	FKnightResult TryActivate(AKnightCharacter* Knight, FGuid RequestId);
	void ResetFixture(FGuid InEncounterId);
	EKnightRunestoneKind GetKind() const;
	EKnightRunestoneState GetState() const;
	float GetRemainingTime() const;
	float GetStateEndServerTime() const;
	FOnKnightRunestoneStateChanged OnStateChanged;

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
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_State, Category = "Knight|SkillLab")
	EKnightRunestoneKind Kind = EKnightRunestoneKind::Healing;
	UPROPERTY(ReplicatedUsing = OnRep_State)
	EKnightRunestoneState State = EKnightRunestoneState::Ready;
	UPROPERTY(ReplicatedUsing = OnRep_State)
	float EndServerTime = 0.0f;
	UPROPERTY(Replicated)
	FKnightEntityId FixtureId;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedObjects;
	TWeakObjectPtr<AKnightCharacter> ChannelKnight;
	TWeakObjectPtr<AController> ChannelController;
	FKnightEntityId ChannelIdentity;
	FGuid EncounterId;
	FGuid ChannelRequest;
	TSet<FGuid> CommittedRequests;
	FTimerHandle ChannelTimer;
	FTimerHandle CooldownTimer;
	int64 Revision = 0;
};
