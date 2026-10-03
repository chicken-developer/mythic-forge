#include "Animation/KnightAnimNotifyState_TraceWindow.h"

#include "Components/KnightCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UKnightAnimNotifyState_TraceWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(Mesh, Animation, TotalDuration, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UKnightCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UKnightCombatComponent>())
		{
			Combat->BeginTraceWindow(WindowId, Animation);
		}
	}
}

void UKnightAnimNotifyState_TraceWindow::NotifyTick(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(Mesh, Animation, FrameDeltaTime, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UKnightCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UKnightCombatComponent>())
		{
			Combat->TickTraceWindow(WindowId, Animation);
		}
	}
}

void UKnightAnimNotifyState_TraceWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(Mesh, Animation, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UKnightCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UKnightCombatComponent>())
		{
			Combat->EndTraceWindow(WindowId, Animation);
		}
	}
}
