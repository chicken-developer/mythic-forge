#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "GameFramework/Character.h"
#include "Types/GolemIdentifiableInterface.h"
#include "Types/GolemResult.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemCharacter.generated.h"

struct FOnAttributeChangeData;

class UCameraComponent;
struct FGolemAttributeDefaults;
class UDamageType;
class UGolemHeroDefinition;
class UGolemAbilitySystemComponent;
class UGolemAttributeSet;
class UGolemLifeComponent;
class UGolemCombatComponent;
class UGolemTraversalComponent;
class UGolemFinisherComponent;
class UGolemCameraModeComponent;
class UMotionWarpingComponent;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class UStaticMeshComponent;
enum class EGolemLifeState : uint8;

DECLARE_MULTICAST_DELEGATE(FOnGolemIdentityChanged);

/** Canonical combat identity survives human/AI controller transfers. */
UCLASS()
class GOLEMFRAMEWORK_API AGolemCharacter : public ACharacter, public IGolemIdentifiableInterface,
	public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGolemCharacter();

	FGolemResult InitializeGolem(const FGolemEntityId& InGolemEntityId, UGolemHeroDefinition* InHeroDefinition);
	virtual FGolemEntityId GetGolemEntityId() const override;
	FPrimaryAssetId GetHeroDefinitionId() const;
	UGolemHeroDefinition* GetHeroDefinition() const;
	FText GetGolemDisplayName() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UGolemAbilitySystemComponent* GetGolemAbilitySystem() const;
	UGolemAttributeSet* GetGolemAttributeSet() const;
	UGolemLifeComponent* GetLifeComponent() const;
	UGolemCombatComponent* GetCombatComponent() const;
	UGolemCameraModeComponent* GetCameraModeComponent() const;
	const FGolemResult& GetAbilityInitializationResult() const;
	virtual bool CanTransferControl() const;
	void ResetControlInput();
	UCameraComponent* GetThirdPersonCamera() const;
	USpringArmComponent* GetCameraBoom() const;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	FOnGolemIdentityChanged OnIdentityChanged;

protected:
	virtual FGolemAttributeDefaults GetInitialAttributeDefaults() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_Owner() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_GolemEntityId();

	UFUNCTION()
	void OnRep_HeroDefinitionId();

	void ResolveHeroDefinition();
	void ApplyHeroPresentation();
	void ApplyHeroMovement();
	void HandleAgilityChanged(const FOnAttributeChangeData& Data);
	FGolemResult InitializeGolemAbilitySystem();
	bool CanRefreshGolemAbilityContext() const;
	void RefreshGolemAbilityContext();
	void HandleLifeStateChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleCombatProfileChanged(class UGolemCombatProfileDefinition* Profile);

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Abilities")
	TObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Attributes")
	TObjectPtr<UGolemAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Life")
	TObjectPtr<UGolemLifeComponent> LifeComponent;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Combat")
	TObjectPtr<UGolemCombatComponent> CombatComponent;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Traversal")
	TObjectPtr<UGolemTraversalComponent> TraversalComponent;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Finisher")
	TObjectPtr<UGolemFinisherComponent> FinisherComponent;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Camera")
	TObjectPtr<UGolemCameraModeComponent> CameraModeComponent;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Traversal")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	UPROPERTY(Transient)
	TObjectPtr<UGolemAbilitySetDefinition> DefaultAbilitySet;

	FGolemAbilitySetHandle DefaultAbilitySetHandle;
	FGolemResult AbilityInitializationResult;
	FDelegateHandle LifeStateHandle;
	FDelegateHandle AgilityHandle;
	FDelegateHandle CombatProfileHandle;
	bool bEndingPlay = false;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Presentation")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Character|Presentation")
	TObjectPtr<UStaticMeshComponent> FacingMarker;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UGolemHeroDefinition> HeroDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_GolemEntityId)
	FGolemEntityId GolemEntityId;

	UPROPERTY(ReplicatedUsing = OnRep_HeroDefinitionId)
	FPrimaryAssetId HeroDefinitionId;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Character|Camera", meta = (ClampMin = "100", Units = "cm"))
	float CameraDistance = 420.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Character|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float JumpSpeed = 520.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Character|Movement", meta = (ClampMin = "0", Units = "deg/s"))
	float TurnSpeed = 540.0f;
};
