#include "Framework/KnightBootGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Assets/KnightDefinition.h"
#include "Engine/World.h"
#include "Framework/KnightBootHUD.h"
#include "GameFramework/DefaultPawn.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Settings/KnightSettings.h"
#include "Tags/KnightGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightBoot, Log, All);

AKnightBootGameMode::AKnightBootGameMode()
{
	HUDClass = AKnightBootHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AKnightBootGameMode::StartPlay()
{
	Super::StartPlay();
	FoundationStatus.Reset();
	bIsFoundationReady = true;
	const TArray<FName> Modules =
	{
		TEXT("ThreeDreamingKnights"), TEXT("KnightCore"), TEXT("KnightFramework"),
		TEXT("KnightAbilitySystem"), TEXT("KnightCombat"), TEXT("KnightAI"), TEXT("KnightItems"),
		TEXT("KnightQuest"), TEXT("KnightDialogue"), TEXT("KnightWorld"), TEXT("KnightUI"),
		TEXT("KnightOnline"), TEXT("KnightSave")
	};
	for (const FName Module : Modules)
	{
		const bool bIsLoaded = FModuleManager::Get().IsModuleLoaded(Module);
		bIsFoundationReady &= bIsLoaded;
		FoundationStatus.Add(FText::Format(NSLOCTEXT("KnightBoot", "ModuleStatus", "{0}: {1}"),
			FText::FromName(Module), bIsLoaded ? NSLOCTEXT("KnightBoot", "Loaded", "loaded")
				: NSLOCTEXT("KnightBoot", "MissingModule", "FAILED to load")));
	}

	UKnightAssetManager* AssetManager = UKnightAssetManager::GetKnightAssetManager();
	bIsFoundationReady &= AssetManager != nullptr && AssetManager->GetCatalogErrors().IsEmpty();
	if (AssetManager != nullptr)
	{
		for (const FKnightResult& CatalogError : AssetManager->GetCatalogErrors())
		{
			FoundationStatus.Add(CatalogError.Message);
		}
	}
	const FPrimaryAssetId FixtureId = GetDefault<UKnightSettings>()->FoundationDefinition;
	UKnightDefinition* Fixture = nullptr;
	const FKnightResult FixtureResult = AssetManager != nullptr
		? AssetManager->ResolveDefinition(FixtureId, Fixture)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("AssetManager"),
			NSLOCTEXT("KnightBoot", "ManagerMissing", "Knight Asset Manager is not configured"));
	const bool bIsFixtureLoaded = FixtureResult.IsSuccess() && Fixture != nullptr;
	bIsFoundationReady &= bIsFixtureLoaded;
	FoundationStatus.Add(FText::Format(NSLOCTEXT("KnightBoot", "FixtureResult", "{0}: {1}"),
		FText::FromString(FixtureId.ToString()), bIsFixtureLoaded
			? NSLOCTEXT("KnightBoot", "FixtureLoaded", "resolved through Asset Manager") : FixtureResult.Message));

	const FPrimaryAssetId MissingId(TEXT("KnightDefinition"), TEXT("Missing"));
	UKnightDefinition* MissingDefinition = nullptr;
	const bool bIsMissingRejected = AssetManager != nullptr
		&& AssetManager->ResolveDefinition(MissingId, MissingDefinition).Error == EKnightError::NotFound
		&& MissingDefinition == nullptr;
	bIsFoundationReady &= bIsMissingRejected;
	FoundationStatus.Add(bIsMissingRejected
		? NSLOCTEXT("KnightBoot", "MissingRejected", "KnightDefinition:Missing rejected: definition not registered")
		: NSLOCTEXT("KnightBoot", "MissingFailed", "FAILED: unexpected definition registered for Missing"));

	UKnightEventSubsystem* EventBus = GetWorld()->GetSubsystem<UKnightEventSubsystem>();
	ReceivedFoundationEvents = 0;
	if (EventBus != nullptr && EventBus->IsReady())
	{
		EventBus->Unsubscribe(FoundationSubscription);
		FoundationSubscription = EventBus->Subscribe<FKnightFoundationEvent>(this,
			[this](const FKnightFoundationEvent& Event)
			{
				++ReceivedFoundationEvents;
			});
		FKnightFoundationEvent StartedEvent;
		StartedEvent.ConfirmationId = FGuid::NewGuid();
		StartedEvent.Revision = 1;
		StartedEvent.Stage = KnightGameplayTags::Event_Foundation_Started;
		const FKnightResult StartedResult = EventBus->PublishConfirmed(StartedEvent);
		const bool bIsReplayRejected = EventBus->PublishConfirmed(StartedEvent).Error
			== EKnightError::DuplicateConfirmation;
		FKnightFoundationEvent LoadedEvent;
		LoadedEvent.ConfirmationId = FGuid::NewGuid();
		LoadedEvent.Revision = 2;
		LoadedEvent.Stage = KnightGameplayTags::Event_Foundation_AssetLoaded;
		LoadedEvent.DefinitionId = FixtureId;
		const FKnightResult LoadedResult = bIsFixtureLoaded ? EventBus->PublishConfirmed(LoadedEvent)
			: FixtureResult;
		const bool bIsLifecycleValid = StartedResult.IsSuccess() && LoadedResult.IsSuccess()
			&& bIsReplayRejected && ReceivedFoundationEvents == 2;
		bIsFoundationReady &= bIsLifecycleValid;
		FoundationStatus.Add(bIsLifecycleValid
			? NSLOCTEXT("KnightBoot", "EventsReady", "Typed lifecycle events delivered once; replay rejected")
			: NSLOCTEXT("KnightBoot", "EventsFailed", "FAILED: typed lifecycle event delivery"));
	}
	else
	{
		bIsFoundationReady = false;
		FoundationStatus.Add(NSLOCTEXT("KnightBoot", "EventBusMissing", "FAILED: event subsystem initialization"));
	}

	UE_LOG(LogKnightBoot, Display, TEXT("FoundationReady=%s Fixture=%s Modules=%d"),
		bIsFoundationReady ? TEXT("true") : TEXT("false"),
		Fixture != nullptr ? *Fixture->GetPathName() : *FixtureResult.Message.ToString(), Modules.Num());
	OnFoundationReady.Broadcast(FoundationStatus, bIsFoundationReady);
	if (FParse::Param(FCommandLine::Get(), TEXT("KnightBootSmoke")))
	{
		FPlatformMisc::RequestExitWithStatus(false, bIsFoundationReady ? 0 : 1);
	}
}

void AKnightBootGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UKnightEventSubsystem* EventBus = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		if (EventBus->IsReady())
		{
			FKnightFoundationEvent StoppedEvent;
			StoppedEvent.ConfirmationId = FGuid::NewGuid();
			StoppedEvent.Revision = 3;
			StoppedEvent.Stage = KnightGameplayTags::Event_Foundation_Stopped;
			EventBus->PublishConfirmed(StoppedEvent);
		}
		EventBus->Unsubscribe(FoundationSubscription);
	}
	Super::EndPlay(EndPlayReason);
}
