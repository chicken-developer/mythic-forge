#include "RPGLoop/GolemRPGUIPlayerController.h"

#include "Character/GolemCharacter.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemRuneDefinition.h"
#include "GearLab/GolemGearLabState.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Models/GolemGearPanelTypes.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGearPresentation.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Widgets/GolemRPGPanel.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void AGolemRPGUIPlayerController::RequestGearFixture(FName Action, FGuid RequestId)
{
	OnGearFixtureResult.Broadcast(RequestId, FGolemResult::Failure(EGolemError::InvalidState, Action,
		NSLOCTEXT("GolemGearUI", "FixtureUnavailable", "Gear fixtures are available only in the isolated GearLab map.")));
}

AGolemCharacter* AGolemRPGUIPlayerController::FindGearGolem() const
{
	return GearViewedGolem.IsValid() && GearViewedGolem->GetGolemEntityId() == GearSelectedGolemId
		? GearViewedGolem.Get() : GetControlledGolem();
}

void AGolemRPGUIPlayerController::BindGearSources()
{
	if (bEndingUIPlay || !RPGState.IsValid() || !RPGState->GetRPG()->GetView().IsItemizationEnabled() || GetWorld() == nullptr)
	{
		return;
	}
	const bool bGearDemo = RPGState->GetRPG()->GetView().bGearDemo;
	if (bGearDemo && !GearLabState.IsValid())
	{
		for (TActorIterator<AGolemGearLabState> It(GetWorld()); It; ++It)
		{
			GearLabState = *It;
			GearStateHandle = It->OnGearDemoChanged.AddWeakLambda(this, [this](const FGolemGearDemoSnapshot& Demo)
			{
				RefreshRPGPresentation();
			});
			break;
		}
	}
	if (!GearViewedGolem.IsValid() || GearViewedGolem->GetGolemEntityId() != GearSelectedGolemId)
	{
		UnbindGearViewedGolem();
		for (TActorIterator<AGolemCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetGolemEntityId() != GearSelectedGolemId)
			{
				continue;
			}
			GearViewedGolem = *It;
			GearViewedASC = It->GetGolemAbilitySystem();
			GearViewedRuntime = It->FindComponentByClass<UGolemSkillRuntimeComponent>();
			GearViewedCombat = It->GetCombatComponent();
			GearLifeHandle = It->GetLifeComponent()->OnLifeStateChanged.AddWeakLambda(this,
				[this](EGolemLifeState Previous, EGolemLifeState Current)
				{
					RefreshRPGPresentation();
				});
			GearProjectionHandle = GearViewedASC->OnSourceProjectionChanged.AddWeakLambda(this,
				[this](const FGolemSourceProjection& Projection)
				{
					RefreshRPGPresentation();
				});
			const TArray<FGameplayAttribute> Attributes{
				UGolemAttributeSet::GetHealthAttribute(), UGolemAttributeSet::GetMaxHealthAttribute(),
				UGolemAttributeSet::GetStaminaAttribute(), UGolemAttributeSet::GetMaxStaminaAttribute(),
				UGolemAttributeSet::GetRuneEnergyAttribute(), UGolemAttributeSet::GetMaxRuneEnergyAttribute(),
				UGolemAttributeSet::GetAttackPowerAttribute(), UGolemAttributeSet::GetDefenseAttribute(),
				UGolemAttributeSet::GetCritChanceAttribute(), UGolemAttributeSet::GetHasteAttribute(),
				UGolemAttributeSet::GetCooldownRecoveryAttribute(), UGolemAttributeSet::GetMasteryBonusAttribute(),
				UGolemAttributeSet::GetVersatilityAttribute()};
			for (const FGameplayAttribute& Attribute : Attributes)
			{
				GearAttributeHandles.Emplace(Attribute, GearViewedASC->GetGameplayAttributeValueChangeDelegate(Attribute)
					.AddWeakLambda(this, [this](const FOnAttributeChangeData& Change)
					{
						if (GearViewedASC.IsValid() && !GearViewedASC->IsMutationFenced())
						{
							RefreshRPGPresentation();
						}
					}));
			}
			if (GearViewedRuntime.IsValid())
			{
				GearRuntimeHandle = GearViewedRuntime->OnSkillStateChanged.AddWeakLambda(this,
					[this](const FGolemSkillRuntimeState& State)
					{
						RefreshRPGPresentation();
					});
			}
			if (GearViewedCombat.IsValid())
			{
				GearCombatHandle = GearViewedCombat->OnCombatSnapshotChanged.AddWeakLambda(this,
					[this](const FGolemCombatSnapshot& State)
					{
						RefreshRPGPresentation();
					});
			}
			break;
		}
	}
	if (((bGearDemo && !GearLabState.IsValid()) || !GearViewedGolem.IsValid()) && ++GearBindingAttempts <= 100)
	{
		GetWorldTimerManager().SetTimer(GearReadinessTimer, this, &ThisClass::RefreshRPGPresentation, 0.1f, false);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(GearReadinessTimer);
	}
}

void AGolemRPGUIPlayerController::UnbindGearViewedGolem()
{
	if (GearViewedGolem.IsValid())
	{
		GearViewedGolem->GetLifeComponent()->OnLifeStateChanged.Remove(GearLifeHandle);
	}
	if (GearViewedASC.IsValid())
	{
		GearViewedASC->OnSourceProjectionChanged.Remove(GearProjectionHandle);
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Binding : GearAttributeHandles)
		{
			GearViewedASC->GetGameplayAttributeValueChangeDelegate(Binding.Key).Remove(Binding.Value);
		}
	}
	if (GearViewedRuntime.IsValid())
	{
		GearViewedRuntime->OnSkillStateChanged.Remove(GearRuntimeHandle);
	}
	if (GearViewedCombat.IsValid())
	{
		GearViewedCombat->OnCombatSnapshotChanged.Remove(GearCombatHandle);
	}
	GearViewedGolem.Reset();
	GearViewedASC.Reset();
	GearViewedRuntime.Reset();
	GearViewedCombat.Reset();
	GearAttributeHandles.Reset();
	GearLifeHandle.Reset();
	GearProjectionHandle.Reset();
	GearRuntimeHandle.Reset();
	GearCombatHandle.Reset();
}

TArray<FGolemGearQuickUseRow> AGolemRPGUIPlayerController::GetQuickUseReadModel() const
{
	AGolemCharacter* Golem = GetControlledGolem();
	return RPGState.IsValid() && Golem != nullptr ? GolemRPGGearPresentation::QuickUses(
		RPGState->GetRPG()->GetGolemView(Golem->GetGolemEntityId()), RPGState->GetInventory()->GetView(), Golem)
		: TArray<FGolemGearQuickUseRow>();
}

void AGolemRPGUIPlayerController::HandleGearFixtureResult(const FGuid& RequestId, const FGolemResult& Result)
{
	if (bEndingUIPlay)
	{
		return;
	}
	LastCompletedRequestId = RequestId;
	if (PendingRequestId == RequestId)
	{
		PendingRequestId.Invalidate();
	}
	Feedback = Result.IsSuccess() ? NSLOCTEXT("GolemGearUI", "FixtureConfirmed", "Fixture action confirmed.") : Result.Message;
	ReconcileClosedPanelReceipt(RequestId);
	RefreshRPGPresentation();
}

void AGolemRPGUIPlayerController::HandleGearPreview(const FGolemGearPanelIntent& Intent)
{
	if (RPGPanel != nullptr && RPGState.IsValid())
	{
		AGolemCharacter* SourceGolem = nullptr;
		AGolemCharacter* TargetGolem = nullptr;
		if (Intent.Action == EGolemGearPanelAction::Transfer)
		{
			const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
			const FGolemItemInstance* Item = Inventory.Items.FindByPredicate([&Intent](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == Intent.InstanceId;
			});
			for (TActorIterator<AGolemCharacter> It(GetWorld()); It; ++It)
			{
				if (Item != nullptr && It->GetGolemEntityId() == Item->AssignedGolemId)
				{
					SourceGolem = *It;
				}
				if (It->GetGolemEntityId() == Intent.TargetGolemId)
				{
					TargetGolem = *It;
				}
			}
		}
		RPGPanel->SetGearCompare(GolemRPGGearPresentation::Compare(RPGState->GetRPG(),
			RPGState->GetInventory()->GetView(), FindGearGolem(), Intent, SourceGolem, TargetGolem));
	}
}

void AGolemRPGUIPlayerController::HandleGearCommand(const FGolemGearPanelIntent& Intent)
{
	if (bEndingUIPlay || RPGPanel == nullptr || !RPGPanel->IsActivated() || PendingRequestId.IsValid() || !RPGState.IsValid())
	{
		return;
	}
	if (Intent.Action == EGolemGearPanelAction::SelectGolem)
	{
		GearSelectedGolemId = Intent.GolemId;
		RefreshRPGPresentation();
		return;
	}
	if (Intent.Action == EGolemGearPanelAction::Fixture && Intent.FixtureAction != TEXT("Guard") &&
		Intent.FixtureAction != TEXT("Mend") && Intent.FixtureAction != TEXT("Strike") && Intent.FixtureAction != TEXT("ClaimReward"))
	{
		PendingRequestId = Intent.RequestId;
		RefreshRPGPresentation();
		RequestGearFixture(Intent.FixtureAction, Intent.RequestId);
		return;
	}
	EGolemRPGOperation Operation = EGolemRPGOperation::Equip;
	switch (Intent.Action)
	{
	case EGolemGearPanelAction::Equip: Operation = EGolemRPGOperation::Equip; break;
	case EGolemGearPanelAction::Unequip: Operation = EGolemRPGOperation::Unequip; break;
	case EGolemGearPanelAction::Transfer: Operation = EGolemRPGOperation::TransferEquipment; break;
	case EGolemGearPanelAction::SwapRings: Operation = EGolemRPGOperation::SwapRings; break;
	case EGolemGearPanelAction::AttachWeaponRune: Operation = EGolemRPGOperation::AttachWeaponRune; break;
	case EGolemGearPanelAction::DetachWeaponRune: Operation = EGolemRPGOperation::DetachWeaponRune; break;
	case EGolemGearPanelAction::AttachSkillRune:
	case EGolemGearPanelAction::DetachSkillRune: Operation = EGolemRPGOperation::ApplyBuild; break;
	case EGolemGearPanelAction::Upgrade: Operation = EGolemRPGOperation::UpgradeItem; break;
	case EGolemGearPanelAction::Buy: Operation = EGolemRPGOperation::BuyItem; break;
	case EGolemGearPanelAction::Sell: Operation = EGolemRPGOperation::SellItem; break;
	case EGolemGearPanelAction::BindConsumable: Operation = EGolemRPGOperation::BindConsumable; break;
	case EGolemGearPanelAction::Fixture: Operation = Intent.FixtureAction == TEXT("ClaimReward")
		? EGolemRPGOperation::ClaimReward : EGolemRPGOperation::ApplyGearPreset; break;
	default: return;
	}
	FGolemRPGCommand Command = CreateCurrentCommand(Operation);
	Command.RequestId = Intent.RequestId;
	Command.ExpectedGolemId = Intent.GolemId;
	Command.ExpectedGolemRevision = Intent.ExpectedGolemRevision;
	Command.ExpectedInventoryRevision = Intent.ExpectedInventoryRevision;
	if (Operation == EGolemRPGOperation::Equip || Operation == EGolemRPGOperation::Unequip ||
		Operation == EGolemRPGOperation::TransferEquipment)
	{
		Command.SlotTag = FGameplayTag::RequestGameplayTag(Intent.SlotId, false);
	}
	if (Operation == EGolemRPGOperation::Equip || Operation == EGolemRPGOperation::Unequip || Operation == EGolemRPGOperation::TransferEquipment ||
		Operation == EGolemRPGOperation::AttachWeaponRune || Operation == EGolemRPGOperation::DetachWeaponRune ||
		Operation == EGolemRPGOperation::UpgradeItem || Operation == EGolemRPGOperation::SellItem)
	{
		Command.ItemInstanceId = Intent.InstanceId;
		Command.ExpectedItemRevision = Intent.ExpectedItemRevision;
	}
	if (Operation == EGolemRPGOperation::TransferEquipment)
	{
		Command.TargetGolemId = Intent.TargetGolemId;
		Command.ExpectedTargetGolemRevision = Intent.ExpectedTargetGolemRevision;
	}
	if (Operation == EGolemRPGOperation::AttachWeaponRune)
	{
		Command.WeaponRuneInstanceId = Intent.RuneInstanceId;
	}
	if (Operation == EGolemRPGOperation::BuyItem || Operation == EGolemRPGOperation::BindConsumable)
	{
		Command.ItemDefinitionId = Intent.DefinitionId;
	}
	if (Operation == EGolemRPGOperation::BindConsumable)
	{
		Command.QuickSlot = Intent.QuickSlot;
	}
	if (Operation == EGolemRPGOperation::SellItem)
	{
		Command.bConfirmEpicSale = Intent.bConfirmEpicSale;
	}
	if (Operation == EGolemRPGOperation::ApplyBuild)
	{
		const FGolemRPGGolemView Viewed = RPGState->GetRPG()->GetGolemView(Intent.GolemId);
		Command.ProposedBuild = Viewed.Build;
		Command.WeaponRuneInstanceId = Viewed.WeaponRuneInstanceId;
		if (Intent.Action == EGolemGearPanelAction::AttachSkillRune)
		{
			const FGolemGearItemRow* Rune = RPGPanel->GetSnapshot().Gear.Items.FindByPredicate([&Intent](const FGolemGearItemRow& Item)
			{
				return Item.InstanceId == Intent.InstanceId;
			});
			Command.ProposedBuild.SkillRune = Rune != nullptr ? Rune->RuneId : NAME_None;
			Command.SkillRuneInstanceId = Intent.InstanceId;
		}
		else
		{
			Command.ProposedBuild.SkillRune = NAME_None;
		}
	}
	if (Operation == EGolemRPGOperation::ApplyGearPreset)
	{
		Command.PresetId = Intent.FixtureAction;
	}
	if (Operation == EGolemRPGOperation::ClaimReward)
	{
		Command.RewardId = Intent.RewardId;
	}
	PendingRequestId = Command.RequestId;
	RefreshRPGPresentation();
	RequestRPGCommand(Command);
}
