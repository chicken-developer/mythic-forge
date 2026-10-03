#include "CombatChallenge/KnightCombatChallengeHUD.h"

#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "CombatChallenge/KnightCombatChallengeState.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/KnightPlayerController.h"

bool AKnightCombatChallengeHUD::IsDisplayingChallengeCompletion() const
{
	return bChallengeCompleted;
}

bool AKnightCombatChallengeHUD::IsDisplayingChallengeFailure() const
{
	return bChallengeFailed;
}

void AKnightCombatChallengeHUD::BeginPlay()
{
	Super::BeginPlay();
	ChallengeSpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &AKnightCombatChallengeHUD::HandleChallengeActorSpawned));
	BindChallengeState(AKnightCombatChallengeState::Find(GetWorld()));
	ChallengeController = Cast<AKnightPlayerController>(PlayerOwner);
	if (ChallengeController.IsValid())
	{
		HandleControlledCombatChanged(nullptr, ChallengeController->GetControlledKnight());
		HandleCombatCommandResult(ChallengeController->GetLastCommandResult());
		ChallengeControlHandle = ChallengeController->OnControlledKnightChanged.AddUObject(this,
			&AKnightCombatChallengeHUD::HandleControlledCombatChanged);
		ChallengeCommandHandle = ChallengeController->OnCommandResult.AddUObject(this,
			&AKnightCombatChallengeHUD::HandleCombatCommandResult);
	}
}

void AKnightCombatChallengeHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->RemoveOnActorSpawnedHandler(ChallengeSpawnHandle);
	if (ChallengeSource.IsValid())
	{
		ChallengeSource->OnSnapshotChanged.Remove(ChallengeStateHandle);
	}
	if (ChallengeController.IsValid())
	{
		ChallengeController->OnControlledKnightChanged.Remove(ChallengeControlHandle);
		ChallengeController->OnCommandResult.Remove(ChallengeCommandHandle);
	}
	UnbindCombat();
	ChallengeSource.Reset();
	ChallengeController.Reset();
	Super::EndPlay(EndPlayReason);
}

void AKnightCombatChallengeHUD::HandleChallengeActorSpawned(AActor* Actor)
{
	if (AKnightCombatChallengeState* State = Cast<AKnightCombatChallengeState>(Actor))
	{
		BindChallengeState(State);
	}
}

void AKnightCombatChallengeHUD::BindChallengeState(AKnightCombatChallengeState* State)
{
	if (ChallengeSource.IsValid())
	{
		ChallengeSource->OnSnapshotChanged.Remove(ChallengeStateHandle);
	}
	ChallengeStateHandle.Reset();
	ChallengeSource = State;
	HandleChallengeSnapshot();
	if (ChallengeSource.IsValid())
	{
		ChallengeStateHandle = ChallengeSource->OnSnapshotChanged.AddUObject(this,
			&AKnightCombatChallengeHUD::HandleChallengeSnapshot);
	}
}

void AKnightCombatChallengeHUD::HandleChallengeSnapshot()
{
	const FKnightCombatChallengeSnapshot Snapshot = ChallengeSource.IsValid()
		? ChallengeSource->GetSnapshot() : FKnightCombatChallengeSnapshot();
	bChallengeCompleted = Snapshot.bCompleted;
	bChallengeFailed = Snapshot.bFailed;
	AttackProgress = FText::Format(NSLOCTEXT("KnightCombatChallenge", "Attacks",
		"Light hits: {0} | Heavy hits: {1} | Combos: {2}"), FText::AsNumber(Snapshot.LightHitCount),
		FText::AsNumber(Snapshot.HeavyHitCount), FText::AsNumber(Snapshot.ComboCount));
	DefenseProgress = FText::Format(NSLOCTEXT("KnightCombatChallenge", "Defense",
		"Blocked: {0} | Parried: {1} | Dodged: {2} | Staggered: {3}"), FText::AsNumber(Snapshot.BlockCount),
		FText::AsNumber(Snapshot.ParryCount), FText::AsNumber(Snapshot.DodgeCount),
		FText::AsNumber(Snapshot.StaggerCount));
	ActionProgress = FText::Format(NSLOCTEXT("KnightCombatChallenge", "Actions",
		"Target lock: {0} | Rune hits: {1} | Finishers: {2} | Traversals: {3}"),
		Snapshot.bTargetLocked ? NSLOCTEXT("KnightCombatChallenge", "Done", "done")
			: NSLOCTEXT("KnightCombatChallenge", "Pending", "pending"),
		FText::AsNumber(Snapshot.RuneHitCount), FText::AsNumber(Snapshot.FinisherCount),
		FText::AsNumber(Snapshot.TraversalCount));
}

void AKnightCombatChallengeHUD::HandleControlledCombatChanged(AKnightCharacter* Previous, AKnightCharacter* Current)
{
	UnbindCombat();
	ControlledCombat = Current != nullptr ? Current->FindComponentByClass<UKnightCombatComponent>() : nullptr;
	ResourceSource = Current != nullptr ? Current->GetKnightAbilitySystem() : nullptr;
	HandleCombatSnapshot(ControlledCombat.IsValid() ? ControlledCombat->GetSnapshot() : FKnightCombatSnapshot());
	RefreshResourceSnapshot();
	if (ControlledCombat.IsValid())
	{
		CombatSnapshotHandle = ControlledCombat->OnCombatSnapshotChanged.AddUObject(this,
			&AKnightCombatChallengeHUD::HandleCombatSnapshot);
	}
	if (ResourceSource.IsValid())
	{
		for (const FGameplayAttribute& Attribute : { UKnightAttributeSet::GetStaminaAttribute(),
			UKnightAttributeSet::GetMaxStaminaAttribute(), UKnightAttributeSet::GetRuneEnergyAttribute(),
			UKnightAttributeSet::GetMaxRuneEnergyAttribute() })
		{
			const FDelegateHandle Handle = ResourceSource->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddUObject(this, &AKnightCombatChallengeHUD::HandleResourceChanged);
			ResourceHandles.Emplace(Attribute, Handle);
		}
	}
}

void AKnightCombatChallengeHUD::HandleCombatSnapshot(const FKnightCombatSnapshot& Snapshot)
{
	FText Action = NSLOCTEXT("KnightCombatChallenge", "Ready", "ready");
	if (Snapshot.bStaggered)
	{
		Action = NSLOCTEXT("KnightCombatChallenge", "Staggered", "STAGGERED");
	}
	else if (Snapshot.bDodging)
	{
		Action = NSLOCTEXT("KnightCombatChallenge", "Dodging", "dodging");
	}
	else if (Snapshot.bParrying)
	{
		Action = NSLOCTEXT("KnightCombatChallenge", "Parrying", "parrying");
	}
	else if (Snapshot.bBlocking)
	{
		Action = NSLOCTEXT("KnightCombatChallenge", "Blocking", "blocking");
	}
	else if (Snapshot.bAttacking)
	{
		Action = NSLOCTEXT("KnightCombatChallenge", "Attacking", "attacking");
	}
	CombatStatus = FText::Format(NSLOCTEXT("KnightCombatChallenge", "CombatStatus",
		"Poise: {0} | Combo stage: {1} | Lock: {2} | {3}"), FText::AsNumber(FMath::RoundToInt(Snapshot.Poise)),
		FText::AsNumber(Snapshot.ComboIndex), IsValid(Snapshot.LockedTarget)
			? NSLOCTEXT("KnightCombatChallenge", "LockOn", "on") : NSLOCTEXT("KnightCombatChallenge", "LockOff", "off"),
		Action);
}

void AKnightCombatChallengeHUD::HandleResourceChanged(const FOnAttributeChangeData& Change)
{
	RefreshResourceSnapshot();
}

void AKnightCombatChallengeHUD::HandleCombatCommandResult(const FKnightResult& Result)
{
	ChallengeCommandStatus = Result.IsSuccess() ? FText::GetEmpty() : Result.Message;
}

void AKnightCombatChallengeHUD::RefreshResourceSnapshot()
{
	const UKnightAttributeSet* Attributes = ResourceSource.IsValid() ? ResourceSource->GetKnightAttributes() : nullptr;
	ResourceStatus = FText::Format(NSLOCTEXT("KnightCombatChallenge", "Resources", "Stamina: {0}/{1} | Rune energy: {2}/{3}"),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetStamina() : 0.0f)),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetMaxStamina() : 0.0f)),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetRuneEnergy() : 0.0f)),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetMaxRuneEnergy() : 0.0f)));
}

void AKnightCombatChallengeHUD::UnbindCombat()
{
	if (ControlledCombat.IsValid())
	{
		ControlledCombat->OnCombatSnapshotChanged.Remove(CombatSnapshotHandle);
	}
	if (ResourceSource.IsValid())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Entry : ResourceHandles)
		{
			ResourceSource->GetGameplayAttributeValueChangeDelegate(Entry.Key).Remove(Entry.Value);
		}
	}
	CombatSnapshotHandle.Reset();
	ResourceHandles.Reset();
	ControlledCombat.Reset();
	ResourceSource.Reset();
}

void AKnightCombatChallengeHUD::DrawHUD()
{
	AHUD::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	const TArray<FText> Lines =
	{
		NSLOCTEXT("KnightCombatChallenge", "Title", "THREE DREAMING KNIGHTS - Combat challenge"),
		FText::Format(NSLOCTEXT("KnightCombatChallenge", "KnightHealth", "{0} | Health: {1}"),
			GetDisplayedKnightName(), FText::AsNumber(FMath::RoundToInt(GetDisplayedHealth()))),
		ResourceStatus,
		CombatStatus,
		NSLOCTEXT("KnightCombatChallenge", "AttackControls", "LMB / RT light | RMB / LT heavy | repeat light during the combo window"),
		NSLOCTEXT("KnightCombatChallenge", "DefenseControls", "Hold Q / LB block | C / left stick click parry | Shift / B dodge"),
		NSLOCTEXT("KnightCombatChallenge", "ActionControls", "MMB / right stick click lock | G / D-pad up rune | H / D-pad right finisher | V / D-pad down traverse"),
		NSLOCTEXT("KnightCombatChallenge", "MovementControls", "WASD / sticks move | Space / A jump | Tab / 1-3 / RB switch | R / Y retry | E / X revive | F / D-pad left telegraph"),
		AttackProgress,
		DefenseProgress,
		ActionProgress,
		bChallengeCompleted ? NSLOCTEXT("KnightCombatChallenge", "Completed", "CHALLENGE COMPLETE - R / Y resets all fixtures")
			: bChallengeFailed ? NSLOCTEXT("KnightCombatChallenge", "Failed", "CHALLENGE FAILED - R / Y retry")
			: ChallengeCommandStatus.IsEmpty() ? NSLOCTEXT("KnightCombatChallenge", "Goal",
				"Use the marked stations to demonstrate attacks, defense, stagger, rune, finisher and traversal.")
				: ChallengeCommandStatus
	};
	const float Width = FMath::Min(1100.0f, static_cast<float>(Canvas->SizeX) - 40.0f);
	const float Scale = FMath::Clamp(Width / 1100.0f, 0.55f, 1.0f);
	const float LineHeight = FMath::Clamp(static_cast<float>(Canvas->SizeY) * 0.33f / Lines.Num(), 12.0f, 20.0f);
	DrawRect(FLinearColor(0.025f, 0.035f, 0.05f, 0.88f), 20.0f, 20.0f, Width,
		LineHeight * Lines.Num() + 24.0f);
	const FLinearColor Color = bChallengeCompleted ? FLinearColor::Green
		: bChallengeFailed ? FLinearColor(1.0f, 0.35f, 0.25f) : FLinearColor(0.8f, 0.95f, 1.0f);
	float Y = 32.0f;
	for (const FText& Line : Lines)
	{
		DrawText(Line.ToString(), Color, 34.0f, Y, GEngine->GetSmallFont(), Scale);
		Y += LineHeight;
	}
}
