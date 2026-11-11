#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GolemGearAppearanceComponent.generated.h"

class AGolemRPGStateActor;
class AGolemRPGGameState;
class UStaticMeshComponent;
class UStaticMesh;

/** Graybox fittings follow confirmed canonical assignments; paired parts share one slot source. */
UCLASS()
class THREEDREAMGOLEMS_API UGolemGearAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UGolemGearAppearanceComponent();
	int32 GetVisiblePartCount(FName SlotName) const;
	FName GetWeaponSilhouette() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	void BindSource();
	void HandleStateReady(AGolemRPGStateActor* State);
	void RefreshAppearance();
	UStaticMeshComponent* CreatePart(FName SlotName, const FVector& Location, const FVector& Scale,
		UStaticMesh* Mesh, bool bWeapon = false);
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Cube;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Cone;
	TArray<FName> PartSlots;
	TWeakObjectPtr<AGolemRPGGameState> GameState;
	TWeakObjectPtr<AGolemRPGStateActor> RPGState;
	FDelegateHandle StateHandle;
	FDelegateHandle InventoryHandle;
	FDelegateHandle RPGHandle;
	FTimerHandle ReadinessTimer;
	FName WeaponSilhouette;
	int32 Attempts = 0;
};
