#include "Campaign/GolemCampaignPlayerController.h"

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Campaign/GolemImpactCameraShake.h"
#include "Camera/GolemCameraModeComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Party/GolemPartyState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Settings/GolemGameUserSettings.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Story/GolemStoryState.h"

void AGolemCampaignPlayerController::UnbindHudSources()
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

void AGolemCampaignPlayerController::BindHudSources()
{
	UnbindHudSources();
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	if (State == nullptr || State->GetPartyState() == nullptr)
	{
		return;
	}
	for (const FGolemRosterEntry& Entry : State->GetPartyState()->GetRoster())
	{
		AGolemCharacter* Member = Entry.Golem;
		if (!IsValid(Member))
		{
			continue;
		}
		UGolemAbilitySystemComponent* ASC = Member->GetGolemAbilitySystem();
		for (const FGameplayAttribute Attribute : {UGolemAttributeSet::GetHealthAttribute(),
			UGolemAttributeSet::GetMaxHealthAttribute(), UGolemAttributeSet::GetStaminaAttribute(),
			UGolemAttributeSet::GetRuneEnergyAttribute()})
		{
			const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddWeakLambda(this, [this](const FOnAttributeChangeData&)
				{
					RefreshHud();
				});
			const TWeakObjectPtr<UGolemAbilitySystemComponent> WeakASC(ASC);
			HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakASC, Attribute, Handle]()
			{
				if (WeakASC.IsValid())
				{
					WeakASC->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(Handle);
				}
			}));
		}
		UGolemLifeComponent* Life = Member->GetLifeComponent();
		const FDelegateHandle LifeSubscription = Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EGolemLifeState, EGolemLifeState)
			{
				RefreshHud();
			});
		const TWeakObjectPtr<UGolemLifeComponent> WeakLife(Life);
		HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakLife, LifeSubscription]()
		{
			if (WeakLife.IsValid())
			{
				WeakLife->OnLifeStateChanged.Remove(LifeSubscription);
			}
		}));
		if (UGolemSkillRuntimeComponent* Runtime = Member->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			const FDelegateHandle Handle = Runtime->OnSkillStateChanged.AddWeakLambda(this,
				[this](const FGolemSkillRuntimeState&)
				{
					RefreshHud();
				});
			const TWeakObjectPtr<UGolemSkillRuntimeComponent> WeakRuntime(Runtime);
			HudUnbind.Add(FSimpleDelegate::CreateLambda([WeakRuntime, Handle]()
			{
				if (WeakRuntime.IsValid())
				{
					WeakRuntime->OnSkillStateChanged.Remove(Handle);
				}
			}));
		}
		UGolemCombatComponent* Combat = Member->GetCombatComponent();
		const FDelegateHandle CombatHandle = Combat->OnCombatSnapshotChanged.AddWeakLambda(this,
			[this, WeakMember = TWeakObjectPtr<AGolemCharacter>(Member)](const FGolemCombatSnapshot& Snapshot)
			{
				HandleCombatHud(WeakMember.Get(), Snapshot);
			});
		const TWeakObjectPtr<UGolemCombatComponent> WeakCombat(Combat);
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

void AGolemCampaignPlayerController::RefreshHud()
{
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	if (State == nullptr || State->GetPartyState() == nullptr)
	{
		return;
	}
	Hud.PartyLines.Reset();
	Hud.Cooldowns.Reset();
	for (const FGolemRosterEntry& Entry : State->GetPartyState()->GetRoster())
	{
		const AGolemCharacter* Member = Entry.Golem;
		if (!IsValid(Member) || Member->GetGolemAttributeSet() == nullptr)
		{
			continue;
		}
		const UGolemAttributeSet* Attributes = Member->GetGolemAttributeSet();
		UGolemSkillRuntimeComponent* Runtime = Member->FindComponentByClass<UGolemSkillRuntimeComponent>();
		const FGolemSkillRuntimeState Skill = Runtime != nullptr ? Runtime->GetSkillState() : FGolemSkillRuntimeState();
		const FText Life = Member->GetLifeComponent()->IsDowned()
			? NSLOCTEXT("GolemCampaignHUD", "Downed", "DOWNED — revive nearby [E / X]")
			: Member->GetLifeComponent()->IsDead() ? NSLOCTEXT("GolemCampaignHUD", "Dead", "DEAD — retry checkpoint")
			: NSLOCTEXT("GolemCampaignHUD", "Alive", "Alive");
		Hud.PartyLines.Add(FText::Format(NSLOCTEXT("GolemCampaignHUD", "PartyLine",
			"{0} | HP {1}/{2} | Shield {3} | Debt {4} | Stamina {5} | Resource {6} | {7}"),
			Member->GetGolemDisplayName(), FText::AsNumber(FMath::RoundToInt(Attributes->GetHealth())),
			FText::AsNumber(FMath::RoundToInt(Attributes->GetMaxHealth())), FText::AsNumber(FMath::RoundToInt(Skill.Shield)),
			FText::AsNumber(FMath::RoundToInt(Skill.DeferredDamage)),
			FText::AsNumber(FMath::RoundToInt(Attributes->GetStamina())),
			FText::AsNumber(FMath::RoundToInt(Attributes->GetRuneEnergy())), Life));
		if (Member == GetControlledGolem())
		{
			for (int32 Index = 0; Index < Skill.CooldownEnds.Num(); ++Index)
			{
				FGolemCampaignCooldownView& Cooldown = Hud.Cooldowns.AddDefaulted_GetRef();
				Cooldown.Label = FText::Format(NSLOCTEXT("GolemCampaignHUD", "SkillSlot", "A{0}"), FText::AsNumber(Index + 1));
				Cooldown.EndTime = Skill.CooldownEnds[Index];
			}
		}
	}
	Hud.Tutorial = BuildTutorial();
	Hud.Subtitle = State->Dialogue->GetSnapshot().bActive ? State->Dialogue->GetSnapshot().Text : FText();
	UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
	Hud.bSubtitleBackground = Settings == nullptr || Settings->bSubtitleBackground;
	Hud.SubtitleScale = Settings != nullptr ? Settings->SubtitleScale : 1.0f;
	Hud.TextScale = Settings != nullptr ? Settings->TextScale : 1.0f;
	Hud.Companion = NSLOCTEXT("GolemCampaignHUD", "Companion", "Companions: N follow | Z hold | B focus | K regroup (gamepad: cycle command)");
	UpdateInteractionHint();
	UGolemCombatComponent* TargetCombat = GetControlledGolem() != nullptr
		&& IsValid(GetControlledGolem()->GetCombatComponent()->GetLockedTarget())
		? GetControlledGolem()->GetCombatComponent()->GetLockedTarget()->FindComponentByClass<UGolemCombatComponent>() : nullptr;
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
				[this](const FGolemCombatSnapshot&)
				{
					RefreshHud();
				});
		}
	}
	Hud.Target = FText();
	if (TargetCombat != nullptr)
	{
		const FGolemCombatSnapshot& Target = TargetCombat->GetSnapshot();
		const FText Phase = Target.bStaggered ? NSLOCTEXT("GolemCampaignHUD", "Stagger", "STAGGERED — finisher opportunity")
			: Target.AttackPhase == 1 ? NSLOCTEXT("GolemCampaignHUD", "Windup", "WINDUP — prepare defense")
			: Target.AttackPhase == 2 ? NSLOCTEXT("GolemCampaignHUD", "Active", "ACTIVE — weapon contact window")
			: Target.AttackPhase == 3 ? NSLOCTEXT("GolemCampaignHUD", "Recovery", "RECOVERY — attack opportunity")
			: NSLOCTEXT("GolemCampaignHUD", "Ready", "Ready");
		Hud.Target = FText::Format(NSLOCTEXT("GolemCampaignHUD", "Target", "Locked target | Poise {0} | {1}"),
			FText::AsNumber(FMath::RoundToInt(Target.Poise)), Phase);
	}
}

void AGolemCampaignPlayerController::HandleCombatHud(AGolemCharacter* Source, const FGolemCombatSnapshot& Snapshot)
{
	if (Source == GetControlledGolem() && Source != nullptr)
	{
		const bool bSameGolem = FeedbackGolem == Source->GetGolemEntityId();
		const double Now = GetWorld()->GetGameState() != nullptr
			? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const bool bFresh = Now - Snapshot.LastOutcomeServerTime >= -0.1 && Now - Snapshot.LastOutcomeServerTime <= 0.75;
		bool bFeedback = false;
		for (const EGolemCombatOutcome Outcome : {EGolemCombatOutcome::LightHit, EGolemCombatOutcome::HeavyHit,
			EGolemCombatOutcome::Blocked, EGolemCombatOutcome::Parried, EGolemCombatOutcome::Dodged,
			EGolemCombatOutcome::Finisher})
		{
			const int32 Index = static_cast<int32>(Outcome);
			if (bSameGolem && bFresh && FeedbackCounts.IsValidIndex(Index) && Snapshot.GetOutcomeCount(Outcome) > FeedbackCounts[Index])
			{
				bFeedback = true;
				if (Outcome == EGolemCombatOutcome::Blocked || Outcome == EGolemCombatOutcome::Parried || Outcome == EGolemCombatOutcome::Dodged)
				{
					if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get())
					{
						Settings->ConfirmTutorial(TEXT("Defense"));
					}
				}
				Hud.CombatFeedback = Outcome == EGolemCombatOutcome::Parried ? NSLOCTEXT("GolemFeedback", "Parry", "PARRY confirmed")
					: Outcome == EGolemCombatOutcome::Blocked ? NSLOCTEXT("GolemFeedback", "Block", "BLOCK confirmed")
					: Outcome == EGolemCombatOutcome::Dodged ? NSLOCTEXT("GolemFeedback", "Dodge", "DODGE confirmed")
					: NSLOCTEXT("GolemFeedback", "Contact", "Weapon contact confirmed");
			}
		}
		FeedbackGolem = Source->GetGolemEntityId();
		FeedbackCounts = Snapshot.OutcomeCounts;
		if (bFeedback && PlayerCameraManager != nullptr && !GetWorld()->IsPaused())
		{
			Hud.CombatFeedbackEnd = GetWorld()->GetGameState() != nullptr
				? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + 1.5 : GetWorld()->GetTimeSeconds() + 1.5;
			PlayConfirmedImpactTone();
			const float Scale = Source->GetCameraModeComponent()->GetLocalFeedbackScale();
			if (Scale > 0.0f)
			{
				FeedbackShake = PlayerCameraManager->StartCameraShake(UGolemImpactCameraShake::StaticClass(), Scale);
			}
		}
	}
	RefreshHud();
}

FText AGolemCampaignPlayerController::BuildTutorial()
{
	UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
	if (Settings == nullptr)
	{
		return FText();
	}
	if (!Settings->HasSeenTutorial(TEXT("Move")))
	{
		return FText::Format(NSLOCTEXT("GolemTutorial", "MoveKeys", "Move: {0}/{1}/{2}/{3} or left stick. Look: mouse / right stick."),
			Settings->GetActionKey(TEXT("Forward")).GetDisplayName(), Settings->GetActionKey(TEXT("Left")).GetDisplayName(),
			Settings->GetActionKey(TEXT("Backward")).GetDisplayName(), Settings->GetActionKey(TEXT("Right")).GetDisplayName());
	}
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	const FGolemRPGView RPGSnapshot = State != nullptr && State->GetRPGState() != nullptr
		? State->GetRPGState()->GetRPG()->GetView() : FGolemRPGView();
	const FGolemRPGGolemView* View = GetControlledGolem() != nullptr
		? RPGSnapshot.Golems.FindByPredicate([this](const FGolemRPGGolemView& Row)
		{
			return Row.GolemId == GetControlledGolem()->GetGolemEntityId();
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
		return NSLOCTEXT("GolemTutorial", "ForgeFirst", "Follow Boro's lesson, claim your hammers, then open I / Menu to equip one in Gear.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Defense")))
	{
		return FText::Format(NSLOCTEXT("GolemTutorial", "DefenseKeys", "Defend against an attack: block {0}, parry {1}, or dodge {2} (pad: LB / L3 / B)."),
			Settings->GetActionKey(TEXT("Block")).GetDisplayName(), Settings->GetActionKey(TEXT("Parry")).GetDisplayName(),
			Settings->GetActionKey(TEXT("Dodge")).GetDisplayName());
	}
	if (!Settings->HasSeenTutorial(TEXT("Companion")))
	{
		return NSLOCTEXT("GolemTutorial", "Companion", "Command companions: N follow, Z hold, B focus, K regroup. Pad: cycle companion command.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Build")))
	{
		return NSLOCTEXT("GolemTutorial", "Build", "Open I / Menu: Specialization and Talents show this Golem's confirmed build and available unlocks.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Rune")))
	{
		return NSLOCTEXT("GolemTutorial", "Rune", "When you find a rune, open I / Menu: Attach Rune to a hammer or its supported skill parent.");
	}
	if (!Settings->HasSeenTutorial(TEXT("Save")))
	{
		return NSLOCTEXT("GolemTutorial", "Save", "J: review quests. M: map. Esc / Menu: save at a safe boundary. Wait for Saved confirmation.");
	}
	return NSLOCTEXT("GolemTutorial", "Complete", "J: journal | M: map | I / Menu: inventory and build | Esc: campaign menu");
}
