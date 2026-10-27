#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/KnightResult.h"
#include "KnightStoryAnchor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;

/** Actor composition is replaceable; AnchorId remains the saved/content identity. */
UCLASS()
class KNIGHTWORLD_API AKnightStoryAnchor : public AActor
{
	GENERATED_BODY()

public:
	AKnightStoryAnchor();
	UPROPERTY(EditInstanceOnly, Category = "Knight|World")
	FName AnchorId;
	UPROPERTY(EditInstanceOnly, Category = "Knight|World")
	FText Label;
	void RefreshPresentation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|World")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Knight|World")
	TObjectPtr<UTextRenderComponent> Caption;
	UPROPERTY(VisibleAnywhere, Category = "Knight|World")
	TObjectPtr<UBoxComponent> Area;
	UFUNCTION()
	void HandleAreaEntered(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
};
