#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Types/GolemResult.h"
#include "GolemAnimInstance.generated.h"

class ACharacter;
class USkeletalMesh;
class UAnimSequence;

/** Native locomotion graph feeds DefaultSlot so GAS montages evaluate real bone poses and notifies. */
UCLASS(Transient)
class GOLEMCOMBAT_API UGolemAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	static FGolemResult ConfigureCharacter(ACharacter* Character, USkeletalMesh* Mesh,
		UAnimSequence* Idle, UAnimSequence* Walk);
	UAnimSequence* GetIdleAnimation() const;
	UAnimSequence* GetWalkAnimation() const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnimation;
};
