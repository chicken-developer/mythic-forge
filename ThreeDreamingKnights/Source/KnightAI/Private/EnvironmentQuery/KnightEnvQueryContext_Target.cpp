#include "EnvironmentQuery/KnightEnvQueryContext_Target.h"

#include "AIController.h"
#include "Context/KnightAIContextComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "GameFramework/Pawn.h"

void UKnightEnvQueryContext_Target::ProvideContext(FEnvQueryInstance& QueryInstance,
	FEnvQueryContextData& ContextData) const
{
	const APawn* Pawn = Cast<APawn>(QueryInstance.Owner.Get());
	const AAIController* Controller = Pawn != nullptr ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const UKnightAIContextComponent* Context = Controller != nullptr
		? Controller->FindComponentByClass<UKnightAIContextComponent>() : nullptr;
	if (Context != nullptr && Context->GetControlledPawn() == Pawn)
	{
		const FKnightAIDecisionSnapshot Snapshot = Context->GetDecisionSnapshot();
		if (Snapshot.bAlive && Snapshot.bHasTarget)
		{
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, Snapshot.Target.Get());
		}
	}
}
