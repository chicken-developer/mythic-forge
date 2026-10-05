#pragma once

#include "CoreMinimal.h"
#include "Types/KnightStableIds.h"
#include "KnightCompanionCommand.generated.h"

UENUM(BlueprintType)
enum class EKnightCompanionCommand : uint8
{
	Follow,
	Hold,
	Focus,
	Regroup
};

UENUM(BlueprintType)
enum class EKnightAILODTier : uint8
{
	Near,
	Far,
	Dormant
};

/** Authority snapshot for command feedback. Identity remains attached to the canonical Knight. */
USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightCompanionCommandSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	EKnightCompanionCommand Command = EKnightCompanionCommand::Follow;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	int32 CommandRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	FKnightEntityId KnightId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	FKnightEntityId FocusTargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	FVector HoldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	EKnightAILODTier DecisionLOD = EKnightAILODTier::Near;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	float DecisionInterval = 0.1f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Commands")
	int64 DecisionTickCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightCompanionCommandChanged, const FKnightCompanionCommandSnapshot&);
