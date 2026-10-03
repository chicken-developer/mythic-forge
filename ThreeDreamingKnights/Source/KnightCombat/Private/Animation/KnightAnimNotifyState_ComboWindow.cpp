#include "Animation/KnightAnimNotifyState_ComboWindow.h"

#include "Components/KnightCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UKnightAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(Mesh, Animation, TotalDuration, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UKnightCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UKnightCombatComponent>())
		{
			Combat->SetComboWindow(true, Animation);
		}
	}
}

void UKnightAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(Mesh, Animation, EventReference);
	if (Mesh != nullptr && Mesh->GetOwner() != nullptr)
	{
		if (UKnightCombatComponent* Combat = Mesh->GetOwner()->FindComponentByClass<UKnightCombatComponent>())
		{
			Combat->SetComboWindow(false, Animation);
		}
	}
}
