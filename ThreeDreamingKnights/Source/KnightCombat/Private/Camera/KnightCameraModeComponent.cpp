#include "Camera/KnightCameraModeComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

UKnightCameraModeComponent::UKnightCameraModeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

FKnightResult UKnightCameraModeComponent::Configure(USpringArmComponent* InBoom, UCameraComponent* InCamera)
{
	if (!IsValid(InBoom) || !IsValid(InCamera) || InBoom->GetOwner() != GetOwner()
		|| InCamera->GetOwner() != GetOwner())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("ConfigureCameraModes"),
			NSLOCTEXT("KnightCombat", "CameraOwnership", "Camera mode components must belong to this pawn."));
	}
	if (bConfigured)
	{
		return Boom == InBoom && Camera == InCamera ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::Conflict, TEXT("ConfigureCameraModes"),
				NSLOCTEXT("KnightCombat", "CameraAlreadyConfigured", "Camera source cannot change while configured."));
	}
	Boom = InBoom;
	Camera = InCamera;
	BaseArmLength = InBoom->TargetArmLength;
	BaseSocketOffset = InBoom->SocketOffset;
	BaseFieldOfView = InCamera->FieldOfView;
	bConfigured = true;
	ApplyTopMode();
	return FKnightResult::Success();
}

FGuid UKnightCameraModeComponent::PushMode(const FKnightCameraModeSettings& Settings)
{
	if (bEndingPlay || GetOwner() == nullptr || !GetOwner()->HasAuthority() || !bConfigured
		|| !FMath::IsFinite(Settings.FieldOfView) || Settings.FieldOfView < 20.0f || Settings.FieldOfView > 150.0f
		|| !FMath::IsFinite(Settings.ArmLength) || Settings.ArmLength < 0.0f || Settings.SocketOffset.ContainsNaN())
	{
		return FGuid();
	}
	FKnightCameraModeEntry Entry;
	Entry.Handle = FGuid::NewGuid();
	Entry.Settings = Settings;
	Modes.Add(Entry);
	ApplyTopMode();
	GetOwner()->ForceNetUpdate();
	return Entry.Handle;
}

FKnightResult UKnightCameraModeComponent::PopMode(const FGuid& Handle)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("PopCameraMode"),
			NSLOCTEXT("KnightCombat", "CameraAuthority", "Only the server removes an authoritative camera mode."));
	}
	const int32 Index = Modes.IndexOfByPredicate([&Handle](const FKnightCameraModeEntry& Entry)
	{
		return Entry.Handle == Handle;
	});
	if (!Handle.IsValid() || Index == INDEX_NONE)
	{
		return FKnightResult::Failure(EKnightError::NotFound, TEXT("PopCameraMode"),
			NSLOCTEXT("KnightCombat", "CameraHandle", "Camera mode handle no longer belongs to this stack."));
	}
	Modes.RemoveAt(Index);
	ApplyTopMode();
	GetOwner()->ForceNetUpdate();
	return FKnightResult::Success();
}

void UKnightCameraModeComponent::ClearModes()
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Modes.Reset();
		ApplyTopMode();
		GetOwner()->ForceNetUpdate();
	}
}

void UKnightCameraModeComponent::HandleControllerChanged()
{
	ReleaseInputLocks();
	ApplyTopMode();
}

int32 UKnightCameraModeComponent::GetModeCount() const
{
	return Modes.Num();
}

bool UKnightCameraModeComponent::HasMode(const FGuid& Handle) const
{
	return Handle.IsValid() && Modes.ContainsByPredicate([&Handle](const FKnightCameraModeEntry& Entry)
	{
		return Entry.Handle == Handle;
	});
}

void UKnightCameraModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UKnightCameraModeComponent, Modes, COND_OwnerOnly);
}

void UKnightCameraModeComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ReleaseInputLocks();
	Modes.Reset();
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UKnightCameraModeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const FKnightCameraModeEntry* Top = FindTopMode();
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
		Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, 6.0f));
	}
}

void UKnightCameraModeComponent::OnRep_Modes()
{
	ApplyTopMode();
}

void UKnightCameraModeComponent::ApplyTopMode()
{
	if (!bConfigured || bEndingPlay || !Boom.IsValid() || !Camera.IsValid())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn != nullptr && Pawn->IsLocallyControlled()
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const FKnightCameraModeEntry* Top = FindTopMode();
	Boom->TargetArmLength = Top != nullptr ? Top->Settings.ArmLength : BaseArmLength;
	Boom->SocketOffset = Top != nullptr ? Top->Settings.SocketOffset : BaseSocketOffset;
	Camera->SetFieldOfView(Top != nullptr ? Top->Settings.FieldOfView : BaseFieldOfView);
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

void UKnightCameraModeComponent::ReleaseInputLocks()
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

const FKnightCameraModeEntry* UKnightCameraModeComponent::FindTopMode() const
{
	const FKnightCameraModeEntry* Top = nullptr;
	for (const FKnightCameraModeEntry& Entry : Modes)
	{
		if (Top == nullptr || Entry.Settings.Priority >= Top->Settings.Priority)
		{
			Top = &Entry;
		}
	}
	return Top;
}
