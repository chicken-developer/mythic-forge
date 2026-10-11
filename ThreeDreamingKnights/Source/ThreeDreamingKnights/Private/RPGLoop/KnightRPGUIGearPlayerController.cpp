#include "RPGLoop/KnightRPGUIPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightRuneDefinition.h"
#include "GearLab/KnightGearLabState.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Models/KnightGearPanelTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGGearPresentation.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Widgets/KnightRPGPanel.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void AKnightRPGUIPlayerController::RequestGearFixture(FName Action, FGuid RequestId)
{
	OnGearFixtureResult.Broadcast(RequestId, FKnightResult::Failure(EKnightError::InvalidState, Action,
		NSLOCTEXT("KnightGearUI", "FixtureUnavailable", "Gear fixtures are available only in the isolated GearLab map.")));
}

AKnightCharacter* AKnightRPGUIPlayerController::FindGearKnight() const
{
	return GearViewedKnight.IsValid() && GearViewedKnight->GetKnightEntityId() == GearSelectedKnightId
		? GearViewedKnight.Get() : GetControlledKnight();
}

void AKnightRPGUIPlayerController::BindGearSources()
{
	if (bEndingUIPlay || !RPGState.IsValid() || !RPGState->GetRPG()->GetView().IsItemizationEnabled() || GetWorld() == nullptr)
	{
		return;
	}
	const bool bGearDemo = RPGState->GetRPG()->GetView().bGearDemo;
	if (bGearDemo && !GearLabState.IsValid())
	{
		for (TActorIterator<AKnightGearLabState> It(GetWorld()); It; ++It)
		{
			GearLabState = *It;
			GearStateHandle = It->OnGearDemoChanged.AddWeakLambda(this, [this](const FKnightGearDemoSnapshot& Demo)
			{
				RefreshRPGPresentation();
			});
			break;
		}
	}
	if (!GearViewedKnight.IsValid() || GearViewedKnight->GetKnightEntityId() != GearSelectedKnightId)
	{
		UnbindGearViewedKnight();
		for (TActorIterator<AKnightCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetKnightEntityId() != GearSelectedKnightId)
			{
				continue;
			}
			GearViewedKnight = *It;
			GearViewedASC = It->GetKnightAbilitySystem();
			GearViewedRuntime = It->FindComponentByClass<UKnightSkillRuntimeComponent>();
			GearViewedCombat = It->GetCombatComponent();
			GearLifeHandle = It->GetLifeComponent()->OnLifeStateChanged.AddWeakLambda(this,
				[this](EKnightLifeState Previous, EKnightLifeState Current)
				{
					RefreshRPGPresentation();
				});
			GearProjectionHandle = GearViewedASC->OnSourceProjectionChanged.AddWeakLambda(this,
				[this](const FKnightSourceProjection& Projection)
				{
					RefreshRPGPresentation();
				});
			const TArray<FGameplayAttribute> Attributes{
				UKnightAttributeSet::GetHealthAttribute(), UKnightAttributeSet::GetMaxHealthAttribute(),
				UKnightAttributeSet::GetStaminaAttribute(), UKnightAttributeSet::GetMaxStaminaAttribute(),
				UKnightAttributeSet::GetRuneEnergyAttribute(), UKnightAttributeSet::GetMaxRuneEnergyAttribute(),
				UKnightAttributeSet::GetAttackPowerAttribute(), UKnightAttributeSet::GetDefenseAttribute(),
				UKnightAttributeSet::GetCritChanceAttribute(), UKnightAttributeSet::GetHasteAttribute(),
				UKnightAttributeSet::GetCooldownRecoveryAttribute(), UKnightAttributeSet::GetMasteryBonusAttribute(),
				UKnightAttributeSet::GetVersatilityAttribute()};
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
					[this](const FKnightSkillRuntimeState& State)
					{
						RefreshRPGPresentation();
					});
			}
			if (GearViewedCombat.IsValid())
			{
				GearCombatHandle = GearViewedCombat->OnCombatSnapshotChanged.AddWeakLambda(this,
					[this](const FKnightCombatSnapshot& State)
					{
						RefreshRPGPresentation();
					});
			}
			break;
		}
	}
	if (((bGearDemo && !GearLabState.IsValid()) || !GearViewedKnight.IsValid()) && ++GearBindingAttempts <= 100)
	{
		GetWorldTimerManager().SetTimer(GearReadinessTimer, this, &ThisClass::RefreshRPGPresentation, 0.1f, false);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(GearReadinessTimer);
	}
}

void AKnightRPGUIPlayerController::UnbindGearViewedKnight()
{
	if (GearViewedKnight.IsValid())
	{
		GearViewedKnight->GetLifeComponent()->OnLifeStateChanged.Remove(GearLifeHandle);
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
	GearViewedKnight.Reset();
	GearViewedASC.Reset();
	GearViewedRuntime.Reset();
	GearViewedCombat.Reset();
	GearAttributeHandles.Reset();
	GearLifeHandle.Reset();
	GearProjectionHandle.Reset();
	GearRuntimeHandle.Reset();
	GearCombatHandle.Reset();
}

TArray<FKnightGearQuickUseRow> AKnightRPGUIPlayerController::GetQuickUseReadModel() const
{
	AKnightCharacter* Knight = GetControlledKnight();
	return RPGState.IsValid() && Knight != nullptr ? KnightRPGGearPresentation::QuickUses(
		RPGState->GetRPG()->GetKnightView(Knight->GetKnightEntityId()), RPGState->GetInventory()->GetView(), Knight)
		: TArray<FKnightGearQuickUseRow>();
}

void AKnightRPGUIPlayerController::HandleGearFixtureResult(const FGuid& RequestId, const FKnightResult& Result)
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
	Feedback = Result.IsSuccess() ? NSLOCTEXT("KnightGearUI", "FixtureConfirmed", "Fixture action confirmed.") : Result.Message;
	ReconcileClosedPanelReceipt(RequestId);
	RefreshRPGPresentation();
}

void AKnightRPGUIPlayerController::HandleGearPreview(const FKnightGearPanelIntent& Intent)
{
	if (RPGPanel != nullptr && RPGState.IsValid())
	{
		AKnightCharacter* SourceKnight = nullptr;
		AKnightCharacter* TargetKnight = nullptr;
		if (Intent.Action == EKnightGearPanelAction::Transfer)
		{
			const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
			const FKnightItemInstance* Item = Inventory.Items.FindByPredicate([&Intent](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == Intent.InstanceId;
			});
			for (TActorIterator<AKnightCharacter> It(GetWorld()); It; ++It)
			{
				if (Item != nullptr && It->GetKnightEntityId() == Item->AssignedKnightId)
				{
					SourceKnight = *It;
				}
				if (It->GetKnightEntityId() == Intent.TargetKnightId)
				{
					TargetKnight = *It;
				}
			}
		}
		RPGPanel->SetGearCompare(KnightRPGGearPresentation::Compare(RPGState->GetRPG(),
			RPGState->GetInventory()->GetView(), FindGearKnight(), Intent, SourceKnight, TargetKnight));
	}
}

void AKnightRPGUIPlayerController::HandleGearCommand(const FKnightGearPanelIntent& Intent)
{
	if (bEndingUIPlay || RPGPanel == nullptr || !RPGPanel->IsActivated() || PendingRequestId.IsValid() || !RPGState.IsValid())
	{
		return;
	}
	if (Intent.Action == EKnightGearPanelAction::SelectKnight)
	{
		GearSelectedKnightId = Intent.KnightId;
		RefreshRPGPresentation();
		return;
	}
	if (Intent.Action == EKnightGearPanelAction::Fixture && Intent.FixtureAction != TEXT("Guard") &&
		Intent.FixtureAction != TEXT("Mend") && Intent.FixtureAction != TEXT("Strike") && Intent.FixtureAction != TEXT("ClaimReward"))
	{
		PendingRequestId = Intent.RequestId;
		RefreshRPGPresentation();
		RequestGearFixture(Intent.FixtureAction, Intent.RequestId);
		return;
	}
	EKnightRPGOperation Operation = EKnightRPGOperation::Equip;
	switch (Intent.Action)
	{
	case EKnightGearPanelAction::Equip: Operation = EKnightRPGOperation::Equip; break;
	case EKnightGearPanelAction::Unequip: Operation = EKnightRPGOperation::Unequip; break;
	case EKnightGearPanelAction::Transfer: Operation = EKnightRPGOperation::TransferEquipment; break;
	case EKnightGearPanelAction::SwapRings: Operation = EKnightRPGOperation::SwapRings; break;
	case EKnightGearPanelAction::AttachWeaponRune: Operation = EKnightRPGOperation::AttachWeaponRune; break;
	case EKnightGearPanelAction::DetachWeaponRune: Operation = EKnightRPGOperation::DetachWeaponRune; break;
	case EKnightGearPanelAction::AttachSkillRune:
	case EKnightGearPanelAction::DetachSkillRune: Operation = EKnightRPGOperation::ApplyBuild; break;
	case EKnightGearPanelAction::Upgrade: Operation = EKnightRPGOperation::UpgradeItem; break;
	case EKnightGearPanelAction::Buy: Operation = EKnightRPGOperation::BuyItem; break;
	case EKnightGearPanelAction::Sell: Operation = EKnightRPGOperation::SellItem; break;
	case EKnightGearPanelAction::BindConsumable: Operation = EKnightRPGOperation::BindConsumable; break;
	case EKnightGearPanelAction::Fixture: Operation = Intent.FixtureAction == TEXT("ClaimReward")
		? EKnightRPGOperation::ClaimReward : EKnightRPGOperation::ApplyGearPreset; break;
	default: return;
	}
	FKnightRPGCommand Command = CreateCurrentCommand(Operation);
	Command.RequestId = Intent.RequestId;
	Command.ExpectedKnightId = Intent.KnightId;
	Command.ExpectedKnightRevision = Intent.ExpectedKnightRevision;
	Command.ExpectedInventoryRevision = Intent.ExpectedInventoryRevision;
	if (Operation == EKnightRPGOperation::Equip || Operation == EKnightRPGOperation::Unequip ||
		Operation == EKnightRPGOperation::TransferEquipment)
	{
		Command.SlotTag = FGameplayTag::RequestGameplayTag(Intent.SlotId, false);
	}
	if (Operation == EKnightRPGOperation::Equip || Operation == EKnightRPGOperation::Unequip || Operation == EKnightRPGOperation::TransferEquipment ||
		Operation == EKnightRPGOperation::AttachWeaponRune || Operation == EKnightRPGOperation::DetachWeaponRune ||
		Operation == EKnightRPGOperation::UpgradeItem || Operation == EKnightRPGOperation::SellItem)
	{
		Command.ItemInstanceId = Intent.InstanceId;
		Command.ExpectedItemRevision = Intent.ExpectedItemRevision;
	}
	if (Operation == EKnightRPGOperation::TransferEquipment)
	{
		Command.TargetKnightId = Intent.TargetKnightId;
		Command.ExpectedTargetKnightRevision = Intent.ExpectedTargetKnightRevision;
	}
	if (Operation == EKnightRPGOperation::AttachWeaponRune)
	{
		Command.WeaponRuneInstanceId = Intent.RuneInstanceId;
	}
	if (Operation == EKnightRPGOperation::BuyItem || Operation == EKnightRPGOperation::BindConsumable)
	{
		Command.ItemDefinitionId = Intent.DefinitionId;
	}
	if (Operation == EKnightRPGOperation::BindConsumable)
	{
		Command.QuickSlot = Intent.QuickSlot;
	}
	if (Operation == EKnightRPGOperation::SellItem)
	{
		Command.bConfirmEpicSale = Intent.bConfirmEpicSale;
	}
	if (Operation == EKnightRPGOperation::ApplyBuild)
	{
		const FKnightRPGKnightView Viewed = RPGState->GetRPG()->GetKnightView(Intent.KnightId);
		Command.ProposedBuild = Viewed.Build;
		Command.WeaponRuneInstanceId = Viewed.WeaponRuneInstanceId;
		if (Intent.Action == EKnightGearPanelAction::AttachSkillRune)
		{
			const FKnightGearItemRow* Rune = RPGPanel->GetSnapshot().Gear.Items.FindByPredicate([&Intent](const FKnightGearItemRow& Item)
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
	if (Operation == EKnightRPGOperation::ApplyGearPreset)
	{
		Command.PresetId = Intent.FixtureAction;
	}
	if (Operation == EKnightRPGOperation::ClaimReward)
	{
		Command.RewardId = Intent.RewardId;
	}
	PendingRequestId = Command.RequestId;
	RefreshRPGPresentation();
	RequestRPGCommand(Command);
}
