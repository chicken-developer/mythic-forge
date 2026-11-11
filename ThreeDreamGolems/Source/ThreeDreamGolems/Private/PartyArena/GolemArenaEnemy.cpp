#include "PartyArena/GolemArenaEnemy.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "Skills/GolemSkillRuntimeComponent.h"

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Boss/GolemBossPhaseComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Controllers/GolemEnemyController.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Effects/GolemArenaResourceRecoveryEffect.h"
#include "Finisher/GolemFinisherComponent.h"
#include "Tags/GolemGameplayTags.h"
#include "Threat/GolemThreatComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemArenaEnemy, Log, All);

AGolemArenaEnemy::AGolemArenaEnemy()
{
	SkillLabArmor = CreateDefaultSubobject<UGolemBossArmorComponent>(TEXT("SkillLabArmor"));
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
	AbilitySystem = CreateDefaultSubobject<UGolemAbilitySystemComponent>(TEXT("EnemyAbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Full);
	Attributes = CreateDefaultSubobject<UGolemAttributeSet>(TEXT("EnemyAttributes"));
	Life = CreateDefaultSubobject<UGolemLifeComponent>(TEXT("EnemyLife"));
	Combat = CreateDefaultSubobject<UGolemCombatComponent>(TEXT("EnemyCombat"));
	Finisher = CreateDefaultSubobject<UGolemFinisherComponent>(TEXT("EnemyFinisher"));
	Threat = CreateDefaultSubobject<UGolemThreatComponent>(TEXT("EnemyThreat"));
	BossPhase = CreateDefaultSubobject<UGolemBossPhaseComponent>(TEXT("BossPhases"));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EnemyLabel"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.0, 0.0, 145.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(24.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor::Red);
	Label->SetText(NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY"));
	InitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaEnemy"),
		NSLOCTEXT("KnightArena", "EnemyPending", "Enemy definition has not initialized."));
}

UAbilitySystemComponent* AGolemArenaEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AGolemArenaEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemArenaEnemy, EnemyId);
	DOREPLIFETIME(AGolemArenaEnemy, ArchetypeId);
	DOREPLIFETIME(AGolemArenaEnemy, BossDefinitionId);
}

FGolemResult AGolemArenaEnemy::InitializeArenaEnemy(const FPrimaryAssetId& InArchetypeId,
	const FPrimaryAssetId& InBossDefinitionId)
{
	if (!HasAuthority() || EnemyId.IsValid())
	{
		return FGolemResult::Failure(HasAuthority() ? EGolemError::Conflict : EGolemError::NotAuthority,
			TEXT("ArenaEnemy"), NSLOCTEXT("KnightArena", "EnemyIdentity", "Only a fresh server enemy can initialize."));
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Definition = nullptr;
	InitializationResult = Manager != nullptr
		? Manager->ResolveDefinition(InArchetypeId, UGolemAIArchetypeDefinition::StaticClass(), Definition)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaEnemy"), FText::GetEmpty());
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	Archetype = Cast<UGolemAIArchetypeDefinition>(Definition);
	InitializationResult = Archetype->ValidateArchetype();
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	ArchetypeId = InArchetypeId;
	BossDefinitionId = InBossDefinitionId;
	EnemyId = FGolemEntityId::Create();
	InitializationResult = AbilitySystem->InitializeGolemActor(this, Attributes, Life,
		EnemyId, Archetype->GetTuning().Attributes, false);
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	AbilitySystem->AddLooseGameplayTag(GolemGameplayTags::Role_Enemy, 1,
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
		SpecializationRuntime = NewObject<UGolemSkillRuntimeComponent>(this, TEXT("EnemySkills"));
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
		UGolemArenaResourceRecoveryEffect::StaticClass(), 1.0f, Context);
	if (Spec.IsValid())
	{
		RecoveryEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	if (!RecoveryEffect.IsValid())
	{
		InitializationResult = FGolemResult::Failure(EGolemError::InvalidState,
			TEXT("ArenaEnemyRecovery"), FText::GetEmpty());
		return InitializationResult;
	}
	Label->SetText(!Archetype->GetDisplayName().IsEmpty() ? Archetype->GetDisplayName() : BossDefinitionId.IsValid()
		? NSLOCTEXT("KnightArena", "BossLabel", "ARENA BOSS: two phases")
		: NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY"));
	ForceNetUpdate();
	return InitializationResult;
}

void AGolemArenaEnemy::BeginPlay()
{
	Super::BeginPlay();
	LifeHandle = Life->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleLifeChanged);
	if (HasAuthority() && InitializationResult.IsSuccess())
	{
		FActorSpawnParameters Parameters;
		// Possession makes this pawn owned by its controller; the controller cannot be owned by the pawn.
		Parameters.Owner = GetOwner();
		AGolemEnemyController* EnemyController = GetWorld()->SpawnActor<AGolemEnemyController>(Parameters);
		if (EnemyController == nullptr)
		{
			InitializationResult = FGolemResult::Failure(EGolemError::InvalidState,
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
			UE_LOG(LogGolemArenaEnemy, Error, TEXT("Enemy %s controller initialization failed: %s"),
				*EnemyId.Value.ToString(), *InitializationResult.Message.ToString());
		}
	}
}

void AGolemArenaEnemy::HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	if (Current != EGolemLifeState::Alive)
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	else if (GetCharacterMovement()->MovementMode == MOVE_None)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void AGolemArenaEnemy::OnRep_EnemyIdentity()
{
	Label->SetText(BossDefinitionId.IsValid()
		? NSLOCTEXT("KnightArena", "BossLabel", "ARENA BOSS: two phases")
		: NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY"));
	if (!EnemyId.IsValid())
	{
		return;
	}
	InitializationResult = AbilitySystem->InitializeGolemActor(this, Attributes, Life, EnemyId,
		FGolemAttributeDefaults(), false);
	if (InitializationResult.IsSuccess() && Combat->GetProfileId().IsValid())
	{
		InitializationResult = Combat->TryInitializePresentation();
	}
}

void AGolemArenaEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

UGolemAbilitySystemComponent* AGolemArenaEnemy::GetGolemAbilitySystem() const
{
	return AbilitySystem;
}

UGolemAttributeSet* AGolemArenaEnemy::GetGolemAttributeSet() const
{
	return Attributes;
}

UGolemLifeComponent* AGolemArenaEnemy::GetLifeComponent() const
{
	return Life;
}

UGolemCombatComponent* AGolemArenaEnemy::GetCombatComponent() const
{
	return Combat;
}

UGolemThreatComponent* AGolemArenaEnemy::GetThreatComponent() const
{
	return Threat;
}

UGolemBossPhaseComponent* AGolemArenaEnemy::GetBossPhaseComponent() const
{
	return BossPhase;
}

FGolemEntityId AGolemArenaEnemy::GetArenaEnemyId() const
{
	return EnemyId;
}

FPrimaryAssetId AGolemArenaEnemy::GetArchetypeId() const
{
	return ArchetypeId;
}

const FGolemResult& AGolemArenaEnemy::GetInitializationResult() const
{
	return InitializationResult;
}

FActiveGameplayEffectHandle AGolemArenaEnemy::GetRecoveryEffectHandle() const
{
	return RecoveryEffect;
}
