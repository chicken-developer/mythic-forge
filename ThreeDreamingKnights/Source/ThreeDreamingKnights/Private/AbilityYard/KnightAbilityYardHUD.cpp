#include "AbilityYard/KnightAbilityYardHUD.h"

#include "AbilityYard/KnightAbilityYardState.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/KnightPlayerController.h"
#include "Tags/KnightGameplayTags.h"

float AKnightAbilityYardHUD::GetDisplayedHealth() const
{
	return DisplayedHealth;
}

EKnightLifeState AKnightAbilityYardHUD::GetDisplayedLifeState() const
{
	return DisplayedLifeState;
}

bool AKnightAbilityYardHUD::IsDisplayingAbilityCompletion() const
{
	return bCompleted;
}

void AKnightAbilityYardHUD::BeginPlay()
{
	Super::BeginPlay();
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &AKnightAbilityYardHUD::HandleActorSpawned));
	BindYardState(AKnightAbilityYardState::Find(GetWorld()));
	AbilityControlSource = Cast<AKnightPlayerController>(PlayerOwner);
	if (AbilityControlSource.IsValid())
	{
		HandleControlledKnightChanged(nullptr, AbilityControlSource->GetControlledKnight());
		HandleAbilityCommandResult(AbilityControlSource->GetLastCommandResult());
		ControlHandle = AbilityControlSource->OnControlledKnightChanged.AddUObject(this,
			&AKnightAbilityYardHUD::HandleControlledKnightChanged);
		AbilityCommandHandle = AbilityControlSource->OnCommandResult.AddUObject(this,
			&AKnightAbilityYardHUD::HandleAbilityCommandResult);
	}
}

void AKnightAbilityYardHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
	if (YardSource.IsValid())
	{
		YardSource->OnSnapshotChanged.Remove(YardHandle);
	}
	if (AbilityControlSource.IsValid())
	{
		AbilityControlSource->OnControlledKnightChanged.Remove(ControlHandle);
		AbilityControlSource->OnCommandResult.Remove(AbilityCommandHandle);
	}
	UnbindKnight();
	YardSource.Reset();
	AbilityControlSource.Reset();
	Super::EndPlay(EndPlayReason);
}

void AKnightAbilityYardHUD::HandleActorSpawned(AActor* Actor)
{
	if (AKnightAbilityYardState* State = Cast<AKnightAbilityYardState>(Actor))
	{
		BindYardState(State);
	}
}

void AKnightAbilityYardHUD::BindYardState(AKnightAbilityYardState* State)
{
	if (YardSource.IsValid())
	{
		YardSource->OnSnapshotChanged.Remove(YardHandle);
	}
	YardHandle.Reset();
	YardSource = State;
	HandleYardSnapshot();
	if (YardSource.IsValid())
	{
		YardHandle = YardSource->OnSnapshotChanged.AddUObject(this, &AKnightAbilityYardHUD::HandleYardSnapshot);
	}
}

void AKnightAbilityYardHUD::HandleYardSnapshot()
{
	const FKnightAbilityYardSnapshot Snapshot = YardSource.IsValid()
		? YardSource->GetSnapshot() : FKnightAbilityYardSnapshot();
	bCompleted = Snapshot.bCompleted;
	DummyStatus = FText::Format(NSLOCTEXT("KnightAbilityYard", "DummyHealth", "Dummy health: {0} / {1}"),
		FText::AsNumber(FMath::RoundToInt(Snapshot.DummyHealth)),
		FText::AsNumber(FMath::RoundToInt(Snapshot.DummyMaxHealth)));
	const int32 Done = static_cast<int32>(Snapshot.bDummyDamaged) + static_cast<int32>(Snapshot.bPlayerDamaged)
		+ static_cast<int32>(Snapshot.bCompanionDowned) + static_cast<int32>(Snapshot.bCompanionRevived);
	ObjectiveStatus = FText::Format(NSLOCTEXT("KnightAbilityYard", "Progress", "Ability objectives: {0} / 4"),
		FText::AsNumber(Done));
}

void AKnightAbilityYardHUD::HandleControlledKnightChanged(AKnightCharacter* Previous, AKnightCharacter* Current)
{
	UnbindKnight();
	AbilitySource = Current != nullptr ? Current->GetKnightAbilitySystem() : nullptr;
	LifeSource = Current != nullptr ? Current->GetLifeComponent() : nullptr;
	RefreshKnightSnapshot();
	if (AbilitySource.IsValid())
	{
		HealthHandle = AbilitySource->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetHealthAttribute())
			.AddUObject(this, &AKnightAbilityYardHUD::HandleHealthChanged);
		MaxHealthHandle = AbilitySource->GetGameplayAttributeValueChangeDelegate(
			UKnightAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &AKnightAbilityYardHUD::HandleHealthChanged);
		CueHandle = AbilitySource->OnGameplayCueProcessed.AddUObject(this, &AKnightAbilityYardHUD::HandleGameplayCue);
		GrantsHandle = AbilitySource->OnAbilityGrantsChanged.AddUObject(this,
			&AKnightAbilityYardHUD::HandleAbilityGrantsChanged);
	}
	if (LifeSource.IsValid())
	{
		LifeHandle = LifeSource->OnLifeStateChanged.AddUObject(this, &AKnightAbilityYardHUD::HandleLifeChanged);
	}
}

void AKnightAbilityYardHUD::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	RefreshKnightSnapshot();
}

void AKnightAbilityYardHUD::HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	RefreshKnightSnapshot();
}

void AKnightAbilityYardHUD::HandleGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType)
{
	RefreshKnightSnapshot();
}

void AKnightAbilityYardHUD::HandleAbilityGrantsChanged()
{
	RefreshKnightSnapshot();
}

void AKnightAbilityYardHUD::HandleAbilityCommandResult(const FKnightResult& Result)
{
	AbilityCommandStatus = Result.IsSuccess() ? FText::GetEmpty() : Result.Message;
}

void AKnightAbilityYardHUD::RefreshKnightSnapshot()
{
	const UKnightAttributeSet* Attributes = AbilitySource.IsValid() ? AbilitySource->GetKnightAttributes() : nullptr;
	DisplayedHealth = Attributes != nullptr ? Attributes->GetHealth() : 0.0f;
	const float MaxHealth = Attributes != nullptr ? Attributes->GetMaxHealth() : 0.0f;
	HealthStatus = FText::Format(NSLOCTEXT("KnightAbilityYard", "Health", "Health: {0} / {1}"),
		FText::AsNumber(FMath::RoundToInt(DisplayedHealth)), FText::AsNumber(FMath::RoundToInt(MaxHealth)));
	DisplayedLifeState = LifeSource.IsValid() ? LifeSource->GetLifeState() : EKnightLifeState::Alive;
	switch (DisplayedLifeState)
	{
	case EKnightLifeState::Downed:
		LifeStatus = NSLOCTEXT("KnightAbilityYard", "Downed", "DOWNED - a teammate can hold E nearby to revive");
		break;
	case EKnightLifeState::Dead:
		LifeStatus = NSLOCTEXT("KnightAbilityYard", "Dead", "DEAD - host can press R to reset the exercise");
		break;
	default:
		LifeStatus = NSLOCTEXT("KnightAbilityYard", "Alive", "ALIVE");
		break;
	}
	const FKnightCueDiagnostics Damage = AbilitySource.IsValid()
		? AbilitySource->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Damage) : FKnightCueDiagnostics();
	const FKnightCueDiagnostics Downed = AbilitySource.IsValid()
		? AbilitySource->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Downed) : FKnightCueDiagnostics();
	const int32 Grants = AbilitySource.IsValid() ? AbilitySource->GetOwnedGrantCount() : 0;
	CueStatus = FText::Format(NSLOCTEXT("KnightAbilityYard", "Diagnostics",
		"GAS grant sources: {0} | Damage cues: {1} | Downed cues: {2}"), FText::AsNumber(Grants),
		FText::AsNumber(Damage.ExecutedCount), FText::AsNumber(Downed.ActiveCount));
}

void AKnightAbilityYardHUD::UnbindKnight()
{
	if (AbilitySource.IsValid())
	{
		AbilitySource->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetHealthAttribute())
			.Remove(HealthHandle);
		AbilitySource->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetMaxHealthAttribute())
			.Remove(MaxHealthHandle);
		AbilitySource->OnGameplayCueProcessed.Remove(CueHandle);
		AbilitySource->OnAbilityGrantsChanged.Remove(GrantsHandle);
	}
	if (LifeSource.IsValid())
	{
		LifeSource->OnLifeStateChanged.Remove(LifeHandle);
	}
	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	LifeHandle.Reset();
	CueHandle.Reset();
	GrantsHandle.Reset();
	AbilitySource.Reset();
	LifeSource.Reset();
}

void AKnightAbilityYardHUD::DrawHUD()
{
	if (Canvas == nullptr)
	{
		Super::DrawHUD();
		return;
	}
	const bool bCompact = Canvas->SizeY < 620 || Canvas->SizeX < 850;
	if (bCompact)
	{
		AHUD::DrawHUD();
	}
	else
	{
		Super::DrawHUD();
	}
	const float Width = FMath::Min(740.0f, static_cast<float>(Canvas->SizeX) - 40.0f);
	const float Top = bCompact ? 20.0f : FMath::Max(310.0f, static_cast<float>(Canvas->SizeY) - 270.0f);
	TArray<FText> Lines =
	{
		NSLOCTEXT("KnightAbilityYard", "Title", "THREE DREAMING KNIGHTS - Ability yard"),
		FText::Format(NSLOCTEXT("KnightAbilityYard", "LifeHealth", "{0} | {1}"), HealthStatus, LifeStatus),
		DummyStatus,
		NSLOCTEXT("KnightAbilityYard", "Pulse", "1. Approach the white dummy; press T / pad RT to damage it."),
		NSLOCTEXT("KnightAbilityYard", "Stations", "2. Press F / pad LB near DAMAGE, then DOWN COMPANION station."),
		NSLOCTEXT("KnightAbilityYard", "Revive", "3. Approach the downed Knight; HOLD E / pad X until revive completes."),
		ObjectiveStatus,
		bCompleted ? NSLOCTEXT("KnightAbilityYard", "Won", "ABILITY DRILL COMPLETE - R / pad Y restarts")
			: AbilityCommandStatus.IsEmpty() ? CueStatus : AbilityCommandStatus
	};
	if (bCompact)
	{
		Lines.Insert(FText::Format(NSLOCTEXT("KnightAbilityYard", "Controlled", "Controlled: {0}"),
			GetDisplayedKnightName()), 1);
		Lines.Insert(NSLOCTEXT("KnightAbilityYard", "CompactControls",
			"WASD / left stick move | Mouse / right stick look | Space / A jump"), 2);
		Lines.Insert(NSLOCTEXT("KnightAbilityYard", "CompactSwitch",
			"Tab / 1-3 / RB switch | R / Y retry | T / RT pulse | F / LB station | hold E / X revive"), 3);
		Lines.Add(IsDisplayingCompletion() ? NSLOCTEXT("KnightAbilityYard", "TrainingWon", "Training drill complete")
			: NSLOCTEXT("KnightAbilityYard", "TrainingGoal", "Training: walk all 3 markers and control all 3 Knights."));
	}
	const float Scale = bCompact ? FMath::Clamp(Width / 760.0f, 0.5f, 0.9f) : 1.0f;
	const float LineHeight = bCompact ? 22.0f : 28.0f;
	DrawRect(FLinearColor(0.035f, 0.025f, 0.05f, 0.9f), 20.0f, Top, Width,
		LineHeight * Lines.Num() + 24.0f);
	float Y = Top + 12.0f;
	const FLinearColor Accent = bCompleted ? FLinearColor::Green : FLinearColor(1.0f, 0.85f, 0.5f);
	for (const FText& Line : Lines)
	{
		DrawText(Line.ToString(), Accent, 34.0f, Y, GEngine->GetSmallFont(), Scale);
		Y += LineHeight;
	}
}
