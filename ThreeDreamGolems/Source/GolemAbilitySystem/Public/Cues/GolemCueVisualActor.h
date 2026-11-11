#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GolemCueVisualActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** Local cue presentation follows its target and has no collision, replication or gameplay mutation. */
UCLASS(NotBlueprintable, Transient)
class GOLEMABILITYSYSTEM_API AGolemCueVisualActor : public AActor
{
	GENERATED_BODY()

public:
	AGolemCueVisualActor();
	void Configure(AActor* Target, FGameplayTag InCueTag, bool bInPersistent);
	FGameplayTag GetCueTag() const;
	bool IsPersistent() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|GAS|Cue")
	TObjectPtr<USceneComponent> CueRoot;

	UPROPERTY(VisibleAnywhere, Category = "Golem|GAS|Cue")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(VisibleAnywhere, Category = "Golem|GAS|Cue")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FGameplayTag CueTag;
	bool bPersistent = false;
};
