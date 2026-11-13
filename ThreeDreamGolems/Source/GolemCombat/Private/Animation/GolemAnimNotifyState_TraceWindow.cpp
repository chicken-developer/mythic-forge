#include "Animation/GolemAnimNotifyState_TraceWindow.h"

#include "Components/GolemCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UGolemAnimNotifyState_TraceWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(Mesh, Animation, TotalDuration, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UGolemCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->BeginTraceWindow(WindowId, Animation);
		}
	}
}

void UGolemAnimNotifyState_TraceWindow::NotifyTick(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(Mesh, Animation, FrameDeltaTime, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UGolemCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->TickTraceWindow(WindowId, Animation);
		}
	}
}

void UGolemAnimNotifyState_TraceWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(Mesh, Animation, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UGolemCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->EndTraceWindow(WindowId, Animation);
		}
	}
}
