#pragma once

#include "CoreMinimal.h"
#include "PartyArena/KnightArenaCharacter.h"
#include "KnightRPGCharacter.generated.h"

class AKnightRPGGameState;
class AKnightRPGStateActor;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Equipment presentation belongs to the canonical Knight, including while a companion controls it. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGCharacter : public AKnightArenaCharacter
{
	GENERATED_BODY()

public:
	AKnightRPGCharacter();
	UStaticMeshComponent* GetWeaponUpgradeVisual() const;
	UStaticMeshComponent* GetRuneVisual() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void BindRPGPresentation();
	void HandleRPGStateReady(AKnightRPGStateActor* InState);
	void RefreshEquipmentVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Knight|RPG|Presentation")
	TObjectPtr<UStaticMeshComponent> WeaponUpgradeVisual;
	UPROPERTY(VisibleAnywhere, Category = "Knight|RPG|Presentation")
	TObjectPtr<UStaticMeshComponent> RuneVisual;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> VisualMaterials;
	TWeakObjectPtr<AKnightRPGGameState> RPGGameState;
	TWeakObjectPtr<AKnightRPGStateActor> RPGState;
	FDelegateHandle StateReadyHandle;
	FDelegateHandle RPGHandle;
	FDelegateHandle IdentityHandle;
	FTimerHandle InitialBindingTimer;
	int32 BindingAttempts = 0;
	bool bEndingRPGPlay = false;
};
