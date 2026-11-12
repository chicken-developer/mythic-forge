#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemXenon.generated.h"

class UGolemAbilitySystemComponent;
class UGolemAttributeSet;
class UGolemColorVoidComponent;
class UGolemLifeComponent;
class UGolemVoidHostComponent;
class UGolemXenonBrainComponent;
class UGolemXenonDefinition;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A dark Xenon: a grey body that barely takes damage and a set of glowing voids that take colour.
 * Filling every void restores it; the actor then stays in the level and stops fighting.
 */
UCLASS()
class GOLEMCOMBAT_API AGolemXenon : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGolemXenon();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority only, once. Seed picks the colour of each void; MixedChanceOverride follows the encounter difficulty. */
	FGolemResult InitializeXenon(const UGolemXenonDefinition* InDefinition, int32 Seed, float MixedChanceOverride = -1.0f);

	UGolemAbilitySystemComponent* GetGolemAbilitySystem() const { return AbilitySystem; }
	UGolemAttributeSet* GetGolemAttributeSet() const { return Attributes; }
	UGolemLifeComponent* GetLifeComponent() const { return Life; }
	UGolemVoidHostComponent* GetVoidHost() const { return VoidHost; }
	UGolemXenonBrainComponent* GetBrain() const { return Brain; }
	FPrimaryAssetId GetDefinitionId() const { return DefinitionId; }
	FGolemEntityId GetXenonId() const { return XenonId; }
	bool IsRestored() const;

	/** Display colour of an element set: red, yellow, blue, orange, green, purple or white. */
	static FLinearColor ColorForMask(uint8 ElementMask);

protected:
	virtual void BeginPlay() override;

private:
	void HandleVoidChanged(UGolemColorVoidComponent* Void);
	void HandleHostChanged();
	void HandleBrainChanged();
	void RefreshBody();

	UFUNCTION()
	void OnRep_Identity();

	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UGolemAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UGolemLifeComponent> Life;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UGolemVoidHostComponent> VoidHost;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UGolemXenonBrainComponent> Brain;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Flat marker on the floor showing where a telegraphed attack will land. */
	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UStaticMeshComponent> TelegraphZone;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TelegraphMaterial;

	/** Fixed pool so every void is a default subobject with a stable network name. */
	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TArray<TObjectPtr<UGolemColorVoidComponent>> VoidPool;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Xenon")
	TObjectPtr<UGolemColorVoidComponent> CoreVoid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> VoidVisuals;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> VoidMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Identity)
	FPrimaryAssetId DefinitionId;

	UPROPERTY(ReplicatedUsing = OnRep_Identity)
	FGolemEntityId XenonId;

	UPROPERTY(Replicated)
	float ReplicatedBodyScale = 1.0f;

	bool bInitialized = false;
};
