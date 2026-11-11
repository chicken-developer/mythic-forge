#include "Animation/GolemAnimNotifyState_ComboWindow.h"

#include "Components/GolemCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UGolemAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(Mesh, Animation, TotalDuration, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UGolemCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->SetComboWindow(true, Animation);
		}
	}
}

void UGolemAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(Mesh, Animation, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UGolemCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->SetComboWindow(false, Animation);
		}
	}
}
