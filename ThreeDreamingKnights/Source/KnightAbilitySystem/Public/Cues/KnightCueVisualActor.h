#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "KnightCueVisualActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** Local cue presentation follows its target and has no collision, replication or gameplay mutation. */
UCLASS(NotBlueprintable, Transient)
class KNIGHTABILITYSYSTEM_API AKnightCueVisualActor : public AActor
{
	GENERATED_BODY()

public:
	AKnightCueVisualActor();
	void Configure(AActor* Target, FGameplayTag InCueTag, bool bInPersistent);
	FGameplayTag GetCueTag() const;
	bool IsPersistent() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|GAS|Cue")
	TObjectPtr<USceneComponent> CueRoot;

	UPROPERTY(VisibleAnywhere, Category = "Knight|GAS|Cue")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(VisibleAnywhere, Category = "Knight|GAS|Cue")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FGameplayTag CueTag;
	bool bPersistent = false;
};
