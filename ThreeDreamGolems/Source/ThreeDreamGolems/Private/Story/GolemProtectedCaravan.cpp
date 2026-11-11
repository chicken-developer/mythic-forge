#include "Story/GolemProtectedCaravan.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Tags/GolemGameplayTags.h"

AGolemProtectedCaravan::AGolemProtectedCaravan()
{
	if (UStaticMeshComponent* CaravanBody = FindComponentByClass<UStaticMeshComponent>())
	{
		// Target-centred pathing queries need a nav polygon at the protected actor's origin.
		CaravanBody->SetCanEverAffectNavigation(false);
		CaravanBody->SetCollisionObjectType(ECC_Pawn);
	}
}

void AGolemProtectedCaravan::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetGolemAbilitySystem()->RemoveLooseGameplayTag(GolemGameplayTags::Role_Enemy, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		GetGolemAbilitySystem()->AddLooseGameplayTag(GolemGameplayTags::Role_Party_Golem, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), ProtectedHealth);
		GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), ProtectedHealth);
	}
	if (UStaticMeshComponent* CaravanBody = FindComponentByClass<UStaticMeshComponent>())
	{
		CaravanBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		CaravanBody->SetRelativeScale3D(FVector(1.8, 1.0, 0.8));
		CaravanBody->SetCollisionObjectType(ECC_Pawn);
		CaravanBody->SetCanEverAffectNavigation(false);
	}
	if (UTextRenderComponent* Caption = FindComponentByClass<UTextRenderComponent>())
	{
		Caption->SetText(NSLOCTEXT("KnightStory", "Caravan", "PROTECT CARAVAN"));
	}
}

FGolemResult AGolemProtectedCaravan::ResetCaravan()
{
	FGolemResult Result = GetLifeComponent()->ResetForCheckpoint();
	if (Result.IsSuccess())
	{
		GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), ProtectedHealth);
		GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), ProtectedHealth);
	}
	return Result;
}
