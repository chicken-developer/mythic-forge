#include "Framework/KnightPartyGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/KnightCharacter.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Definitions/KnightPartyDefinition.h"
#include "Framework/KnightGameState.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Player/KnightPlayerState.h"
#include "Settings/KnightSettings.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightPartyDefinitionTest, "Knight.Framework.PartyDefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightPartyDefinitionTest::RunTest(const FString& Parameters)
{
	UKnightHeroDefinition* Hero = NewObject<UKnightHeroDefinition>();
	TestTrue(TEXT("Authored hero identity"), Hero->TrySetIdentity(TEXT("TestHero"), TEXT("KnightHero")));
	TestTrue(TEXT("Native hero defaults validate"), Hero->ValidateHero().IsSuccess());
	const float OriginalSpeed = Hero->GetWalkSpeed();
	TestEqual(TEXT("Invalid tuning is rejected atomically"),
		Hero->TrySetPresentation(FLinearColor::Red, -1.0f, 700.0f, FVector::ZeroVector).Error,
		EKnightError::InvalidData);
	TestEqual(TEXT("Rejected tuning preserves authored movement"), Hero->GetWalkSpeed(), OriginalSpeed);
	UKnightPartyDefinition* Party = NewObject<UKnightPartyDefinition>();
	Party->TrySetIdentity(TEXT("TestParty"), TEXT("KnightParty"));
	TArray<FPrimaryAssetId> Ids
	{
		FPrimaryAssetId(TEXT("KnightHero"), TEXT("Hero_One")),
		FPrimaryAssetId(TEXT("KnightHero"), TEXT("Hero_Two")),
		FPrimaryAssetId(TEXT("KnightHero"), TEXT("Hero_Three"))
	};
	TestTrue(TEXT("Three distinct canonical heroes are accepted"), Party->TrySetHeroDefinitions(Ids).IsSuccess());
	TestTrue(TEXT("Complete party definition validates"), Party->ValidateParty().IsSuccess());
	Ids.Add(FPrimaryAssetId(TEXT("KnightHero"), TEXT("Hero_Four")));
	TestEqual(TEXT("A fourth hero is prohibited by architecture"), Party->TrySetHeroDefinitions(Ids).Error,
		EKnightError::InvalidData);
	TestEqual(TEXT("Invalid cardinality preserves canonical definition"), Party->GetHeroDefinitions().Num(), 3);
	Ids.RemoveAt(3);
	Ids[2] = Ids[0];
	TestEqual(TEXT("Duplicate hero IDs are rejected"), Party->TrySetHeroDefinitions(Ids).Error, EKnightError::DuplicateId);
	Ids[2] = FPrimaryAssetId(TEXT("KnightDefinition"), TEXT("Foundation"));
	TestEqual(TEXT("Wrong-domain IDs cannot enter a party"), Party->TrySetHeroDefinitions(Ids).Error, EKnightError::InvalidId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightPartyTransactionTest, "Knight.Framework.CanonicalPartyTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightPartyTransactionTest::RunTest(const FString& Parameters)
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
	World->GetWorldSettings()->DefaultGameMode = AKnightPartyGameMode::StaticClass();
	FURL WorldURL;
	WorldURL.Map = TEXT("PartyTestWorld");
	WorldURL.AddOption(*FString::Printf(TEXT("game=%s"), *AKnightPartyGameMode::StaticClass()->GetPathName()));
	World->SpawnActor<APlayerStart>();
	if (!TestTrue(TEXT("World creates and registers its authoritative GameMode"), World->SetGameMode(WorldURL)))
	{
		return false;
	}
	World->InitializeActorsForPlay(WorldURL);
	AKnightPartyGameMode* Mode = World->GetAuthGameMode<AKnightPartyGameMode>();
	if (!TestNotNull(TEXT("Session resolves the registered native party GameMode"), Mode)
		|| !TestNotNull(TEXT("World lifecycle initializes a real GameSession"), Mode->GameSession.Get()))
	{
		return false;
	}
	Mode->GameSession->MaxPlayers = UKnightSettings::CanonicalPartySize;
	AKnightPartyState* Party = Mode->GetPartyState();
	if (!TestTrue(TEXT("Real hero fixtures initialize the canonical party"), Mode->GetInitializationResult().IsSuccess())
		|| !TestNotNull(TEXT("Canonical PartyState is present"), Party))
	{
		return false;
	}
	TestEqual(TEXT("Exactly three canonical entries"), Party->GetRoster().Num(), 3);
	TestEqual(TEXT("Every empty human slot initially has companion ownership"), Party->GetHumanCount(), 0);
	TArray<FKnightEntityId> Ids;
	TArray<AKnightCharacter*> Knights;
	for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
	{
		const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
		if (!TestNotNull(TEXT("Every canonical slot exists"), Entry))
		{
			return false;
		}
		Ids.Add(Entry->KnightId);
		Knights.Add(Entry->Knight);
	}
	TestEqual(TEXT("An initialized roster cannot be silently recreated"),
		Party->InitializeRoster(Party->GetRoster()).Error, EKnightError::InvalidState);

	const TFunction<AKnightPlayerController*()> CreateHuman = [World]() -> AKnightPlayerController*
	{
		AKnightPlayerController* Player = World->SpawnActor<AKnightPlayerController>();
		APlayerState* InitialState = Player->PlayerState;
		FActorSpawnParameters StateParameters;
		StateParameters.Owner = Player;
		AKnightPlayerState* Human = World->SpawnActor<AKnightPlayerState>(StateParameters);
		Player->SetPlayerState(Human);
		if (InitialState != nullptr)
		{
			InitialState->Destroy();
		}
		return Player;
	};
	AKnightPlayerController* Human = CreateHuman();
	Mode->RestartPlayer(Human);
	TestEqual(TEXT("Solo assignment gives one human and two companions"), Party->GetHumanCount(), 1);
	TestEqual(TEXT("Human connection is assigned the first available Knight"), Party->GetControlledSlot(Human), 0);
	const FGuid HumanId = Human->GetPlayerState<AKnightPlayerState>()->GetHumanId();
	TestTrue(TEXT("Human connection identity is valid"), HumanId.IsValid());

	const int32 RevisionBeforeFall = Party->GetRosterRevision();
	const FKnightTrainingProgress ProgressBeforeFall = Mode->GetGameState<AKnightGameState>()->GetTrainingProgress();
	for (int32 FallenSlot = 0; FallenSlot <= 1; ++FallenSlot)
	{
		AKnightCharacter* FallenKnight = Knights[FallenSlot];
		AController* ControllerBeforeFall = FallenKnight->GetController();
		const FPrimaryAssetId HeroBeforeFall = FallenKnight->GetHeroDefinitionId();
		const FVector FallenLocation(2500.0, 0.0, World->GetWorldSettings()->KillZ - 100.0f);
		FallenKnight->SetActorLocationAndRotation(FallenLocation, FRotator(0.0, 110.0, 0.0),
			false, nullptr, ETeleportType::TeleportPhysics);
		FallenKnight->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		FallenKnight->GetCharacterMovement()->Velocity = FVector(50.0, 0.0, -3000.0);
		FallenKnight->AddMovementInput(FVector::ForwardVector, 1.0f, true);
		TestFalse(TEXT("The fall setup contains actual pending movement input"),
			FallenKnight->GetPendingMovementInputVector().IsNearlyZero());
		FallenKnight->FellOutOfWorld(*GetDefault<UDamageType>());
		if (!TestTrue(TEXT("The engine fall hook preserves the canonical actor"),
			IsValid(FallenKnight) && !FallenKnight->IsActorBeingDestroyed()))
		{
			return false;
		}
		TestTrue(TEXT("The fallen Knight returns to its authored recovery transform"),
			FallenKnight->GetActorLocation().Equals(Mode->GetSpawnLocationForSlot(FallenSlot))
			&& FallenKnight->GetActorRotation().Equals(FRotator::ZeroRotator));
		TestTrue(TEXT("Recovery preserves actual human or companion possession"),
			FallenKnight->GetController() == ControllerBeforeFall
			&& ControllerBeforeFall->GetPawn() == FallenKnight);
		TestTrue(TEXT("Recovery preserves the Knight and hero identities"),
			FallenKnight->GetKnightEntityId() == Ids[FallenSlot]
			&& FallenKnight->GetHeroDefinitionId() == HeroBeforeFall
			&& Party->FindSlot(FallenSlot)->Knight == FallenKnight);
		TestEqual(TEXT("Recovery returns movement to walking"),
			FallenKnight->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
		TestTrue(TEXT("Recovery removes fall velocity and input"),
			FallenKnight->GetCharacterMovement()->Velocity.IsNearlyZero()
			&& FallenKnight->GetPendingMovementInputVector().IsNearlyZero());
	}
	TestEqual(TEXT("Fall recovery does not commit an ownership change"), Party->GetRosterRevision(), RevisionBeforeFall);
	TestEqual(TEXT("Fall recovery does not change training progress"),
		Mode->GetGameState<AKnightGameState>()->GetTrainingProgress().Revision, ProgressBeforeFall.Revision);
	TestEqual(TEXT("The same human slot survives falling"), Party->GetControlledSlot(Human), 0);
	TestTrue(TEXT("All three canonical actors remain ready after falls"), Party->IsCanonicalRosterReady());
	TestEqual(TEXT("Recovery rejects an actor outside the canonical ownership contract"),
		Mode->TryRecoverKnightFromFall(nullptr).Error, EKnightError::NotOwned);

	// Detach the human identity only after actual target possession to fail the domain commit, not preflight.
	const TStrongObjectPtr<AKnightPlayerState> RetainedHumanState(Human->GetPlayerState<AKnightPlayerState>());
	const TWeakObjectPtr<AKnightPlayerController> FaultOwner(Human);
	const TWeakObjectPtr<AKnightCharacter> FaultTarget(Knights[1]);
	AAIController* OriginalCompanion = CastChecked<AAIController>(Knights[1]->GetController());
	const FTransform OriginalHumanTransform = Knights[0]->GetActorTransform();
	const FTransform OriginalTargetTransform = Knights[1]->GetActorTransform();
	Knights[0]->GetCharacterMovement()->Velocity = FVector(25.0, 15.0, 0.0);
	Knights[1]->GetCharacterMovement()->Velocity = FVector(-10.0, 20.0, 0.0);
	const FVector OriginalHumanVelocity = Knights[0]->GetCharacterMovement()->Velocity;
	const FVector OriginalTargetVelocity = Knights[1]->GetCharacterMovement()->Velocity;
	const int32 OriginalRosterRevision = Party->GetRosterRevision();
	AKnightGameState* State = Mode->GetGameState<AKnightGameState>();
	const FKnightTrainingProgress OriginalProgress = State->GetTrainingProgress();
	bool bFaultInjected = false;
	const FDelegateHandle FaultHandle = Human->OnControlledKnightChanged.AddWeakLambda(Human,
		[FaultOwner, FaultTarget, &bFaultInjected](AKnightCharacter* Previous, AKnightCharacter* Current)
		{
			if (!bFaultInjected && Current == FaultTarget.Get())
			{
				if (AKnightPlayerController* FaultHuman = FaultOwner.Get())
				{
					bFaultInjected = true;
					FaultHuman->SetPlayerState(nullptr);
				}
			}
		});
	AddExpectedError(TEXT("Knight ownership commit failed; rollback succeeded"),
		EAutomationExpectedErrorFlags::Contains, 1);
	const FKnightResult FaultedSwitch = Mode->TrySwitchKnight(Human, 1);
	Human->OnControlledKnightChanged.Remove(FaultHandle);
	Human->SetPlayerState(RetainedHumanState.Get());
	TestTrue(TEXT("Fault injection executes after actual target possession"), bFaultInjected);
	TestEqual(TEXT("Missing identity causes the actual ownership commit to fail"), FaultedSwitch.Error,
		EKnightError::InvalidState);
	TestTrue(TEXT("Rollback restores the original human controller and Knight"),
		Human->GetPawn() == Knights[0] && Knights[0]->GetController() == Human);
	TestTrue(TEXT("Rollback restores the original target companion controller"),
		OriginalCompanion->GetPawn() == Knights[1] && Knights[1]->GetController() == OriginalCompanion);
	TestEqual(TEXT("Failed commit preserves public slot assignment"), Party->GetControlledSlot(Human), 0);
	TestEqual(TEXT("Failed commit preserves human PlayerState assignment"), RetainedHumanState->GetAssignedSlot(), 0);
	TestEqual(TEXT("Failed commit does not advance authoritative roster revision"),
		Party->GetRosterRevision(), OriginalRosterRevision);
	TestTrue(TEXT("Rollback preserves both Knight transforms"),
		Knights[0]->GetActorTransform().Equals(OriginalHumanTransform)
		&& Knights[1]->GetActorTransform().Equals(OriginalTargetTransform));
	TestTrue(TEXT("Rollback preserves movement state"),
		Knights[0]->GetCharacterMovement()->Velocity.Equals(OriginalHumanVelocity)
		&& Knights[1]->GetCharacterMovement()->Velocity.Equals(OriginalTargetVelocity));
	TestEqual(TEXT("Failed transaction cannot count as training progress"),
		State->GetTrainingProgress().Revision, OriginalProgress.Revision);
	TestEqual(TEXT("Failed transaction preserves controlled Knight progress"),
		State->GetTrainingProgress().ControlledMask, OriginalProgress.ControlledMask);
	for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
	{
		TestTrue(TEXT("Rollback keeps the original canonical actor and stable identity"),
			Party->FindSlot(Slot)->Knight == Knights[Slot] && Party->FindSlot(Slot)->KnightId == Ids[Slot]);
	}
	for (int32 DesiredSlot = 1; DesiredSlot <= 2; ++DesiredSlot)
	{
		const FTransform PreviousTransform = Human->GetPawn()->GetActorTransform();
		AKnightCharacter* PreviousKnight = CastChecked<AKnightCharacter>(Human->GetPawn());
		const FKnightResult Switch = Mode->TrySwitchKnight(Human, DesiredSlot);
		TestTrue(TEXT("Valid possession resumes after lifecycle fault rollback"), Switch.IsSuccess());
		TestTrue(TEXT("Previous Knight transform survives control transfer"),
			PreviousKnight->GetActorTransform().Equals(PreviousTransform));
		TestEqual(TEXT("Human identity never becomes Knight identity"),
			Human->GetPlayerState<AKnightPlayerState>()->GetHumanId(), HumanId);
	}
	TestEqual(TEXT("Invalid network slot is rejected"), Mode->TrySwitchKnight(Human, 99).Error, EKnightError::OutOfRange);
	Knights[0]->GetCharacterMovement()->SetMovementMode(MOVE_None);
	TestEqual(TEXT("Unavailable gameplay state blocks switching before mutation"),
		Mode->TrySwitchKnight(Human, 0).Error, EKnightError::InvalidState);
	TestEqual(TEXT("Rejected switch preserves current human assignment"), Party->GetControlledSlot(Human), 2);
	Knights[0]->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	for (int32 Marker = 0; Marker < AKnightGameState::RequiredMarkerCount; ++Marker)
	{
		TestTrue(TEXT("Authoritative marker command records real progress"),
			State->RecordTrainingMarker(Human, Marker).IsSuccess());
	}
	TestTrue(TEXT("All controls and route markers complete the drill"), State->GetTrainingProgress().bCompleted);
	TestTrue(TEXT("Retry resets the actual domain progress"), Mode->TryRestartDrill(Human).IsSuccess());
	TestFalse(TEXT("Retry removes prior completion"), State->GetTrainingProgress().bCompleted);
	TestEqual(TEXT("Retry clears marker progress"), State->GetTrainingProgress().MarkerMask, static_cast<uint8>(0));
	Mode->Logout(Human);
	TestEqual(TEXT("Disconnect returns the same Knight to AI ownership"), Party->GetHumanCount(), 0);
	TestEqual(TEXT("Human slot is cleared on disconnect"), Human->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot(), INDEX_NONE);
	AKnightPlayerController* Rejoined = CreateHuman();
	TestTrue(TEXT("A reconnect can request its previous free slot"),
		Mode->InitNewPlayer(Rejoined, FUniqueNetIdRepl(), TEXT("?KnightSlot=2"), FString()).IsEmpty());
	Mode->RestartPlayer(Rejoined);
	TestEqual(TEXT("Rejoined human reclaims the existing Knight"), Party->GetControlledSlot(Rejoined), 2);
	AKnightPlayerController* Second = CreateHuman();
	AKnightPlayerController* Third = CreateHuman();
	Mode->RestartPlayer(Second);
	Mode->RestartPlayer(Third);
	TestEqual(TEXT("Co-op assignment supports exactly three humans"), Party->GetHumanCount(), 3);
	FString FourthError;
	Mode->PreLogin(FString(), TEXT("127.0.0.1"), FUniqueNetIdRepl(), FourthError);
	TestFalse(TEXT("Fourth connection is rejected before assignment"), FourthError.IsEmpty());
	TestEqual(TEXT("Solo transfer is disabled for concurrent human owners"),
		Mode->TrySwitchKnight(Rejoined, 0).Error, EKnightError::InvalidState);
	int32 CharacterCount = 0;
	for (TActorIterator<AKnightCharacter> Iterator(World); Iterator; ++Iterator)
	{
		++CharacterCount;
	}
	TestEqual(TEXT("Join, transfer, disconnect and reclaim never create a fourth Knight"), CharacterCount, 3);
	for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
	{
		TestTrue(TEXT("Canonical actor survives every ownership lifecycle"), Party->FindSlot(Slot)->Knight == Knights[Slot]);
		TestTrue(TEXT("Persistent Knight ID survives every ownership lifecycle"), Party->FindSlot(Slot)->KnightId == Ids[Slot]);
	}
	return true;
}

#endif
