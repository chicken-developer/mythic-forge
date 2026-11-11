#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Types/GolemResult.h"
#include "GolemGameState.generated.h"

class APlayerController;
class AGolemPartyState;

USTRUCT(BlueprintType)
struct GOLEMFRAMEWORK_API FGolemTrainingProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Training")
	uint8 ControlledMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Training")
	uint8 MarkerMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Training")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Training")
	bool bCompleted = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemPartyStateChanged, AGolemPartyState*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemTrainingProgressChanged, const FGolemTrainingProgress&);

UCLASS()
class GOLEMFRAMEWORK_API AGolemGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	AGolemPartyState* GetPartyState() const;
	const FGolemTrainingProgress& GetTrainingProgress() const;
	FGolemResult SetPartyState(AGolemPartyState* InPartyState);
	FGolemResult RecordControlledSlot(APlayerController* Player, int32 Slot);
	FGolemResult RecordTrainingMarker(APlayerController* Player, int32 MarkerIndex);
	FGolemResult ResetTrainingProgress(APlayerController* Player);
	FOnGolemPartyStateChanged OnPartyStateChanged;
	FOnGolemTrainingProgressChanged OnTrainingProgressChanged;

	static constexpr int32 RequiredMarkerCount = 3;

private:
	FGolemResult ValidateHumanCommand(const APlayerController* Player) const;
	void CommitTrainingProgress();

	UFUNCTION()
	void OnRep_PartyState();

	UFUNCTION()
	void OnRep_TrainingProgress();

	UPROPERTY(ReplicatedUsing = OnRep_PartyState)
	TObjectPtr<AGolemPartyState> PartyState;

	UPROPERTY(ReplicatedUsing = OnRep_TrainingProgress)
	FGolemTrainingProgress TrainingProgress;
};
