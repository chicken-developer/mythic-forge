#include "Components/KnightCombatComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/KnightGameplayAbility.h"
#include "Abilities/KnightRuneSlowEffect.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/KnightAnimInstance.h"
#include "Assets/KnightAssetManager.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/KnightEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightCombat, Log, All);

namespace
{
	FKnightResult KnightCombatFailure(EKnightError Error, FName Context)
	{
		FText Message = NSLOCTEXT("KnightCombat", "ActionUnavailable", "This combat action is unavailable right now.");
		if (Error == EKnightError::InsufficientResources)
		{
			Message = NSLOCTEXT("KnightCombat", "InsufficientResources", "Not enough stamina or rune energy.");
		}
		else if (Error == EKnightError::OutOfRange)
		{
			Message = NSLOCTEXT("KnightCombat", "TargetOutOfRange", "Move closer and keep the target in sight.");
		}
		else if (Error == EKnightError::NotFound)
		{
			Message = NSLOCTEXT("KnightCombat", "NoTarget", "No eligible combat target is in sight.");
		}
		return FKnightResult::Failure(Error, Context, Message);
	}
	UKnightAbilitySystemComponent* KnightActorASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UKnightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	FKnightResult KnightSpendResource(UKnightAbilitySystemComponent* ASC,
		const FGameplayAttribute& Attribute, float Amount)
	{
		if (ASC == nullptr || !IsValid(ASC->GetOwner()) || !ASC->GetOwner()->HasAuthority() ||
			!FMath::IsFinite(Amount) || Amount < 0.0f)
		{
			return KnightCombatFailure(EKnightError::InvalidData, TEXT("CombatResourceCost"));
		}
		if (ASC->IsMutationFenced())
		{
			return KnightCombatFailure(EKnightError::Conflict, TEXT("CombatProjectionFence"));
		}
		if (ASC->GetNumericAttribute(Attribute) + KINDA_SMALL_NUMBER < Amount)
		{
			return KnightCombatFailure(EKnightError::InsufficientResources, TEXT("CombatResourceCost"));
		}
		if (Amount == 0.0f)
		{
			return FKnightResult::Success();
		}
		UGameplayEffect* Cost = NewObject<UGameplayEffect>(GetTransientPackage());
		Cost->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Cost->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(-Amount);
		const FGameplayEffectSpec Spec(Cost, ASC->MakeEffectContext(), 1.0f);
		return ASC->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied() ? FKnightResult::Success()
			: KnightCombatFailure(EKnightError::InvalidState, TEXT("CombatResourceGE"));
	}
}

int32 FKnightCombatSnapshot::GetOutcomeCount(EKnightCombatOutcome Outcome) const
{
	const int32 Index = static_cast<int32>(Outcome);
	return OutcomeCounts.IsValidIndex(Index) ? OutcomeCounts[Index] : 0;
}

UKnightCombatComponent::UKnightCombatComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UKnightCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightCombatComponent, ProfileId);
	DOREPLIFETIME(UKnightCombatComponent, Snapshot);
}

FKnightResult UKnightCombatComponent::Configure(const FPrimaryAssetId& InProfileId)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightCombatFailure(EKnightError::NotAuthority, TEXT("ConfigureCombat"));
	}
	if (!InProfileId.IsValid() || InProfileId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightCombatProfile")))
	{
		return KnightCombatFailure(EKnightError::InvalidId, TEXT("CombatProfileId"));
	}
	if (ProfileId.IsValid())
	{
		return ProfileId == InProfileId && Profile != nullptr ? FKnightResult::Success()
			: KnightCombatFailure(EKnightError::Conflict, TEXT("CombatProfileImmutable"));
	}
	ProfileId = InProfileId;
	const FKnightResult Result = ApplyProfile();
	if (!Result.IsSuccess())
	{
		ProfileId = FPrimaryAssetId();
		Profile = nullptr;
		return Result;
	}
	Snapshot.Poise = Profile->GetTuning().MaxPoise;
	Snapshot.OutcomeCounts.Init(0, static_cast<int32>(EKnightCombatOutcome::Traversal) + 1);
	RefreshSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightCombatComponent::ApplyProfile()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UKnightAbilitySystemComponent* ASC = KnightActorASC(Character);
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (Character == nullptr || ASC == nullptr || ASC->GetLifeComponent() == nullptr || Manager == nullptr)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("CombatActorComposition"));
	}
	UKnightDefinition* Definition = nullptr;
	FKnightResult Result = Manager->ResolveDefinition(ProfileId, UKnightCombatProfileDefinition::StaticClass(), Definition);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightCombatProfileDefinition* Resolved = CastChecked<UKnightCombatProfileDefinition>(Definition);
	Result = Resolved->ValidateCombatProfile();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightDefinition* SetDefinition = nullptr;
	Result = Manager->ResolveDefinition(Resolved->GetCombatAbilitySetId(),
		UKnightAbilitySetDefinition::StaticClass(), SetDefinition);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightAbilitySetDefinition* Set = CastChecked<UKnightAbilitySetDefinition>(SetDefinition);
	Result = Set->ValidateAbilitySet();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = UKnightAnimInstance::ConfigureCharacter(Character, Resolved->GetSkeletalMesh(),
		Resolved->GetIdleAnimation(), Resolved->GetWalkAnimation());
	if (!Result.IsSuccess())
	{
		return Result;
	}
	AbilitySystem = ASC;
	if (Character->HasAuthority())
	{
		Result = ASC->GrantAbilitySet(Set, TEXT("CombatProfile"), GrantHandle);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	Profile = Resolved;
	CombatAbilitySet = Set;
	ASC->RefreshReplicatedMontagePresentation();
	if (Hammer == nullptr)
	{
		Hammer = NewObject<UStaticMeshComponent>(Character, TEXT("KnightCombatHammer"));
		Hammer->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Hammer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hammer->SetCanEverAffectNavigation(false);
		Character->AddInstanceComponent(Hammer);
		Hammer->SetupAttachment(Character->GetMesh(), TEXT("hand_r"));
		Hammer->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
		Hammer->SetRelativeScale3D(FVector(0.12f, 0.12f, 1.0f));
		Hammer->RegisterComponent();
		UStaticMeshComponent* Head = NewObject<UStaticMeshComponent>(Character, TEXT("KnightCombatHammerHead"));
		Head->SetStaticMesh(Hammer->GetStaticMesh());
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Head->SetCanEverAffectNavigation(false);
		Character->AddInstanceComponent(Head);
		Head->SetupAttachment(Character->GetMesh(), TEXT("hand_r"));
		Head->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
		Head->SetRelativeScale3D(FVector(0.55f, 0.3f, 0.25f));
		Head->RegisterComponent();
	}
	if (!RuneTagHandle.IsValid())
	{
		RuneTagHandle = ASC->RegisterGameplayTagEvent(KnightGameplayTags::State_Combat_RuneSlowed)
			.AddUObject(this, &ThisClass::HandleRuneTagChanged);
		DownedTagHandle = ASC->RegisterGameplayTagEvent(KnightGameplayTags::State_Life_Downed)
			.AddUObject(this, &ThisClass::HandleLifeTagChanged);
		DeadTagHandle = ASC->RegisterGameplayTagEvent(KnightGameplayTags::State_Life_Dead)
			.AddUObject(this, &ThisClass::HandleLifeTagChanged);
	}
	HandleRuneTagChanged(KnightGameplayTags::State_Combat_RuneSlowed,
		ASC->GetTagCount(KnightGameplayTags::State_Combat_RuneSlowed));
	SetComponentTickEnabled(Character->HasAuthority());
	OnProfileChanged.Broadcast(Profile);
	return FKnightResult::Success();
}

FKnightResult UKnightCombatComponent::TryInitializePresentation()
{
	if (!ProfileId.IsValid() || bEndingPlay)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("CombatPresentationIdentity"));
	}
	return Profile != nullptr ? FKnightResult::Success() : ApplyProfile();
}

FKnightResult UKnightCombatComponent::RefreshBaseWalkSpeed(float BaseSpeed)
{
	if (!FMath::IsFinite(BaseSpeed) || BaseSpeed <= 0.0f || BaseSpeed > 5000.0f)
	{
		return KnightCombatFailure(EKnightError::InvalidData, TEXT("CombatMovementBaseline"));
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (bEndingPlay || !IsValid(Character) || Character->IsActorBeingDestroyed() || GetWorld() == nullptr ||
		GetWorld()->bIsTearingDown || Character->GetCharacterMovement() == nullptr)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("CombatMovementPresentation"));
	}
	// Hero identity and ASC/profile state replicate independently; the tag is the current source of truth.
	const UKnightAbilitySystemComponent* ASC = AbilitySystem.IsValid() ? AbilitySystem.Get() : KnightActorASC(Character);
	const bool bSlowed = ASC != nullptr && ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed);
	SavedWalkSpeed = bSlowed ? BaseSpeed : 0.0f;
	Character->GetCharacterMovement()->MaxWalkSpeed = bSlowed ? BaseSpeed * 0.5f : BaseSpeed;
	return FKnightResult::Success();
}

void UKnightCombatComponent::OnRep_ProfileId()
{
	// Replicated identity may arrive after the profile. The character repeats initialization after ASC readiness.
	const UKnightAbilitySystemComponent* ASC = KnightActorASC(GetOwner());
	if (ASC == nullptr || ASC->GetLifeComponent() == nullptr)
	{
		return;
	}
	const FKnightResult Result = ApplyProfile();
	if (!Result.IsSuccess())
	{
		UE_LOG(LogKnightCombat, Error, TEXT("Combat profile %s failed: %s"), *ProfileId.ToString(),
			*Result.Context.ToString());
	}
}
void UKnightCombatComponent::OnRep_Snapshot()
{
	OnCombatSnapshotChanged.Broadcast(Snapshot);
}
UKnightCombatProfileDefinition* UKnightCombatComponent::GetProfile() const
{
	return Profile;
}
FPrimaryAssetId UKnightCombatComponent::GetProfileId() const
{
	return ProfileId;
}
const FKnightCombatSnapshot& UKnightCombatComponent::GetSnapshot() const
{
	return Snapshot;
}
bool UKnightCombatComponent::CanTransferControl() const
{
	return !Snapshot.bAttacking && !Snapshot.bBlocking && !Snapshot.bParrying && !Snapshot.bDodging &&
		!Snapshot.bStaggered && (!AbilitySystem.IsValid() ||
			(!AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing) &&
			!AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing)));
}
bool UKnightCombatComponent::HasOpenComboWindow() const
{
	return bComboWindow && Snapshot.bAttacking;
}
bool UKnightCombatComponent::IsAuthorityAlive() const
{
	return !bEndingPlay && Profile != nullptr && IsValid(GetOwner()) && GetOwner()->HasAuthority() &&
		GetWorld() != nullptr && !GetWorld()->bIsTearingDown && AbilitySystem.IsValid() &&
		!AbilitySystem->IsMutationFenced() && AbilitySystem->GetLifeComponent() != nullptr
		&& AbilitySystem->GetLifeComponent()->IsAlive();
}

void UKnightCombatComponent::RefreshSnapshot()
{
	if (!bEndingPlay && IsValid(GetOwner()) && GetOwner()->HasAuthority())
	{
		++Snapshot.Revision;
		OnCombatSnapshotChanged.Broadcast(Snapshot);
		GetOwner()->ForceNetUpdate();
	}
}

FKnightResult UKnightCombatComponent::SpendStamina(float Amount, FGameplayTag ActionTag)
{
	const float Scale = AbilitySystem.IsValid() ? AbilitySystem->GetActionModifier(ActionTag).CostScale : 1.0f;
	return KnightSpendResource(AbilitySystem.Get(), UKnightAttributeSet::GetStaminaAttribute(), Amount * Scale);
}

float UKnightCombatComponent::GetProjectedDamage(FGameplayTag ActionTag, float AuthoredDamage) const
{
	if (!AbilitySystem.IsValid() || AbilitySystem->GetKnightAttributes() == nullptr)
	{
		return AuthoredDamage;
	}
	const float Delta = AbilitySystem->GetKnightAttributes()->GetAttackPower() - AbilitySystem->GetInitialAttackPower();
	return FMath::Clamp(FMath::Max(0.0f, AuthoredDamage + Delta)
		* AbilitySystem->GetActionModifier(ActionTag).DamageScale, 0.0f, 1000000.0f);
}

FKnightResult UKnightCombatComponent::BeginAttack(FGameplayTag ActionTag, UKnightGameplayAbility* Ability,
	UAnimMontage*& OutMontage, FName& OutSection)
{
	OutMontage = nullptr;
	OutSection = NAME_None;
	if (!IsAuthorityAlive() || Ability == nullptr || !CanTransferControl())
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("BeginAttack"));
	}
	const TArray<FKnightAttackDefinition>& Attacks = Profile->GetAttacks();
	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (Attacks[Index].ActionTag != ActionTag)
		{
			continue;
		}
		const FKnightResult Cost = SpendStamina(Attacks[Index].StaminaCost, ActionTag);
		if (!Cost.IsSuccess())
		{
			return Cost;
		}
		++TraceGeneration;
		AttackIndex = Index;
		AttackAction = ActionTag;
		AttackAbility = Ability;
		Snapshot.bAttacking = true;
		Snapshot.ComboIndex = 1;
		bComboWindow = false;
		bBufferedAttack = false;
		ActiveWindow = NAME_None;
		WindowHitActors.Reset();
		OutMontage = Attacks[Index].Montage.LoadSynchronous();
		OutSection = Attacks[Index].SectionName;
		if (IsValid(Snapshot.LockedTarget) && ValidateCombatTarget(Snapshot.LockedTarget, Profile->GetTuning().LockRange).IsSuccess())
		{
			const FVector Direction = Snapshot.LockedTarget->GetActorLocation() - GetOwner()->GetActorLocation();
			GetOwner()->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
		}
		RefreshSnapshot();
		return FKnightResult::Success();
	}
	return KnightCombatFailure(EKnightError::NotFound, TEXT("AttackDefinition"));
}

const FKnightAttackDefinition* UKnightCombatComponent::GetCurrentAttack() const
{
	return Profile != nullptr && Profile->GetAttacks().IsValidIndex(AttackIndex)
		? &Profile->GetAttacks()[AttackIndex] : nullptr;
}
void UKnightCombatComponent::BufferAttackInput()
{
	if (IsAuthorityAlive() && Snapshot.bAttacking && bComboWindow &&
		AttackAction == KnightGameplayTags::Ability_Combat_Primary)
	{
		bBufferedAttack = true;
	}
}
void UKnightCombatComponent::EndAttack(UKnightGameplayAbility* Ability)
{
	if (AttackAbility.Get() != Ability)
	{
		return;
	}
	++TraceGeneration;
	AttackAbility.Reset();
	AttackIndex = INDEX_NONE;
	AttackAction = FGameplayTag();
	ActiveWindow = NAME_None;
	WindowHitActors.Reset();
	bComboWindow = false;
	bBufferedAttack = false;
	Snapshot.bAttacking = false;
	RefreshSnapshot();
}

void UKnightCombatComponent::BeginTraceWindow(FName WindowId, UAnimSequenceBase* Animation)
{
	const FKnightAttackDefinition* Attack = GetCurrentAttack();
	if (!IsAuthorityAlive() || !Snapshot.bAttacking || Attack == nullptr || WindowId.IsNone() ||
		Animation != Attack->Montage.Get() || !AbilitySystem.IsValid() ||
		AbilitySystem->GetCurrentMontageSectionName() != Attack->SectionName)
	{
		return;
	}
	if (ActiveWindow == WindowId)
	{
		return;
	}
	++TraceGeneration;
	ActiveWindow = WindowId;
	WindowHitActors.Reset();
	PreviousHammerHead = Hammer != nullptr ? Hammer->GetComponentLocation() + Hammer->GetUpVector() * 50.0f
		: GetOwner()->GetActorLocation();
	PerformWeaponSweep();
}
void UKnightCombatComponent::TickTraceWindow(FName WindowId, UAnimSequenceBase* Animation)
{
	const FKnightAttackDefinition* Attack = GetCurrentAttack();
	if (IsAuthorityAlive() && Snapshot.bAttacking && ActiveWindow == WindowId && Attack != nullptr &&
		Animation == Attack->Montage.Get())
	{
		PerformWeaponSweep();
	}
}
void UKnightCombatComponent::EndTraceWindow(FName WindowId, UAnimSequenceBase* Animation)
{
	const FKnightAttackDefinition* Attack = GetCurrentAttack();
	if (ActiveWindow == WindowId && Attack != nullptr && Animation == Attack->Montage.Get())
	{
		++TraceGeneration;
		ActiveWindow = NAME_None;
	}
}

void UKnightCombatComponent::SetComboWindow(bool bOpen, UAnimSequenceBase* Animation)
{
	const FKnightAttackDefinition* Attack = GetCurrentAttack();
	if (!IsAuthorityAlive() || Attack == nullptr || Animation != Attack->Montage.Get() ||
		AttackAction != KnightGameplayTags::Ability_Combat_Primary || !Snapshot.bAttacking)
	{
		return;
	}
	bComboWindow = bOpen;
	if (bOpen || !bBufferedAttack)
	{
		return;
	}
	bBufferedAttack = false;
	const TArray<FKnightAttackDefinition>& Attacks = Profile->GetAttacks();
	for (int32 Next = AttackIndex + 1; Next < Attacks.Num(); ++Next)
	{
		if (Attacks[Next].ActionTag != AttackAction)
		{
			continue;
		}
		if (Attacks[Next].Montage.Get() != Animation || !SpendStamina(Attacks[Next].StaminaCost, AttackAction).IsSuccess())
		{
			return;
		}
		++TraceGeneration;
		AttackIndex = Next;
		++Snapshot.ComboIndex;
		ActiveWindow = NAME_None;
		WindowHitActors.Reset();
		AbilitySystem->CurrentMontageJumpToSection(Attacks[Next].SectionName);
		PublishOutcome(EKnightCombatOutcome::Combo, AttackAction, Snapshot.LockedTarget, Snapshot.ComboIndex);
		return;
	}
}

void UKnightCombatComponent::PerformWeaponSweep()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const FKnightAttackDefinition* CurrentAttack = GetCurrentAttack();
	const uint64 Generation = TraceGeneration;
	const FName WindowId = ActiveWindow;
	const TWeakObjectPtr<UKnightGameplayAbility> Ability = AttackAbility;
	const int32 Index = AttackIndex;
	if (!IsValid(Character) || !IsValid(Character->GetMesh()) || CurrentAttack == nullptr ||
		!IsValid(Hammer) || !IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
	{
		return;
	}
	// Damage execution and confirmed-event listeners may synchronously cancel or replace this action.
	const FKnightAttackDefinition Attack = *CurrentAttack;
	const FGameplayTag ActionTag = AttackAction;
	const FVector Hand = Character->GetMesh()->GetSocketLocation(TEXT("hand_r"));
	const FVector Head = Hammer->GetComponentLocation() + Hammer->GetUpVector() * 50.0f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightWeaponSweep), false, GetOwner());
	Query.bReturnPhysicalMaterial = false;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	TArray<FHitResult> MovingHits;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Attack.TraceRadius);
	GetWorld()->SweepMultiByObjectType(Hits, Hand, Head, FQuat::Identity, Objects, Shape, Query);
	GetWorld()->SweepMultiByObjectType(MovingHits, PreviousHammerHead, Head, FQuat::Identity, Objects, Shape, Query);
	Hits.Append(MovingHits);
	PreviousHammerHead = Head;
	for (const FHitResult& Hit : Hits)
	{
		if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
		{
			return;
		}
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || WindowHitActors.Contains(Target) ||
			!ValidateCombatTarget(Target, 400.0f).IsSuccess())
		{
			continue;
		}
		UKnightAbilitySystemComponent* TargetASC = KnightActorASC(Target);
		if (TargetASC == nullptr || TargetASC->GetKnightAttributes() == nullptr)
		{
			continue;
		}
		WindowHitActors.Add(Target);
		const float Before = TargetASC->GetKnightAttributes()->GetHealth();
		const FKnightResult Result = UKnightAbilitySystemComponent::ApplyCombatDamage(GetOwner(), Target,
			GetProjectedDamage(ActionTag, Attack.Damage), Attack.PoiseDamage, true, &Hit);
		const float Applied = Before - TargetASC->GetKnightAttributes()->GetHealth();
		if (Result.IsSuccess() && Applied > 0.0f)
		{
			PublishOutcome(ActionTag == KnightGameplayTags::Ability_Combat_Heavy ? EKnightCombatOutcome::HeavyHit
				: EKnightCombatOutcome::LightHit, ActionTag, Target, Applied);
		}
		if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
		{
			return;
		}
	}
}

bool UKnightCombatComponent::IsTraceCapabilityCurrent(uint64 Generation, FName WindowId,
	const TWeakObjectPtr<UKnightGameplayAbility>& Ability, int32 Index) const
{
	return IsAuthorityAlive() && Snapshot.bAttacking && Generation == TraceGeneration &&
		!WindowId.IsNone() && ActiveWindow == WindowId && AttackIndex == Index &&
		Ability.IsValid() && AttackAbility == Ability && Ability->IsActive();
}

FKnightResult UKnightCombatComponent::BeginDefense(FGameplayTag ActionTag)
{
	if (!IsAuthorityAlive() || Snapshot.bAttacking || Snapshot.bDodging || Snapshot.bStaggered ||
		Snapshot.bBlocking || Snapshot.bParrying)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("BeginDefense"));
	}
	if (ActionTag == KnightGameplayTags::Ability_Combat_Parry)
	{
		const FKnightResult Cost = SpendStamina(Profile->GetTuning().ParryStaminaCost, ActionTag);
		if (!Cost.IsSuccess())
		{
			return Cost;
		}
		Snapshot.bParrying = true;
		ParryStartedAt = GetWorld()->GetTimeSeconds();
	}
	else if (ActionTag == KnightGameplayTags::Ability_Combat_Block)
	{
		Snapshot.bBlocking = true;
	}
	else
	{
		return KnightCombatFailure(EKnightError::InvalidData, TEXT("DefenseAction"));
	}
	RefreshSnapshot();
	return FKnightResult::Success();
}
void UKnightCombatComponent::EndDefense(FGameplayTag ActionTag)
{
	if (ActionTag == KnightGameplayTags::Ability_Combat_Block)
	{
		Snapshot.bBlocking = false;
	}
	else if (ActionTag == KnightGameplayTags::Ability_Combat_Parry)
	{
		Snapshot.bParrying = false;
	}
	RefreshSnapshot();
}

FKnightResult UKnightCombatComponent::BeginDodge()
{
	if (!IsAuthorityAlive() || Snapshot.bDodging || Snapshot.bStaggered)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("BeginDodge"));
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || Character->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("DodgeMovement"));
	}
	const FKnightResult Cost = SpendStamina(Profile->GetTuning().DodgeStaminaCost, KnightGameplayTags::Ability_Combat_Dodge);
	if (!Cost.IsSuccess())
	{
		return Cost;
	}
	Snapshot.bDodging = true;
	DodgeStartedAt = GetWorld()->GetTimeSeconds();
	FVector Direction = Character->GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector();
	}
	Character->GetCharacterMovement()->Velocity = Direction * Profile->GetTuning().DodgeSpeed;
	RefreshSnapshot();
	return FKnightResult::Success();
}
void UKnightCombatComponent::EndDodge()
{
	if (Snapshot.bDodging)
	{
		Snapshot.bDodging = false;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
		RefreshSnapshot();
	}
}

FKnightResult UKnightCombatComponent::ValidateCombatTarget(AActor* Target, float Range) const
{
	if (!IsValid(Target) || !IsValid(GetOwner()) || Target == GetOwner() || Target->GetWorld() != GetWorld() ||
		!FMath::IsFinite(Range) || Range <= 0.0f)
	{
		return KnightCombatFailure(EKnightError::NotFound, TEXT("CombatTarget"));
	}
	const UKnightAbilitySystemComponent* SourceASC = AbilitySystem.Get();
	const UKnightAbilitySystemComponent* TargetASC = KnightActorASC(Target);
	if (SourceASC == nullptr || TargetASC == nullptr || TargetASC->GetLifeComponent() == nullptr ||
		!TargetASC->GetLifeComponent()->IsAlive())
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("CombatTargetLife"));
	}
	if (SourceASC->IsMutationFenced() || TargetASC->IsMutationFenced())
	{
		return KnightCombatFailure(EKnightError::Conflict, TEXT("CombatTargetProjectionFence"));
	}
	const bool bHostile = (SourceASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
		TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy)) ||
		(SourceASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) &&
		TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight));
	if (!bHostile)
	{
		return KnightCombatFailure(EKnightError::NotOwned, TEXT("CombatFriendlyTarget"));
	}
	if (FVector::DistSquared(Target->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(Range))
	{
		return KnightCombatFailure(EKnightError::OutOfRange, TEXT("CombatTargetRange"));
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightCombatLOS), false, GetOwner());
	Query.AddIgnoredActor(Target);
	FHitResult Obstruction;
	if (GetWorld() == nullptr || GetWorld()->LineTraceSingleByChannel(Obstruction,
		GetOwner()->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
	{
		return KnightCombatFailure(EKnightError::OutOfRange, TEXT("CombatTargetVisibility"));
	}
	return FKnightResult::Success();
}

AActor* UKnightCombatComponent::FindCombatTarget(float Range, bool bRequireStagger) const
{
	if (GetWorld() == nullptr || GetOwner() == nullptr)
	{
		return nullptr;
	}
	AActor* Best = nullptr;
	float BestDistance = FMath::Square(Range);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!ValidateCombatTarget(Candidate, Range).IsSuccess())
		{
			continue;
		}
		const UKnightCombatComponent* TargetCombat = Candidate->FindComponentByClass<UKnightCombatComponent>();
		if (bRequireStagger && (TargetCombat == nullptr || !TargetCombat->GetSnapshot().bStaggered))
		{
			continue;
		}
		const float Distance = FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	return Best;
}

FKnightResult UKnightCombatComponent::SetValidatedTarget(AActor* Target)
{
	if (!IsAuthorityAlive())
	{
		return KnightCombatFailure(EKnightError::NotAuthority, TEXT("SetCombatTarget"));
	}
	const FKnightResult Result = ValidateCombatTarget(Target, Profile->GetTuning().LockRange);
	if (Result.IsSuccess())
	{
		Snapshot.LockedTarget = Target;
		UpdateLockCamera();
		RefreshSnapshot();
	}
	return Result;
}
FKnightResult UKnightCombatComponent::TryToggleTargetLock(AActor* Target)
{
	if (!IsAuthorityAlive())
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("ToggleCombatLock"));
	}
	if (IsValid(Snapshot.LockedTarget) && (Target == nullptr || Target == Snapshot.LockedTarget))
	{
		Snapshot.LockedTarget = nullptr;
		UpdateLockCamera();
		RefreshSnapshot();
		return FKnightResult::Success();
	}
	Target = Target != nullptr ? Target : FindCombatTarget(Profile->GetTuning().LockRange);
	const FKnightResult Result = SetValidatedTarget(Target);
	if (Result.IsSuccess())
	{
		PublishOutcome(EKnightCombatOutcome::TargetLocked, KnightGameplayTags::Ability_Combat_LockOn, Target);
	}
	return Result;
}
AActor* UKnightCombatComponent::GetLockedTarget() const
{
	return IsValid(Snapshot.LockedTarget) ? Snapshot.LockedTarget.Get() : nullptr;
}

void UKnightCombatComponent::UpdateLockCamera()
{
	UKnightCameraModeComponent* Camera = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UKnightCameraModeComponent>() : nullptr;
	if (Camera == nullptr)
	{
		LockCameraHandle.Invalidate();
		return;
	}
	if (LockCameraHandle.IsValid())
	{
		Camera->PopMode(LockCameraHandle);
		LockCameraHandle.Invalidate();
	}
	if (IsValid(Snapshot.LockedTarget) && !bEndingPlay)
	{
		FKnightCameraModeSettings Settings;
		Settings.Priority = 10;
		Settings.FocusTarget = Snapshot.LockedTarget;
		Settings.FieldOfView = 75.0f;
		Settings.ArmLength = 360.0f;
		Settings.SocketOffset = FVector(0.0f, 45.0f, 65.0f);
		LockCameraHandle = Camera->PushMode(Settings);
	}
}

void UKnightCombatComponent::ResolveIncomingDamage(AActor* Source, float InDamage, float PoiseDamage,
	bool bCanDefend, float& OutHealthDamage)
{
	OutHealthDamage = InDamage;
	if (!IsAuthorityAlive() || !FMath::IsFinite(InDamage) || !FMath::IsFinite(PoiseDamage))
	{
		return;
	}
	const FKnightCombatTuning& Tuning = Profile->GetTuning();
	if (bCanDefend && Snapshot.bDodging)
	{
		const float Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartedAt;
		if (Elapsed >= Tuning.DodgeInvulnerabilityStart && Elapsed <= Tuning.DodgeInvulnerabilityEnd)
		{
			OutHealthDamage = 0.0f;
			PublishOutcome(EKnightCombatOutcome::Dodged, KnightGameplayTags::Ability_Combat_Dodge, Source);
			return;
		}
	}
	const FVector IncomingDirection = IsValid(Source)
		? (Source->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	const bool bFront = !IncomingDirection.IsNearlyZero() &&
		FVector::DotProduct(GetOwner()->GetActorForwardVector(), IncomingDirection) >= Tuning.BlockFacingCosine;
	if (bCanDefend && bFront && Snapshot.bParrying &&
		GetWorld()->GetTimeSeconds() - ParryStartedAt <= Tuning.ParryWindow)
	{
		OutHealthDamage = 0.0f;
		AbilitySystem->ExecuteGameplayCue(KnightGameplayTags::GameplayCue_Parry);
		PublishOutcome(EKnightCombatOutcome::Parried, KnightGameplayTags::Ability_Combat_Parry, Source);
		if (IsValid(Source))
		{
			if (UKnightCombatComponent* SourceCombat = Source->FindComponentByClass<UKnightCombatComponent>())
			{
				SourceCombat->EnterStagger(GetOwner());
			}
		}
		return;
	}
	if (bCanDefend && bFront && Snapshot.bBlocking)
	{
		if (SpendStamina(Tuning.BlockStaminaCost, KnightGameplayTags::Ability_Combat_Block).IsSuccess())
		{
			OutHealthDamage *= Tuning.BlockDamageFraction;
			PublishOutcome(EKnightCombatOutcome::Blocked, KnightGameplayTags::Ability_Combat_Block, Source, OutHealthDamage);
			return;
		}
		FGameplayTagContainer BlockTags(KnightGameplayTags::Ability_Combat_Block);
		AbilitySystem->CancelAbilities(&BlockTags);
	}
	if (PoiseDamage > 0.0f && !Snapshot.bStaggered)
	{
		Snapshot.Poise = FMath::Max(0.0f, Snapshot.Poise - PoiseDamage);
		if (Snapshot.Poise <= 0.0f)
		{
			EnterStagger(Source);
		}
		else
		{
			// The same authored graybox reaction is used as a flinch only while no action owns the montage.
			if (InDamage > 0.0f && !Snapshot.bAttacking && !Snapshot.bBlocking && !Snapshot.bParrying &&
				!Snapshot.bDodging && !AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing) &&
				!AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing))
			{
				AbilitySystem->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), Profile->GetStaggerMontage(), 2.0f);
			}
			RefreshSnapshot();
		}
	}
}

void UKnightCombatComponent::EnterStagger(AActor* Source)
{
	if (!IsAuthorityAlive() || Snapshot.bStaggered)
	{
		return;
	}
	ClearTransientActions();
	Snapshot.bStaggered = true;
	Snapshot.Poise = 0.0f;
	AbilitySystem->SetLooseGameplayTagCount(KnightGameplayTags::State_Combat_Staggered, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->ExecuteGameplayCue(KnightGameplayTags::GameplayCue_Stagger);
	AbilitySystem->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), Profile->GetStaggerMontage(), 1.0f);
	GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &ThisClass::EndStagger,
		Profile->GetTuning().StaggerDuration, false);
	PublishOutcome(EKnightCombatOutcome::Staggered, KnightGameplayTags::State_Combat_Staggered, Source);
}
void UKnightCombatComponent::EndStagger()
{
	if (!bEndingPlay && Profile != nullptr && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Snapshot.bStaggered = false;
		Snapshot.Poise = Profile->GetTuning().MaxPoise;
		if (AbilitySystem.IsValid())
		{
			AbilitySystem->SetLooseGameplayTagCount(KnightGameplayTags::State_Combat_Staggered, 0,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		RefreshSnapshot();
	}
}

FKnightResult UKnightCombatComponent::CastRune(AActor* Target)
{
	if (!IsAuthorityAlive() || Snapshot.bStaggered)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("CastRune"));
	}
	Target = IsValid(Target) ? Target : GetLockedTarget();
	if (Target == nullptr)
	{
		Target = FindCombatTarget(Profile->GetTuning().RuneRange);
	}
	FKnightResult Result = ValidateCombatTarget(Target, Profile->GetTuning().RuneRange);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FGameplayEffectSpecHandle Status = AbilitySystem->MakeOutgoingSpec(UKnightRuneSlowEffect::StaticClass(),
		1.0f, AbilitySystem->MakeEffectContext());
	if (!Status.IsValid())
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("RuneStatusSpec"));
	}
	const FKnightActionModifier RuneModifier = AbilitySystem->GetActionModifier(KnightGameplayTags::Ability_Combat_Rune);
	Status.Data->SetDuration(Profile->GetTuning().RuneSlowDuration * RuneModifier.DurationScale, true);
	Result = KnightSpendResource(AbilitySystem.Get(), UKnightAttributeSet::GetRuneEnergyAttribute(),
		Profile->GetTuning().RuneEnergyCost * RuneModifier.CostScale);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightAbilitySystemComponent* TargetASC = KnightActorASC(Target);
	const float Before = TargetASC->GetKnightAttributes()->GetHealth();
	Result = UKnightAbilitySystemComponent::ApplyCombatDamage(GetOwner(), Target,
		GetProjectedDamage(KnightGameplayTags::Ability_Combat_Rune, Profile->GetTuning().RuneDamage),
		Profile->GetTuning().RunePoiseDamage, false);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (TargetASC->GetLifeComponent()->IsAlive())
	{
		const FActiveGameplayEffectHandle StatusHandle =
			AbilitySystem->ApplyGameplayEffectSpecToTarget(*Status.Data.Get(), TargetASC);
		if (!StatusHandle.WasSuccessfullyApplied() || !StatusHandle.IsValid() ||
			TargetASC->GetActiveGameplayEffect(StatusHandle) == nullptr)
		{
			// Damage and its energy cost already occurred; do not invent a rollback or confirm the missing status.
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RuneStatusRejected"),
				NSLOCTEXT("KnightCombat", "RuneStatusRejected", "Rune damage landed, but its slow effect was rejected."));
		}
	}
	TargetASC->ExecuteGameplayCue(KnightGameplayTags::GameplayCue_Rune);
	PublishOutcome(EKnightCombatOutcome::RuneHit, KnightGameplayTags::Ability_Combat_Rune, Target,
		Before - TargetASC->GetKnightAttributes()->GetHealth());
	return FKnightResult::Success();
}

void UKnightCombatComponent::PublishOutcome(EKnightCombatOutcome Outcome, FGameplayTag ActionTag,
	AActor* Target, float Amount)
{
	if (bEndingPlay || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !AbilitySystem.IsValid())
	{
		return;
	}
	const int32 Index = static_cast<int32>(Outcome);
	if (!Snapshot.OutcomeCounts.IsValidIndex(Index))
	{
		return;
	}
	++Snapshot.OutcomeCounts[Index];
	RefreshSnapshot();
	FKnightCombatOutcome Event;
	Event.ConfirmationId = FGuid::NewGuid();
	Event.Revision = Snapshot.Revision;
	Event.SourceId = AbilitySystem->GetEntityId();
	Event.TargetId = KnightActorASC(Target) != nullptr ? KnightActorASC(Target)->GetEntityId() : FKnightEntityId();
	Event.Outcome = Outcome;
	Event.ActionTag = ActionTag;
	Event.Amount = Amount;
	if (UKnightEventSubsystem* Events = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		const FKnightResult Published = Events->PublishConfirmed(Event);
		if (!Published.IsSuccess())
		{
			UE_LOG(LogKnightCombat, Error, TEXT("Combat confirmation rejected: %s"), *Published.Context.ToString());
			return;
		}
	}
	OnCombatOutcome.Broadcast(Event);
}

void UKnightCombatComponent::HandleRuneTagChanged(const FGameplayTag Tag, int32 Count)
{
	if (bEndingPlay)
	{
		return;
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Count > 0 && SavedWalkSpeed <= 0.0f)
		{
			SavedWalkSpeed = Movement->MaxWalkSpeed;
			Movement->MaxWalkSpeed = SavedWalkSpeed * 0.5f;
		}
		else if (Count == 0 && SavedWalkSpeed > 0.0f)
		{
			Movement->MaxWalkSpeed = SavedWalkSpeed;
			SavedWalkSpeed = 0.0f;
		}
	}
}
void UKnightCombatComponent::HandleLifeTagChanged(const FGameplayTag Tag, int32 Count)
{
	if (Count > 0 && !bEndingPlay && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		ClearTransientActions();
		if (GetWorld() != nullptr)
		{
			GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
		}
		EndStagger();
	}
}

void UKnightCombatComponent::ClearTransientActions()
{
	++TraceGeneration;
	if (AbilitySystem.IsValid())
	{
		FGameplayTagContainer CombatTags;
		for (const FGameplayTag Tag : {FGameplayTag(KnightGameplayTags::Ability_Combat_Primary),
			FGameplayTag(KnightGameplayTags::Ability_Combat_Heavy), FGameplayTag(KnightGameplayTags::Ability_Combat_Block),
			FGameplayTag(KnightGameplayTags::Ability_Combat_Parry), FGameplayTag(KnightGameplayTags::Ability_Combat_Dodge),
			FGameplayTag(KnightGameplayTags::Ability_Combat_Rune), FGameplayTag(KnightGameplayTags::Ability_Combat_Finisher),
			FGameplayTag(KnightGameplayTags::Ability_World_Traverse)})
		{
			CombatTags.AddTag(Tag);
		}
		AbilitySystem->CancelAbilities(&CombatTags);
	}
	Snapshot.bAttacking = false;
	Snapshot.bBlocking = false;
	Snapshot.bParrying = false;
	Snapshot.bDodging = false;
	Snapshot.LockedTarget = nullptr;
	UpdateLockCamera();
	AttackAbility.Reset();
	AttackIndex = INDEX_NONE;
	ActiveWindow = NAME_None;
	WindowHitActors.Reset();
	bBufferedAttack = false;
	bComboWindow = false;
	RefreshSnapshot();
}

FKnightResult UKnightCombatComponent::ResetForCheckpoint()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || Profile == nullptr)
	{
		return KnightCombatFailure(EKnightError::InvalidState, TEXT("ResetCombat"));
	}
	ClearTransientActions();
	GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
	EndStagger();
	FGameplayTagContainer RuneTags(KnightGameplayTags::State_Combat_RuneSlowed);
	AbilitySystem->RemoveActiveEffectsWithGrantedTags(RuneTags);
	Snapshot.ComboIndex = 0;
	Snapshot.OutcomeCounts.Init(0, static_cast<int32>(EKnightCombatOutcome::Traversal) + 1);
	RefreshSnapshot();
	return FKnightResult::Success();
}

void UKnightCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bEndingPlay || Profile == nullptr || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	LockValidationElapsed += DeltaTime;
	if (LockValidationElapsed >= 0.1f)
	{
		LockValidationElapsed = 0.0f;
		if (Snapshot.LockedTarget != nullptr &&
			!ValidateCombatTarget(Snapshot.LockedTarget, Profile->GetTuning().LockRange).IsSuccess())
		{
			Snapshot.LockedTarget = nullptr;
			UpdateLockCamera();
			RefreshSnapshot();
		}
	}
}

void UKnightCombatComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ClearTransientActions();
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
	}
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(KnightGameplayTags::State_Combat_RuneSlowed).Remove(RuneTagHandle);
		AbilitySystem->RegisterGameplayTagEvent(KnightGameplayTags::State_Life_Downed).Remove(DownedTagHandle);
		AbilitySystem->RegisterGameplayTagEvent(KnightGameplayTags::State_Life_Dead).Remove(DeadTagHandle);
		if (GrantHandle.IsValid() && GetOwner() != nullptr && GetOwner()->HasAuthority())
		{
			AbilitySystem->RevokeAbilitySet(GrantHandle);
		}
	}
	AbilitySystem.Reset();
	AttackAbility.Reset();
	OnCombatOutcome.Clear();
	OnCombatSnapshotChanged.Clear();
	OnProfileChanged.Clear();
	Super::EndPlay(EndPlayReason);
}
