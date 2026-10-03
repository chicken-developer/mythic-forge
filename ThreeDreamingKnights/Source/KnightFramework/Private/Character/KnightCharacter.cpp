#include "Character/KnightCharacter.h"

#include "AIController.h"
#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/KnightPartyGameMode.h"
#include "Finisher/KnightFinisherComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayAbilitySpec.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Tags/KnightGameplayTags.h"
#include "Traversal/KnightTraversalComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightCharacter, Log, All);

AKnightCharacter::AKnightCharacter()
{
	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	SetReplicateMovement(true);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnSpeed, 0.0f);
	GetCharacterMovement()->JumpZVelocity = JumpSpeed;
	AbilitySystem = CreateDefaultSubobject<UKnightAbilitySystemComponent>(TEXT("KnightAbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	Attributes = CreateDefaultSubobject<UKnightAttributeSet>(TEXT("KnightAttributes"));
	LifeComponent = CreateDefaultSubobject<UKnightLifeComponent>(TEXT("KnightLife"));
	CombatComponent = CreateDefaultSubobject<UKnightCombatComponent>(TEXT("KnightCombat"));
	TraversalComponent = CreateDefaultSubobject<UKnightTraversalComponent>(TEXT("KnightTraversal"));
	FinisherComponent = CreateDefaultSubobject<UKnightFinisherComponent>(TEXT("KnightFinisher"));
	CameraModeComponent = CreateDefaultSubobject<UKnightCameraModeComponent>(TEXT("KnightCameraModes"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("KnightMotionWarping"));
	AbilityInitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("InitializeKnightAbilities"),
		NSLOCTEXT("KnightCharacter", "AbilitiesPending", "Knight identity and ability defaults are not initialized yet."));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->SocketOffset = FVector(0.0f, 55.0f, 45.0f);
	CameraBoom->bUsePawnControlRotation = true;
	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.6f));
	FacingMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacingMarker"));
	FacingMarker->SetupAttachment(GetRootComponent());
	FacingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FacingMarker->SetRelativeLocation(FVector(42.0f, 0.0f, 45.0f));
	FacingMarker->SetRelativeScale3D(FVector(0.42f, 0.18f, 0.18f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}
	if (CubeMesh.Succeeded())
	{
		FacingMarker->SetStaticMesh(CubeMesh.Object);
	}
	if (BaseMaterial.Succeeded())
	{
		BodyMesh->SetMaterial(0, BaseMaterial.Object);
		FacingMarker->SetMaterial(0, BaseMaterial.Object);
	}
}

FKnightResult AKnightCharacter::InitializeKnight(
	const FKnightEntityId& InKnightEntityId, UKnightHeroDefinition* InHeroDefinition)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("InitializeKnight"),
			NSLOCTEXT("KnightCharacter", "Authority", "Only the server initializes a canonical Knight."));
	}
	if (!InKnightEntityId.IsValid() || !IsValid(InHeroDefinition))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("InitializeKnight"),
			NSLOCTEXT("KnightCharacter", "MissingIdentity", "Knight instance and hero definition IDs are required."));
	}
	const FKnightResult DefinitionResult = InHeroDefinition->ValidateHero();
	if (!DefinitionResult.IsSuccess())
	{
		return DefinitionResult;
	}
	const FPrimaryAssetId InHeroDefinitionId = InHeroDefinition->GetPrimaryAssetId();
	if (KnightEntityId.IsValid())
	{
		if (KnightEntityId == InKnightEntityId && HeroDefinitionId == InHeroDefinitionId)
		{
			AbilityInitializationResult = InitializeKnightAbilitySystem();
			return AbilityInitializationResult;
		}
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InitializeKnight"),
			NSLOCTEXT("KnightCharacter", "ImmutableIdentity", "An initialized Knight cannot change persistent identity."));
	}
	KnightEntityId = InKnightEntityId;
	HeroDefinitionId = InHeroDefinitionId;
	HeroDefinition = InHeroDefinition;
	ApplyHeroPresentation();
	AbilityInitializationResult = InitializeKnightAbilitySystem();
	if (!AbilityInitializationResult.IsSuccess())
	{
		return AbilityInitializationResult;
	}
	ForceNetUpdate();
	OnIdentityChanged.Broadcast();
	return FKnightResult::Success();
}

FKnightEntityId AKnightCharacter::GetKnightEntityId() const
{
	return KnightEntityId;
}

FPrimaryAssetId AKnightCharacter::GetHeroDefinitionId() const
{
	return HeroDefinitionId;
}

UKnightHeroDefinition* AKnightCharacter::GetHeroDefinition() const
{
	return HeroDefinition;
}

FText AKnightCharacter::GetKnightDisplayName() const
{
	return HeroDefinition != nullptr ? HeroDefinition->GetDisplayName()
		: NSLOCTEXT("KnightCharacter", "DefinitionPending", "Knight definition is loading");
}

UAbilitySystemComponent* AKnightCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UKnightAbilitySystemComponent* AKnightCharacter::GetKnightAbilitySystem() const
{
	return AbilitySystem;
}

UKnightAttributeSet* AKnightCharacter::GetKnightAttributeSet() const
{
	return Attributes;
}

UKnightLifeComponent* AKnightCharacter::GetLifeComponent() const
{
	return LifeComponent;
}

UKnightCombatComponent* AKnightCharacter::GetCombatComponent() const
{
	return CombatComponent;
}

UKnightCameraModeComponent* AKnightCharacter::GetCameraModeComponent() const
{
	return CameraModeComponent;
}

const FKnightResult& AKnightCharacter::GetAbilityInitializationResult() const
{
	return AbilityInitializationResult;
}

bool AKnightCharacter::CanTransferControl() const
{
	const UWorld* World = GetWorld();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return KnightEntityId.IsValid() && !IsActorBeingDestroyed() && World != nullptr
		&& !World->bIsTearingDown && !World->IsInSeamlessTravel()
		&& LifeComponent != nullptr && LifeComponent->IsAlive()
		&& CombatComponent != nullptr && CombatComponent->CanTransferControl()
		&& !TraversalComponent->HasActiveTraversal() && !FinisherComponent->IsReserved()
		&& Movement != nullptr && Movement->MovementMode != MOVE_None;
}

void AKnightCharacter::ResetControlInput()
{
	StopJumping();
	ConsumeMovementInputVector();
	if (HasAuthority() && CanRefreshKnightAbilityContext())
	{
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Ability_Test, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Block, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Primary, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Heavy, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Parry, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Dodge, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_LockOn, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Rune, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Finisher, false);
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_World_Traverse, false);
	}
}

UCameraComponent* AKnightCharacter::GetThirdPersonCamera() const
{
	return ThirdPersonCamera;
}

USpringArmComponent* AKnightCharacter::GetCameraBoom() const
{
	return CameraBoom;
}

void AKnightCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}
	if (!HasAuthority() && KnightEntityId.IsValid())
	{
		// Preserve the replicated canonical actor while the server sends its recovery correction.
		return;
	}
	AKnightPartyGameMode* Mode = World->GetAuthGameMode<AKnightPartyGameMode>();
	AKnightPartyState* Party = Mode != nullptr ? Mode->GetPartyState() : nullptr;
	const FKnightRosterEntry* Entry = Party != nullptr ? Party->FindKnight(this) : nullptr;
	if (HasAuthority() && Entry != nullptr && Entry->KnightId == KnightEntityId
		&& Entry->HeroDefinitionId == HeroDefinitionId)
	{
		const FKnightResult Recovery = Mode->TryRecoverKnightFromFall(this);
		if (!Recovery.IsSuccess())
		{
			UE_LOG(LogKnightCharacter, Error, TEXT("Canonical Knight %s fall recovery failed [%s]: %s; actor preserved"),
				*KnightEntityId.Value.ToString(), *Recovery.Context.ToString(), *Recovery.Message.ToString());
		}
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

void AKnightCharacter::PossessedBy(AController* NewController)
{
	if (CanRefreshKnightAbilityContext())
	{
		if (HasAuthority())
		{
			CombatComponent->ClearTransientActions();
		}
		AbilitySystem->HandleControllerChanged();
	}
	Super::PossessedBy(NewController);
	RefreshKnightAbilityContext();
	CameraModeComponent->HandleControllerChanged();
}

void AKnightCharacter::UnPossessed()
{
	if (CanRefreshKnightAbilityContext())
	{
		if (HasAuthority())
		{
			CombatComponent->ClearTransientActions();
		}
		AbilitySystem->HandleControllerChanged();
	}
	Super::UnPossessed();
	RefreshKnightAbilityContext();
	CameraModeComponent->HandleControllerChanged();
}

void AKnightCharacter::BeginPlay()
{
	Super::BeginPlay();
	CameraBoom->TargetArmLength = CameraDistance;
	CameraModeComponent->Configure(CameraBoom, ThirdPersonCamera);
	CombatProfileHandle = CombatComponent->OnProfileChanged.AddUObject(this, &ThisClass::HandleCombatProfileChanged);
	HandleCombatProfileChanged(CombatComponent->GetProfile());
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnSpeed, 0.0f);
	GetCharacterMovement()->JumpZVelocity = JumpSpeed;
	if (HeroDefinition != nullptr)
	{
		ApplyHeroPresentation();
	}
	else
	{
		ResolveHeroDefinition();
	}
	RefreshKnightAbilityContext();
}

void AKnightCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	TraversalComponent->PrepareForEndPlay();
	FinisherComponent->PrepareForEndPlay(EndPlayReason == EEndPlayReason::EndPlayInEditor
		|| EndPlayReason == EEndPlayReason::Quit || EndPlayReason == EEndPlayReason::LevelTransition);
	CombatComponent->OnProfileChanged.Remove(CombatProfileHandle);
	CombatProfileHandle.Reset();
	LifeComponent->OnLifeStateChanged.Remove(LifeStateHandle);
	LifeStateHandle.Reset();
	AbilitySystem->CancelAllAbilities();
	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		Spec.InputPressed = false;
	}
	if (HasAuthority() && DefaultAbilitySetHandle.IsValid())
	{
		const FKnightResult Revoke = AbilitySystem->RevokeAbilitySet(DefaultAbilitySetHandle);
		if (!Revoke.IsSuccess())
		{
			UE_LOG(LogKnightCharacter, Error, TEXT("Knight %s default grant cleanup failed: %s"),
				*KnightEntityId.Value.ToString(), *Revoke.Message.ToString());
		}
	}
	DefaultAbilitySetHandle = FKnightAbilitySetHandle();
	DefaultAbilitySet = nullptr;
	AbilitySystem->ClearActorInfo();
	Super::EndPlay(EndPlayReason);
}

void AKnightCharacter::OnRep_Controller()
{
	if (CanRefreshKnightAbilityContext())
	{
		AbilitySystem->HandleControllerChanged();
	}
	Super::OnRep_Controller();
	RefreshKnightAbilityContext();
	CameraModeComponent->HandleControllerChanged();
}

void AKnightCharacter::OnRep_Owner()
{
	Super::OnRep_Owner();
	RefreshKnightAbilityContext();
}

void AKnightCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightCharacter, KnightEntityId);
	DOREPLIFETIME(AKnightCharacter, HeroDefinitionId);
}

void AKnightCharacter::OnRep_KnightEntityId()
{
	RefreshKnightAbilityContext();
	OnIdentityChanged.Broadcast();
}

void AKnightCharacter::OnRep_HeroDefinitionId()
{
	ResolveHeroDefinition();
}

void AKnightCharacter::ResolveHeroDefinition()
{
	const UWorld* World = GetWorld();
	if (bEndingPlay || World == nullptr || World->bIsTearingDown || !HeroDefinitionId.IsValid())
	{
		return;
	}
	if (HeroDefinition != nullptr && HeroDefinition->GetPrimaryAssetId() == HeroDefinitionId)
	{
		return;
	}
	UKnightAssetManager* AssetManager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* ResolvedDefinition = nullptr;
	const FKnightResult Result = AssetManager != nullptr
		? AssetManager->ResolveDefinition(HeroDefinitionId, UKnightHeroDefinition::StaticClass(), ResolvedDefinition)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("ResolveHeroDefinition"),
			NSLOCTEXT("KnightCharacter", "NoAssetManager", "Knight Asset Manager is unavailable."));
	HeroDefinition = Cast<UKnightHeroDefinition>(ResolvedDefinition);
	if (!Result.IsSuccess() || HeroDefinition == nullptr)
	{
		UE_LOG(LogKnightCharacter, Error, TEXT("Hero definition %s failed: %s"),
			*HeroDefinitionId.ToString(), *Result.Message.ToString());
		OnIdentityChanged.Broadcast();
		return;
	}
	ApplyHeroPresentation();
	RefreshKnightAbilityContext();
	OnIdentityChanged.Broadcast();
}

FKnightResult AKnightCharacter::InitializeKnightAbilitySystem()
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("InitializeKnightAbilities"),
			NSLOCTEXT("KnightCharacter", "AbilityWorldEnding", "Knight abilities cannot initialize while ending play."));
	}
	if (!KnightEntityId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("InitializeKnightAbilities"),
			NSLOCTEXT("KnightCharacter", "AbilityIdentityPending", "Knight abilities require a replicated stable identity."));
	}
	if (!LifeStateHandle.IsValid())
	{
		LifeStateHandle = LifeComponent->OnLifeStateChanged.AddUObject(this, &AKnightCharacter::HandleLifeStateChanged);
	}
	const FKnightResult Initialize = AbilitySystem->InitializeKnightActor(this, Attributes, LifeComponent, KnightEntityId);
	if (!Initialize.IsSuccess())
	{
		return Initialize;
	}
	HandleLifeStateChanged(LifeComponent->GetLifeState(), LifeComponent->GetLifeState());
	if (!HasAuthority() || DefaultAbilitySetHandle.IsValid())
	{
		return FKnightResult::Success();
	}
	if (HeroDefinition == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InitializeKnightAbilities"),
			NSLOCTEXT("KnightCharacter", "AbilityHeroPending", "Hero data is required before granting Knight defaults."));
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Definition = nullptr;
	const FKnightResult Resolve = Manager != nullptr
		? Manager->ResolveDefinition(HeroDefinition->GetDefaultAbilitySetId(),
			UKnightAbilitySetDefinition::StaticClass(), Definition)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("InitializeKnightAbilities"),
			NSLOCTEXT("KnightCharacter", "AbilityManagerMissing", "Knight Asset Manager is unavailable for ability defaults."));
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	DefaultAbilitySet = Cast<UKnightAbilitySetDefinition>(Definition);
	return AbilitySystem->GrantAbilitySet(DefaultAbilitySet, FName(TEXT("HeroDefaults")), DefaultAbilitySetHandle);
}

bool AKnightCharacter::CanRefreshKnightAbilityContext() const
{
	const UWorld* World = GetWorld();
	return !bEndingPlay && !IsActorBeingDestroyed() && IsValid(AbilitySystem) && KnightEntityId.IsValid()
		&& World != nullptr && !World->bIsTearingDown;
}

void AKnightCharacter::RefreshKnightAbilityContext()
{
	if (!CanRefreshKnightAbilityContext())
	{
		return;
	}
	AbilityInitializationResult = InitializeKnightAbilitySystem();
	if (!AbilityInitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightCharacter, Error, TEXT("Knight %s ability initialization failed: %s"),
			*KnightEntityId.Value.ToString(), *AbilityInitializationResult.Message.ToString());
	}
	else if (CombatComponent->GetProfileId().IsValid())
	{
		CombatComponent->TryInitializePresentation();
	}
}

void AKnightCharacter::HandleLifeStateChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	if (!CanRefreshKnightAbilityContext())
	{
		return;
	}
	if (Current != EKnightLifeState::Alive)
	{
		ResetControlInput();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		if (AKnightPlayerController* Human = Cast<AKnightPlayerController>(GetController()))
		{
			Human->ResetControlInput();
		}
		if (HasAuthority())
		{
			if (AAIController* Companion = Cast<AAIController>(GetController()))
			{
				Companion->StopMovement();
				if (UBrainComponent* Brain = Companion->GetBrainComponent())
				{
					Brain->StopLogic(TEXT("Knight is downed or dead"));
				}
			}
		}
	}
	else
	{
		if (GetCharacterMovement()->MovementMode == MOVE_None && !FinisherComponent->IsReserved()
			&& !TraversalComponent->HasActiveTraversal()
			&& !AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Staggered))
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		if (HasAuthority() && Previous != Current)
		{
			if (AAIController* Companion = Cast<AAIController>(GetController()))
			{
				if (UBrainComponent* Brain = Companion->GetBrainComponent())
				{
					Brain->RestartLogic();
				}
			}
		}
	}
}

void AKnightCharacter::HandleCombatProfileChanged(UKnightCombatProfileDefinition* Profile)
{
	const bool bHasCombatPresentation = Profile != nullptr;
	BodyMesh->SetHiddenInGame(bHasCombatPresentation);
	FacingMarker->SetHiddenInGame(bHasCombatPresentation);
}

void AKnightCharacter::ApplyHeroPresentation()
{
	if (HeroDefinition == nullptr)
	{
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed = HeroDefinition->GetWalkSpeed();
	const FKnightResult MovementPresentation = CombatComponent->RefreshBaseWalkSpeed(HeroDefinition->GetWalkSpeed());
	if (!MovementPresentation.IsSuccess())
	{
		UE_LOG(LogKnightCharacter, Error, TEXT("Knight %s movement presentation failed [%s]: %s"),
			*KnightEntityId.Value.ToString(), *MovementPresentation.Context.ToString(),
			*MovementPresentation.Message.ToString());
	}
	if (BodyMaterial == nullptr)
	{
		BodyMaterial = BodyMesh->CreateDynamicMaterialInstance(0);
	}
	if (BodyMaterial != nullptr)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), HeroDefinition->GetBodyColor());
		FacingMarker->SetMaterial(0, BodyMaterial);
	}
}
