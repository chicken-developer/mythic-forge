#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GolemAnimNotifyState_TraceWindow.generated.h"

UCLASS(meta = (DisplayName = "Golem authoritative weapon trace"))
class GOLEMCOMBAT_API UGolemAnimNotifyState_TraceWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Golem|Combat|Trace")
	FName WindowId = TEXT("WeaponHit");
	virtual void NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
