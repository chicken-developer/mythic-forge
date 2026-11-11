#include "Framework/GolemPartyGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/GolemCharacter.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Definitions/GolemPartyDefinition.h"
#include "Framework/GolemGameState.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Player/GolemPlayerState.h"
#include "Settings/GolemSettings.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemPartyDefinitionTest, "Golem.Framework.PartyDefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemPartyDefinitionTest::RunTest(const FString& Parameters)
{
	UGolemHeroDefinition* Hero = NewObject<UGolemHeroDefinition>();
	TestTrue(TEXT("Authored hero identity"), Hero->TrySetIdentity(TEXT("TestHero"), TEXT("GolemHero")));
	TestTrue(TEXT("Native hero defaults validate"), Hero->ValidateHero().IsSuccess());
	const float OriginalSpeed = Hero->GetWalkSpeed();
	TestEqual(TEXT("Invalid tuning is rejected atomically"),
		Hero->TrySetPresentation(FLinearColor::Red, -1.0f, 700.0f, FVector::ZeroVector).Error,
		EGolemError::InvalidData);
	TestEqual(TEXT("Rejected tuning preserves authored movement"), Hero->GetWalkSpeed(), OriginalSpeed);
	UGolemPartyDefinition* Party = NewObject<UGolemPartyDefinition>();
	Party->TrySetIdentity(TEXT("TestParty"), TEXT("GolemParty"));
	TArray<FPrimaryAssetId> Ids
	{
		FPrimaryAssetId(TEXT("GolemHero"), TEXT("Hero_One")),
		FPrimaryAssetId(TEXT("GolemHero"), TEXT("Hero_Two")),
		FPrimaryAssetId(TEXT("GolemHero"), TEXT("Hero_Three"))
	};
	TestTrue(TEXT("Three distinct canonical heroes are accepted"), Party->TrySetHeroDefinitions(Ids).IsSuccess());
	TestTrue(TEXT("Complete party definition validates"), Party->ValidateParty().IsSuccess());
	Ids.Add(FPrimaryAssetId(TEXT("GolemHero"), TEXT("Hero_Four")));
	TestEqual(TEXT("A fourth hero is prohibited by architecture"), Party->TrySetHeroDefinitions(Ids).Error,
		EGolemError::InvalidData);
	TestEqual(TEXT("Invalid cardinality preserves canonical definition"), Party->GetHeroDefinitions().Num(), 3);
	Ids.RemoveAt(3);
	Ids[2] = Ids[0];
	TestEqual(TEXT("Duplicate hero IDs are rejected"), Party->TrySetHeroDefinitions(Ids).Error, EGolemError::DuplicateId);
	Ids[2] = FPrimaryAssetId(TEXT("GolemDefinition"), TEXT("Foundation"));
	TestEqual(TEXT("Wrong-domain IDs cannot enter a party"), Party->TrySetHeroDefinitions(Ids).Error, EGolemError::InvalidId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemPartyTransactionTest, "Golem.Framework.CanonicalPartyTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemPartyTransactionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	ON_SCOPE_EXIT
	{
		GameInstance->Shutdown();
		if (World != nullptr)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
	if (!TestNotNull(TEXT("Initialized GameInstance owns the test world"), World))
	{
		return false;
	}
	World->SetShouldTick(false);
	World->GetWorldSettings()->DefaultGameMode = AGolemPartyGameMode::StaticClass();
	FURL WorldURL;
	WorldURL.Map = TEXT("PartyTestWorld");
	WorldURL.AddOption(*FString::Printf(TEXT("game=%s"), *AGolemPartyGameMode::StaticClass()->GetPathName()));
	World->SpawnActor<APlayerStart>();
	if (!TestTrue(TEXT("World creates and registers its authoritative GameMode"), World->SetGameMode(WorldURL)))
	{
		return false;
	}
	World->InitializeActorsForPlay(WorldURL);
	AGolemPartyGameMode* Mode = World->GetAuthGameMode<AGolemPartyGameMode>();
	if (!TestNotNull(TEXT("Session resolves the registered native party GameMode"), Mode)
		|| !TestNotNull(TEXT("World lifecycle initializes a real GameSession"), Mode->GameSession.Get()))
	{
		return false;
	}
	Mode->GameSession->MaxPlayers = UGolemSettings::CanonicalPartySize;
	AGolemPartyState* Party = Mode->GetPartyState();
	if (!TestTrue(TEXT("Real hero fixtures initialize the canonical party"), Mode->GetInitializationResult().IsSuccess())
		|| !TestNotNull(TEXT("Canonical PartyState is present"), Party))
	{
		return false;
	}
	TestEqual(TEXT("Exactly three canonical entries"), Party->GetRoster().Num(), 3);
	TestEqual(TEXT("Every empty human slot initially has companion ownership"), Party->GetHumanCount(), 0);
	TArray<FGolemEntityId> Ids;
	TArray<AGolemCharacter*> Golems;
	for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
	{
		const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
		if (!TestNotNull(TEXT("Every canonical slot exists"), Entry))
		{
			return false;
		}
		Ids.Add(Entry->GolemId);
		Golems.Add(Entry->Golem);
	}
	TestEqual(TEXT("An initialized roster cannot be silently recreated"),
		Party->InitializeRoster(Party->GetRoster()).Error, EGolemError::InvalidState);

	const TFunction<AGolemPlayerController*()> CreateHuman = [World]() -> AGolemPlayerController*
	{
		AGolemPlayerController* Player = World->SpawnActor<AGolemPlayerController>();
		APlayerState* InitialState = Player->PlayerState;
		FActorSpawnParameters StateParameters;
		StateParameters.Owner = Player;
		AGolemPlayerState* Human = World->SpawnActor<AGolemPlayerState>(StateParameters);
		Player->SetPlayerState(Human);
		if (InitialState != nullptr)
		{
			InitialState->Destroy();
		}
		return Player;
	};
	AGolemPlayerController* Human = CreateHuman();
	Mode->RestartPlayer(Human);
	TestEqual(TEXT("Solo assignment gives one human and two companions"), Party->GetHumanCount(), 1);
	TestEqual(TEXT("Human connection is assigned the first available Golem"), Party->GetControlledSlot(Human), 0);
	const FGuid HumanId = Human->GetPlayerState<AGolemPlayerState>()->GetHumanId();
	TestTrue(TEXT("Human connection identity is valid"), HumanId.IsValid());

	const int32 RevisionBeforeFall = Party->GetRosterRevision();
	const FGolemTrainingProgress ProgressBeforeFall = Mode->GetGameState<AGolemGameState>()->GetTrainingProgress();
	for (int32 FallenSlot = 0; FallenSlot <= 1; ++FallenSlot)
	{
		AGolemCharacter* FallenGolem = Golems[FallenSlot];
		AController* ControllerBeforeFall = FallenGolem->GetController();
		const FPrimaryAssetId HeroBeforeFall = FallenGolem->GetHeroDefinitionId();
		const FVector FallenLocation(2500.0, 0.0, World->GetWorldSettings()->KillZ - 100.0f);
		FallenGolem->SetActorLocationAndRotation(FallenLocation, FRotator(0.0, 110.0, 0.0),
			false, nullptr, ETeleportType::TeleportPhysics);
		FallenGolem->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		FallenGolem->GetCharacterMovement()->Velocity = FVector(50.0, 0.0, -3000.0);
		FallenGolem->AddMovementInput(FVector::ForwardVector, 1.0f, true);
		TestFalse(TEXT("The fall setup contains actual pending movement input"),
			FallenGolem->GetPendingMovementInputVector().IsNearlyZero());
		FallenGolem->FellOutOfWorld(*GetDefault<UDamageType>());
		if (!TestTrue(TEXT("The engine fall hook preserves the canonical actor"),
			IsValid(FallenGolem) && !FallenGolem->IsActorBeingDestroyed()))
		{
			return false;
		}
		TestTrue(TEXT("The fallen Golem returns to its authored recovery transform"),
			FallenGolem->GetActorLocation().Equals(Mode->GetSpawnLocationForSlot(FallenSlot))
			&& FallenGolem->GetActorRotation().Equals(FRotator::ZeroRotator));
		TestTrue(TEXT("Recovery preserves actual human or companion possession"),
			FallenGolem->GetController() == ControllerBeforeFall
			&& ControllerBeforeFall->GetPawn() == FallenGolem);
		TestTrue(TEXT("Recovery preserves the Golem and hero identities"),
			FallenGolem->GetGolemEntityId() == Ids[FallenSlot]
			&& FallenGolem->GetHeroDefinitionId() == HeroBeforeFall
			&& Party->FindSlot(FallenSlot)->Golem == FallenGolem);
		TestEqual(TEXT("Recovery returns movement to walking"),
			FallenGolem->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
		TestTrue(TEXT("Recovery removes fall velocity and input"),
			FallenGolem->GetCharacterMovement()->Velocity.IsNearlyZero()
			&& FallenGolem->GetPendingMovementInputVector().IsNearlyZero());
	}
	TestEqual(TEXT("Fall recovery does not commit an ownership change"), Party->GetRosterRevision(), RevisionBeforeFall);
	TestEqual(TEXT("Fall recovery does not change training progress"),
		Mode->GetGameState<AGolemGameState>()->GetTrainingProgress().Revision, ProgressBeforeFall.Revision);
	TestEqual(TEXT("The same human slot survives falling"), Party->GetControlledSlot(Human), 0);
	TestTrue(TEXT("All three canonical actors remain ready after falls"), Party->IsCanonicalRosterReady());
	TestEqual(TEXT("Recovery rejects an actor outside the canonical ownership contract"),
		Mode->TryRecoverGolemFromFall(nullptr).Error, EGolemError::NotOwned);

	// Detach the human identity only after actual target possession to fail the domain commit, not preflight.
	const TStrongObjectPtr<AGolemPlayerState> RetainedHumanState(Human->GetPlayerState<AGolemPlayerState>());
	const TWeakObjectPtr<AGolemPlayerController> FaultOwner(Human);
	const TWeakObjectPtr<AGolemCharacter> FaultTarget(Golems[1]);
	AAIController* OriginalCompanion = CastChecked<AAIController>(Golems[1]->GetController());
	const FTransform OriginalHumanTransform = Golems[0]->GetActorTransform();
	const FTransform OriginalTargetTransform = Golems[1]->GetActorTransform();
	Golems[0]->GetCharacterMovement()->Velocity = FVector(25.0, 15.0, 0.0);
	Golems[1]->GetCharacterMovement()->Velocity = FVector(-10.0, 20.0, 0.0);
	const FVector OriginalHumanVelocity = Golems[0]->GetCharacterMovement()->Velocity;
	const FVector OriginalTargetVelocity = Golems[1]->GetCharacterMovement()->Velocity;
	const int32 OriginalRosterRevision = Party->GetRosterRevision();
	AGolemGameState* State = Mode->GetGameState<AGolemGameState>();
	const FGolemTrainingProgress OriginalProgress = State->GetTrainingProgress();
	bool bFaultInjected = false;
	const FDelegateHandle FaultHandle = Human->OnControlledGolemChanged.AddWeakLambda(Human,
		[FaultOwner, FaultTarget, &bFaultInjected](AGolemCharacter* Previous, AGolemCharacter* Current)
		{
			if (!bFaultInjected && Current == FaultTarget.Get())
			{
				if (AGolemPlayerController* FaultHuman = FaultOwner.Get())
				{
					bFaultInjected = true;
					FaultHuman->SetPlayerState(nullptr);
				}
			}
		});
	AddExpectedError(TEXT("Golem ownership commit failed; rollback succeeded"),
		EAutomationExpectedErrorFlags::Contains, 1);
	const FGolemResult FaultedSwitch = Mode->TrySwitchGolem(Human, 1);
	Human->OnControlledGolemChanged.Remove(FaultHandle);
	Human->SetPlayerState(RetainedHumanState.Get());
	TestTrue(TEXT("Fault injection executes after actual target possession"), bFaultInjected);
	TestEqual(TEXT("Missing identity causes the actual ownership commit to fail"), FaultedSwitch.Error,
		EGolemError::InvalidState);
	TestTrue(TEXT("Rollback restores the original human controller and Golem"),
		Human->GetPawn() == Golems[0] && Golems[0]->GetController() == Human);
	TestTrue(TEXT("Rollback restores the original target companion controller"),
		OriginalCompanion->GetPawn() == Golems[1] && Golems[1]->GetController() == OriginalCompanion);
	TestEqual(TEXT("Failed commit preserves public slot assignment"), Party->GetControlledSlot(Human), 0);
	TestEqual(TEXT("Failed commit preserves human PlayerState assignment"), RetainedHumanState->GetAssignedSlot(), 0);
	TestEqual(TEXT("Failed commit does not advance authoritative roster revision"),
		Party->GetRosterRevision(), OriginalRosterRevision);
	TestTrue(TEXT("Rollback preserves both Golem transforms"),
		Golems[0]->GetActorTransform().Equals(OriginalHumanTransform)
		&& Golems[1]->GetActorTransform().Equals(OriginalTargetTransform));
	TestTrue(TEXT("Rollback preserves movement state"),
		Golems[0]->GetCharacterMovement()->Velocity.Equals(OriginalHumanVelocity)
		&& Golems[1]->GetCharacterMovement()->Velocity.Equals(OriginalTargetVelocity));
	TestEqual(TEXT("Failed transaction cannot count as training progress"),
		State->GetTrainingProgress().Revision, OriginalProgress.Revision);
	TestEqual(TEXT("Failed transaction preserves controlled Golem progress"),
		State->GetTrainingProgress().ControlledMask, OriginalProgress.ControlledMask);
	for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
	{
		TestTrue(TEXT("Rollback keeps the original canonical actor and stable identity"),
			Party->FindSlot(Slot)->Golem == Golems[Slot] && Party->FindSlot(Slot)->GolemId == Ids[Slot]);
	}
	for (int32 DesiredSlot = 1; DesiredSlot <= 2; ++DesiredSlot)
	{
		const FTransform PreviousTransform = Human->GetPawn()->GetActorTransform();
		AGolemCharacter* PreviousGolem = CastChecked<AGolemCharacter>(Human->GetPawn());
		const FGolemResult Switch = Mode->TrySwitchGolem(Human, DesiredSlot);
		TestTrue(TEXT("Valid possession resumes after lifecycle fault rollback"), Switch.IsSuccess());
		TestTrue(TEXT("Previous Golem transform survives control transfer"),
			PreviousGolem->GetActorTransform().Equals(PreviousTransform));
		TestEqual(TEXT("Human identity never becomes Golem identity"),
			Human->GetPlayerState<AGolemPlayerState>()->GetHumanId(), HumanId);
	}
	TestEqual(TEXT("Invalid network slot is rejected"), Mode->TrySwitchGolem(Human, 99).Error, EGolemError::OutOfRange);
	Golems[0]->GetCharacterMovement()->SetMovementMode(MOVE_None);
	TestEqual(TEXT("Unavailable gameplay state blocks switching before mutation"),
		Mode->TrySwitchGolem(Human, 0).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Rejected switch preserves current human assignment"), Party->GetControlledSlot(Human), 2);
	Golems[0]->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	for (int32 Marker = 0; Marker < AGolemGameState::RequiredMarkerCount; ++Marker)
	{
		TestTrue(TEXT("Authoritative marker command records real progress"),
			State->RecordTrainingMarker(Human, Marker).IsSuccess());
	}
	TestTrue(TEXT("All controls and route markers complete the drill"), State->GetTrainingProgress().bCompleted);
	TestTrue(TEXT("Retry resets the actual domain progress"), Mode->TryRestartDrill(Human).IsSuccess());
	TestFalse(TEXT("Retry removes prior completion"), State->GetTrainingProgress().bCompleted);
	TestEqual(TEXT("Retry clears marker progress"), State->GetTrainingProgress().MarkerMask, static_cast<uint8>(0));
	Mode->Logout(Human);
	TestEqual(TEXT("Disconnect returns the same Golem to AI ownership"), Party->GetHumanCount(), 0);
	TestEqual(TEXT("Human slot is cleared on disconnect"), Human->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot(), INDEX_NONE);
	AGolemPlayerController* Rejoined = CreateHuman();
	TestTrue(TEXT("A reconnect can request its previous free slot"),
		Mode->InitNewPlayer(Rejoined, FUniqueNetIdRepl(), TEXT("?GolemSlot=2"), FString()).IsEmpty());
	Mode->RestartPlayer(Rejoined);
	TestEqual(TEXT("Rejoined human reclaims the existing Golem"), Party->GetControlledSlot(Rejoined), 2);
	AGolemPlayerController* Second = CreateHuman();
	AGolemPlayerController* Third = CreateHuman();
	Mode->RestartPlayer(Second);
	Mode->RestartPlayer(Third);
	TestEqual(TEXT("Co-op assignment supports exactly three humans"), Party->GetHumanCount(), 3);
	FString FourthError;
	Mode->PreLogin(FString(), TEXT("127.0.0.1"), FUniqueNetIdRepl(), FourthError);
	TestFalse(TEXT("Fourth connection is rejected before assignment"), FourthError.IsEmpty());
	TestEqual(TEXT("Solo transfer is disabled for concurrent human owners"),
		Mode->TrySwitchGolem(Rejoined, 0).Error, EGolemError::InvalidState);
	int32 CharacterCount = 0;
	for (TActorIterator<AGolemCharacter> Iterator(World); Iterator; ++Iterator)
	{
		++CharacterCount;
	}
	TestEqual(TEXT("Join, transfer, disconnect and reclaim never create a fourth Golem"), CharacterCount, 3);
	for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
	{
		TestTrue(TEXT("Canonical actor survives every ownership lifecycle"), Party->FindSlot(Slot)->Golem == Golems[Slot]);
		TestTrue(TEXT("Persistent Golem ID survives every ownership lifecycle"), Party->FindSlot(Slot)->GolemId == Ids[Slot]);
	}
	return true;
}

#endif
