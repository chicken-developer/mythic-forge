#include "CombatChallenge/GolemCombatFixture.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Finisher/GolemFinisherComponent.h"
#include "Tags/GolemGameplayTags.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemCombatFixture, Log, All);

AGolemCombatFixture::AGolemCombatFixture()
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
	AbilitySystem = CreateDefaultSubobject<UGolemAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UGolemAttributeSet>(TEXT("Attributes"));
	Life = CreateDefaultSubobject<UGolemLifeComponent>(TEXT("Life"));
	Combat = CreateDefaultSubobject<UGolemCombatComponent>(TEXT("Combat"));
	Finisher = CreateDefaultSubobject<UGolemFinisherComponent>(TEXT("Finisher"));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FixtureLabel"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.0, 0.0, 135.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(22.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	RefreshLabel();
}

UAbilitySystemComponent* AGolemCombatFixture::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AGolemCombatFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemCombatFixture, FixtureId);
	DOREPLIFETIME(AGolemCombatFixture, bTelegraphing);
}

UGolemAbilitySystemComponent* AGolemCombatFixture::GetGolemAbilitySystem() const
{
	return AbilitySystem;
}

UGolemAttributeSet* AGolemCombatFixture::GetGolemAttributeSet() const
{
	return Attributes;
}

UGolemLifeComponent* AGolemCombatFixture::GetLifeComponent() const
{
	return Life;
}

UGolemCombatComponent* AGolemCombatFixture::GetCombatComponent() const
{
	return Combat;
}

FGolemEntityId AGolemCombatFixture::GetFixtureId() const
{
	return FixtureId;
}

EGolemCombatFixtureKind AGolemCombatFixture::GetFixtureKind() const
{
	return FixtureKind;
}

bool AGolemCombatFixture::IsTelegraphing() const
{
	return bTelegraphing;
}

const FGolemResult& AGolemCombatFixture::GetInitializationResult() const
{
	return InitializationResult;
}

void AGolemCombatFixture::SetFixtureKind(const EGolemCombatFixtureKind InKind)
{
	if (!HasActorBegunPlay())
	{
		FixtureKind = InKind;
		RefreshLabel();
	}
}

void AGolemCombatFixture::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	if (HasAuthority() && !FixtureId.IsValid())
	{
		FixtureId = FGolemEntityId::Create();
	}
	InitializeFixture();
}

void AGolemCombatFixture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTelegraph();
	const bool bWorldEnding = EndPlayReason == EEndPlayReason::EndPlayInEditor
		|| EndPlayReason == EEndPlayReason::Quit || EndPlayReason == EEndPlayReason::LevelTransition;
	Finisher->PrepareForEndPlay(bWorldEnding);
	Super::EndPlay(EndPlayReason);
}

void AGolemCombatFixture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLabel();
}

void AGolemCombatFixture::InitializeFixture()
{
	if (!FixtureId.IsValid())
	{
		return;
	}
	FGolemAttributeDefaults Defaults;
	Defaults.MaxHealth = 400.0f;
	Defaults.MaxStamina = 400.0f;
	Defaults.MaxRuneEnergy = 100.0f;
	InitializationResult = AbilitySystem->InitializeGolemActor(this, Attributes, Life, FixtureId, Defaults, false);
	if (InitializationResult.IsSuccess() && HasAuthority())
	{
		if (!AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy))
		{
			AbilitySystem->AddLooseGameplayTag(GolemGameplayTags::Role_Enemy, 1,
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
		UE_LOG(LogGolemCombatFixture, Error, TEXT("Fixture %s initialization failed: %s."),
			*GetName(), *InitializationResult.Message.ToString());
	}
}

FGolemResult AGolemCombatFixture::TryTelegraphAttack(AActor* Target)
{
	if (!HasAuthority() || FixtureKind != EGolemCombatFixtureKind::ScriptedAttacker)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("Telegraph"),
			NSLOCTEXT("KnightCombatChallenge", "ScriptAuthority", "Only the scripted attacker may prepare this exercise."));
	}
	if (bTelegraphing || !Life->IsAlive() || Combat->GetSnapshot().bAttacking || !InitializationResult.IsSuccess())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("Telegraph"),
			NSLOCTEXT("KnightCombatChallenge", "ScriptBusy", "Wait for the scripted attack to finish."));
	}
	const FGolemResult TargetValidation = Combat->ValidateCombatTarget(Target, 220.0f);
	if (!TargetValidation.IsSuccess())
	{
		return TargetValidation;
	}
	const FGolemResult Lock = Combat->SetValidatedTarget(Target);
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
	return FGolemResult::Success();
}

void AGolemCombatFixture::ReleaseScriptedAttack()
{
	bTelegraphing = false;
	RefreshLabel();
	ForceNetUpdate();
	if (HasAuthority() && AttackTarget.IsValid() && Life->IsAlive())
	{
		const FGolemResult Result = AbilitySystem->RequestAbilityInput(
			GolemGameplayTags::Input_Combat_Primary, true, AttackTarget.Get());
		AbilitySystem->RequestAbilityInput(GolemGameplayTags::Input_Combat_Primary, false);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogGolemCombatFixture, Display, TEXT("Scripted attack rejected safely: %s."), *Result.Message.ToString());
		}
	}
	AttackTarget.Reset();
}

void AGolemCombatFixture::ClearTelegraph()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(TelegraphTimer);
	}
	AttackTarget.Reset();
	bTelegraphing = false;
}

FGolemResult AGolemCombatFixture::ResetForCheckpoint()
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("FixtureReset"), FText::GetEmpty());
	}
	ClearTelegraph();
	if (Finisher->HasActiveFinisher())
	{
		Finisher->EndFinisher(Finisher->GetReservation(), false);
	}
	Combat->ClearTransientActions();
	const FGolemResult Reset = Life->ResetForCheckpoint();
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	const FGolemResult CombatReset = Combat->ResetForCheckpoint();
	GetCharacterMovement()->StopMovementImmediately();
	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	RefreshLabel();
	ForceNetUpdate();
	return CombatReset;
}

void AGolemCombatFixture::RefreshLabel()
{
	if (Label == nullptr)
	{
		return;
	}
	if (FixtureKind == EGolemCombatFixtureKind::ScriptedAttacker)
	{
		Label->SetText(bTelegraphing
			? NSLOCTEXT("KnightCombatChallenge", "Incoming", "INCOMING: block Q / parry C / dodge Shift")
			: NSLOCTEXT("KnightCombatChallenge", "Scripted", "F / D-pad Left: telegraph attack (0.8 seconds)"));
		Label->SetTextRenderColor(bTelegraphing ? FColor::Red : FColor::Yellow);
	}
	else
	{
		Label->SetText(FixtureKind == EGolemCombatFixtureKind::MeleeTarget
			? NSLOCTEXT("KnightCombatChallenge", "Melee", "LMB combo / RMB heavy / H finisher when staggered")
			: NSLOCTEXT("KnightCombatChallenge", "Rune", "MMB lock / G rune / H finisher when staggered"));
		Label->SetTextRenderColor(FixtureKind == EGolemCombatFixtureKind::MeleeTarget ? FColor::Cyan : FColor::Purple);
	}
}

void AGolemCombatFixture::OnRep_FixtureId()
{
	InitializeFixture();
}

void AGolemCombatFixture::OnRep_Telegraphing()
{
	RefreshLabel();
}
