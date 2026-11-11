#include "RPGLoop/GolemRPGUIPlayerController.h"

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Definitions/GolemSkillDefinition.h"
#include "RPGLoop/GolemRPGSkillPresentation.h"
#include "RPGLoop/GolemRPGGearPresentation.h"
#include "GearLab/GolemGearLabState.h"
#include "RPGLoop/GolemRPGLoopGameMode.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Loot/GolemItemPickup.h"
#include "Models/GolemRPGPanelTypes.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Tags/GolemGameplayTags.h"
#include "Widgets/GolemRPGPanel.h"

#include "CommonInputModeTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBindingHandle.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

namespace GolemRPGPresentation
{
	const UGolemDefinition* Resolve(const FPrimaryAssetId& Id, UClass* ExpectedClass)
	{
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		UGolemDefinition* Definition = nullptr;
		return Manager != nullptr && Manager->ResolveDefinition(Id, ExpectedClass, Definition).IsSuccess()
			? Definition : nullptr;
	}

	FText Name(const FPrimaryAssetId& Id)
	{
		const UGolemDefinition* Definition = Resolve(Id, UGolemDefinition::StaticClass());
		return Definition != nullptr ? Definition->GetDisplayName()
			: NSLOCTEXT("KnightRPG", "UnknownDefinition", "Unavailable definition");
	}

	FText Objective(const EGolemRPGLoopStage Stage)
	{
		switch (Stage)
		{
		case EGolemRPGLoopStage::EncounterA:
			return NSLOCTEXT("KnightRPG", "ObjectiveA", "Encounter A: go to START and defeat the wave and boss.");
		case EGolemRPGLoopStage::RewardAvailable:
			return NSLOCTEXT("KnightRPG", "ObjectiveLoot", "Collect the loot at the cyan station with P / F. Equip or buy a skill, then Continue.");
		case EGolemRPGLoopStage::Upgrade:
			return NSLOCTEXT("KnightRPG", "ObjectiveUpgrade", "Upgrade your current Golem at the station. Host: Continue, then return to START for encounter B.");
		case EGolemRPGLoopStage::EncounterB:
			return NSLOCTEXT("KnightRPG", "ObjectiveB", "Encounter B: defeat the wave and boss with your upgraded loadout. Retry preserves loot and upgrades.");
		case EGolemRPGLoopStage::Complete:
			return NSLOCTEXT("KnightRPG", "ObjectiveComplete", "RPG loop complete! Host R / Y replays encounter B without duplicating the reward.");
		}
		return FText::GetEmpty();
	}
}

void AGolemRPGUIPlayerController::SetPlayer(UPlayer* InPlayer)
{
	CloseRPGPanel();
	if (RPGPanel != nullptr)
	{
		RPGPanel->OnActionRequested.Remove(PanelHandle);
		RPGPanel->OnCommandRequested.Remove(PanelCommandHandle);
		RPGPanel->OnPreviewRequested.Remove(PreviewHandle);
		RPGPanel->OnGearCommandRequested.Remove(GearCommandHandle);
		RPGPanel->OnGearPreviewRequested.Remove(GearPreviewHandle);
		RPGPanel = nullptr;
		PanelHandle.Reset();
	}
	RemoveRPGMappings();
	Super::SetPlayer(InPlayer);
	InstallRPGMappings();
}

void AGolemRPGUIPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InstallRPGMappings();
	if (IsLocalController())
	{
		ControlHandle = OnControlledGolemChanged.AddUObject(this, &ThisClass::HandleControlledGolem);
		ResultHandle = OnRPGCommandResult.AddUObject(this, &ThisClass::HandleRPGResult);
		LoopResultHandle = OnRPGLoopResult.AddUObject(this, &ThisClass::HandleLoopResult);
		GearFixtureResultHandle = OnGearFixtureResult.AddUObject(this, &ThisClass::HandleGearFixtureResult);
		HandleControlledGolem(nullptr, GetControlledGolem());
		BindRPGSources();
	}
}

void AGolemRPGUIPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InstallRPGMappings();
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (Enhanced == nullptr || (BoundRPGInput.Get() == Enhanced && !RPGInputHandles.IsEmpty()))
	{
		return;
	}
	if (BoundRPGInput.IsValid())
	{
		for (const uint32 Handle : RPGInputHandles)
		{
			BoundRPGInput->RemoveBindingByHandle(Handle);
		}
	}
	RPGInputHandles.Reset();
	BoundRPGInput = Enhanced;
	RPGInputHandles.Add(Enhanced->BindAction(PanelAction, ETriggerEvent::Started,
		this, &ThisClass::ToggleRPGPanel).GetHandle());
	RPGInputHandles.Add(Enhanced->BindAction(PickupAction, ETriggerEvent::Started,
		this, &ThisClass::RequestPickup).GetHandle());
}

void AGolemRPGUIPlayerController::InstallRPGMappings()
{
	if (bEndingUIPlay)
	{
		return;
	}
	if (RPGMappings == nullptr)
	{
		RPGMappings = NewObject<UInputMappingContext>(this, TEXT("RPGInteraction"));
		PanelAction = NewObject<UInputAction>(this, TEXT("RPGPanel"));
		PickupAction = NewObject<UInputAction>(this, TEXT("RPGPickup"));
		PanelAction->ValueType = EInputActionValueType::Boolean;
		PickupAction->ValueType = EInputActionValueType::Boolean;
		RPGMappings->MapKey(PanelAction, EKeys::I);
		RPGMappings->MapKey(PanelAction, EKeys::Gamepad_Special_Right);
		RPGMappings->MapKey(PickupAction, EKeys::P);
	}
	ULocalPlayer* Local = IsLocalController() ? GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Local != nullptr
		? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem != nullptr && MappingSubsystem.Get() != Subsystem)
	{
		RemoveRPGMappings();
		Subsystem->AddMappingContext(RPGMappings, 30);
		MappingSubsystem = Subsystem;
	}
}

void AGolemRPGUIPlayerController::RemoveRPGMappings()
{
	if (MappingSubsystem.IsValid() && RPGMappings != nullptr)
	{
		MappingSubsystem->RemoveMappingContext(RPGMappings);
	}
	MappingSubsystem.Reset();
}

void AGolemRPGUIPlayerController::BindRPGSources()
{
	if (bEndingUIPlay || !IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}
	AGolemRPGGameState* State = GetWorld()->GetGameState<AGolemRPGGameState>();
	if (State == nullptr)
	{
		if (++BindingAttempts <= 100)
		{
			GetWorldTimerManager().SetTimer(SourceReadinessTimer, this, &ThisClass::BindRPGSources, 0.1f, false);
		}
		else
		{
			Feedback = NSLOCTEXT("KnightRPG", "MissingState", "RPG state is unavailable.");
			RefreshRPGPresentation();
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(SourceReadinessTimer);
	if (RPGGameState.Get() != State)
	{
		UnbindRPGSources();
		RPGGameState = State;
		StateReadyHandle = State->OnRPGStateChanged.AddUObject(this, &ThisClass::HandleRPGStateReady);
	}
	HandleRPGStateReady(State->GetRPGState());
}

void AGolemRPGUIPlayerController::UnbindRPGSources()
{
	GetWorldTimerManager().ClearTimer(GearReadinessTimer);
	UnbindGearViewedGolem();
	if (GearLabState.IsValid())
	{
		GearLabState->OnGearDemoChanged.Remove(GearStateHandle);
	}
	GearLabState.Reset();
	GearStateHandle.Reset();
	if (RPGState.IsValid())
	{
		RPGState->GetInventory()->OnSnapshotChanged.Remove(InventoryHandle);
		RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
		RPGState->OnLoopChanged.Remove(LoopHandle);
	}
	if (RPGGameState.IsValid())
	{
		RPGGameState->OnRPGStateChanged.Remove(StateReadyHandle);
	}
	RPGState.Reset();
	RPGGameState.Reset();
	InventoryHandle.Reset();
	RPGHandle.Reset();
	LoopHandle.Reset();
	StateReadyHandle.Reset();
}

void AGolemRPGUIPlayerController::HandleRPGStateReady(AGolemRPGStateActor* InState)
{
	if (RPGState.Get() != InState)
	{
		if (RPGState.IsValid())
		{
			RPGState->GetInventory()->OnSnapshotChanged.Remove(InventoryHandle);
			RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
			RPGState->OnLoopChanged.Remove(LoopHandle);
		}
		RPGState = InState;
		InventoryHandle.Reset();
		RPGHandle.Reset();
		LoopHandle.Reset();
		if (IsValid(InState))
		{
			InventoryHandle = InState->GetInventory()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshRPGPresentation);
			RPGHandle = InState->GetRPG()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshRPGPresentation);
			LoopHandle = InState->OnLoopChanged.AddWeakLambda(this,
				[this](const FGolemRPGLoopSnapshot& Snapshot)
				{
					RefreshRPGPresentation();
				});
		}
	}
	RefreshRPGPresentation();
}

void AGolemRPGUIPlayerController::HandleControlledGolem(AGolemCharacter* Previous, AGolemCharacter* Current)
{
	GearSelectedGolemId = Current != nullptr ? Current->GetGolemEntityId() : FGolemEntityId();
	CloseRPGPanel();
	UnbindDisplayedGolem();
	DisplayedGolem = Current;
	if (IsValid(Current))
	{
		DisplayedASC = Current->GetGolemAbilitySystem();
		LifeHandle = Current->GetLifeComponent()->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleLifeChanged);
		ProjectionHandle = Current->GetGolemAbilitySystem()->OnSourceProjectionChanged.AddUObject(
			this, &ThisClass::HandleProjectionChanged);
		const TArray<FGameplayAttribute> ObservedAttributes{
			UGolemAttributeSet::GetHealthAttribute(), UGolemAttributeSet::GetMaxHealthAttribute(),
			UGolemAttributeSet::GetStaminaAttribute(), UGolemAttributeSet::GetMaxStaminaAttribute(),
			UGolemAttributeSet::GetRuneEnergyAttribute(), UGolemAttributeSet::GetMaxRuneEnergyAttribute(),
			UGolemAttributeSet::GetAttackPowerAttribute(), UGolemAttributeSet::GetDefenseAttribute(),
			UGolemAttributeSet::GetCritChanceAttribute(), UGolemAttributeSet::GetCritMultiplierAttribute(),
			UGolemAttributeSet::GetHasteAttribute(), UGolemAttributeSet::GetDamageReductionAttribute(),
			UGolemAttributeSet::GetArcaneResistanceAttribute(), UGolemAttributeSet::GetLightningResistanceAttribute(),
			UGolemAttributeSet::GetFrostResistanceAttribute(), UGolemAttributeSet::GetFireResistanceAttribute(),
			UGolemAttributeSet::GetMasteryBonusAttribute(), UGolemAttributeSet::GetVersatilityAttribute(),
			UGolemAttributeSet::GetCooldownRecoveryAttribute()};
		for (const FGameplayAttribute& Attribute : ObservedAttributes)
		{
			const FDelegateHandle Handle = DisplayedASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddWeakLambda(this, [this](const FOnAttributeChangeData& Change)
				{
					if (DisplayedASC.IsValid() && !DisplayedASC->IsMutationFenced())
					{
						RefreshRPGPresentation();
					}
				});
			AttributeHandles.Emplace(Attribute, Handle);
		}
		DisplayedRuntime = Current->FindComponentByClass<UGolemSkillRuntimeComponent>();
		DisplayedCombat = Current->GetCombatComponent();
		if (DisplayedCombat.IsValid())
		{
			CombatStateHandle = DisplayedCombat->OnCombatSnapshotChanged.AddWeakLambda(this,
				[this](const FGolemCombatSnapshot& State)
				{
					if (DisplayedASC.IsValid() && !DisplayedASC->IsMutationFenced())
					{
						RefreshRPGPresentation();
					}
				});
		}
		if (DisplayedRuntime.IsValid())
		{
			RuntimeStateHandle = DisplayedRuntime->OnSkillStateChanged.AddWeakLambda(this,
				[this](const FGolemSkillRuntimeState& State)
				{
					if (DisplayedASC.IsValid() && !DisplayedASC->IsMutationFenced())
					{
						RefreshRPGPresentation();
					}
				});
		}
	}
	RefreshRPGPresentation();
}

void AGolemRPGUIPlayerController::UnbindDisplayedGolem()
{
	if (DisplayedGolem.IsValid())
	{
		DisplayedGolem->GetLifeComponent()->OnLifeStateChanged.Remove(LifeHandle);
	}
	if (DisplayedASC.IsValid())
	{
		DisplayedASC->OnSourceProjectionChanged.Remove(ProjectionHandle);
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Binding : AttributeHandles)
		{
			DisplayedASC->GetGameplayAttributeValueChangeDelegate(Binding.Key).Remove(Binding.Value);
		}
	}
	if (DisplayedRuntime.IsValid())
	{
		DisplayedRuntime->OnSkillStateChanged.Remove(RuntimeStateHandle);
	}
	AttributeHandles.Reset();
	if (DisplayedCombat.IsValid())
	{
		DisplayedCombat->OnCombatSnapshotChanged.Remove(CombatStateHandle);
	}
	DisplayedCombat.Reset();
	CombatStateHandle.Reset();
	RuntimeStateHandle.Reset();
	DisplayedRuntime.Reset();
	LifeHandle.Reset();
	ProjectionHandle.Reset();
	DisplayedGolem.Reset();
	DisplayedASC.Reset();
}

void AGolemRPGUIPlayerController::HandleLifeChanged(const EGolemLifeState Previous, const EGolemLifeState Current)
{
	if (Current != EGolemLifeState::Alive)
	{
		CloseRPGPanel();
	}
	RefreshRPGPresentation();
}

void AGolemRPGUIPlayerController::HandleProjectionChanged(const FGolemSourceProjection& Projection)
{
	RefreshRPGPresentation();
}

FGolemRPGCommand AGolemRPGUIPlayerController::CreateCurrentCommand(const EGolemRPGOperation Operation) const
{
	FGolemRPGCommand Command;
	Command.Operation = Operation;
	Command.RequestId = FGuid::NewGuid();
	AGolemCharacter* Golem = GetControlledGolem();
	if (Golem != nullptr && RPGState.IsValid())
	{
		const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
		const FGolemRPGGolemView GolemView = RPGState->GetRPG()->GetGolemView(Golem->GetGolemEntityId());
		Command.ExpectedGolemId = Golem->GetGolemEntityId();
		Command.InventoryId = Inventory.InventoryId;
		Command.ExpectedInventoryRevision = Inventory.Revision;
		Command.ExpectedGolemRevision = GolemView.Revision;
	}
	return Command;
}

void AGolemRPGUIPlayerController::RequestPickup()
{
	AGolemCharacter* Golem = GetControlledGolem();
	if (bEndingUIPlay || !IsLocalController() || Golem == nullptr || GetWorld() == nullptr
		|| !Golem->GetLifeComponent()->IsAlive())
	{
		return;
	}
	AGolemItemPickup* Nearest = nullptr;
	double Best = FMath::Square(static_cast<double>(GolemRPG::PickupRange));
	for (TActorIterator<AGolemItemPickup> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (!Iterator->IsClaimed())
		{
			const double Distance = FVector::DistSquared(Golem->GetActorLocation(), Iterator->GetActorLocation());
			if (Distance <= Best)
			{
				Best = Distance;
				Nearest = *Iterator;
			}
		}
	}
	if (Nearest == nullptr)
	{
		Feedback = NSLOCTEXT("KnightRPG", "NoPickup", "Move closer to an unclaimed loot pickup.");
		RefreshRPGPresentation();
		return;
	}
	FGolemRPGCommand Command = CreateCurrentCommand(EGolemRPGOperation::Pickup);
	Command.PickupId = Nearest->GetPickupId();
	RequestRPGCommand(Command);
}

void AGolemRPGUIPlayerController::ToggleRPGPanel()
{
	if (RPGPanel != nullptr && RPGPanel->IsActivated())
	{
		CloseRPGPanel();
		return;
	}
	AGolemCharacter* Golem = GetControlledGolem();
	if (bEndingUIPlay || !IsLocalController() || Golem == nullptr || !Golem->CanTransferControl())
	{
		Feedback = NSLOCTEXT("KnightRPG", "PanelBusy", "Finish your action before opening the upgrade panel.");
		RefreshRPGPresentation();
		return;
	}
	ResetControlInput();
	if (RPGPanel == nullptr)
	{
		RPGPanel = CreateWidget<UGolemRPGPanel>(this, UGolemRPGPanel::StaticClass());
		if (RPGPanel == nullptr)
		{
			Feedback = NSLOCTEXT("KnightRPG", "PanelUnavailable", "The upgrade panel is unavailable.");
			RefreshRPGPresentation();
			return;
		}
		PanelHandle = RPGPanel->OnActionRequested.AddUObject(this, &ThisClass::HandlePanelAction);
		PanelCommandHandle = RPGPanel->OnCommandRequested.AddUObject(this, &ThisClass::HandlePanelCommand);
		PreviewHandle = RPGPanel->OnPreviewRequested.AddUObject(this, &ThisClass::HandleBuildPreview);
		GearCommandHandle = RPGPanel->OnGearCommandRequested.AddUObject(this, &ThisClass::HandleGearCommand);
		GearPreviewHandle = RPGPanel->OnGearPreviewRequested.AddUObject(this, &ThisClass::HandleGearPreview);
	}
	// A closed panel receives no live snapshots. Resynchronize before CommonUI restores its focus.
	BindGearSources();
	RPGPanel->SetSnapshot(BuildPanelSnapshot());
	RefreshRPGPresentation();
	RPGPanel->AddToPlayerScreen(100);
	RPGPanel->ActivateWidget();
}

UGolemRPGPanel* AGolemRPGUIPlayerController::GetRPGPanel() const
{
	return RPGPanel;
}

FText AGolemRPGUIPlayerController::GetRPGFeedback() const
{
	return Feedback;
}

void AGolemRPGUIPlayerController::CloseRPGPanel()
{
	if (RPGPanel == nullptr || !RPGPanel->IsActivated())
	{
		return;
	}
	RPGPanel->DeactivateWidget();
	RPGPanel->RemoveFromParent();
	ULocalPlayer* Local = GetLocalPlayer();
	UCommonUIActionRouterBase* Router = Local != nullptr ? Local->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	if (Router != nullptr && !bEndingUIPlay)
	{
		Router->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game,
			EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown), this);
	}
}

void AGolemRPGUIPlayerController::HandlePanelAction(const EGolemRPGPanelAction Action)
{
	if (bEndingUIPlay || RPGPanel == nullptr || !RPGPanel->IsActivated())
	{
		return;
	}
	if (Action == EGolemRPGPanelAction::Close)
	{
		CloseRPGPanel();
		return;
	}
	if (Action == EGolemRPGPanelAction::Continue)
	{
		RequestContinueRPGLoop();
		return;
	}
	FGolemRPGCommand Command;
	switch (Action)
	{
	case EGolemRPGPanelAction::EquipWeapon:
	case EGolemRPGPanelAction::EquipRune:
	{
		const bool bRune = Action == EGolemRPGPanelAction::EquipRune;
		Command = CreateCurrentCommand(bRune ? EGolemRPGOperation::Rune : EGolemRPGOperation::Equip);
		Command.SlotTag = bRune ? GolemGameplayTags::Slot_Equipment_Rune : GolemGameplayTags::Slot_Equipment_Weapon;
		if (RPGState.IsValid())
		{
			const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
			const FGolemRPGView View = RPGState->GetRPG()->GetView();
			TSet<FGuid> Assigned;
			for (const FGolemRPGGolemView& Entry : View.Golems)
			{
				for (const FGolemEquipmentAssignment& Assignment : Entry.Assignments)
				{
					Assigned.Add(Assignment.ItemInstanceId);
				}
			}
			for (const FGolemItemInstance& Item : Inventory.Items)
			{
				const UGolemItemDefinition* Definition = Cast<UGolemItemDefinition>(
					GolemRPGPresentation::Resolve(Item.DefinitionId, UGolemItemDefinition::StaticClass()));
				const UGolemRuneDefinition* Rune = Cast<UGolemRuneDefinition>(Definition);
				if (bRune && Rune != nullptr && !Rune->GetAttachmentRuneId().IsNone())
				{
					continue;
				}
				if (Definition != nullptr && !Assigned.Contains(Item.InstanceId)
					&& Definition->GetItemKind() == (bRune ? EGolemItemKind::Rune : EGolemItemKind::Equipment))
				{
					Command.ItemInstanceId = Item.InstanceId;
					break;
				}
			}
		}
		if (!Command.ItemInstanceId.IsValid())
		{
			Feedback = NSLOCTEXT("KnightRPG", "NoAvailableEquipment", "No unassigned item is available. Collect loot or unequip it from its current Golem.");
			RefreshRPGPresentation();
			return;
		}
		break;
	}
	case EGolemRPGPanelAction::UnequipWeapon:
	case EGolemRPGPanelAction::UnequipRune:
	{
		Command = CreateCurrentCommand(EGolemRPGOperation::Unequip);
		Command.SlotTag = Action == EGolemRPGPanelAction::UnequipRune
			? GolemGameplayTags::Slot_Equipment_Rune : GolemGameplayTags::Slot_Equipment_Weapon;
		const FGolemRPGGolemView View = RPGState.IsValid()
			? RPGState->GetRPG()->GetGolemView(Command.ExpectedGolemId) : FGolemRPGGolemView();
		const FGolemEquipmentAssignment* Assignment = View.FindAssignment(Command.SlotTag);
		if (Assignment == nullptr)
		{
			Feedback = NSLOCTEXT("KnightRPG", "NoAssignedEquipment", "This Golem has no item assigned to that slot.");
			RefreshRPGPresentation();
			return;
		}
		Command.ItemInstanceId = Assignment->ItemInstanceId;
		break;
	}
	case EGolemRPGPanelAction::PurchaseHammerSkill:
	case EGolemRPGPanelAction::PurchaseRuneSkill:
		Command = CreateCurrentCommand(EGolemRPGOperation::PurchaseSkill);
		Command.SkillDefinitionId = FPrimaryAssetId(TEXT("KnightSkill"),
			Action == EGolemRPGPanelAction::PurchaseHammerSkill ? FName(TEXT("HammerForce")) : FName(TEXT("RuneFocus")));
		break;
	case EGolemRPGPanelAction::Respec:
		Command = CreateCurrentCommand(EGolemRPGOperation::Respec);
		break;
	default:
		return;
	}
	RequestRPGCommand(Command);
}

void AGolemRPGUIPlayerController::HandlePanelCommand(const FGolemRPGPanelCommand& Intent)
{
	if (bEndingUIPlay || RPGPanel == nullptr || !RPGPanel->IsActivated() || PendingRequestId.IsValid())
	{
		return;
	}
	if (Intent.Action == EGolemRPGPanelAction::SelectGolem)
	{
		for (TActorIterator<AGolemPartyState> It(GetWorld()); It; ++It)
		{
			for (const FGolemRosterEntry& Entry : It->GetRoster())
			{
				if (Entry.GolemId == Intent.GolemId)
				{
					RequestSwitchGolem(Entry.Slot);
					return;
				}
			}
		}
		return;
	}
	EGolemRPGOperation Operation;
	switch (Intent.Action)
	{
	case EGolemRPGPanelAction::ApplyBuild: Operation = EGolemRPGOperation::ApplyBuild; break;
	case EGolemRPGPanelAction::NewTalentLoadout: Operation = EGolemRPGOperation::NewTalentLoadout; break;
	case EGolemRPGPanelAction::RenameTalentLoadout: Operation = EGolemRPGOperation::RenameTalentLoadout; break;
	case EGolemRPGPanelAction::DuplicateTalentLoadout: Operation = EGolemRPGOperation::DuplicateTalentLoadout; break;
	case EGolemRPGPanelAction::DeleteTalentLoadout: Operation = EGolemRPGOperation::DeleteTalentLoadout; break;
	case EGolemRPGPanelAction::ApplyTalentLoadout: Operation = EGolemRPGOperation::ApplyTalentLoadout; break;
	default: return;
	}
	FGolemRPGCommand Command = CreateCurrentCommand(Operation);
	Command.ExpectedGolemId = Intent.GolemId;
	Command.ExpectedGolemRevision = Intent.ExpectedRevision;
	Command.RequestId = Intent.RequestId;
	Command.ProposedBuild = Intent.ProposedBuild;
	Command.WeaponRuneInstanceId = Intent.WeaponRuneInstanceId;
	Command.SkillRuneInstanceId = Intent.SkillRuneInstanceId;
	Command.TalentLoadoutId = Intent.TalentLoadoutId;
	Command.TalentLoadoutName = Intent.TalentLoadoutName;
	PendingRequestId = Command.RequestId;
	RefreshRPGPresentation();
	RequestRPGCommand(Command);
}

void AGolemRPGUIPlayerController::HandleBuildPreview(const FGolemSkillBuild& Build)
{
	AGolemCharacter* Golem = GetControlledGolem();
	if (RPGPanel == nullptr || Golem == nullptr)
	{
		return;
	}
	FGolemRPGBuildPreview Preview = GolemRPGSkillPresentation::Preview(Build,
		Golem->GetGolemAttributeSet()->GetAttackPower());
	const FGolemSkillBuild& Confirmed = RPGPanel->GetSnapshot().Build;
	const float ProposedStamina = Golem->GetGolemAttributeSet()->GetMaxStamina()
		+ (Build.ClassChoice == TEXT("Endurance") ? 10.0f : 0.0f)
		- (Confirmed.ClassChoice == TEXT("Endurance") ? 10.0f : 0.0f);
	const float ProposedResource = Golem->GetGolemAttributeSet()->GetMaxRuneEnergy()
		+ (Build.ClassChoice == TEXT("Reservoir") ? 10.0f : 0.0f)
		- (Confirmed.ClassChoice == TEXT("Reservoir") ? 10.0f : 0.0f);
	Preview.Summary = FText::Format(NSLOCTEXT("GolemSkillsUI", "DraftStatPreview",
		"{0}\nProposed MaxStamina {1}; MaxResource {2}. Current pools are preserved and clamped on confirmation."),
		Preview.Summary, FText::AsNumber(ProposedStamina), FText::AsNumber(ProposedResource));
	Preview.GolemId = RPGPanel->GetSnapshot().GolemId;
	Preview.Runes = BuildPanelSnapshot().Runes;
	for (FGolemRPGRuneRow& Rune : Preview.Runes)
	{
		Rune.bCompatible = FGolemSkillCatalog::IsRuneCompatible(Build.SpecId, Rune.RuneId)
			&& (Rune.IncompatibilityReason.IsEmpty() || Rune.IncompatibilityReason.EqualTo(
				NSLOCTEXT("GolemSkillsUI", "RuneIncompatible", "The selected specialization has no compatible skill for this rune.")));
	}
	RPGPanel->SetBuildPreview(Preview);
}

void AGolemRPGUIPlayerController::HandleRPGResult(const FGolemRPGCommandResult& Outcome)
{
	if (bEndingUIPlay)
	{
		return;
	}
	// A delayed receipt describes its original Golem; it never replaces the currently observed readmodel.
	LastCompletedRequestId = Outcome.RequestId;
	if (PendingRequestId == Outcome.RequestId)
	{
		PendingRequestId.Invalidate();
	}
	Feedback = Outcome.Result.IsSuccess()
		? NSLOCTEXT("KnightRPG", "TransactionCommitted", "Upgrade transaction committed.") : Outcome.Result.Message;
	ReconcileClosedPanelReceipt(Outcome.RequestId);
	RefreshRPGPresentation();
}

void AGolemRPGUIPlayerController::HandleLoopResult(const FGolemResult& Result)
{
	if (bEndingUIPlay)
	{
		return;
	}
	Feedback = Result.IsSuccess() ? NSLOCTEXT("KnightRPG", "ContinueCommitted", "Encounter B is ready. Return to START.")
		: Result.Message;
	if (Result.IsSuccess())
	{
		CloseRPGPanel();
	}
	RefreshRPGPresentation();
}

void AGolemRPGUIPlayerController::RefreshRPGPresentation()
{
	if (bEndingUIPlay)
	{
		return;
	}
	BindGearSources();
	if (RPGPanel != nullptr && RPGPanel->IsActivated())
	{
		RPGPanel->SetSnapshot(BuildPanelSnapshot());
	}
	OnRPGPresentationChanged.Broadcast();
}

void AGolemRPGUIPlayerController::ReconcileClosedPanelReceipt(const FGuid& RequestId)
{
	if (RPGPanel != nullptr && !RPGPanel->IsActivated() && RequestId.IsValid() &&
		RPGPanel->GetPendingRequestId() == RequestId)
	{
		// Consume this exact receipt once, before a later gameplay receipt replaces its ID.
		RPGPanel->SetSnapshot(BuildPanelSnapshot());
	}
}

FGolemRPGPanelSnapshot AGolemRPGUIPlayerController::BuildPanelSnapshot() const
{
	FGolemRPGPanelSnapshot Snapshot;
	Snapshot.Feedback = Feedback;
	AGolemCharacter* Golem = GetControlledGolem();
	if (Golem == nullptr || !RPGState.IsValid())
	{
		Snapshot.Title = NSLOCTEXT("KnightRPG", "PanelWaiting", "Waiting for RPG state");
		return Snapshot;
	}
	Snapshot.GolemId = Golem->GetGolemEntityId();
	Snapshot.Title = FText::Format(NSLOCTEXT("KnightRPG", "PanelTitle", "Upgrade {0}"), Golem->GetGolemDisplayName());
	const FGolemRPGLoopSnapshot& Loop = RPGState->GetLoopSnapshot();
	Snapshot.Objective = GolemRPGPresentation::Objective(Loop.Stage);
	Snapshot.bCanContinue = Loop.Stage == EGolemRPGLoopStage::RewardAvailable || Loop.Stage == EGolemRPGLoopStage::Upgrade;
	const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
	const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(Snapshot.GolemId);
	GolemRPGSkillPresentation::Enrich(Snapshot, View, RPGState->GetRPG()->GetView(), Inventory, Golem);
	Snapshot.LastCompletedRequestId = LastCompletedRequestId;
	Snapshot.bCommandPending = PendingRequestId.IsValid();
	if (const AGolemRPGLoopGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemRPGLoopGameMode>())
	{
		Snapshot.bCanEditBuild = Mode->IsInUpgradeSafeZone(Golem);
	}
	else
	{
		for (TActorIterator<AGolemPartyArenaState> It(GetWorld()); It; ++It)
		{
			Snapshot.bCanEditBuild = It->GetSnapshot().AliveEnemies == 0
				&& (It->GetSnapshot().Stage == EGolemArenaStage::Victory
					|| It->GetSnapshot().Stage == EGolemArenaStage::AwaitingStart);
			break;
		}
		bool bAtSanctuary = false;
		for (TActorIterator<AGolemRPGUpgradeStation> It(GetWorld()); It; ++It)
		{
			bAtSanctuary |= FVector::DistSquared(Golem->GetActorLocation(), It->GetActorLocation())
				<= FMath::Square(It->GetInteractionRange());
		}
		Snapshot.bCanEditBuild &= bAtSanctuary;
	}
	Snapshot.bCanEditBuild &= !View.Build.SpecId.IsNone();
	if (const UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>())
	{
		Snapshot.bCanEditBuild &= !Runtime->HasDeferredDamage();
	}
	Snapshot.EditLockReason = Snapshot.bCanEditBuild ? FText::GetEmpty()
		: NSLOCTEXT("GolemSkillsUI", "SanctuaryLock", "Build edits require the sanctuary outside an encounter. View only here.");
	FString InventoryText;
	for (const FGolemItemInstance& Item : Inventory.Items)
	{
		InventoryText += FText::Format(NSLOCTEXT("KnightRPG", "InventoryLine", "{0} x{1}\n"),
			GolemRPGPresentation::Name(Item.DefinitionId), FText::AsNumber(Item.Count)).ToString();
	}
	Snapshot.Inventory = FText::Format(NSLOCTEXT("KnightRPG", "InventoryHeading", "Shared inventory:\n{0}"),
		InventoryText.IsEmpty() ? NSLOCTEXT("KnightRPG", "EmptyInventory", "No loot collected") : FText::FromString(InventoryText));
	FString EquipmentText;
	for (const FGolemEquipmentAssignment& Assignment : View.Assignments)
	{
		EquipmentText += GolemRPGPresentation::Name(Assignment.DefinitionId).ToString() + TEXT("\n");
	}
	Snapshot.Equipment = FText::Format(NSLOCTEXT("KnightRPG", "EquipmentHeading", "This Golem's equipment:\n{0}"),
		EquipmentText.IsEmpty() ? NSLOCTEXT("KnightRPG", "EmptyEquipment", "Default hammer; no rune upgrade")
		: FText::FromString(EquipmentText));
	FString SkillText;
	const TArray<FPrimaryAssetId> SkillIds{FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")),
		FPrimaryAssetId(TEXT("KnightSkill"), TEXT("RuneFocus"))};
	for (const FPrimaryAssetId& Id : SkillIds)
	{
		const UGolemSkillDefinition* Definition = Cast<UGolemSkillDefinition>(
			GolemRPGPresentation::Resolve(Id, UGolemSkillDefinition::StaticClass()));
		if (Definition == nullptr)
		{
			continue;
		}
		FString Costs;
		for (const FGolemMaterialCost& Cost : Definition->GetMaterialCosts())
		{
			Costs += FText::Format(NSLOCTEXT("KnightRPG", "MaterialCost", "; {0} {1}"),
				FText::AsNumber(Cost.Count), GolemRPGPresentation::Name(Cost.DefinitionId)).ToString();
		}
		for (const FPrimaryAssetId& Prerequisite : Definition->GetPrerequisites())
		{
			Costs += FText::Format(NSLOCTEXT("KnightRPG", "SkillPrerequisite", "; requires {0}"),
				GolemRPGPresentation::Name(Prerequisite)).ToString();
		}
		SkillText += FText::Format(NSLOCTEXT("KnightRPG", "SkillLine", "{0}: {1} points{2} {3}\n"),
			Definition->GetDisplayName(), FText::AsNumber(Definition->GetPointCost()), FText::FromString(Costs),
			View.HasSkill(Id) ? NSLOCTEXT("KnightRPG", "Purchased", "[purchased]") : FText::GetEmpty()).ToString();
	}
	Snapshot.Skills = FText::Format(NSLOCTEXT("KnightRPG", "SkillHeading", "Skill points: {0}\n{1}"),
		FText::AsNumber(View.SkillPoints), FText::FromString(SkillText));
	Snapshot.Stats = FText::Format(NSLOCTEXT("KnightRPG", "StatHeading", "Bonuses: health +{0}, attack +{1} | Rune damage x{2}, cost x{3}, duration x{4}"),
		FText::AsNumber(View.StatBonuses.MaxHealth), FText::AsNumber(View.StatBonuses.AttackPower),
		FText::AsNumber(View.RuneTuning.DamageScale), FText::AsNumber(View.RuneTuning.CostScale),
		FText::AsNumber(View.RuneTuning.DurationScale));
	if (AGolemCharacter* GearGolem = FindGearGolem())
	{
		const FGolemRPGView Party = RPGState->GetRPG()->GetView();
		bool bGearAtSanctuary = Snapshot.bCanEditBuild;
		if (const AGolemRPGLoopGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemRPGLoopGameMode>())
		{
			bGearAtSanctuary = Mode->IsInUpgradeSafeZone(GearGolem);
		}
		else if (Party.IsItemizationEnabled() && GearGolem != Golem)
		{
			bool bOutsideEncounter = false;
			for (TActorIterator<AGolemPartyArenaState> It(GetWorld()); It; ++It)
			{
				bOutsideEncounter = It->GetSnapshot().AliveEnemies == 0 &&
					(It->GetSnapshot().Stage == EGolemArenaStage::Victory ||
						It->GetSnapshot().Stage == EGolemArenaStage::AwaitingStart);
				break;
			}
			bool bNearStation = false;
			for (TActorIterator<AGolemRPGUpgradeStation> It(GetWorld()); It; ++It)
			{
				bNearStation |= FVector::DistSquared(GearGolem->GetActorLocation(), It->GetActorLocation())
					<= FMath::Square(It->GetInteractionRange());
			}
			bGearAtSanctuary = bOutsideEncounter && bNearStation;
		}
		if (Party.IsItemizationEnabled() && GearGolem != Golem)
		{
			FGolemRPGPanelSnapshot GearStats;
			GolemRPGSkillPresentation::Enrich(GearStats, RPGState->GetRPG()->GetGolemView(GearGolem->GetGolemEntityId()), Party, Inventory, GearGolem);
			Snapshot.StatRows = GearStats.StatRows;
		}
		GolemRPGGearPresentation::Enrich(Snapshot, Party, Inventory, GearGolem, this,
			bGearAtSanctuary, GearLabState.IsValid() ? &GearLabState->GetSnapshot() : nullptr);
	}
	return Snapshot;
}

void AGolemRPGUIPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingUIPlay = true;
	GetWorldTimerManager().ClearTimer(SourceReadinessTimer);
	CloseRPGPanel();
	if (RPGPanel != nullptr)
	{
		RPGPanel->OnActionRequested.Remove(PanelHandle);
		RPGPanel->OnCommandRequested.Remove(PanelCommandHandle);
		RPGPanel->OnPreviewRequested.Remove(PreviewHandle);
		RPGPanel->OnGearCommandRequested.Remove(GearCommandHandle);
		RPGPanel->OnGearPreviewRequested.Remove(GearPreviewHandle);
		RPGPanel->RemoveFromParent();
	}
	PanelHandle.Reset();
	UnbindDisplayedGolem();
	UnbindRPGSources();
	OnControlledGolemChanged.Remove(ControlHandle);
	OnRPGCommandResult.Remove(ResultHandle);
	OnRPGLoopResult.Remove(LoopResultHandle);
	OnGearFixtureResult.Remove(GearFixtureResultHandle);
	OnRPGPresentationChanged.Clear();
	RemoveRPGMappings();
	if (BoundRPGInput.IsValid())
	{
		for (const uint32 Handle : RPGInputHandles)
		{
			BoundRPGInput->RemoveBindingByHandle(Handle);
		}
	}
	RPGInputHandles.Reset();
	BoundRPGInput.Reset();
	RPGPanel = nullptr;
	Super::EndPlay(EndPlayReason);
}
