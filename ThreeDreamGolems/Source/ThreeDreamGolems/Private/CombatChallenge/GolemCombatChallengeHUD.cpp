#include "CombatChallenge/GolemCombatChallengeHUD.h"

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "CombatChallenge/GolemCombatChallengeState.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/GolemPlayerController.h"

bool AGolemCombatChallengeHUD::IsDisplayingChallengeCompletion() const
{
	return bChallengeCompleted;
}

bool AGolemCombatChallengeHUD::IsDisplayingChallengeFailure() const
{
	return bChallengeFailed;
}

void AGolemCombatChallengeHUD::BeginPlay()
{
	Super::BeginPlay();
	ChallengeSpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &AGolemCombatChallengeHUD::HandleChallengeActorSpawned));
	BindChallengeState(AGolemCombatChallengeState::Find(GetWorld()));
	ChallengeController = Cast<AGolemPlayerController>(PlayerOwner);
	if (ChallengeController.IsValid())
	{
		HandleControlledCombatChanged(nullptr, ChallengeController->GetControlledGolem());
		HandleCombatCommandResult(ChallengeController->GetLastCommandResult());
		ChallengeControlHandle = ChallengeController->OnControlledGolemChanged.AddUObject(this,
			&AGolemCombatChallengeHUD::HandleControlledCombatChanged);
		ChallengeCommandHandle = ChallengeController->OnCommandResult.AddUObject(this,
			&AGolemCombatChallengeHUD::HandleCombatCommandResult);
	}
}

void AGolemCombatChallengeHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->RemoveOnActorSpawnedHandler(ChallengeSpawnHandle);
	if (ChallengeSource.IsValid())
	{
		ChallengeSource->OnSnapshotChanged.Remove(ChallengeStateHandle);
	}
	if (ChallengeController.IsValid())
	{
		ChallengeController->OnControlledGolemChanged.Remove(ChallengeControlHandle);
		ChallengeController->OnCommandResult.Remove(ChallengeCommandHandle);
	}
	UnbindCombat();
	ChallengeSource.Reset();
	ChallengeController.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGolemCombatChallengeHUD::HandleChallengeActorSpawned(AActor* Actor)
{
	if (AGolemCombatChallengeState* State = Cast<AGolemCombatChallengeState>(Actor))
	{
		BindChallengeState(State);
	}
}

void AGolemCombatChallengeHUD::BindChallengeState(AGolemCombatChallengeState* State)
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
			&AGolemCombatChallengeHUD::HandleChallengeSnapshot);
	}
}

void AGolemCombatChallengeHUD::HandleChallengeSnapshot()
{
	const FGolemCombatChallengeSnapshot Snapshot = ChallengeSource.IsValid()
		? ChallengeSource->GetSnapshot() : FGolemCombatChallengeSnapshot();
	bChallengeCompleted = Snapshot.bCompleted;
	bChallengeFailed = Snapshot.bFailed;
	AttackProgress = FText::Format(NSLOCTEXT("GolemCombatChallenge", "Attacks",
		"Light hits: {0} | Heavy hits: {1} | Combos: {2}"), FText::AsNumber(Snapshot.LightHitCount),
		FText::AsNumber(Snapshot.HeavyHitCount), FText::AsNumber(Snapshot.ComboCount));
	DefenseProgress = FText::Format(NSLOCTEXT("GolemCombatChallenge", "Defense",
		"Blocked: {0} | Parried: {1} | Dodged: {2} | Staggered: {3}"), FText::AsNumber(Snapshot.BlockCount),
		FText::AsNumber(Snapshot.ParryCount), FText::AsNumber(Snapshot.DodgeCount),
		FText::AsNumber(Snapshot.StaggerCount));
	ActionProgress = FText::Format(NSLOCTEXT("GolemCombatChallenge", "Actions",
		"Target lock: {0} | Rune hits: {1} | Finishers: {2} | Traversals: {3}"),
		Snapshot.bTargetLocked ? NSLOCTEXT("GolemCombatChallenge", "Done", "done")
			: NSLOCTEXT("GolemCombatChallenge", "Pending", "pending"),
		FText::AsNumber(Snapshot.RuneHitCount), FText::AsNumber(Snapshot.FinisherCount),
		FText::AsNumber(Snapshot.TraversalCount));
}

void AGolemCombatChallengeHUD::HandleControlledCombatChanged(AGolemCharacter* Previous, AGolemCharacter* Current)
{
	UnbindCombat();
	ControlledCombat = Current != nullptr ? Current->FindComponentByClass<UGolemCombatComponent>() : nullptr;
	ResourceSource = Current != nullptr ? Current->GetGolemAbilitySystem() : nullptr;
	HandleCombatSnapshot(ControlledCombat.IsValid() ? ControlledCombat->GetSnapshot() : FGolemCombatSnapshot());
	RefreshResourceSnapshot();
	if (ControlledCombat.IsValid())
	{
		CombatSnapshotHandle = ControlledCombat->OnCombatSnapshotChanged.AddUObject(this,
			&AGolemCombatChallengeHUD::HandleCombatSnapshot);
	}
	if (ResourceSource.IsValid())
	{
		for (const FGameplayAttribute& Attribute : { UGolemAttributeSet::GetStaminaAttribute(),
			UGolemAttributeSet::GetMaxStaminaAttribute(), UGolemAttributeSet::GetRuneEnergyAttribute(),
			UGolemAttributeSet::GetMaxRuneEnergyAttribute() })
		{
			const FDelegateHandle Handle = ResourceSource->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddUObject(this, &AGolemCombatChallengeHUD::HandleResourceChanged);
			ResourceHandles.Emplace(Attribute, Handle);
		}
	}
}

void AGolemCombatChallengeHUD::HandleCombatSnapshot(const FGolemCombatSnapshot& Snapshot)
{
	FText Action = NSLOCTEXT("GolemCombatChallenge", "Ready", "ready");
	if (Snapshot.bStaggered)
	{
		Action = NSLOCTEXT("GolemCombatChallenge", "Staggered", "STAGGERED");
	}
	else if (Snapshot.bDodging)
	{
		Action = NSLOCTEXT("GolemCombatChallenge", "Dodging", "dodging");
	}
	else if (Snapshot.bParrying)
	{
		Action = NSLOCTEXT("GolemCombatChallenge", "Parrying", "parrying");
	}
	else if (Snapshot.bBlocking)
	{
		Action = NSLOCTEXT("GolemCombatChallenge", "Blocking", "blocking");
	}
	else if (Snapshot.bAttacking)
	{
		Action = NSLOCTEXT("GolemCombatChallenge", "Attacking", "attacking");
	}
	CombatStatus = FText::Format(NSLOCTEXT("GolemCombatChallenge", "CombatStatus",
		"Poise: {0} | Combo stage: {1} | Lock: {2} | {3}"), FText::AsNumber(FMath::RoundToInt(Snapshot.Poise)),
		FText::AsNumber(Snapshot.ComboIndex), IsValid(Snapshot.LockedTarget)
			? NSLOCTEXT("GolemCombatChallenge", "LockOn", "on") : NSLOCTEXT("GolemCombatChallenge", "LockOff", "off"),
		Action);
}

void AGolemCombatChallengeHUD::HandleResourceChanged(const FOnAttributeChangeData& Change)
{
	RefreshResourceSnapshot();
}

void AGolemCombatChallengeHUD::HandleCombatCommandResult(const FGolemResult& Result)
{
	ChallengeCommandStatus = Result.IsSuccess() ? FText::GetEmpty() : Result.Message;
}

void AGolemCombatChallengeHUD::RefreshResourceSnapshot()
{
	const UGolemAttributeSet* Attributes = ResourceSource.IsValid() ? ResourceSource->GetGolemAttributes() : nullptr;
	ResourceStatus = FText::Format(NSLOCTEXT("GolemCombatChallenge", "Resources", "Stamina: {0}/{1} | Rune energy: {2}/{3}"),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetStamina() : 0.0f)),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetMaxStamina() : 0.0f)),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetRuneEnergy() : 0.0f)),
		FText::AsNumber(FMath::RoundToInt(Attributes != nullptr ? Attributes->GetMaxRuneEnergy() : 0.0f)));
}

void AGolemCombatChallengeHUD::UnbindCombat()
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

void AGolemCombatChallengeHUD::DrawHUD()
{
	AHUD::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	const TArray<FText> Lines =
	{
		NSLOCTEXT("GolemCombatChallenge", "Title", "THREE DREAMING GOLEMS - Combat challenge"),
		FText::Format(NSLOCTEXT("GolemCombatChallenge", "GolemHealth", "{0} | Health: {1}"),
			GetDisplayedGolemName(), FText::AsNumber(FMath::RoundToInt(GetDisplayedHealth()))),
		ResourceStatus,
		CombatStatus,
		NSLOCTEXT("GolemCombatChallenge", "AttackControls", "LMB / RT light | RMB / LT heavy | repeat light during the combo window"),
		NSLOCTEXT("GolemCombatChallenge", "DefenseControls", "Hold Q / LB block | C / left stick click parry | Shift / B dodge"),
		NSLOCTEXT("GolemCombatChallenge", "ActionControls", "MMB / right stick click lock | G / D-pad up rune | H / D-pad right finisher | V / D-pad down traverse"),
		NSLOCTEXT("GolemCombatChallenge", "MovementControls", "WASD / sticks move | Space / A jump | Tab / 1-3 / RB switch | R / Y retry | E / X revive | F / D-pad left telegraph"),
		AttackProgress,
		DefenseProgress,
		ActionProgress,
		bChallengeCompleted ? NSLOCTEXT("GolemCombatChallenge", "Completed", "CHALLENGE COMPLETE - R / Y resets all fixtures")
			: bChallengeFailed ? NSLOCTEXT("GolemCombatChallenge", "Failed", "CHALLENGE FAILED - R / Y retry")
			: ChallengeCommandStatus.IsEmpty() ? NSLOCTEXT("GolemCombatChallenge", "Goal",
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
