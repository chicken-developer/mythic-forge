#pragma once

#include "CoreMinimal.h"
#include "Types/GolemStableIds.h"
#include "GolemCompanionCommand.generated.h"

UENUM(BlueprintType)
enum class EGolemCompanionCommand : uint8
{
	Follow,
	Hold,
	Focus,
	Regroup
};

UENUM(BlueprintType)
enum class EGolemAILODTier : uint8
{
	Near,
	Far,
	Dormant
};

/** Authority snapshot for command feedback. Identity remains attached to the canonical Golem. */
USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemCompanionCommandSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	EGolemCompanionCommand Command = EGolemCompanionCommand::Follow;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	int32 CommandRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	FGolemEntityId GolemId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	FGolemEntityId FocusTargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	FVector HoldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	EGolemAILODTier DecisionLOD = EGolemAILODTier::Near;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	float DecisionInterval = 0.1f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Commands")
	int64 DecisionTickCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemCompanionCommandChanged, const FGolemCompanionCommandSnapshot&);
