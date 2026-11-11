#pragma once

#include "CoreMinimal.h"
#include "PartyArena/GolemArenaCharacter.h"
#include "GolemRPGCharacter.generated.h"

class AGolemRPGGameState;
class AGolemRPGStateActor;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UGolemSkillRuntimeComponent;
class UGolemGearAppearanceComponent;

/** Equipment presentation belongs to the canonical Golem, including while a companion controls it. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGCharacter : public AGolemArenaCharacter
{
	GENERATED_BODY()

public:
	AGolemRPGCharacter();
	UStaticMeshComponent* GetWeaponUpgradeVisual() const;
	UStaticMeshComponent* GetRuneVisual() const;

protected:
	virtual FGolemAttributeDefaults GetInitialAttributeDefaults() const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Presentation")
	TObjectPtr<UGolemGearAppearanceComponent> GearAppearance;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Skills")
	TObjectPtr<UGolemSkillRuntimeComponent> SkillRuntime;
	void BindRPGPresentation();
	void HandleRPGStateReady(AGolemRPGStateActor* InState);
	void RefreshEquipmentVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Presentation")
	TObjectPtr<UStaticMeshComponent> WeaponUpgradeVisual;
	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Presentation")
	TObjectPtr<UStaticMeshComponent> RuneVisual;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> VisualMaterials;
	TWeakObjectPtr<AGolemRPGGameState> RPGGameState;
	TWeakObjectPtr<AGolemRPGStateActor> RPGState;
	FDelegateHandle StateReadyHandle;
	FDelegateHandle RPGHandle;
	FDelegateHandle IdentityHandle;
	FTimerHandle InitialBindingTimer;
	int32 BindingAttempts = 0;
	bool bEndingRPGPlay = false;
};
