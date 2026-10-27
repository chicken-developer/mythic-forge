#include "PartyArena/KnightArenaEnemy.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "Skills/KnightSkillRuntimeComponent.h"

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Controllers/KnightEnemyController.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Effects/KnightArenaResourceRecoveryEffect.h"
#include "Finisher/KnightFinisherComponent.h"
#include "Tags/KnightGameplayTags.h"
#include "Threat/KnightThreatComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightArenaEnemy, Log, All);

AKnightArenaEnemy::AKnightArenaEnemy()
{
	SkillLabArmor = CreateDefaultSubobject<UKnightBossArmorComponent>(TEXT("SkillLabArmor"));
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.0f);
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -88.0));
	GetMesh()->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 540.0, 0.0);
	AbilitySystem = CreateDefaultSubobject<UKnightAbilitySystemComponent>(TEXT("EnemyAbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Full);
	Attributes = CreateDefaultSubobject<UKnightAttributeSet>(TEXT("EnemyAttributes"));
	Life = CreateDefaultSubobject<UKnightLifeComponent>(TEXT("EnemyLife"));
	Combat = CreateDefaultSubobject<UKnightCombatComponent>(TEXT("EnemyCombat"));
	Finisher = CreateDefaultSubobject<UKnightFinisherComponent>(TEXT("EnemyFinisher"));
	Threat = CreateDefaultSubobject<UKnightThreatComponent>(TEXT("EnemyThreat"));
	BossPhase = CreateDefaultSubobject<UKnightBossPhaseComponent>(TEXT("BossPhases"));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EnemyLabel"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.0, 0.0, 145.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(24.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor::Red);
	Label->SetText(NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY"));
	InitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaEnemy"),
		NSLOCTEXT("KnightArena", "EnemyPending", "Enemy definition has not initialized."));
}

UAbilitySystemComponent* AKnightArenaEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AKnightArenaEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightArenaEnemy, EnemyId);
	DOREPLIFETIME(AKnightArenaEnemy, ArchetypeId);
	DOREPLIFETIME(AKnightArenaEnemy, BossDefinitionId);
}

FKnightResult AKnightArenaEnemy::InitializeArenaEnemy(const FPrimaryAssetId& InArchetypeId,
	const FPrimaryAssetId& InBossDefinitionId)
{
	if (!HasAuthority() || EnemyId.IsValid())
	{
		return FKnightResult::Failure(HasAuthority() ? EKnightError::Conflict : EKnightError::NotAuthority,
			TEXT("ArenaEnemy"), NSLOCTEXT("KnightArena", "EnemyIdentity", "Only a fresh server enemy can initialize."));
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Definition = nullptr;
	InitializationResult = Manager != nullptr
		? Manager->ResolveDefinition(InArchetypeId, UKnightAIArchetypeDefinition::StaticClass(), Definition)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaEnemy"), FText::GetEmpty());
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	Archetype = Cast<UKnightAIArchetypeDefinition>(Definition);
	InitializationResult = Archetype->ValidateArchetype();
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	ArchetypeId = InArchetypeId;
	BossDefinitionId = InBossDefinitionId;
	EnemyId = FKnightEntityId::Create();
	InitializationResult = AbilitySystem->InitializeKnightActor(this, Attributes, Life,
		EnemyId, Archetype->GetTuning().Attributes, false);
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	AbilitySystem->AddLooseGameplayTag(KnightGameplayTags::Role_Enemy, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	InitializationResult = Combat->Configure(Archetype->GetCombatProfileId());
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	if (BossDefinitionId.IsValid())
	{
		InitializationResult = BossPhase->Configure(BossDefinitionId);
		if (!InitializationResult.IsSuccess())
		{
			return InitializationResult;
		}
	}
	if (!Archetype->Specialization.SpecId.IsNone())
	{
		SpecializationRuntime = NewObject<UKnightSkillRuntimeComponent>(this, TEXT("EnemySkills"));
		SpecializationRuntime->RegisterComponent();
		InitializationResult = SpecializationRuntime->ConfigureBuild(Archetype->Specialization);
		if (!InitializationResult.IsSuccess())
		{
			return InitializationResult;
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = Archetype->GetTuning().WalkSpeed;
	FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
		UKnightArenaResourceRecoveryEffect::StaticClass(), 1.0f, Context);
	if (Spec.IsValid())
	{
		RecoveryEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	if (!RecoveryEffect.IsValid())
	{
		InitializationResult = FKnightResult::Failure(EKnightError::InvalidState,
			TEXT("ArenaEnemyRecovery"), FText::GetEmpty());
		return InitializationResult;
	}
	Label->SetText(!Archetype->GetDisplayName().IsEmpty() ? Archetype->GetDisplayName() : BossDefinitionId.IsValid()
		? NSLOCTEXT("KnightArena", "BossLabel", "ARENA BOSS: two phases")
		: NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY"));
	ForceNetUpdate();
	return InitializationResult;
}

void AKnightArenaEnemy::BeginPlay()
{
	Super::BeginPlay();
	LifeHandle = Life->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleLifeChanged);
	if (HasAuthority() && InitializationResult.IsSuccess())
	{
		FActorSpawnParameters Parameters;
		// Possession makes this pawn owned by its controller; the controller cannot be owned by the pawn.
		Parameters.Owner = GetOwner();
		AKnightEnemyController* EnemyController = GetWorld()->SpawnActor<AKnightEnemyController>(Parameters);
		if (EnemyController == nullptr)
		{
			InitializationResult = FKnightResult::Failure(EKnightError::InvalidState,
				TEXT("ArenaEnemyController"), FText::GetEmpty());
			return;
		}
		InitializationResult = EnemyController->ConfigureArchetype(ArchetypeId);
		if (InitializationResult.IsSuccess())
		{
			EnemyController->Possess(this);
			InitializationResult = EnemyController->GetInitializationResult();
		}
		if (!InitializationResult.IsSuccess())
		{
			EnemyController->UnPossess();
			EnemyController->Destroy();
			UE_LOG(LogKnightArenaEnemy, Error, TEXT("Enemy %s controller initialization failed: %s"),
				*EnemyId.Value.ToString(), *InitializationResult.Message.ToString());
		}
	}
}

void AKnightArenaEnemy::HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	if (Current != EKnightLifeState::Alive)
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	else if (GetCharacterMovement()->MovementMode == MOVE_None)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void AKnightArenaEnemy::OnRep_EnemyIdentity()
{
	Label->SetText(BossDefinitionId.IsValid()
		? NSLOCTEXT("KnightArena", "BossLabel", "ARENA BOSS: two phases")
		: NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY"));
	if (!EnemyId.IsValid())
	{
		return;
	}
	InitializationResult = AbilitySystem->InitializeKnightActor(this, Attributes, Life, EnemyId,
		FKnightAttributeDefaults(), false);
	if (InitializationResult.IsSuccess() && Combat->GetProfileId().IsValid())
	{
		InitializationResult = Combat->TryInitializePresentation();
	}
}

void AKnightArenaEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Life->OnLifeStateChanged.Remove(LifeHandle);
	LifeHandle.Reset();
	Finisher->PrepareForEndPlay(EndPlayReason == EEndPlayReason::EndPlayInEditor
		|| EndPlayReason == EEndPlayReason::Quit || EndPlayReason == EEndPlayReason::LevelTransition);
	if (HasAuthority() && RecoveryEffect.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(RecoveryEffect);
		RecoveryEffect.Invalidate();
	}
	const bool bWorldEnding = EndPlayReason == EEndPlayReason::EndPlayInEditor
		|| EndPlayReason == EEndPlayReason::Quit || EndPlayReason == EEndPlayReason::LevelTransition;
	if (AController* OwnedController = GetController(); HasAuthority() && !bWorldEnding && IsValid(OwnedController))
	{
		OwnedController->UnPossess();
		OwnedController->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

UKnightAbilitySystemComponent* AKnightArenaEnemy::GetKnightAbilitySystem() const
{
	return AbilitySystem;
}

UKnightAttributeSet* AKnightArenaEnemy::GetKnightAttributeSet() const
{
	return Attributes;
}

UKnightLifeComponent* AKnightArenaEnemy::GetLifeComponent() const
{
	return Life;
}

UKnightCombatComponent* AKnightArenaEnemy::GetCombatComponent() const
{
	return Combat;
}

UKnightThreatComponent* AKnightArenaEnemy::GetThreatComponent() const
{
	return Threat;
}

UKnightBossPhaseComponent* AKnightArenaEnemy::GetBossPhaseComponent() const
{
	return BossPhase;
}

FKnightEntityId AKnightArenaEnemy::GetArenaEnemyId() const
{
	return EnemyId;
}

FPrimaryAssetId AKnightArenaEnemy::GetArchetypeId() const
{
	return ArchetypeId;
}

const FKnightResult& AKnightArenaEnemy::GetInitializationResult() const
{
	return InitializationResult;
}

FActiveGameplayEffectHandle AKnightArenaEnemy::GetRecoveryEffectHandle() const
{
	return RecoveryEffect;
}
