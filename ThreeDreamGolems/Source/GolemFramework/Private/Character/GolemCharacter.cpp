#include "Character/GolemCharacter.h"

#include "AIController.h"
#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/GolemCameraModeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/GolemPartyGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayAbilitySpec.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemCharacter, Log, All);

AGolemCharacter::AGolemCharacter()
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
	AbilitySystem = CreateDefaultSubobject<UGolemAbilitySystemComponent>(TEXT("GolemAbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	Attributes = CreateDefaultSubobject<UGolemAttributeSet>(TEXT("GolemAttributes"));
	LifeComponent = CreateDefaultSubobject<UGolemLifeComponent>(TEXT("GolemLife"));
	CombatComponent = CreateDefaultSubobject<UGolemCombatComponent>(TEXT("GolemCombat"));
	CameraModeComponent = CreateDefaultSubobject<UGolemCameraModeComponent>(TEXT("GolemCameraModes"));
	AbilityInitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("InitializeGolemAbilities"),
		NSLOCTEXT("GolemCharacter", "AbilitiesPending", "Golem identity and ability defaults are not initialized yet."));

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

FGolemResult AGolemCharacter::InitializeGolem(
	const FGolemEntityId& InGolemEntityId, UGolemHeroDefinition* InHeroDefinition)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("InitializeGolem"),
			NSLOCTEXT("GolemCharacter", "Authority", "Only the server initializes a canonical Golem."));
	}
	if (!InGolemEntityId.IsValid() || !IsValid(InHeroDefinition))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("InitializeGolem"),
			NSLOCTEXT("GolemCharacter", "MissingIdentity", "Golem instance and hero definition IDs are required."));
	}
	const FGolemResult DefinitionResult = InHeroDefinition->ValidateHero();
	if (!DefinitionResult.IsSuccess())
	{
		return DefinitionResult;
	}
	const FPrimaryAssetId InHeroDefinitionId = InHeroDefinition->GetPrimaryAssetId();
	if (GolemEntityId.IsValid())
	{
		if (GolemEntityId == InGolemEntityId && HeroDefinitionId == InHeroDefinitionId)
		{
			AbilityInitializationResult = InitializeGolemAbilitySystem();
			return AbilityInitializationResult;
		}
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InitializeGolem"),
			NSLOCTEXT("GolemCharacter", "ImmutableIdentity", "An initialized Golem cannot change persistent identity."));
	}
	GolemEntityId = InGolemEntityId;
	HeroDefinitionId = InHeroDefinitionId;
	HeroDefinition = InHeroDefinition;
	ApplyHeroPresentation();
	AbilityInitializationResult = InitializeGolemAbilitySystem();
	if (!AbilityInitializationResult.IsSuccess())
	{
		return AbilityInitializationResult;
	}
	ForceNetUpdate();
	OnIdentityChanged.Broadcast();
	return FGolemResult::Success();
}

FGolemEntityId AGolemCharacter::GetGolemEntityId() const
{
	return GolemEntityId;
}

FPrimaryAssetId AGolemCharacter::GetHeroDefinitionId() const
{
	return HeroDefinitionId;
}

UGolemHeroDefinition* AGolemCharacter::GetHeroDefinition() const
{
	return HeroDefinition;
}

FText AGolemCharacter::GetGolemDisplayName() const
{
	return HeroDefinition != nullptr ? HeroDefinition->GetDisplayName()
		: NSLOCTEXT("GolemCharacter", "DefinitionPending", "Golem definition is loading");
}

UAbilitySystemComponent* AGolemCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UGolemAbilitySystemComponent* AGolemCharacter::GetGolemAbilitySystem() const
{
	return AbilitySystem;
}

UGolemAttributeSet* AGolemCharacter::GetGolemAttributeSet() const
{
	return Attributes;
}

UGolemLifeComponent* AGolemCharacter::GetLifeComponent() const
{
	return LifeComponent;
}

UGolemCombatComponent* AGolemCharacter::GetCombatComponent() const
{
	return CombatComponent;
}

UGolemCameraModeComponent* AGolemCharacter::GetCameraModeComponent() const
{
	return CameraModeComponent;
}

const FGolemResult& AGolemCharacter::GetAbilityInitializationResult() const
{
	return AbilityInitializationResult;
}

bool AGolemCharacter::CanTransferControl() const
{
	const UWorld* World = GetWorld();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return GolemEntityId.IsValid() && !IsActorBeingDestroyed() && World != nullptr
		&& !World->bIsTearingDown && !World->IsInSeamlessTravel()
		&& LifeComponent != nullptr && LifeComponent->IsAlive()
		&& AbilitySystem != nullptr
		&& CombatComponent != nullptr && CombatComponent->CanTransferControl()
		&& Movement != nullptr && Movement->MovementMode != MOVE_None;
}

void AGolemCharacter::ResetControlInput()
{
	StopJumping();
	ConsumeMovementInputVector();
	if (HasAuthority() && CanRefreshGolemAbilityContext())
	{
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Ability_Test, false);
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, false);
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Combat_Primary, false);
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, false);
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Combat_Dodge, false);
	}
}

UCameraComponent* AGolemCharacter::GetThirdPersonCamera() const
{
	return ThirdPersonCamera;
}

USpringArmComponent* AGolemCharacter::GetCameraBoom() const
{
	return CameraBoom;
}

void AGolemCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}
	if (!HasAuthority() && GolemEntityId.IsValid())
	{
		// Preserve the replicated canonical actor while the server sends its recovery correction.
		return;
	}
	AGolemPartyGameMode* Mode = World->GetAuthGameMode<AGolemPartyGameMode>();
	AGolemPartyState* Party = Mode != nullptr ? Mode->GetPartyState() : nullptr;
	const FGolemRosterEntry* Entry = Party != nullptr ? Party->FindGolem(this) : nullptr;
	if (HasAuthority() && Entry != nullptr && Entry->GolemId == GolemEntityId
		&& Entry->HeroDefinitionId == HeroDefinitionId)
	{
		const FGolemResult Recovery = Mode->TryRecoverGolemFromFall(this);
		if (!Recovery.IsSuccess())
		{
			UE_LOG(LogGolemCharacter, Error, TEXT("Canonical Golem %s fall recovery failed [%s]: %s; actor preserved"),
				*GolemEntityId.Value.ToString(), *Recovery.Context.ToString(), *Recovery.Message.ToString());
		}
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

void AGolemCharacter::PossessedBy(AController* NewController)
{
	if (CanRefreshGolemAbilityContext())
	{
		if (HasAuthority())
		{
			CombatComponent->ClearTransientActions();
		}
		AbilitySystem->HandleControllerChanged();
	}
	Super::PossessedBy(NewController);
	RefreshGolemAbilityContext();
	CameraModeComponent->HandleControllerChanged();
}

void AGolemCharacter::UnPossessed()
{
	if (CanRefreshGolemAbilityContext())
	{
		if (HasAuthority())
		{
			CombatComponent->ClearTransientActions();
		}
		AbilitySystem->HandleControllerChanged();
	}
	Super::UnPossessed();
	RefreshGolemAbilityContext();
	CameraModeComponent->HandleControllerChanged();
}

void AGolemCharacter::BeginPlay()
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
	RefreshGolemAbilityContext();
}

void AGolemCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	CombatComponent->OnProfileChanged.Remove(CombatProfileHandle);
	CombatProfileHandle.Reset();
	LifeComponent->OnLifeStateChanged.Remove(LifeStateHandle);
	LifeStateHandle.Reset();
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetAgilityAttribute()).Remove(AgilityHandle);
	AgilityHandle.Reset();
	AbilitySystem->CancelAllAbilities();
	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		Spec.InputPressed = false;
	}
	if (HasAuthority() && DefaultAbilitySetHandle.IsValid())
	{
		const FGolemResult Revoke = AbilitySystem->RevokeAbilitySet(DefaultAbilitySetHandle);
		if (!Revoke.IsSuccess())
		{
			UE_LOG(LogGolemCharacter, Error, TEXT("Golem %s default grant cleanup failed: %s"),
				*GolemEntityId.Value.ToString(), *Revoke.Message.ToString());
		}
	}
	DefaultAbilitySetHandle = FGolemAbilitySetHandle();
	DefaultAbilitySet = nullptr;
	AbilitySystem->ClearActorInfo();
	Super::EndPlay(EndPlayReason);
}

void AGolemCharacter::OnRep_Controller()
{
	if (CanRefreshGolemAbilityContext())
	{
		AbilitySystem->HandleControllerChanged();
	}
	Super::OnRep_Controller();
	RefreshGolemAbilityContext();
	CameraModeComponent->HandleControllerChanged();
}

void AGolemCharacter::OnRep_Owner()
{
	Super::OnRep_Owner();
	RefreshGolemAbilityContext();
}

void AGolemCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemCharacter, GolemEntityId);
	DOREPLIFETIME(AGolemCharacter, HeroDefinitionId);
}

void AGolemCharacter::OnRep_GolemEntityId()
{
	RefreshGolemAbilityContext();
	OnIdentityChanged.Broadcast();
}

void AGolemCharacter::OnRep_HeroDefinitionId()
{
	ResolveHeroDefinition();
}

void AGolemCharacter::ResolveHeroDefinition()
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
	UGolemAssetManager* AssetManager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* ResolvedDefinition = nullptr;
	const FGolemResult Result = AssetManager != nullptr
		? AssetManager->ResolveDefinition(HeroDefinitionId, UGolemHeroDefinition::StaticClass(), ResolvedDefinition)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("ResolveHeroDefinition"),
			NSLOCTEXT("GolemCharacter", "NoAssetManager", "Golem Asset Manager is unavailable."));
	HeroDefinition = Cast<UGolemHeroDefinition>(ResolvedDefinition);
	if (!Result.IsSuccess() || HeroDefinition == nullptr)
	{
		UE_LOG(LogGolemCharacter, Error, TEXT("Hero definition %s failed: %s"),
			*HeroDefinitionId.ToString(), *Result.Message.ToString());
		OnIdentityChanged.Broadcast();
		return;
	}
	ApplyHeroPresentation();
	RefreshGolemAbilityContext();
	OnIdentityChanged.Broadcast();
}

FGolemAttributeDefaults AGolemCharacter::GetInitialAttributeDefaults() const
{
	// Each Golem starts from its own designed stats; a pawn without hero data keeps the fallbacks.
	return HeroDefinition != nullptr ? HeroDefinition->GetBaseStats() : FGolemAttributeDefaults();
}

FGolemResult AGolemCharacter::InitializeGolemAbilitySystem()
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("InitializeGolemAbilities"),
			NSLOCTEXT("GolemCharacter", "AbilityWorldEnding", "Golem abilities cannot initialize while ending play."));
	}
	if (!GolemEntityId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("InitializeGolemAbilities"),
			NSLOCTEXT("GolemCharacter", "AbilityIdentityPending", "Golem abilities require a replicated stable identity."));
	}
	if (!LifeStateHandle.IsValid())
	{
		LifeStateHandle = LifeComponent->OnLifeStateChanged.AddUObject(this, &AGolemCharacter::HandleLifeStateChanged);
	}
	const FGolemResult Initialize = AbilitySystem->InitializeGolemActor(this, Attributes, LifeComponent, GolemEntityId,
		GetInitialAttributeDefaults());
	if (!Initialize.IsSuccess())
	{
		return Initialize;
	}
	HandleLifeStateChanged(LifeComponent->GetLifeState(), LifeComponent->GetLifeState());
	if (!AgilityHandle.IsValid())
	{
		// Agility scales move speed on the host and on every client, including when it replicates late.
		AgilityHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetAgilityAttribute())
			.AddUObject(this, &AGolemCharacter::HandleAgilityChanged);
		ApplyHeroMovement();
	}
	if (!HasAuthority() || DefaultAbilitySetHandle.IsValid())
	{
		return FGolemResult::Success();
	}
	if (HeroDefinition == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InitializeGolemAbilities"),
			NSLOCTEXT("GolemCharacter", "AbilityHeroPending", "Hero data is required before granting Golem defaults."));
	}
	// The element decides which colour this Golem places on a void; both tags replicate with the ASC.
	for (const FGameplayTag ArchetypeTag : { HeroDefinition->GetElementTag(), HeroDefinition->GetClassTag() })
	{
		if (ArchetypeTag.IsValid())
		{
			AbilitySystem->SetLooseGameplayTagCount(ArchetypeTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Definition = nullptr;
	const FGolemResult Resolve = Manager != nullptr
		? Manager->ResolveDefinition(HeroDefinition->GetDefaultAbilitySetId(),
			UGolemAbilitySetDefinition::StaticClass(), Definition)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("InitializeGolemAbilities"),
			NSLOCTEXT("GolemCharacter", "AbilityManagerMissing", "Golem Asset Manager is unavailable for ability defaults."));
	if (!Resolve.IsSuccess())
	{
		return Resolve;
	}
	DefaultAbilitySet = Cast<UGolemAbilitySetDefinition>(Definition);
	return AbilitySystem->GrantAbilitySet(DefaultAbilitySet, FName(TEXT("HeroDefaults")), DefaultAbilitySetHandle);
}

bool AGolemCharacter::CanRefreshGolemAbilityContext() const
{
	const UWorld* World = GetWorld();
	return !bEndingPlay && !IsActorBeingDestroyed() && IsValid(AbilitySystem) && GolemEntityId.IsValid()
		&& World != nullptr && !World->bIsTearingDown;
}

void AGolemCharacter::RefreshGolemAbilityContext()
{
	if (!CanRefreshGolemAbilityContext())
	{
		return;
	}
	AbilityInitializationResult = InitializeGolemAbilitySystem();
	if (!AbilityInitializationResult.IsSuccess())
	{
		UE_LOG(LogGolemCharacter, Error, TEXT("Golem %s ability initialization failed: %s"),
			*GolemEntityId.Value.ToString(), *AbilityInitializationResult.Message.ToString());
	}
	else if (CombatComponent->GetProfileId().IsValid())
	{
		CombatComponent->TryInitializePresentation();
	}
}

void AGolemCharacter::HandleLifeStateChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	if (!CanRefreshGolemAbilityContext())
	{
		return;
	}
	if (Current != EGolemLifeState::Alive)
	{
		ResetControlInput();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		if (AGolemPlayerController* Human = Cast<AGolemPlayerController>(GetController()))
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
					Brain->StopLogic(TEXT("Golem is downed or dead"));
				}
			}
		}
	}
	else
	{
		if (GetCharacterMovement()->MovementMode == MOVE_None)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		if (HasAuthority() && Previous != Current && !LifeComponent->ShouldDeferControllerRestart())
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

void AGolemCharacter::HandleCombatProfileChanged(UGolemCombatProfileDefinition* Profile)
{
	const bool bHasCombatPresentation = Profile != nullptr;
	BodyMesh->SetHiddenInGame(bHasCombatPresentation);
	FacingMarker->SetHiddenInGame(bHasCombatPresentation);
}

void AGolemCharacter::HandleAgilityChanged(const FOnAttributeChangeData& Data)
{
	ApplyHeroMovement();
}

void AGolemCharacter::ApplyHeroMovement()
{
	if (HeroDefinition == nullptr || bEndingPlay)
	{
		return;
	}
	const FGolemResult MovementPresentation = CombatComponent->RefreshBaseWalkSpeed(HeroDefinition->GetWalkSpeed());
	if (!MovementPresentation.IsSuccess())
	{
		UE_LOG(LogGolemCharacter, Error, TEXT("Golem %s movement presentation failed [%s]: %s"),
			*GolemEntityId.Value.ToString(), *MovementPresentation.Context.ToString(),
			*MovementPresentation.Message.ToString());
	}
}

void AGolemCharacter::ApplyHeroPresentation()
{
	if (HeroDefinition == nullptr)
	{
		return;
	}
	ApplyHeroMovement();
	const FGolemResult MovesResult = CombatComponent->SetHeroMoves(HeroDefinition->GetMoves());
	if (!MovesResult.IsSuccess())
	{
		UE_LOG(LogGolemCharacter, Error, TEXT("Golem %s move tuning rejected [%s]."),
			*GolemEntityId.Value.ToString(), *MovesResult.Context.ToString());
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
