#include "CombatChallenge/KnightCombatFixture.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Finisher/KnightFinisherComponent.h"
#include "Tags/KnightGameplayTags.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightCombatFixture, Log, All);

AKnightCombatFixture::AKnightCombatFixture()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.0f);
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -88.0));
	GetMesh()->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetCharacterMovement()->MaxWalkSpeed = 150.0f;
	AbilitySystem = CreateDefaultSubobject<UKnightAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UKnightAttributeSet>(TEXT("Attributes"));
	Life = CreateDefaultSubobject<UKnightLifeComponent>(TEXT("Life"));
	Combat = CreateDefaultSubobject<UKnightCombatComponent>(TEXT("Combat"));
	Finisher = CreateDefaultSubobject<UKnightFinisherComponent>(TEXT("Finisher"));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FixtureLabel"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.0, 0.0, 135.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(22.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	RefreshLabel();
}

UAbilitySystemComponent* AKnightCombatFixture::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AKnightCombatFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightCombatFixture, FixtureId);
	DOREPLIFETIME(AKnightCombatFixture, bTelegraphing);
}

UKnightAbilitySystemComponent* AKnightCombatFixture::GetKnightAbilitySystem() const
{
	return AbilitySystem;
}

UKnightAttributeSet* AKnightCombatFixture::GetKnightAttributeSet() const
{
	return Attributes;
}

UKnightLifeComponent* AKnightCombatFixture::GetLifeComponent() const
{
	return Life;
}

UKnightCombatComponent* AKnightCombatFixture::GetCombatComponent() const
{
	return Combat;
}

FKnightEntityId AKnightCombatFixture::GetFixtureId() const
{
	return FixtureId;
}

EKnightCombatFixtureKind AKnightCombatFixture::GetFixtureKind() const
{
	return FixtureKind;
}

bool AKnightCombatFixture::IsTelegraphing() const
{
	return bTelegraphing;
}

const FKnightResult& AKnightCombatFixture::GetInitializationResult() const
{
	return InitializationResult;
}

void AKnightCombatFixture::SetFixtureKind(const EKnightCombatFixtureKind InKind)
{
	if (!HasActorBegunPlay())
	{
		FixtureKind = InKind;
		RefreshLabel();
	}
}

void AKnightCombatFixture::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	if (HasAuthority() && !FixtureId.IsValid())
	{
		FixtureId = FKnightEntityId::Create();
	}
	InitializeFixture();
}

void AKnightCombatFixture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTelegraph();
	const bool bWorldEnding = EndPlayReason == EEndPlayReason::EndPlayInEditor
		|| EndPlayReason == EEndPlayReason::Quit || EndPlayReason == EEndPlayReason::LevelTransition;
	Finisher->PrepareForEndPlay(bWorldEnding);
	Super::EndPlay(EndPlayReason);
}

void AKnightCombatFixture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLabel();
}

void AKnightCombatFixture::InitializeFixture()
{
	if (!FixtureId.IsValid())
	{
		return;
	}
	FKnightAttributeDefaults Defaults;
	Defaults.MaxHealth = 400.0f;
	Defaults.MaxStamina = 400.0f;
	Defaults.MaxRuneEnergy = 100.0f;
	InitializationResult = AbilitySystem->InitializeKnightActor(this, Attributes, Life, FixtureId, Defaults, false);
	if (InitializationResult.IsSuccess() && HasAuthority())
	{
		if (!AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy))
		{
			AbilitySystem->AddLooseGameplayTag(KnightGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		InitializationResult = Combat->Configure(FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat")));
	}
	else if (InitializationResult.IsSuccess() && Combat->GetProfileId().IsValid())
	{
		InitializationResult = Combat->TryInitializePresentation();
	}
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightCombatFixture, Error, TEXT("Fixture %s initialization failed: %s."),
			*GetName(), *InitializationResult.Message.ToString());
	}
}

FKnightResult AKnightCombatFixture::TryTelegraphAttack(AActor* Target)
{
	if (!HasAuthority() || FixtureKind != EKnightCombatFixtureKind::ScriptedAttacker)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("Telegraph"),
			NSLOCTEXT("KnightCombatChallenge", "ScriptAuthority", "Only the scripted attacker may prepare this exercise."));
	}
	if (bTelegraphing || !Life->IsAlive() || Combat->GetSnapshot().bAttacking || !InitializationResult.IsSuccess())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("Telegraph"),
			NSLOCTEXT("KnightCombatChallenge", "ScriptBusy", "Wait for the scripted attack to finish."));
	}
	const FKnightResult TargetValidation = Combat->ValidateCombatTarget(Target, 220.0f);
	if (!TargetValidation.IsSuccess())
	{
		return TargetValidation;
	}
	const FKnightResult Lock = Combat->SetValidatedTarget(Target);
	if (!Lock.IsSuccess())
	{
		return Lock;
	}
	SetActorRotation((Target->GetActorLocation() - GetActorLocation()).Rotation());
	AttackTarget = Target;
	bTelegraphing = true;
	RefreshLabel();
	ForceNetUpdate();
	GetWorldTimerManager().SetTimer(TelegraphTimer, this, &ThisClass::ReleaseScriptedAttack, 0.8f, false);
	return FKnightResult::Success();
}

void AKnightCombatFixture::ReleaseScriptedAttack()
{
	bTelegraphing = false;
	RefreshLabel();
	ForceNetUpdate();
	if (HasAuthority() && AttackTarget.IsValid() && Life->IsAlive())
	{
		const FKnightResult Result = AbilitySystem->RequestAbilityInput(
			KnightGameplayTags::Input_Combat_Primary, true, AttackTarget.Get());
		AbilitySystem->RequestAbilityInput(KnightGameplayTags::Input_Combat_Primary, false);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogKnightCombatFixture, Display, TEXT("Scripted attack rejected safely: %s."), *Result.Message.ToString());
		}
	}
	AttackTarget.Reset();
}

void AKnightCombatFixture::ClearTelegraph()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(TelegraphTimer);
	}
	AttackTarget.Reset();
	bTelegraphing = false;
}

FKnightResult AKnightCombatFixture::ResetForCheckpoint()
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("FixtureReset"), FText::GetEmpty());
	}
	ClearTelegraph();
	if (Finisher->HasActiveFinisher())
	{
		Finisher->EndFinisher(Finisher->GetReservation(), false);
	}
	Combat->ClearTransientActions();
	const FKnightResult Reset = Life->ResetForCheckpoint();
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	const FKnightResult CombatReset = Combat->ResetForCheckpoint();
	GetCharacterMovement()->StopMovementImmediately();
	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	RefreshLabel();
	ForceNetUpdate();
	return CombatReset;
}

void AKnightCombatFixture::RefreshLabel()
{
	if (Label == nullptr)
	{
		return;
	}
	if (FixtureKind == EKnightCombatFixtureKind::ScriptedAttacker)
	{
		Label->SetText(bTelegraphing
			? NSLOCTEXT("KnightCombatChallenge", "Incoming", "INCOMING: block Q / parry C / dodge Shift")
			: NSLOCTEXT("KnightCombatChallenge", "Scripted", "F / D-pad Left: telegraph attack (0.8 seconds)"));
		Label->SetTextRenderColor(bTelegraphing ? FColor::Red : FColor::Yellow);
	}
	else
	{
		Label->SetText(FixtureKind == EKnightCombatFixtureKind::MeleeTarget
			? NSLOCTEXT("KnightCombatChallenge", "Melee", "LMB combo / RMB heavy / H finisher when staggered")
			: NSLOCTEXT("KnightCombatChallenge", "Rune", "MMB lock / G rune / H finisher when staggered"));
		Label->SetTextRenderColor(FixtureKind == EKnightCombatFixtureKind::MeleeTarget ? FColor::Cyan : FColor::Purple);
	}
}

void AKnightCombatFixture::OnRep_FixtureId()
{
	InitializeFixture();
}

void AKnightCombatFixture::OnRep_Telegraphing()
{
	RefreshLabel();
}
