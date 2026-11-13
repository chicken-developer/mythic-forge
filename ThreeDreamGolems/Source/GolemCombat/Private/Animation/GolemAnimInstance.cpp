#include "Animation/GolemAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"

namespace
{
	class FGolemAnimInstanceProxy : public FAnimInstanceProxy
	{
	public:
		explicit FGolemAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
		{
			Blend.A.SetLinkNode(&Idle);
			Blend.B.SetLinkNode(&Walk);
			Slot.Source.SetLinkNode(&Blend);
			Slot.SlotName = TEXT("DefaultSlot");
			Slot.bAlwaysUpdateSourcePose = true;
			Idle.SetLoopAnimation(true);
			Walk.SetLoopAnimation(true);
		}

		virtual FAnimNode_Base* GetCustomRootNode() override
		{
			return &Slot;
		}

		virtual void Initialize(UAnimInstance* Instance) override
		{
			const UGolemAnimInstance* GolemInstance = Cast<UGolemAnimInstance>(Instance);
			if (GolemInstance != nullptr)
			{
				Idle.SetSequence(GolemInstance->GetIdleAnimation());
				Walk.SetSequence(GolemInstance->GetWalkAnimation());
			}
			FAnimInstanceProxy::Initialize(Instance);
		}

		virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
		{
			OutNodes.Append({&Idle, &Walk, &Blend, &Slot});
		}

		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			const UGolemAnimInstance* GolemInstance = Cast<UGolemAnimInstance>(Instance);
			if (GolemInstance != nullptr)
			{
				Idle.SetSequence(GolemInstance->GetIdleAnimation());
				Walk.SetSequence(GolemInstance->GetWalkAnimation());
				const APawn* Pawn = GolemInstance->TryGetPawnOwner();
				Speed = Pawn != nullptr ? Pawn->GetVelocity().Size2D() : 0.0f;
			}
		}

		virtual void Update(float DeltaSeconds) override
		{
			Blend.Alpha = FMath::FInterpTo(Blend.Alpha, FMath::Clamp(Speed / 150.0f, 0.0f, 1.0f),
				DeltaSeconds, 8.0f);
			Walk.SetPlayRate(FMath::Clamp(Speed / 150.0f, 0.5f, 3.0f));
		}

	private:
		FAnimNode_SequencePlayer_Standalone Idle;
		FAnimNode_SequencePlayer_Standalone Walk;
		FAnimNode_TwoWayBlend Blend;
		FAnimNode_Slot Slot;
		float Speed = 0.0f;
	};
}

FGolemResult UGolemAnimInstance::ConfigureCharacter(ACharacter* Character, USkeletalMesh* Mesh,
	UAnimSequence* Idle, UAnimSequence* Walk)
{
	if (!IsValid(Character) || !IsValid(Mesh) || !IsValid(Idle) || !IsValid(Walk)
		|| Mesh->GetSkeleton() != Idle->GetSkeleton() || Mesh->GetSkeleton() != Walk->GetSkeleton())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ConfigureCombatAnimation"),
			NSLOCTEXT("GolemCombat", "AnimationSkeleton", "Combat mesh and locomotion require the same valid skeleton."));
	}
	USkeletalMeshComponent* Component = Character->GetMesh();
	if (Component == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ConfigureCombatAnimation"),
			NSLOCTEXT("GolemCombat", "AnimationMesh", "Combat character requires a native skeletal mesh component."));
	}
	Component->SetSkeletalMesh(Mesh);
	Component->SetRelativeLocation(FVector(0.0f, 0.0f, -Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	Component->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	// Runtime presentation can follow PostInitializeComponents; warping and smoothing use the cached mesh basis.
	Character->CacheInitialMeshOffset(Component->GetRelativeLocation(), Component->GetRelativeRotation());
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Component->bEnableUpdateRateOptimizations = false;
	Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Component->SetAnimInstanceClass(StaticClass());
	UGolemAnimInstance* Instance = Cast<UGolemAnimInstance>(Component->GetAnimInstance());
	if (Instance == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ConfigureCombatAnimation"),
			NSLOCTEXT("GolemCombat", "AnimationInstance", "Native combat animation instance could not initialize."));
	}
	Instance->IdleAnimation = Idle;
	Instance->WalkAnimation = Walk;
	Instance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	Component->InitAnim(true);
	return FGolemResult::Success();
}

UAnimSequence* UGolemAnimInstance::GetIdleAnimation() const
{
	return IdleAnimation;
}

UAnimSequence* UGolemAnimInstance::GetWalkAnimation() const
{
	return WalkAnimation;
}

FAnimInstanceProxy* UGolemAnimInstance::CreateAnimInstanceProxy()
{
	return new FGolemAnimInstanceProxy(this);
}

void UGolemAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
