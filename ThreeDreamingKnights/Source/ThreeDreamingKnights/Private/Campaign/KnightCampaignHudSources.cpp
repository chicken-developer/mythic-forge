#include "Campaign/KnightCampaignPlayerController.h"

#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Campaign/KnightImpactCameraShake.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Party/KnightPartyState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/KnightGameUserSettings.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Story/KnightStoryState.h"

void AKnightCampaignPlayerController::UnbindHudSources()
{
	if (TargetHudSource.IsValid())
	{
		TargetHudSource->OnCombatSnapshotChanged.Remove(TargetHudHandle);
	}
	TargetHudSource.Reset();
	TargetHudHandle.Reset();
	for (FSimpleDelegate& Release : HudUnbind)
	{
		Release.ExecuteIfBound();
	}
	HudUnbind.Reset();
}

void AKnightCampaignPlayerController::BindHudSources()
{
	UnbindHudSources();
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State == nullptr || State->GetPartyState() == nullptr)
	{
		return;
	}
	for (const FKnightRosterEntry& Entry : State->GetPartyState()->GetRoster())
	{
		AKnightCharacter* Member = Entry.Knight;
		if (!IsValid(Member))
		{
			continue;
		}
		UKnightAbilitySystemComponent* ASC = Member->GetKnightAbilitySystem();
		for (const FGameplayAttribute Attribute : {UKnightAttributeSet::GetHealthAttribute(),
			UKnightAttributeSet::GetMaxHealthAttribute(), UKnightAttributeSet::GetStaminaAttribute(),
			UKnightAttributeSet::GetRuneEnergyAttribute()})
		{
			const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddWeakLambda(this, [this](const FOnAttributeChangeData&)
				{
					RefreshHud();
				});
			const TWeakObjectPtr<UKnightAbilitySystemComponent> WeakASC(ASC);
			HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakASC, Attribute, Handle]()
			{
				if (WeakASC.IsValid())
				{
					WeakASC->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(Handle);
				}
			}));
		}
		UKnightLifeComponent* Life = Member->GetLifeComponent();
		const FDelegateHandle LifeSubscription = Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EKnightLifeState, EKnightLifeState)
			{
				RefreshHud();
			});
		const TWeakObjectPtr<UKnightLifeComponent> WeakLife(Life);
		HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakLife, LifeSubscription]()
		{
			if (WeakLife.IsValid())
			{
				WeakLife->OnLifeStateChanged.Remove(LifeSubscription);
			}
		}));
		if (UKnightSkillRuntimeComponent* Runtime = Member->FindComponentByClass<UKnightSkillRuntimeComponent>())
		{
			const FDelegateHandle Handle = Runtime->OnSkillStateChanged.AddWeakLambda(this,
				[this](const FKnightSkillRuntimeState&)
				{
					RefreshHud();
				});
			const TWeakObjectPtr<UKnightSkillRuntimeComponent> WeakRuntime(Runtime);
			HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakRuntime, Handle]()
			{
				if (WeakRuntime.IsValid())
				{
					WeakRuntime->OnSkillStateChanged.Remove(Handle);
				}
			}));
		}
		UKnightCombatComponent* Combat = Member->GetCombatComponent();
		const FDelegateHandle CombatHandle = Combat->OnCombatSnapshotChanged.AddWeakLambda(this,
			[this, WeakMember = TWeakObjectPtr<AKnightCharacter>(Member)](const FKnightCombatSnapshot& Snapshot)
			{
				HandleCombatHud(WeakMember.Get(), Snapshot);
			});
		const TWeakObjectPtr<UKnightCombatComponent> WeakCombat(Combat);
		HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakCombat, CombatHandle]()
		{
			if (WeakCombat.IsValid())
			{
				WeakCombat->OnCombatSnapshotChanged.Remove(CombatHandle);
			}
		}));
	}
	RefreshHud();
}

void AKnightCampaignPlayerController::RefreshHud()
{
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State == nullptr || State->GetPartyState() == nullptr)
	{
		return;
	}
	Hud.PartyLines.Reset();
	Hud.Cooldowns.Reset();
	for (const FKnightRosterEntry& Entry : State->GetPartyState()->GetRoster())
	{
		const AKnightCharacter* Member = Entry.Knight;
		if (!IsValid(Member) || Member->GetKnightAttributeSet() == nullptr)
		{
			continue;
		}
		const UKnightAttributeSet* Attributes = Member->GetKnightAttributeSet();
		UKnightSkillRuntimeComponent* Runtime = Member->FindComponentByClass<UKnightSkillRuntimeComponent>();
		const FKnightSkillRuntimeState Skill = Runtime != nullptr ? Runtime->GetSkillState() : FKnightSkillRuntimeState();
		const FText Life = Member->GetLifeComponent()->IsDowned()
			? NSLOCTEXT("KnightCampaignHUD", "Downed", "DOWNED — revive nearby [E / X]")
			: Member->GetLifeComponent()->IsDead() ? NSLOCTEXT("KnightCampaignHUD", "Dead", "DEAD — retry checkpoint")
			: NSLOCTEXT("KnightCampaignHUD", "Alive", "Alive");
		Hud.PartyLines.Add(FText::Format(NSLOCTEXT("KnightCampaignHUD", "PartyLine",
			"{0} | HP {1}/{2} | Shield {3} | Debt {4} | Stamina {5} | Resource {6} | {7}"),
			Member->GetKnightDisplayName(), FText::AsNumber(FMath::RoundToInt(Attributes->GetHealth())),
			FText::AsNumber(FMath::RoundToInt(Attributes->GetMaxHealth())), FText::AsNumber(FMath::RoundToInt(Skill.Shield)),
			FText::AsNumber(FMath::RoundToInt(Skill.DeferredDamage)),
			FText::AsNumber(FMath::RoundToInt(Attributes->GetStamina())),
			FText::AsNumber(FMath::RoundToInt(Attributes->GetRuneEnergy())), Life));
		if (Member == GetControlledKnight())
		{
			for (int32 Index = 0; Index < Skill.CooldownEnds.Num(); ++Index)
			{
				FKnightCampaignCooldownView& Cooldown = Hud.Cooldowns.AddDefaulted_GetRef();
				Cooldown.Label = FText::Format(NSLOCTEXT("KnightCampaignHUD", "SkillSlot", "A{0}"), FText::AsNumber(Index + 1));
				Cooldown.EndTime = Skill.CooldownEnds[Index];
			}
		}
	}
	Hud.Tutorial = BuildTutorial();
	Hud.Subtitle = State->Dialogue->GetSnapshot().bActive ? State->Dialogue->GetSnapshot().Text : FText();
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	Hud.bSubtitleBackground = Settings == nullptr || Settings->bSubtitleBackground;
	Hud.SubtitleScale = Settings != nullptr ? Settings->SubtitleScale : 1.0f;
	Hud.TextScale = Settings != nullptr ? Settings->TextScale : 1.0f;
	Hud.Companion = NSLOCTEXT("KnightCampaignHUD", "Companion", "Companions: N follow | Z hold | B focus | K regroup (gamepad: cycle command)");
	UpdateInteractionHint();
	UKnightCombatComponent* TargetCombat = GetControlledKnight() != nullptr
		&& IsValid(GetControlledKnight()->GetCombatComponent()->GetLockedTarget())
		? GetControlledKnight()->GetCombatComponent()->GetLockedTarget()->FindComponentByClass<UKnightCombatComponent>() : nullptr;
	if (TargetCombat != TargetHudSource.Get())
	{
		if (TargetHudSource.IsValid())
		{
			TargetHudSource->OnCombatSnapshotChanged.Remove(TargetHudHandle);
		}
		TargetHudSource = TargetCombat;
		TargetHudHandle.Reset();
		if (TargetCombat != nullptr)
		{
			TargetHudHandle = TargetCombat->OnCombatSnapshotChanged.AddWeakLambda(this,
				[this](const FKnightCombatSnapshot&)
				{
					RefreshHud();
				});
		}
	}
	Hud.Target = FText();
	if (TargetCombat != nullptr)
	{
		const FKnightCombatSnapshot& Target = TargetCombat->GetSnapshot();
		const FText Phase = Target.bStaggered ? NSLOCTEXT("KnightCampaignHUD", "Stagger", "STAGGERED — finisher opportunity")
			: Target.AttackPhase == 1 ? NSLOCTEXT("KnightCampaignHUD", "Windup", "WINDUP — prepare defense")
			: Target.AttackPhase == 2 ? NSLOCTEXT("KnightCampaignHUD", "Active", "ACTIVE — weapon contact window")
			: Target.AttackPhase == 3 ? NSLOCTEXT("KnightCampaignHUD", "Recovery", "RECOVERY — attack opportunity")
			: NSLOCTEXT("KnightCampaignHUD", "Ready", "Ready");
		Hud.Target = FText::Format(NSLOCTEXT("KnightCampaignHUD", "Target", "Locked target | Poise {0} | {1}"),
			FText::AsNumber(FMath::RoundToInt(Target.Poise)), Phase);
	}
}

void AKnightCampaignPlayerController::HandleCombatHud(AKnightCharacter* Source, const FKnightCombatSnapshot& Snapshot)
{
	if (Source == GetControlledKnight() && Source != nullptr)
	{
		const bool bSameKnight = FeedbackKnight == Source->GetKnightEntityId();
		const double Now = GetWorld()->GetGameState() != nullptr
			? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const bool bFresh = Now - Snapshot.LastOutcomeServerTime >= -0.1 && Now - Snapshot.LastOutcomeServerTime <= 0.75;
		bool bFeedback = false;
		for (const EKnightCombatOutcome Outcome : {EKnightCombatOutcome::LightHit, EKnightCombatOutcome::HeavyHit,
			EKnightCombatOutcome::Blocked, EKnightCombatOutcome::Parried, EKnightCombatOutcome::Dodged,
			EKnightCombatOutcome::Finisher})
		{
			const int32 Index = static_cast<int32>(Outcome);
			if (bSameKnight && bFresh && FeedbackCounts.IsValidIndex(Index) && Snapshot.GetOutcomeCount(Outcome) > FeedbackCounts[Index])
			{
				bFeedback = true;
				if (Outcome == EKnightCombatOutcome::Blocked || Outcome == EKnightCombatOutcome::Parried || Outcome == EKnightCombatOutcome::Dodged)
				{
					if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get())
					{
						Settings->ConfirmTutorial(TEXT("Defense"));
					}
				}
				Hud.CombatFeedback = Outcome == EKnightCombatOutcome::Parried ? NSLOCTEXT("KnightFeedback", "Parry", "PARRY confirmed")
					: Outcome == EKnightCombatOutcome::Blocked ? NSLOCTEXT("KnightFeedback", "Block", "BLOCK confirmed")
					: Outcome == EKnightCombatOutcome::Dodged ? NSLOCTEXT("KnightFeedback", "Dodge", "DODGE confirmed")
					: NSLOCTEXT("KnightFeedback", "Contact", "Weapon contact confirmed");
			}
		}
		FeedbackKnight = Source->GetKnightEntityId();
		FeedbackCounts = Snapshot.OutcomeCounts;
		if (bFeedback && PlayerCameraManager != nullptr && !GetWorld()->IsPaused())
		{
			Hud.CombatFeedbackEnd = GetWorld()->GetGameState() != nullptr
				? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + 1.5 : GetWorld()->GetTimeSeconds() + 1.5;
			PlayConfirmedImpactTone();
			const float Scale = Source->GetCameraModeComponent()->GetLocalFeedbackScale();
			if (Scale > 0.0f)
			{
				FeedbackShake = PlayerCameraManager->StartCameraShake(UKnightImpactCameraShake::StaticClass(), Scale);
			}
		}
	}
	RefreshHud();
}

FText AKnightCampaignPlayerController::BuildTutorial()
{
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	if (Settings == nullptr)
	{
		return FText();
	}
	if (!Settings->HasSeenTutorial(TEXT("Move")))
	{
		return FText::Format(NSLOCTEXT("KnightTutorial", "MoveKeys", "Move: {0}/{1}/{2}/{3} or left stick. Look: mouse / right stick."),
			Settings->GetActionKey(TEXT("Forward")).GetDisplayName(), Settings->GetActionKey(TEXT("Left")).GetDisplayName(),
			Settings->GetActionKey(TEXT("Backward")).GetDisplayName(), Settings->GetActionKey(TEXT("Right")).GetDisplayName());
	}
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	const FKnightRPGView RPGSnapshot = State != nullptr && State->GetRPGState() != nullptr
		? State->GetRPGState()->GetRPG()->GetView() : FKnightRPGView();
	const FKnightRPGKnightView* View = GetControlledKnight() != nullptr
		? RPGSnapshot.Knights.FindByPredicate([this](const FKnightRPGKnightView& Row)
		{
			return Row.KnightId == GetControlledKnight()->GetKnightEntityId();
		}) : nullptr;
	if (View != nullptr)
	{
		if (!View->Build.SpecId.IsNone())
		{
			Settings->ConfirmTutorial(TEXT("Build"));
		}
		if (!View->Assignments.IsEmpty())
		{
			Settings->ConfirmTutorial(TEXT("Equip"));
		}
		if (View->WeaponRuneInstanceId.IsValid() || View->SkillRuneInstanceId.IsValid())
		{
			Settings->ConfirmTutorial(TEXT("Rune"));
		}
	}
	if (!Settings->HasSeenTutorial(TEXT("Equip")))
	{
		return NSLOCTEXT("KnightTutorial", "ForgeFirst", "Follow Boro's lesson, claim your hammers, then open I / Menu to equip one in Gear.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Defense")))
	{
		return FText::Format(NSLOCTEXT("KnightTutorial", "DefenseKeys", "Defend against an attack: block {0}, parry {1}, or dodge {2} (pad: LB / L3 / B)."),
			Settings->GetActionKey(TEXT("Block")).GetDisplayName(), Settings->GetActionKey(TEXT("Parry")).GetDisplayName(),
			Settings->GetActionKey(TEXT("Dodge")).GetDisplayName());
	}
	if (!Settings->HasSeenTutorial(TEXT("Companion")))
	{
		return NSLOCTEXT("KnightTutorial", "Companion", "Command companions: N follow, Z hold, B focus, K regroup. Pad: cycle companion command.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Build")))
	{
		return NSLOCTEXT("KnightTutorial", "Build", "Open I / Menu: Specialization and Talents show this Knight's confirmed build and available unlocks.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Rune")))
	{
		return NSLOCTEXT("KnightTutorial", "Rune", "When you find a rune, open I / Menu: Attach Rune to a hammer or its supported skill parent.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Save")))
	{
		return NSLOCTEXT("KnightTutorial", "Save", "J: review quests. M: map. Esc / Menu: save at a safe boundary. Wait for Saved confirmation.");
	}
	return NSLOCTEXT("KnightTutorial", "Complete", "J: journal | M: map | I / Menu: inventory and build | Esc: campaign menu");
}
