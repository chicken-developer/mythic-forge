#include "Story/KnightProtectedCaravan.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Tags/KnightGameplayTags.h"

AKnightProtectedCaravan::AKnightProtectedCaravan()
{
	if (UStaticMeshComponent* CaravanBody = FindComponentByClass<UStaticMeshComponent>())
	{
		// Target-centred pathing queries need a nav polygon at the protected actor's origin.
		CaravanBody->SetCanEverAffectNavigation(false);
		CaravanBody->SetCollisionObjectType(ECC_Pawn);
	}
}

void AKnightProtectedCaravan::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetKnightAbilitySystem()->RemoveLooseGameplayTag(KnightGameplayTags::Role_Enemy, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		GetKnightAbilitySystem()->AddLooseGameplayTag(KnightGameplayTags::Role_Party_Knight, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
		GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetMaxHealthAttribute(), ProtectedHealth);
		GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), ProtectedHealth);
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

FKnightResult AKnightProtectedCaravan::ResetCaravan()
{
	FKnightResult Result = GetLifeComponent()->ResetForCheckpoint();
	if (Result.IsSuccess())
	{
		GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetMaxHealthAttribute(), ProtectedHealth);
		GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), ProtectedHealth);
	}
	return Result;
}
