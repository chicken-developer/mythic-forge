#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "GameFramework/Character.h"
#include "Types/KnightIdentifiableInterface.h"
#include "Types/KnightResult.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightCharacter.generated.h"

class UCameraComponent;
class UDamageType;
class UKnightHeroDefinition;
class UKnightAbilitySystemComponent;
class UKnightAttributeSet;
class UKnightLifeComponent;
class UKnightCombatComponent;
class UKnightTraversalComponent;
class UKnightFinisherComponent;
class UKnightCameraModeComponent;
class UMotionWarpingComponent;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class UStaticMeshComponent;
enum class EKnightLifeState : uint8;

DECLARE_MULTICAST_DELEGATE(FOnKnightIdentityChanged);

/** Canonical combat identity survives human/AI controller transfers. */
UCLASS()
class KNIGHTFRAMEWORK_API AKnightCharacter : public ACharacter, public IKnightIdentifiableInterface,
	public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AKnightCharacter();

	FKnightResult InitializeKnight(const FKnightEntityId& InKnightEntityId, UKnightHeroDefinition* InHeroDefinition);
	virtual FKnightEntityId GetKnightEntityId() const override;
	FPrimaryAssetId GetHeroDefinitionId() const;
	UKnightHeroDefinition* GetHeroDefinition() const;
	FText GetKnightDisplayName() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UKnightAbilitySystemComponent* GetKnightAbilitySystem() const;
	UKnightAttributeSet* GetKnightAttributeSet() const;
	UKnightLifeComponent* GetLifeComponent() const;
	UKnightCombatComponent* GetCombatComponent() const;
	UKnightCameraModeComponent* GetCameraModeComponent() const;
	const FKnightResult& GetAbilityInitializationResult() const;
	virtual bool CanTransferControl() const;
	void ResetControlInput();
	UCameraComponent* GetThirdPersonCamera() const;
	USpringArmComponent* GetCameraBoom() const;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	FOnKnightIdentityChanged OnIdentityChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_Owner() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_KnightEntityId();

	UFUNCTION()
	void OnRep_HeroDefinitionId();

	void ResolveHeroDefinition();
	void ApplyHeroPresentation();
	FKnightResult InitializeKnightAbilitySystem();
	bool CanRefreshKnightAbilityContext() const;
	void RefreshKnightAbilityContext();
	void HandleLifeStateChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleCombatProfileChanged(class UKnightCombatProfileDefinition* Profile);

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Abilities")
	TObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Attributes")
	TObjectPtr<UKnightAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Life")
	TObjectPtr<UKnightLifeComponent> LifeComponent;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Combat")
	TObjectPtr<UKnightCombatComponent> CombatComponent;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Traversal")
	TObjectPtr<UKnightTraversalComponent> TraversalComponent;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Finisher")
	TObjectPtr<UKnightFinisherComponent> FinisherComponent;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Camera")
	TObjectPtr<UKnightCameraModeComponent> CameraModeComponent;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Traversal")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	UPROPERTY(Transient)
	TObjectPtr<UKnightAbilitySetDefinition> DefaultAbilitySet;

	FKnightAbilitySetHandle DefaultAbilitySetHandle;
	FKnightResult AbilityInitializationResult;
	FDelegateHandle LifeStateHandle;
	FDelegateHandle CombatProfileHandle;
	bool bEndingPlay = false;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Presentation")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Character|Presentation")
	TObjectPtr<UStaticMeshComponent> FacingMarker;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UKnightHeroDefinition> HeroDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_KnightEntityId)
	FKnightEntityId KnightEntityId;

	UPROPERTY(ReplicatedUsing = OnRep_HeroDefinitionId)
	FPrimaryAssetId HeroDefinitionId;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Character|Camera", meta = (ClampMin = "100", Units = "cm"))
	float CameraDistance = 420.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Character|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float JumpSpeed = 520.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Character|Movement", meta = (ClampMin = "0", Units = "deg/s"))
	float TurnSpeed = 540.0f;
};
