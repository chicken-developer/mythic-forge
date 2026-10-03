#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Types/KnightResult.h"
#include "KnightGameState.generated.h"

class APlayerController;
class AKnightPartyState;

USTRUCT(BlueprintType)
struct KNIGHTFRAMEWORK_API FKnightTrainingProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Training")
	uint8 ControlledMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Training")
	uint8 MarkerMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Training")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Training")
	bool bCompleted = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightPartyStateChanged, AKnightPartyState*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightTrainingProgressChanged, const FKnightTrainingProgress&);

UCLASS()
class KNIGHTFRAMEWORK_API AKnightGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	AKnightPartyState* GetPartyState() const;
	const FKnightTrainingProgress& GetTrainingProgress() const;
	FKnightResult SetPartyState(AKnightPartyState* InPartyState);
	FKnightResult RecordControlledSlot(APlayerController* Player, int32 Slot);
	FKnightResult RecordTrainingMarker(APlayerController* Player, int32 MarkerIndex);
	FKnightResult ResetTrainingProgress(APlayerController* Player);
	FOnKnightPartyStateChanged OnPartyStateChanged;
	FOnKnightTrainingProgressChanged OnTrainingProgressChanged;

	static constexpr int32 RequiredMarkerCount = 3;

private:
	FKnightResult ValidateHumanCommand(const APlayerController* Player) const;
	void CommitTrainingProgress();

	UFUNCTION()
	void OnRep_PartyState();

	UFUNCTION()
	void OnRep_TrainingProgress();

	UPROPERTY(ReplicatedUsing = OnRep_PartyState)
	TObjectPtr<AKnightPartyState> PartyState;

	UPROPERTY(ReplicatedUsing = OnRep_TrainingProgress)
	FKnightTrainingProgress TrainingProgress;
};
