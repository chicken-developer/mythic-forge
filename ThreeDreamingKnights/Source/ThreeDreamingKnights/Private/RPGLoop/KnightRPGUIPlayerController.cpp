#include "RPGLoop/KnightRPGUIPlayerController.h"

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Models/KnightRPGPanelTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Tags/KnightGameplayTags.h"
#include "Widgets/KnightRPGPanel.h"

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

namespace KnightRPGPresentation
{
	const UKnightDefinition* Resolve(const FPrimaryAssetId& Id, UClass* ExpectedClass)
	{
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		UKnightDefinition* Definition = nullptr;
		return Manager != nullptr && Manager->ResolveDefinition(Id, ExpectedClass, Definition).IsSuccess()
			? Definition : nullptr;
	}

	FText Name(const FPrimaryAssetId& Id)
	{
		const UKnightDefinition* Definition = Resolve(Id, UKnightDefinition::StaticClass());
		return Definition != nullptr ? Definition->GetDisplayName()
			: NSLOCTEXT("KnightRPG", "UnknownDefinition", "Unavailable definition");
	}

	FText Objective(const EKnightRPGLoopStage Stage)
	{
		switch (Stage)
		{
		case EKnightRPGLoopStage::EncounterA:
			return NSLOCTEXT("KnightRPG", "ObjectiveA", "Encounter A: go to START and defeat the wave and boss.");
		case EKnightRPGLoopStage::RewardAvailable:
			return NSLOCTEXT("KnightRPG", "ObjectiveLoot", "Collect the loot at the cyan station with P / F. Equip or buy a skill, then Continue.");
		case EKnightRPGLoopStage::Upgrade:
			return NSLOCTEXT("KnightRPG", "ObjectiveUpgrade", "Upgrade your current Knight at the station. Host: Continue, then return to START for encounter B.");
		case EKnightRPGLoopStage::EncounterB:
			return NSLOCTEXT("KnightRPG", "ObjectiveB", "Encounter B: defeat the wave and boss with your upgraded loadout. Retry preserves loot and upgrades.");
		case EKnightRPGLoopStage::Complete:
			return NSLOCTEXT("KnightRPG", "ObjectiveComplete", "RPG loop complete! Host R / Y replays encounter B without duplicating the reward.");
		}
		return FText::GetEmpty();
	}
}

void AKnightRPGUIPlayerController::SetPlayer(UPlayer* InPlayer)
{
	CloseRPGPanel();
	if (RPGPanel != nullptr)
	{
		RPGPanel->OnActionRequested.Remove(PanelHandle);
		RPGPanel = nullptr;
		PanelHandle.Reset();
	}
	RemoveRPGMappings();
	Super::SetPlayer(InPlayer);
	InstallRPGMappings();
}

void AKnightRPGUIPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InstallRPGMappings();
	if (IsLocalController())
	{
		ControlHandle = OnControlledKnightChanged.AddUObject(this, &ThisClass::HandleControlledKnight);
		ResultHandle = OnRPGCommandResult.AddUObject(this, &ThisClass::HandleRPGResult);
		LoopResultHandle = OnRPGLoopResult.AddUObject(this, &ThisClass::HandleLoopResult);
		HandleControlledKnight(nullptr, GetControlledKnight());
		BindRPGSources();
	}
}

void AKnightRPGUIPlayerController::SetupInputComponent()
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

void AKnightRPGUIPlayerController::InstallRPGMappings()
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

void AKnightRPGUIPlayerController::RemoveRPGMappings()
{
	if (MappingSubsystem.IsValid() && RPGMappings != nullptr)
	{
		MappingSubsystem->RemoveMappingContext(RPGMappings);
	}
	MappingSubsystem.Reset();
}

void AKnightRPGUIPlayerController::BindRPGSources()
{
	if (bEndingUIPlay || !IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}
	AKnightRPGGameState* State = GetWorld()->GetGameState<AKnightRPGGameState>();
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

void AKnightRPGUIPlayerController::UnbindRPGSources()
{
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

void AKnightRPGUIPlayerController::HandleRPGStateReady(AKnightRPGStateActor* InState)
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
				[this](const FKnightRPGLoopSnapshot& Snapshot)
				{
					RefreshRPGPresentation();
				});
		}
	}
	RefreshRPGPresentation();
}

void AKnightRPGUIPlayerController::HandleControlledKnight(AKnightCharacter* Previous, AKnightCharacter* Current)
{
	CloseRPGPanel();
	UnbindDisplayedKnight();
	DisplayedKnight = Current;
	if (IsValid(Current))
	{
		DisplayedASC = Current->GetKnightAbilitySystem();
		LifeHandle = Current->GetLifeComponent()->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleLifeChanged);
		ProjectionHandle = Current->GetKnightAbilitySystem()->OnSourceProjectionChanged.AddUObject(
			this, &ThisClass::HandleProjectionChanged);
	}
	RefreshRPGPresentation();
}

void AKnightRPGUIPlayerController::UnbindDisplayedKnight()
{
	if (DisplayedKnight.IsValid())
	{
		DisplayedKnight->GetLifeComponent()->OnLifeStateChanged.Remove(LifeHandle);
	}
	if (DisplayedASC.IsValid())
	{
		DisplayedASC->OnSourceProjectionChanged.Remove(ProjectionHandle);
	}
	LifeHandle.Reset();
	ProjectionHandle.Reset();
	DisplayedKnight.Reset();
	DisplayedASC.Reset();
}

void AKnightRPGUIPlayerController::HandleLifeChanged(const EKnightLifeState Previous, const EKnightLifeState Current)
{
	if (Current != EKnightLifeState::Alive)
	{
		CloseRPGPanel();
	}
	RefreshRPGPresentation();
}

void AKnightRPGUIPlayerController::HandleProjectionChanged(const FKnightSourceProjection& Projection)
{
	RefreshRPGPresentation();
}

FKnightRPGCommand AKnightRPGUIPlayerController::CreateCurrentCommand(const EKnightRPGOperation Operation) const
{
	FKnightRPGCommand Command;
	Command.Operation = Operation;
	Command.RequestId = FGuid::NewGuid();
	AKnightCharacter* Knight = GetControlledKnight();
	if (Knight != nullptr && RPGState.IsValid())
	{
		const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
		const FKnightRPGKnightView KnightView = RPGState->GetRPG()->GetKnightView(Knight->GetKnightEntityId());
		Command.ExpectedKnightId = Knight->GetKnightEntityId();
		Command.InventoryId = Inventory.InventoryId;
		Command.ExpectedInventoryRevision = Inventory.Revision;
		Command.ExpectedKnightRevision = KnightView.Revision;
	}
	return Command;
}

void AKnightRPGUIPlayerController::RequestPickup()
{
	AKnightCharacter* Knight = GetControlledKnight();
	if (bEndingUIPlay || !IsLocalController() || Knight == nullptr || GetWorld() == nullptr
		|| !Knight->GetLifeComponent()->IsAlive())
	{
		return;
	}
	AKnightItemPickup* Nearest = nullptr;
	double Best = FMath::Square(static_cast<double>(KnightRPG::PickupRange));
	for (TActorIterator<AKnightItemPickup> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (!Iterator->IsClaimed())
		{
			const double Distance = FVector::DistSquared(Knight->GetActorLocation(), Iterator->GetActorLocation());
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
	FKnightRPGCommand Command = CreateCurrentCommand(EKnightRPGOperation::Pickup);
	Command.PickupId = Nearest->GetPickupId();
	RequestRPGCommand(Command);
}

void AKnightRPGUIPlayerController::ToggleRPGPanel()
{
	if (RPGPanel != nullptr && RPGPanel->IsActivated())
	{
		CloseRPGPanel();
		return;
	}
	AKnightCharacter* Knight = GetControlledKnight();
	if (bEndingUIPlay || !IsLocalController() || Knight == nullptr || !Knight->CanTransferControl())
	{
		Feedback = NSLOCTEXT("KnightRPG", "PanelBusy", "Finish your action before opening the upgrade panel.");
		RefreshRPGPresentation();
		return;
	}
	ResetControlInput();
	if (RPGPanel == nullptr)
	{
		RPGPanel = CreateWidget<UKnightRPGPanel>(this, UKnightRPGPanel::StaticClass());
		if (RPGPanel == nullptr)
		{
			Feedback = NSLOCTEXT("KnightRPG", "PanelUnavailable", "The upgrade panel is unavailable.");
			RefreshRPGPresentation();
			return;
		}
		PanelHandle = RPGPanel->OnActionRequested.AddUObject(this, &ThisClass::HandlePanelAction);
	}
	RefreshRPGPresentation();
	RPGPanel->AddToPlayerScreen(100);
	RPGPanel->ActivateWidget();
}

UKnightRPGPanel* AKnightRPGUIPlayerController::GetRPGPanel() const
{
	return RPGPanel;
}

FText AKnightRPGUIPlayerController::GetRPGFeedback() const
{
	return Feedback;
}

void AKnightRPGUIPlayerController::CloseRPGPanel()
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

void AKnightRPGUIPlayerController::HandlePanelAction(const EKnightRPGPanelAction Action)
{
	if (bEndingUIPlay || RPGPanel == nullptr || !RPGPanel->IsActivated())
	{
		return;
	}
	if (Action == EKnightRPGPanelAction::Close)
	{
		CloseRPGPanel();
		return;
	}
	if (Action == EKnightRPGPanelAction::Continue)
	{
		RequestContinueRPGLoop();
		return;
	}
	FKnightRPGCommand Command;
	switch (Action)
	{
	case EKnightRPGPanelAction::EquipWeapon:
	case EKnightRPGPanelAction::EquipRune:
	{
		const bool bRune = Action == EKnightRPGPanelAction::EquipRune;
		Command = CreateCurrentCommand(bRune ? EKnightRPGOperation::Rune : EKnightRPGOperation::Equip);
		Command.SlotTag = bRune ? KnightGameplayTags::Slot_Equipment_Rune : KnightGameplayTags::Slot_Equipment_Weapon;
		if (RPGState.IsValid())
		{
			const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
			const FKnightRPGView View = RPGState->GetRPG()->GetView();
			TSet<FGuid> Assigned;
			for (const FKnightRPGKnightView& Entry : View.Knights)
			{
				for (const FKnightEquipmentAssignment& Assignment : Entry.Assignments)
				{
					Assigned.Add(Assignment.ItemInstanceId);
				}
			}
			for (const FKnightItemInstance& Item : Inventory.Items)
			{
				const UKnightItemDefinition* Definition = Cast<UKnightItemDefinition>(
					KnightRPGPresentation::Resolve(Item.DefinitionId, UKnightItemDefinition::StaticClass()));
				if (Definition != nullptr && !Assigned.Contains(Item.InstanceId)
					&& Definition->GetItemKind() == (bRune ? EKnightItemKind::Rune : EKnightItemKind::Equipment))
				{
					Command.ItemInstanceId = Item.InstanceId;
					break;
				}
			}
		}
		if (!Command.ItemInstanceId.IsValid())
		{
			Feedback = NSLOCTEXT("KnightRPG", "NoAvailableEquipment", "No unassigned item is available. Collect loot or unequip it from its current Knight.");
			RefreshRPGPresentation();
			return;
		}
		break;
	}
	case EKnightRPGPanelAction::UnequipWeapon:
	case EKnightRPGPanelAction::UnequipRune:
	{
		Command = CreateCurrentCommand(EKnightRPGOperation::Unequip);
		Command.SlotTag = Action == EKnightRPGPanelAction::UnequipRune
			? KnightGameplayTags::Slot_Equipment_Rune : KnightGameplayTags::Slot_Equipment_Weapon;
		const FKnightRPGKnightView View = RPGState.IsValid()
			? RPGState->GetRPG()->GetKnightView(Command.ExpectedKnightId) : FKnightRPGKnightView();
		const FKnightEquipmentAssignment* Assignment = View.FindAssignment(Command.SlotTag);
		if (Assignment == nullptr)
		{
			Feedback = NSLOCTEXT("KnightRPG", "NoAssignedEquipment", "This Knight has no item assigned to that slot.");
			RefreshRPGPresentation();
			return;
		}
		Command.ItemInstanceId = Assignment->ItemInstanceId;
		break;
	}
	case EKnightRPGPanelAction::PurchaseHammerSkill:
	case EKnightRPGPanelAction::PurchaseRuneSkill:
		Command = CreateCurrentCommand(EKnightRPGOperation::PurchaseSkill);
		Command.SkillDefinitionId = FPrimaryAssetId(TEXT("KnightSkill"),
			Action == EKnightRPGPanelAction::PurchaseHammerSkill ? FName(TEXT("HammerForce")) : FName(TEXT("RuneFocus")));
		break;
	case EKnightRPGPanelAction::Respec:
		Command = CreateCurrentCommand(EKnightRPGOperation::Respec);
		break;
	default:
		return;
	}
	RequestRPGCommand(Command);
}

void AKnightRPGUIPlayerController::HandleRPGResult(const FKnightRPGCommandResult& Outcome)
{
	if (bEndingUIPlay)
	{
		return;
	}
	// A delayed receipt describes its original Knight; it never replaces the currently observed readmodel.
	Feedback = Outcome.Result.IsSuccess()
		? NSLOCTEXT("KnightRPG", "TransactionCommitted", "Upgrade transaction committed.") : Outcome.Result.Message;
	RefreshRPGPresentation();
}

void AKnightRPGUIPlayerController::HandleLoopResult(const FKnightResult& Result)
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

void AKnightRPGUIPlayerController::RefreshRPGPresentation()
{
	if (bEndingUIPlay)
	{
		return;
	}
	if (RPGPanel != nullptr)
	{
		RPGPanel->SetSnapshot(BuildPanelSnapshot());
	}
	OnRPGPresentationChanged.Broadcast();
}

FKnightRPGPanelSnapshot AKnightRPGUIPlayerController::BuildPanelSnapshot() const
{
	FKnightRPGPanelSnapshot Snapshot;
	Snapshot.Feedback = Feedback;
	AKnightCharacter* Knight = GetControlledKnight();
	if (Knight == nullptr || !RPGState.IsValid())
	{
		Snapshot.Title = NSLOCTEXT("KnightRPG", "PanelWaiting", "Waiting for RPG state");
		return Snapshot;
	}
	Snapshot.KnightId = Knight->GetKnightEntityId();
	Snapshot.Title = FText::Format(NSLOCTEXT("KnightRPG", "PanelTitle", "Upgrade {0}"), Knight->GetKnightDisplayName());
	const FKnightRPGLoopSnapshot& Loop = RPGState->GetLoopSnapshot();
	Snapshot.Objective = KnightRPGPresentation::Objective(Loop.Stage);
	Snapshot.bCanContinue = Loop.Stage == EKnightRPGLoopStage::RewardAvailable || Loop.Stage == EKnightRPGLoopStage::Upgrade;
	const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
	const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(Snapshot.KnightId);
	FString InventoryText;
	for (const FKnightItemInstance& Item : Inventory.Items)
	{
		InventoryText += FText::Format(NSLOCTEXT("KnightRPG", "InventoryLine", "{0} x{1}\n"),
			KnightRPGPresentation::Name(Item.DefinitionId), FText::AsNumber(Item.Count)).ToString();
	}
	Snapshot.Inventory = FText::Format(NSLOCTEXT("KnightRPG", "InventoryHeading", "Shared inventory:\n{0}"),
		InventoryText.IsEmpty() ? NSLOCTEXT("KnightRPG", "EmptyInventory", "No loot collected") : FText::FromString(InventoryText));
	FString EquipmentText;
	for (const FKnightEquipmentAssignment& Assignment : View.Assignments)
	{
		EquipmentText += KnightRPGPresentation::Name(Assignment.DefinitionId).ToString() + TEXT("\n");
	}
	Snapshot.Equipment = FText::Format(NSLOCTEXT("KnightRPG", "EquipmentHeading", "This Knight's equipment:\n{0}"),
		EquipmentText.IsEmpty() ? NSLOCTEXT("KnightRPG", "EmptyEquipment", "Default hammer; no rune upgrade")
		: FText::FromString(EquipmentText));
	FString SkillText;
	const TArray<FPrimaryAssetId> SkillIds{FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")),
		FPrimaryAssetId(TEXT("KnightSkill"), TEXT("RuneFocus"))};
	for (const FPrimaryAssetId& Id : SkillIds)
	{
		const UKnightSkillDefinition* Definition = Cast<UKnightSkillDefinition>(
			KnightRPGPresentation::Resolve(Id, UKnightSkillDefinition::StaticClass()));
		if (Definition == nullptr)
		{
			continue;
		}
		FString Costs;
		for (const FKnightMaterialCost& Cost : Definition->GetMaterialCosts())
		{
			Costs += FText::Format(NSLOCTEXT("KnightRPG", "MaterialCost", "; {0} {1}"),
				FText::AsNumber(Cost.Count), KnightRPGPresentation::Name(Cost.DefinitionId)).ToString();
		}
		for (const FPrimaryAssetId& Prerequisite : Definition->GetPrerequisites())
		{
			Costs += FText::Format(NSLOCTEXT("KnightRPG", "SkillPrerequisite", "; requires {0}"),
				KnightRPGPresentation::Name(Prerequisite)).ToString();
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
	return Snapshot;
}

void AKnightRPGUIPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingUIPlay = true;
	GetWorldTimerManager().ClearTimer(SourceReadinessTimer);
	CloseRPGPanel();
	if (RPGPanel != nullptr)
	{
		RPGPanel->OnActionRequested.Remove(PanelHandle);
		RPGPanel->RemoveFromParent();
	}
	PanelHandle.Reset();
	UnbindDisplayedKnight();
	UnbindRPGSources();
	OnControlledKnightChanged.Remove(ControlHandle);
	OnRPGCommandResult.Remove(ResultHandle);
	OnRPGLoopResult.Remove(LoopResultHandle);
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
