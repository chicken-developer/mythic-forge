#include "Framework/GolemBootGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Assets/GolemDefinition.h"
#include "Engine/World.h"
#include "Framework/GolemBootHUD.h"
#include "GameFramework/DefaultPawn.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Settings/GolemSettings.h"
#include "Tags/GolemGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemBoot, Log, All);

AGolemBootGameMode::AGolemBootGameMode()
{
	HUDClass = AGolemBootHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AGolemBootGameMode::StartPlay()
{
	Super::StartPlay();
	FoundationStatus.Reset();
	bIsFoundationReady = true;
	const TArray<FName> Modules =
	{
		TEXT("ThreeDreamGolems"), TEXT("GolemCore"), TEXT("GolemFramework"),
		TEXT("GolemAbilitySystem"), TEXT("GolemCombat"), TEXT("GolemAI"), TEXT("GolemItems"),
		TEXT("GolemQuest"), TEXT("GolemDialogue"), TEXT("GolemWorld"), TEXT("GolemUI"),
		TEXT("GolemOnline"), TEXT("GolemSave")
	};
	for (const FName Module : Modules)
	{
		const bool bIsLoaded = FModuleManager::Get().IsModuleLoaded(Module);
		bIsFoundationReady &= bIsLoaded;
		FoundationStatus.Add(FText::Format(NSLOCTEXT("GolemBoot", "ModuleStatus", "{0}: {1}"),
			FText::FromName(Module), bIsLoaded ? NSLOCTEXT("GolemBoot", "Loaded", "loaded")
				: NSLOCTEXT("GolemBoot", "MissingModule", "FAILED to load")));
	}

	UGolemAssetManager* AssetManager = UGolemAssetManager::GetGolemAssetManager();
	bIsFoundationReady &= AssetManager != nullptr && AssetManager->GetCatalogErrors().IsEmpty();
	if (AssetManager != nullptr)
	{
		for (const FGolemResult& CatalogError : AssetManager->GetCatalogErrors())
		{
			FoundationStatus.Add(CatalogError.Message);
		}
	}
	const FPrimaryAssetId FixtureId = GetDefault<UGolemSettings>()->FoundationDefinition;
	UGolemDefinition* Fixture = nullptr;
	const FGolemResult FixtureResult = AssetManager != nullptr
		? AssetManager->ResolveDefinition(FixtureId, Fixture)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("AssetManager"),
			NSLOCTEXT("GolemBoot", "ManagerMissing", "Golem Asset Manager is not configured"));
	const bool bIsFixtureLoaded = FixtureResult.IsSuccess() && Fixture != nullptr;
	bIsFoundationReady &= bIsFixtureLoaded;
	FoundationStatus.Add(FText::Format(NSLOCTEXT("GolemBoot", "FixtureResult", "{0}: {1}"),
		FText::FromString(FixtureId.ToString()), bIsFixtureLoaded
			? NSLOCTEXT("GolemBoot", "FixtureLoaded", "resolved through Asset Manager") : FixtureResult.Message));

	const FPrimaryAssetId MissingId(TEXT("GolemDefinition"), TEXT("Missing"));
	UGolemDefinition* MissingDefinition = nullptr;
	const bool bIsMissingRejected = AssetManager != nullptr
		&& AssetManager->ResolveDefinition(MissingId, MissingDefinition).Error == EGolemError::NotFound
		&& MissingDefinition == nullptr;
	bIsFoundationReady &= bIsMissingRejected;
	FoundationStatus.Add(bIsMissingRejected
		? NSLOCTEXT("GolemBoot", "MissingRejected", "GolemDefinition:Missing rejected: definition not registered")
		: NSLOCTEXT("GolemBoot", "MissingFailed", "FAILED: unexpected definition registered for Missing"));

	UGolemEventSubsystem* EventBus = GetWorld()->GetSubsystem<UGolemEventSubsystem>();
	ReceivedFoundationEvents = 0;
	if (EventBus != nullptr && EventBus->IsReady())
	{
		EventBus->Unsubscribe(FoundationSubscription);
		FoundationSubscription = EventBus->Subscribe<FGolemFoundationEvent>(this,
			[this](const FGolemFoundationEvent& Event)
			{
				++ReceivedFoundationEvents;
			});
		FGolemFoundationEvent StartedEvent;
		StartedEvent.ConfirmationId = FGuid::NewGuid();
		StartedEvent.Revision = 1;
		StartedEvent.Stage = GolemGameplayTags::Event_Foundation_Started;
		const FGolemResult StartedResult = EventBus->PublishConfirmed(StartedEvent);
		const bool bIsReplayRejected = EventBus->PublishConfirmed(StartedEvent).Error
			== EGolemError::DuplicateConfirmation;
		FGolemFoundationEvent LoadedEvent;
		LoadedEvent.ConfirmationId = FGuid::NewGuid();
		LoadedEvent.Revision = 2;
		LoadedEvent.Stage = GolemGameplayTags::Event_Foundation_AssetLoaded;
		LoadedEvent.DefinitionId = FixtureId;
		const FGolemResult LoadedResult = bIsFixtureLoaded ? EventBus->PublishConfirmed(LoadedEvent)
			: FixtureResult;
		const bool bIsLifecycleValid = StartedResult.IsSuccess() && LoadedResult.IsSuccess()
			&& bIsReplayRejected && ReceivedFoundationEvents == 2;
		bIsFoundationReady &= bIsLifecycleValid;
		FoundationStatus.Add(bIsLifecycleValid
			? NSLOCTEXT("GolemBoot", "EventsReady", "Typed lifecycle events delivered once; replay rejected")
			: NSLOCTEXT("GolemBoot", "EventsFailed", "FAILED: typed lifecycle event delivery"));
	}
	else
	{
		bIsFoundationReady = false;
		FoundationStatus.Add(NSLOCTEXT("GolemBoot", "EventBusMissing", "FAILED: event subsystem initialization"));
	}

	UE_LOG(LogGolemBoot, Display, TEXT("FoundationReady=%s Fixture=%s Modules=%d"),
		bIsFoundationReady ? TEXT("true") : TEXT("false"),
		Fixture != nullptr ? *Fixture->GetPathName() : *FixtureResult.Message.ToString(), Modules.Num());
	OnFoundationReady.Broadcast(FoundationStatus, bIsFoundationReady);
	if (FParse::Param(FCommandLine::Get(), TEXT("GolemBootSmoke")))
	{
		FPlatformMisc::RequestExitWithStatus(false, bIsFoundationReady ? 0 : 1);
	}
}

void AGolemBootGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGolemEventSubsystem* EventBus = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		if (EventBus->IsReady())
		{
			FGolemFoundationEvent StoppedEvent;
			StoppedEvent.ConfirmationId = FGuid::NewGuid();
			StoppedEvent.Revision = 3;
			StoppedEvent.Stage = GolemGameplayTags::Event_Foundation_Stopped;
			EventBus->PublishConfirmed(StoppedEvent);
		}
		EventBus->Unsubscribe(FoundationSubscription);
	}
	Super::EndPlay(EndPlayReason);
}
