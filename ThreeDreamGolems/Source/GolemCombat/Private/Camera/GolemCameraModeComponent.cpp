#include "Camera/GolemCameraModeComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

UGolemCameraModeComponent::UGolemCameraModeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

FGolemResult UGolemCameraModeComponent::Configure(USpringArmComponent* InBoom, UCameraComponent* InCamera)
{
	if (!IsValid(InBoom) || !IsValid(InCamera) || InBoom->GetOwner() != GetOwner()
		|| InCamera->GetOwner() != GetOwner())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("ConfigureCameraModes"),
			NSLOCTEXT("GolemCombat", "CameraOwnership", "Camera mode components must belong to this pawn."));
	}
	if (bConfigured)
	{
		return Boom == InBoom && Camera == InCamera ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::Conflict, TEXT("ConfigureCameraModes"),
				NSLOCTEXT("GolemCombat", "CameraAlreadyConfigured", "Camera source cannot change while configured."));
	}
	Boom = InBoom;
	Camera = InCamera;
	BaseArmLength = InBoom->TargetArmLength;
	BaseSocketOffset = InBoom->SocketOffset;
	BaseFieldOfView = InCamera->FieldOfView;
	bBaseCameraLag = InBoom->bEnableCameraLag;
	bBaseRotationLag = InBoom->bEnableCameraRotationLag;
	bConfigured = true;
	ApplyTopMode();
	return FGolemResult::Success();
}

FGuid UGolemCameraModeComponent::PushMode(const FGolemCameraModeSettings& Settings)
{
	if (bEndingPlay || GetOwner() == nullptr || !GetOwner()->HasAuthority() || !bConfigured
		|| !FMath::IsFinite(Settings.FieldOfView) || Settings.FieldOfView < 20.0f || Settings.FieldOfView > 150.0f
		|| !FMath::IsFinite(Settings.ArmLength) || Settings.ArmLength < 0.0f || Settings.SocketOffset.ContainsNaN())
	{
		return FGuid();
	}
	FGolemCameraModeEntry Entry;
	Entry.Handle = FGuid::NewGuid();
	Entry.Settings = Settings;
	Modes.Add(Entry);
	ApplyTopMode();
	GetOwner()->ForceNetUpdate();
	return Entry.Handle;
}

FGolemResult UGolemCameraModeComponent::PopMode(const FGuid& Handle)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("PopCameraMode"),
			NSLOCTEXT("GolemCombat", "CameraAuthority", "Only the server removes an authoritative camera mode."));
	}
	const int32 Index = Modes.IndexOfByPredicate([&Handle](const FGolemCameraModeEntry& Entry)
	{
		return Entry.Handle == Handle;
	});
	if (!Handle.IsValid() || Index == INDEX_NONE)
	{
		return FGolemResult::Failure(EGolemError::NotFound, TEXT("PopCameraMode"),
			NSLOCTEXT("GolemCombat", "CameraHandle", "Camera mode handle no longer belongs to this stack."));
	}
	Modes.RemoveAt(Index);
	ApplyTopMode();
	GetOwner()->ForceNetUpdate();
	return FGolemResult::Success();
}

void UGolemCameraModeComponent::ClearModes()
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Modes.Reset();
		ApplyTopMode();
		GetOwner()->ForceNetUpdate();
	}
}

void UGolemCameraModeComponent::HandleControllerChanged()
{
	ReleaseInputLocks();
	ApplyTopMode();
}

int32 UGolemCameraModeComponent::GetModeCount() const
{
	return Modes.Num();
}

bool UGolemCameraModeComponent::HasMode(const FGuid& Handle) const
{
	return Handle.IsValid() && Modes.ContainsByPredicate([&Handle](const FGolemCameraModeEntry& Entry)
	{
		return Entry.Handle == Handle;
	});
}

void UGolemCameraModeComponent::SetLocalMotionPreferences(const float ShakeScale, const bool bReduceMotion)
{
	if (!FMath::IsFinite(ShakeScale))
	{
		return;
	}
	LocalFeedbackScale = FMath::Clamp(ShakeScale, 0.0f, 1.0f);
	bReducedMotion = bReduceMotion;
	ApplyTopMode();
}

float UGolemCameraModeComponent::GetLocalFeedbackScale() const
{
	return bReducedMotion ? 0.0f : LocalFeedbackScale;
}

void UGolemCameraModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UGolemCameraModeComponent, Modes, COND_OwnerOnly);
}

void UGolemCameraModeComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ReleaseInputLocks();
	Modes.Reset();
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UGolemCameraModeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const FGolemCameraModeEntry* Top = FindTopMode();
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn != nullptr && Pawn->IsLocallyControlled()
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller != LockedController.Get())
	{
		HandleControllerChanged();
	}
	if (Controller != nullptr && Top != nullptr && Top->Settings.FocusTarget.IsValid()
		&& !Top->Settings.FocusTarget->IsActorBeingDestroyed() && Camera.IsValid())
	{
		const FRotator Desired = (Top->Settings.FocusTarget->GetActorLocation()
			- Camera->GetComponentLocation()).Rotation();
		Controller->SetControlRotation(bReducedMotion
			? FMath::RInterpConstantTo(Controller->GetControlRotation(), Desired, DeltaTime, 90.0f)
			: FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, 6.0f));
	}
}

void UGolemCameraModeComponent::OnRep_Modes()
{
	ApplyTopMode();
}

void UGolemCameraModeComponent::ApplyTopMode()
{
	if (!bConfigured || bEndingPlay || !Boom.IsValid() || !Camera.IsValid())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn != nullptr && Pawn->IsLocallyControlled()
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const FGolemCameraModeEntry* Top = FindTopMode();
	Boom->TargetArmLength = Top != nullptr ? Top->Settings.ArmLength : BaseArmLength;
	Boom->SocketOffset = Top != nullptr ? Top->Settings.SocketOffset : BaseSocketOffset;
	Camera->SetFieldOfView(Top != nullptr && !bReducedMotion ? Top->Settings.FieldOfView : BaseFieldOfView);
	Boom->bEnableCameraLag = bBaseCameraLag && !bReducedMotion;
	Boom->bEnableCameraRotationLag = bBaseRotationLag && !bReducedMotion;
	const bool bMoveLock = Controller != nullptr && Top != nullptr && Top->Settings.bLockMovementInput;
	const bool bLookLock = Controller != nullptr && Top != nullptr && Top->Settings.bLockLookInput;
	if (Controller != LockedController.Get())
	{
		ReleaseInputLocks();
		LockedController = Controller;
	}
	if (Controller != nullptr)
	{
		if (bMoveLock != bOwnMoveLock)
		{
			Controller->SetIgnoreMoveInput(bMoveLock);
			bOwnMoveLock = bMoveLock;
		}
		if (bLookLock != bOwnLookLock)
		{
			Controller->SetIgnoreLookInput(bLookLock);
			bOwnLookLock = bLookLock;
		}
	}
	SetComponentTickEnabled(Top != nullptr);
}

void UGolemCameraModeComponent::ReleaseInputLocks()
{
	if (APlayerController* Controller = LockedController.Get())
	{
		if (bOwnMoveLock)
		{
			Controller->SetIgnoreMoveInput(false);
		}
		if (bOwnLookLock)
		{
			Controller->SetIgnoreLookInput(false);
		}
	}
	bOwnMoveLock = false;
	bOwnLookLock = false;
	LockedController.Reset();
}

const FGolemCameraModeEntry* UGolemCameraModeComponent::FindTopMode() const
{
	const FGolemCameraModeEntry* Top = nullptr;
	for (const FGolemCameraModeEntry& Entry : Modes)
	{
		if (Top == nullptr || Entry.Settings.Priority >= Top->Settings.Priority)
		{
			Top = &Entry;
		}
	}
	return Top;
}
