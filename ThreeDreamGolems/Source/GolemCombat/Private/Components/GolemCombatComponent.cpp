#include "Components/GolemCombatComponent.h"

#include "Types/GolemElement.h"
#include "Voids/GolemColorVoidComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/GolemGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/GolemAnimInstance.h"
#include "Assets/GolemAssetManager.h"
#include "Camera/GolemCameraModeComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GolemEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemCombat, Log, All);

namespace
{
	FGolemResult GolemCombatFailure(EGolemError Error, FName Context)
	{
		FText Message = NSLOCTEXT("GolemCombat", "ActionUnavailable", "This combat action is unavailable right now.");
		if (Error == EGolemError::InsufficientResources)
		{
			Message = NSLOCTEXT("GolemCombat", "InsufficientResources", "Not enough stamina or rune energy.");
		}
		else if (Error == EGolemError::OutOfRange)
		{
			Message = NSLOCTEXT("GolemCombat", "TargetOutOfRange", "Move closer and keep the target in sight.");
		}
		else if (Error == EGolemError::NotFound)
		{
			Message = NSLOCTEXT("GolemCombat", "NoTarget", "No eligible combat target is in sight.");
		}
		return FGolemResult::Failure(Error, Context, Message);
	}
	UGolemAbilitySystemComponent* GolemActorASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	bool IsOwnedArmorZone(const UPrimitiveComponent* Component, const AActor* Target)
	{
		return IsValid(Component) && IsValid(Target) && Component->GetOwner() == Target &&
			Component->IsRegistered() && Component->IsQueryCollisionEnabled() &&
			Component->ComponentHasTag(TEXT("GolemArmorZone"));
	}
	bool HasOwnedArmorZone(const AActor* Target)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components(Target);
		for (const UPrimitiveComponent* Component : Components)
		{
			if (IsOwnedArmorZone(Component, Target))
			{
				return true;
			}
		}
		return false;
	}
}

int32 FGolemCombatSnapshot::GetOutcomeCount(EGolemCombatOutcome Outcome) const
{
	const int32 Index = static_cast<int32>(Outcome);
	return OutcomeCounts.IsValidIndex(Index) ? OutcomeCounts[Index] : 0;
}

UGolemCombatComponent::UGolemCombatComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGolemCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemCombatComponent, ProfileId);
	DOREPLIFETIME(UGolemCombatComponent, Snapshot);
}

FGolemResult UGolemCombatComponent::Configure(const FPrimaryAssetId& InProfileId)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemCombatFailure(EGolemError::NotAuthority, TEXT("ConfigureCombat"));
	}
	if (!InProfileId.IsValid() || InProfileId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemCombatProfile")))
	{
		return GolemCombatFailure(EGolemError::InvalidId, TEXT("CombatProfileId"));
	}
	if (ProfileId.IsValid())
	{
		return ProfileId == InProfileId && Profile != nullptr ? FGolemResult::Success()
			: GolemCombatFailure(EGolemError::Conflict, TEXT("CombatProfileImmutable"));
	}
	ProfileId = InProfileId;
	const FGolemResult Result = ApplyProfile();
	if (!Result.IsSuccess())
	{
		ProfileId = FPrimaryAssetId();
		Profile = nullptr;
		return Result;
	}
	Snapshot.Poise = GetMaxPoise();
	Snapshot.OutcomeCounts.Init(0, static_cast<int32>(EGolemCombatOutcome::Traversal) + 1);
	RefreshSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemCombatComponent::ApplyProfile()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UGolemAbilitySystemComponent* ASC = GolemActorASC(Character);
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (Character == nullptr || ASC == nullptr || ASC->GetLifeComponent() == nullptr || Manager == nullptr)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("CombatActorComposition"));
	}
	UGolemDefinition* Definition = nullptr;
	FGolemResult Result = Manager->ResolveDefinition(ProfileId, UGolemCombatProfileDefinition::StaticClass(), Definition);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemCombatProfileDefinition* Resolved = CastChecked<UGolemCombatProfileDefinition>(Definition);
	Result = Resolved->ValidateCombatProfile();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemDefinition* SetDefinition = nullptr;
	Result = Manager->ResolveDefinition(Resolved->GetCombatAbilitySetId(),
		UGolemAbilitySetDefinition::StaticClass(), SetDefinition);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemAbilitySetDefinition* Set = CastChecked<UGolemAbilitySetDefinition>(SetDefinition);
	Result = Set->ValidateAbilitySet();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = UGolemAnimInstance::ConfigureCharacter(Character, Resolved->GetSkeletalMesh(),
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
		Hammer = NewObject<UStaticMeshComponent>(Character, TEXT("GolemCombatHammer"));
		Hammer->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Hammer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hammer->SetCanEverAffectNavigation(false);
		Character->AddInstanceComponent(Hammer);
		Hammer->SetupAttachment(Character->GetMesh(), TEXT("hand_r"));
		Hammer->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
		Hammer->SetRelativeScale3D(FVector(0.12f, 0.12f, 1.0f));
		Hammer->RegisterComponent();
		UStaticMeshComponent* Head = NewObject<UStaticMeshComponent>(Character, TEXT("GolemCombatHammerHead"));
		Head->SetStaticMesh(Hammer->GetStaticMesh());
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Head->SetCanEverAffectNavigation(false);
		Character->AddInstanceComponent(Head);
		Head->SetupAttachment(Character->GetMesh(), TEXT("hand_r"));
		Head->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
		Head->SetRelativeScale3D(FVector(0.55f, 0.3f, 0.25f));
		Head->RegisterComponent();
	}
	if (!DownedTagHandle.IsValid())
	{
		DownedTagHandle = ASC->RegisterGameplayTagEvent(GolemGameplayTags::State_Life_Downed)
			.AddUObject(this, &ThisClass::HandleLifeTagChanged);
		DeadTagHandle = ASC->RegisterGameplayTagEvent(GolemGameplayTags::State_Life_Dead)
			.AddUObject(this, &ThisClass::HandleLifeTagChanged);
	}
	SetComponentTickEnabled(Character->HasAuthority());
	OnProfileChanged.Broadcast(Profile);
	return FGolemResult::Success();
}

FGolemResult UGolemCombatComponent::TryInitializePresentation()
{
	if (!ProfileId.IsValid() || bEndingPlay)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("CombatPresentationIdentity"));
	}
	return Profile != nullptr ? FGolemResult::Success() : ApplyProfile();
}

FGolemResult UGolemCombatComponent::RefreshBaseWalkSpeed(float BaseSpeed)
{
	if (!FMath::IsFinite(BaseSpeed) || BaseSpeed <= 0.0f || BaseSpeed > 5000.0f)
	{
		return GolemCombatFailure(EGolemError::InvalidData, TEXT("CombatMovementBaseline"));
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (bEndingPlay || !IsValid(Character) || Character->IsActorBeingDestroyed() || GetWorld() == nullptr ||
		GetWorld()->bIsTearingDown || Character->GetCharacterMovement() == nullptr)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("CombatMovementPresentation"));
	}
	// Hero identity and ASC/profile state replicate independently; the tag is the current source of truth.
	const UGolemAbilitySystemComponent* ASC = AbilitySystem.IsValid() ? AbilitySystem.Get() : GolemActorASC(Character);
	Character->GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
	return FGolemResult::Success();
}

void UGolemCombatComponent::OnRep_ProfileId()
{
	// Replicated identity may arrive after the profile. The character repeats initialization after ASC readiness.
	const UGolemAbilitySystemComponent* ASC = GolemActorASC(GetOwner());
	if (ASC == nullptr || ASC->GetLifeComponent() == nullptr)
	{
		return;
	}
	const FGolemResult Result = ApplyProfile();
	if (!Result.IsSuccess())
	{
		UE_LOG(LogGolemCombat, Error, TEXT("Combat profile %s failed: %s"), *ProfileId.ToString(),
			*Result.Context.ToString());
	}
}
void UGolemCombatComponent::OnRep_Snapshot()
{
	OnCombatSnapshotChanged.Broadcast(Snapshot);
}
UGolemCombatProfileDefinition* UGolemCombatComponent::GetProfile() const
{
	return Profile;
}
FPrimaryAssetId UGolemCombatComponent::GetProfileId() const
{
	return ProfileId;
}
const FGolemCombatSnapshot& UGolemCombatComponent::GetSnapshot() const
{
	return Snapshot;
}
bool UGolemCombatComponent::CanTransferControl() const
{
	return !Snapshot.bAttacking && !Snapshot.bBlocking && !Snapshot.bParrying && !Snapshot.bDodging &&
		!Snapshot.bStaggered && (!AbilitySystem.IsValid() ||
			(!AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Finishing) &&
			!AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Traversing)));
}
bool UGolemCombatComponent::HasOpenComboWindow() const
{
	return bComboWindow && Snapshot.bAttacking;
}
bool UGolemCombatComponent::IsAuthorityAlive() const
{
	return !bEndingPlay && Profile != nullptr && IsValid(GetOwner()) && GetOwner()->HasAuthority() &&
		GetWorld() != nullptr && !GetWorld()->bIsTearingDown && AbilitySystem.IsValid() &&
		AbilitySystem->GetLifeComponent() != nullptr
		&& AbilitySystem->GetLifeComponent()->IsAlive();
}

void UGolemCombatComponent::RefreshSnapshot()
{
	if (!bEndingPlay && IsValid(GetOwner()) && GetOwner()->HasAuthority())
	{
		++Snapshot.Revision;
		OnCombatSnapshotChanged.Broadcast(Snapshot);
		GetOwner()->ForceNetUpdate();
	}
}

float UGolemCombatComponent::GetProjectedDamage(FGameplayTag ActionTag, float AuthoredDamage) const
{
	if (!AbilitySystem.IsValid() || AbilitySystem->GetGolemAttributes() == nullptr)
	{
		return AuthoredDamage;
	}
	// Authored damage assumes the definition's starting Power; later Power changes shift it one for one.
	const float Delta = AbilitySystem->GetGolemAttributes()->GetPower() - AbilitySystem->GetInitialPower();
	return FMath::Clamp(AuthoredDamage + Delta, 0.0f, 1000000.0f);
}

FGolemResult UGolemCombatComponent::BeginAttack(FGameplayTag ActionTag, UGolemGameplayAbility* Ability,
	UAnimMontage*& OutMontage, FName& OutSection)
{
	OutMontage = nullptr;
	OutSection = NAME_None;
	if (!IsAuthorityAlive() || Ability == nullptr || !CanTransferControl())
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("BeginAttack"));
	}
	const TArray<FGolemAttackDefinition>& Attacks = Profile->GetAttacks();
	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (Attacks[Index].ActionTag != ActionTag)
		{
			continue;
		}
		++TraceGeneration;
		AttackIndex = Index;
		AttackAction = ActionTag;
		AttackAbility = Ability;
		AttackActivationId = FGuid::NewGuid();
		WindowHitIndex = 0;
		Snapshot.bAttacking = true;
		Snapshot.AttackPhase = 1;
		Snapshot.ComboIndex = 1;
		bComboWindow = false;
		bBufferedAttack = false;
		ActiveWindow = NAME_None;
		WindowHitActors.Reset();
		ClearDeferredWeaponHits();
		OutMontage = Attacks[Index].Montage.LoadSynchronous();
		OutSection = Attacks[Index].SectionName;
		if (IsValid(Snapshot.LockedTarget) && ValidateCombatTarget(Snapshot.LockedTarget, Profile->GetTuning().LockRange).IsSuccess())
		{
			const FVector Direction = Snapshot.LockedTarget->GetActorLocation() - GetOwner()->GetActorLocation();
			GetOwner()->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
		}
		RefreshSnapshot();
		return FGolemResult::Success();
	}
	return GolemCombatFailure(EGolemError::NotFound, TEXT("AttackDefinition"));
}

const FGolemAttackDefinition* UGolemCombatComponent::GetCurrentAttack() const
{
	return Profile != nullptr && Profile->GetAttacks().IsValidIndex(AttackIndex)
		? &Profile->GetAttacks()[AttackIndex] : nullptr;
}
void UGolemCombatComponent::BufferAttackInput()
{
	if (IsAuthorityAlive() && Snapshot.bAttacking && bComboWindow &&
		AttackAction == GolemGameplayTags::Ability_Combat_Primary)
	{
		bBufferedAttack = true;
	}
}
void UGolemCombatComponent::EndAttack(UGolemGameplayAbility* Ability)
{
	if (AttackAbility.Get() != Ability)
	{
		return;
	}
	EndTraceGeometryDiagnostics(TEXT("AttackEnded"));
	++TraceGeneration;
	AttackAbility.Reset();
	AttackIndex = INDEX_NONE;
	AttackAction = FGameplayTag();
	ActiveWindow = NAME_None;
	WindowHitActors.Reset();
	ClearDeferredWeaponHits();
	bComboWindow = false;
	bBufferedAttack = false;
	Snapshot.bAttacking = false;
	Snapshot.AttackPhase = 0;
	RefreshSnapshot();
}

void UGolemCombatComponent::BeginTraceWindow(FName WindowId, UAnimSequenceBase* Animation)
{
	const FGolemAttackDefinition* Attack = GetCurrentAttack();
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
	Snapshot.AttackPhase = 2;
	++WindowHitIndex;
	WindowHitActors.Reset();
	ClearDeferredWeaponHits();
	PreviousHammerHead = Hammer != nullptr ? Hammer->GetComponentLocation() + Hammer->GetUpVector() * 50.0f
		: GetOwner()->GetActorLocation();
	BeginTraceGeometryDiagnostics();
	PerformWeaponSweep();
	RefreshSnapshot();
}
void UGolemCombatComponent::TickTraceWindow(FName WindowId, UAnimSequenceBase* Animation)
{
	const FGolemAttackDefinition* Attack = GetCurrentAttack();
	if (IsAuthorityAlive() && Snapshot.bAttacking && ActiveWindow == WindowId && Attack != nullptr &&
		Animation == Attack->Montage.Get())
	{
		PerformWeaponSweep();
	}
}
void UGolemCombatComponent::EndTraceWindow(FName WindowId, UAnimSequenceBase* Animation)
{
	const FGolemAttackDefinition* Attack = GetCurrentAttack();
	if (ActiveWindow == WindowId && Attack != nullptr && Animation == Attack->Montage.Get())
	{
		const uint64 Generation = TraceGeneration;
		const TWeakObjectPtr<UGolemGameplayAbility> Ability = AttackAbility;
		const int32 Index = AttackIndex;
		FlushDeferredWeaponHits(Generation, WindowId, Ability, Index);
		if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
		{
			return;
		}
		EndTraceGeometryDiagnostics(TEXT("NotifyEnded"));
		++TraceGeneration;
		ActiveWindow = NAME_None;
		Snapshot.AttackPhase = 3;
		RefreshSnapshot();
	}
}

void UGolemCombatComponent::SetComboWindow(bool bOpen, UAnimSequenceBase* Animation)
{
	const FGolemAttackDefinition* Attack = GetCurrentAttack();
	if (!IsAuthorityAlive() || Attack == nullptr || Animation != Attack->Montage.Get() ||
		AttackAction != GolemGameplayTags::Ability_Combat_Primary || !Snapshot.bAttacking)
	{
		return;
	}
	bComboWindow = bOpen;
	if (bOpen || !bBufferedAttack)
	{
		return;
	}
	bBufferedAttack = false;
	const TArray<FGolemAttackDefinition>& Attacks = Profile->GetAttacks();
	for (int32 Next = AttackIndex + 1; Next < Attacks.Num(); ++Next)
	{
		if (Attacks[Next].ActionTag != AttackAction)
		{
			continue;
		}
		if (Attacks[Next].Montage.Get() != Animation)
		{
			return;
		}
		++TraceGeneration;
		AttackIndex = Next;
		++Snapshot.ComboIndex;
		Snapshot.AttackPhase = 1;
		ActiveWindow = NAME_None;
		WindowHitActors.Reset();
		ClearDeferredWeaponHits();
		AbilitySystem->CurrentMontageJumpToSection(Attacks[Next].SectionName);
		PublishOutcome(EGolemCombatOutcome::Combo, AttackAction, Snapshot.LockedTarget, Snapshot.ComboIndex);
		return;
	}
}

void UGolemCombatComponent::PerformWeaponSweep()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const FGolemAttackDefinition* CurrentAttack = GetCurrentAttack();
	const uint64 Generation = TraceGeneration;
	const FName WindowId = ActiveWindow;
	const TWeakObjectPtr<UGolemGameplayAbility> Ability = AttackAbility;
	const int32 Index = AttackIndex;
	if (!IsValid(Character) || !IsValid(Character->GetMesh()) || CurrentAttack == nullptr ||
		!IsValid(Hammer) || !IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
	{
		return;
	}
	// Damage execution and confirmed-event listeners may synchronously cancel or replace this action.
	const FGolemAttackDefinition Attack = *CurrentAttack;
	const FGameplayTag ActionTag = AttackAction;
	const FVector Hand = Character->GetMesh()->GetSocketLocation(TEXT("hand_r"));
	const FVector Head = Hammer->GetComponentLocation() + Hammer->GetUpVector() * 50.0f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemWeaponSweep), false, GetOwner());
	Query.bReturnPhysicalMaterial = false;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	TArray<FHitResult> MovingHits;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Attack.TraceRadius);
	GetWorld()->SweepMultiByObjectType(Hits, Hand, Head, FQuat::Identity, Objects, Shape, Query);
	GetWorld()->SweepMultiByObjectType(MovingHits, PreviousHammerHead, Head, FQuat::Identity, Objects, Shape, Query);
	RecordTraceGeometrySweep(Hand, Head, PreviousHammerHead, Hits, MovingHits);
	Hits.Append(MovingHits);
	TSet<AActor*> ArmorTargets;
	for (const FHitResult& Candidate : Hits)
	{
		if (IsOwnedArmorZone(Candidate.GetComponent(), Candidate.GetActor()))
		{
			ArmorTargets.Add(Candidate.GetActor());
		}
	}
	PreviousHammerHead = Head;
	for (const FHitResult& Hit : Hits)
	{
		if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
		{
			return;
		}
		AActor* Target = Hit.GetActor();
		if (ArmorTargets.Contains(Target) && !IsOwnedArmorZone(Hit.GetComponent(), Target))
		{
			RecordTraceGeometryDecision(Hit, TEXT("SameSweepPrefersArmor"));
			continue;
		}
		if (IsValid(Target) && WindowHitActors.Contains(Target))
		{
			RecordTraceGeometryDecision(Hit, TEXT("ActorAlreadyHit"));
		}
		if (!IsValid(Target) || WindowHitActors.Contains(Target) ||
			!ValidateCombatTarget(Target, 400.0f).IsSuccess())
		{
			continue;
		}
		UGolemAbilitySystemComponent* TargetASC = GolemActorASC(Target);
		if (TargetASC == nullptr || TargetASC->GetGolemAttributes() == nullptr)
		{
			continue;
		}
		if (!IsOwnedArmorZone(Hit.GetComponent(), Target) && HasOwnedArmorZone(Target))
		{
			// An early capsule touch cannot consume the actor before this swing reaches its actual armor zone.
			if (!DeferredWindowHits.Contains(Target))
			{
				DeferredWindowHits.Add(Target, Hit);
				DeferredWindowHitOrder.Add(Target);
				RecordTraceGeometryDecision(Hit, TEXT("DeferredBody"));
			}
			continue;
		}
		CommitWeaponHit(Hit, Attack, ActionTag, Generation, WindowId, Ability, Index);
		if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
		{
			return;
		}
	}
}

void UGolemCombatComponent::CommitWeaponHit(const FHitResult& Hit, const FGolemAttackDefinition& Attack,
	FGameplayTag ActionTag, uint64 Generation, FName WindowId,
	const TWeakObjectPtr<UGolemGameplayAbility>& Ability, int32 Index)
{
	AActor* Target = Hit.GetActor();
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index) || !IsValid(Target) ||
		WindowHitActors.Contains(Target) || !IsValid(Component) || Component->GetOwner() != Target ||
		!Component->IsRegistered() || !Component->IsQueryCollisionEnabled() ||
		!ValidateCombatTarget(Target, 400.0f).IsSuccess())
	{
		return;
	}
	const TWeakObjectPtr<UGolemAbilitySystemComponent> TargetASC = GolemActorASC(Target);
	if (!TargetASC.IsValid() || TargetASC->GetGolemAttributes() == nullptr)
	{
		return;
	}
	RecordTraceGeometryDecision(Hit, TEXT("Accepted"));
	// Retire the pending contact and reserve dedup before any synchronous damage/proc/event callback.
	DeferredWindowHits.Remove(Target);
	WindowHitActors.Add(Target);
	if (UGolemColorVoidComponent* Void = Attack.bAppliesColor ? Cast<UGolemColorVoidComponent>(Hit.GetComponent()) : nullptr)
	{
		// A colour-carrying hit on a void only places colour; the fill, not the swing, hurts the Xenon.
		FGameplayTagContainer OwnedTags;
		AbilitySystem->GetOwnedGameplayTags(OwnedTags);
		EGolemElement Element = EGolemElement::Earth;
		if (GolemElements::FromOwnedTags(OwnedTags, Element))
		{
			const UGolemAttributeSet* SourceAttributes = AbilitySystem->GetGolemAttributes();
			Void->ApplyElement(Element, SourceAttributes != nullptr ? SourceAttributes->GetColorHold() : 0.0f, GetOwner());
			return;
		}
	}
	const float Before = TargetASC->GetGolemAttributes()->GetHealth();
	const FGolemResult Result = UGolemAbilitySystemComponent::ApplyCombatDamage(GetOwner(), Target,
		GetProjectedDamage(ActionTag, Attack.Damage), Attack.PoiseDamage, true, &Hit);
	if (!TargetASC.IsValid() || TargetASC->GetGolemAttributes() == nullptr || !IsValid(Target))
	{
		return;
	}
	if (!TargetASC.IsValid() || TargetASC->GetGolemAttributes() == nullptr || !IsValid(Target))
	{
		return;
	}
	const float Applied = Before - TargetASC->GetGolemAttributes()->GetHealth();
	if (Result.IsSuccess() && Applied > 0.0f)
	{
		PublishOutcome(ActionTag == GolemGameplayTags::Ability_Combat_Heavy ? EGolemCombatOutcome::HeavyHit
			: EGolemCombatOutcome::LightHit, ActionTag, Target, Applied);
	}
}

void UGolemCombatComponent::FlushDeferredWeaponHits(uint64 Generation, FName WindowId,
	const TWeakObjectPtr<UGolemGameplayAbility>& Ability, int32 Index)
{
	const FGolemAttackDefinition* CurrentAttack = GetCurrentAttack();
	if (CurrentAttack == nullptr || !IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
	{
		ClearDeferredWeaponHits();
		return;
	}
	const FGolemAttackDefinition Attack = *CurrentAttack;
	const FGameplayTag ActionTag = AttackAction;
	TMap<TWeakObjectPtr<AActor>, FHitResult> PendingHits = MoveTemp(DeferredWindowHits);
	TArray<TWeakObjectPtr<AActor>> PendingOrder = MoveTemp(DeferredWindowHitOrder);
	for (const TWeakObjectPtr<AActor>& Target : PendingOrder)
	{
		if (!IsTraceCapabilityCurrent(Generation, WindowId, Ability, Index))
		{
			return;
		}
		if (const FHitResult* Hit = PendingHits.Find(Target))
		{
			CommitWeaponHit(*Hit, Attack, ActionTag, Generation, WindowId, Ability, Index);
		}
	}
}

void UGolemCombatComponent::ClearDeferredWeaponHits()
{
	DeferredWindowHits.Reset();
	DeferredWindowHitOrder.Reset();
}

bool UGolemCombatComponent::IsTraceCapabilityCurrent(uint64 Generation, FName WindowId,
	const TWeakObjectPtr<UGolemGameplayAbility>& Ability, int32 Index) const
{
	return IsAuthorityAlive() && Snapshot.bAttacking && Generation == TraceGeneration &&
		!WindowId.IsNone() && ActiveWindow == WindowId && AttackIndex == Index &&
		Ability.IsValid() && AttackAbility == Ability && Ability->IsActive();
}

FGolemResult UGolemCombatComponent::BeginDefense(FGameplayTag ActionTag)
{
	if (!IsAuthorityAlive() || Snapshot.bAttacking || Snapshot.bDodging || Snapshot.bStaggered ||
		Snapshot.bBlocking || Snapshot.bParrying)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("BeginDefense"));
	}
	if (ActionTag == GolemGameplayTags::Ability_Combat_Parry)
	{
		Snapshot.bParrying = true;
		ParryStartedAt = GetWorld()->GetTimeSeconds();
	}
	else if (ActionTag == GolemGameplayTags::Ability_Combat_Block)
	{
		Snapshot.bBlocking = true;
	}
	else
	{
		return GolemCombatFailure(EGolemError::InvalidData, TEXT("DefenseAction"));
	}
	RefreshSnapshot();
	return FGolemResult::Success();
}
void UGolemCombatComponent::EndDefense(FGameplayTag ActionTag)
{
	if (ActionTag == GolemGameplayTags::Ability_Combat_Block)
	{
		Snapshot.bBlocking = false;
	}
	else if (ActionTag == GolemGameplayTags::Ability_Combat_Parry)
	{
		Snapshot.bParrying = false;
	}
	RefreshSnapshot();
}

FGolemResult UGolemCombatComponent::BeginDodge()
{
	if (!IsAuthorityAlive() || Snapshot.bDodging || Snapshot.bStaggered)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("BeginDodge"));
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || Character->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("DodgeMovement"));
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
	return FGolemResult::Success();
}
void UGolemCombatComponent::EndDodge()
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

FGolemResult UGolemCombatComponent::ValidateCombatTarget(AActor* Target, float Range) const
{
	if (!IsValid(Target) || !IsValid(GetOwner()) || Target == GetOwner() || Target->GetWorld() != GetWorld() ||
		!FMath::IsFinite(Range) || Range <= 0.0f)
	{
		return GolemCombatFailure(EGolemError::NotFound, TEXT("CombatTarget"));
	}
	const UGolemAbilitySystemComponent* SourceASC = AbilitySystem.Get();
	const UGolemAbilitySystemComponent* TargetASC = GolemActorASC(Target);
	if (SourceASC == nullptr || TargetASC == nullptr || TargetASC->GetLifeComponent() == nullptr ||
		!TargetASC->GetLifeComponent()->IsAlive())
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("CombatTargetLife"));
	}
	const bool bHostile = (SourceASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
		TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy)) ||
		(SourceASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) &&
		TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem));
	if (!bHostile)
	{
		return GolemCombatFailure(EGolemError::NotOwned, TEXT("CombatFriendlyTarget"));
	}
	if (FVector::DistSquared(Target->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(Range))
	{
		return GolemCombatFailure(EGolemError::OutOfRange, TEXT("CombatTargetRange"));
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemCombatLOS), false, GetOwner());
	Query.AddIgnoredActor(Target);
	FHitResult Obstruction;
	if (GetWorld() == nullptr || GetWorld()->LineTraceSingleByChannel(Obstruction,
		GetOwner()->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
	{
		return GolemCombatFailure(EGolemError::OutOfRange, TEXT("CombatTargetVisibility"));
	}
	return FGolemResult::Success();
}

AActor* UGolemCombatComponent::FindCombatTarget(float Range, bool bRequireStagger) const
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
		const UGolemCombatComponent* TargetCombat = Candidate->FindComponentByClass<UGolemCombatComponent>();
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

FGolemResult UGolemCombatComponent::SetValidatedTarget(AActor* Target)
{
	if (!IsAuthorityAlive())
	{
		return GolemCombatFailure(EGolemError::NotAuthority, TEXT("SetCombatTarget"));
	}
	const FGolemResult Result = ValidateCombatTarget(Target, Profile->GetTuning().LockRange);
	if (Result.IsSuccess())
	{
		Snapshot.LockedTarget = Target;
		UpdateLockCamera();
		RefreshSnapshot();
	}
	return Result;
}
FGolemResult UGolemCombatComponent::TryToggleTargetLock(AActor* Target)
{
	if (!IsAuthorityAlive())
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("ToggleCombatLock"));
	}
	if (IsValid(Snapshot.LockedTarget) && (Target == nullptr || Target == Snapshot.LockedTarget))
	{
		Snapshot.LockedTarget = nullptr;
		UpdateLockCamera();
		RefreshSnapshot();
		return FGolemResult::Success();
	}
	Target = Target != nullptr ? Target : FindCombatTarget(Profile->GetTuning().LockRange);
	const FGolemResult Result = SetValidatedTarget(Target);
	if (Result.IsSuccess())
	{
		PublishOutcome(EGolemCombatOutcome::TargetLocked, GolemGameplayTags::Ability_Combat_LockOn, Target);
	}
	return Result;
}
AActor* UGolemCombatComponent::GetLockedTarget() const
{
	return IsValid(Snapshot.LockedTarget) ? Snapshot.LockedTarget.Get() : nullptr;
}

void UGolemCombatComponent::UpdateLockCamera()
{
	UGolemCameraModeComponent* Camera = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UGolemCameraModeComponent>() : nullptr;
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
		FGolemCameraModeSettings Settings;
		Settings.Priority = 10;
		Settings.FocusTarget = Snapshot.LockedTarget;
		Settings.FieldOfView = 75.0f;
		Settings.ArmLength = 360.0f;
		Settings.SocketOffset = FVector(0.0f, 45.0f, 65.0f);
		LockCameraHandle = Camera->PushMode(Settings);
	}
}

float UGolemCombatComponent::GetMaxPoise() const
{
	return Profile != nullptr ? Profile->GetTuning().MaxPoise : 0.0f;
}

FGolemResult UGolemCombatComponent::RestoreSnapshotPoise(float InPoise)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemCombatFailure(EGolemError::NotAuthority, TEXT("SnapshotPoiseAuthority"));
	}
	if (!IsAuthorityAlive())
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("SnapshotPoiseState"));
	}
	if (!FMath::IsFinite(InPoise) || InPoise < 0.0f || InPoise > GetMaxPoise())
	{
		return GolemCombatFailure(EGolemError::InvalidData, TEXT("SnapshotPoiseRange"));
	}
	Snapshot.Poise = InPoise;
	RefreshSnapshot();
	return FGolemResult::Success();
}

void UGolemCombatComponent::ResolveIncomingDamage(AActor* Source, float InDamage, float PoiseDamage,
	bool bCanDefend, float& OutHealthDamage)
{
	OutHealthDamage = InDamage;
	if (!IsAuthorityAlive() || !FMath::IsFinite(InDamage) || !FMath::IsFinite(PoiseDamage))
	{
		return;
	}
	const FGolemCombatTuning& Tuning = Profile->GetTuning();
	if (bCanDefend && Snapshot.bDodging)
	{
		const float Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartedAt;
		if (Elapsed >= Tuning.DodgeInvulnerabilityStart && Elapsed <= Tuning.DodgeInvulnerabilityEnd)
		{
			OutHealthDamage = 0.0f;
			PublishOutcome(EGolemCombatOutcome::Dodged, GolemGameplayTags::Ability_Combat_Dodge, Source);
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
		AbilitySystem->ExecuteGameplayCue(GolemGameplayTags::GameplayCue_Parry);
		PublishOutcome(EGolemCombatOutcome::Parried, GolemGameplayTags::Ability_Combat_Parry, Source);
		if (IsValid(Source))
		{
			if (UGolemCombatComponent* SourceCombat = Source->FindComponentByClass<UGolemCombatComponent>())
			{
				SourceCombat->EnterStagger(GetOwner());
			}
		}
		return;
	}
	if (bCanDefend && bFront && Snapshot.bBlocking)
	{
		OutHealthDamage *= Tuning.BlockDamageFraction;
		PublishOutcome(EGolemCombatOutcome::Blocked, GolemGameplayTags::Ability_Combat_Block, Source, OutHealthDamage);
		return;
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
				!Snapshot.bDodging && !AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Finishing) &&
				!AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Traversing))
			{
				AbilitySystem->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), Profile->GetStaggerMontage(), 2.0f);
			}
			RefreshSnapshot();
		}
	}
}

void UGolemCombatComponent::EnterStagger(AActor* Source)
{
	if (!IsAuthorityAlive() || Snapshot.bStaggered)
	{
		return;
	}
	ClearTransientActions();
	Snapshot.bStaggered = true;
	Snapshot.Poise = 0.0f;
	AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->ExecuteGameplayCue(GolemGameplayTags::GameplayCue_Stagger);
	AbilitySystem->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), Profile->GetStaggerMontage(), 1.0f);
	GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &ThisClass::EndStagger,
		Profile->GetTuning().StaggerDuration, false);
	PublishOutcome(EGolemCombatOutcome::Staggered, GolemGameplayTags::State_Combat_Staggered, Source);
}
void UGolemCombatComponent::EndStagger()
{
	if (!bEndingPlay && Profile != nullptr && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Snapshot.bStaggered = false;
		Snapshot.Poise = GetMaxPoise();
		if (AbilitySystem.IsValid())
		{
			AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 0,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		RefreshSnapshot();
	}
}

void UGolemCombatComponent::PublishOutcome(EGolemCombatOutcome Outcome, FGameplayTag ActionTag,
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
	Snapshot.LastOutcomeServerTime = GetWorld()->GetGameState() != nullptr
		? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	RefreshSnapshot();
	FGolemCombatOutcome Event;
	Event.ConfirmationId = FGuid::NewGuid();
	Event.Revision = Snapshot.Revision;
	Event.SourceId = AbilitySystem->GetEntityId();
	Event.TargetId = GolemActorASC(Target) != nullptr ? GolemActorASC(Target)->GetEntityId() : FGolemEntityId();
	Event.Outcome = Outcome;
	Event.ActionTag = ActionTag;
	Event.Amount = Amount;
	if (UGolemEventSubsystem* Events = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		const FGolemResult Published = Events->PublishConfirmed(Event);
		if (!Published.IsSuccess())
		{
			UE_LOG(LogGolemCombat, Error, TEXT("Combat confirmation rejected: %s"), *Published.Context.ToString());
			return;
		}
	}
	OnCombatOutcome.Broadcast(Event);
}

void UGolemCombatComponent::HandleLifeTagChanged(const FGameplayTag Tag, int32 Count)
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

void UGolemCombatComponent::ClearTransientActions()
{
	EndTraceGeometryDiagnostics(TEXT("TransientActionsCleared"));
	ClearDeferredWeaponHits();
	++TraceGeneration;
	if (AbilitySystem.IsValid())
	{
		FGameplayTagContainer CombatTags;
		for (const FGameplayTag Tag : {FGameplayTag(GolemGameplayTags::Ability_Combat_Primary),
			FGameplayTag(GolemGameplayTags::Ability_Combat_Heavy), FGameplayTag(GolemGameplayTags::Ability_Combat_Block),
			FGameplayTag(GolemGameplayTags::Ability_Combat_Parry), FGameplayTag(GolemGameplayTags::Ability_Combat_Dodge),
			FGameplayTag(GolemGameplayTags::Ability_Combat_Finisher),
			FGameplayTag(GolemGameplayTags::Ability_World_Traverse)})
		{
			CombatTags.AddTag(Tag);
		}
		AbilitySystem->CancelAbilities(&CombatTags);
	}
	Snapshot.bAttacking = false;
	Snapshot.bBlocking = false;
	Snapshot.AttackPhase = 0;
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

FGolemResult UGolemCombatComponent::ResetForCheckpoint()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || Profile == nullptr)
	{
		return GolemCombatFailure(EGolemError::InvalidState, TEXT("ResetCombat"));
	}
	ClearTransientActions();
	GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
	EndStagger();
	Snapshot.ComboIndex = 0;
	Snapshot.OutcomeCounts.Init(0, static_cast<int32>(EGolemCombatOutcome::Traversal) + 1);
	RefreshSnapshot();
	return FGolemResult::Success();
}

void UGolemCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
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

void UGolemCombatComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ClearTransientActions();
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
	}
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(GolemGameplayTags::State_Life_Downed).Remove(DownedTagHandle);
		AbilitySystem->RegisterGameplayTagEvent(GolemGameplayTags::State_Life_Dead).Remove(DeadTagHandle);
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
