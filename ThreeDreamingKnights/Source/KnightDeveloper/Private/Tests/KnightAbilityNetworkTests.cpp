#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Controllers/KnightCompanionController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/KnightGameState.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/KnightPlayerController.h"
#include "Player/KnightPlayerState.h"
#include "Settings/KnightSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/KnightGameplayTags.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightAbilityNetwork
{
	const TCHAR* AbilityMap = TEXT("/Game/Knight/Maps/Playables/L_M02_AbilityYard");
	const TCHAR* DisconnectMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	constexpr uint16 ServerPort = 17807;
	constexpr double StartupTimeoutSeconds = 45.0;
	constexpr double PhaseTimeoutSeconds = 20.0;
	constexpr double CleanupTimeoutSeconds = 15.0;
	constexpr float SourceDamage = 17.0f;
	constexpr float OtherDamage = 23.0f;
	constexpr float FixtureBleedoutSeconds = 300.0f;
	constexpr float FixtureReviveSeconds = 30.0f;

	enum class EKnightAbilityNetworkPhase : uint8
	{
		Start,
		WaitRoster,
		WaitDamage,
		WaitRevive,
		WaitDownedDamage,
		WaitTakeover,
		WaitJIP,
		Cleanup,
		Complete
	};

	AKnightPartyState* GetParty(UWorld* World)
	{
		const AKnightGameState* State = World != nullptr ? World->GetGameState<AKnightGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	int32 CountKnights(UWorld* World)
	{
		int32 Count = 0;
		if (World != nullptr)
		{
			for (TActorIterator<AKnightCharacter> Iterator(World); Iterator; ++Iterator)
			{
				++Count;
			}
		}
		return Count;
	}

	TArray<FGameplayAbilitySpecHandle> GetSpecHandles(const UKnightAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Handles;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Handles.Add(Spec.Handle);
		}
		return Handles;
	}

	struct FKnightNetworkAttributeSnapshot
	{
		TArray<float> Values;

		explicit FKnightNetworkAttributeSnapshot(const UKnightAttributeSet* Attributes)
			: Values { Attributes->GetHealth(), Attributes->GetMaxHealth(), Attributes->GetStamina(),
				Attributes->GetMaxStamina(), Attributes->GetRuneEnergy(), Attributes->GetMaxRuneEnergy(),
				Attributes->GetAttackPower(), Attributes->GetDefense() }
		{
		}

		bool Matches(const UKnightAttributeSet* Attributes) const
		{
			const FKnightNetworkAttributeSnapshot Current(Attributes);
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				if (!FMath::IsNearlyEqual(Values[Index], Current.Values[Index], 0.01f))
				{
					return false;
				}
			}
			return true;
		}
	};

	/** Runs actual network travel and owning RPCs; damage is an explicit authoritative GAS fixture setup. */
	class FKnightAbilityNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightAbilityNetworkCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual ~FKnightAbilityNetworkCommand() override
		{
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
				if (!GEditor->IsPlaySessionInProgress())
				{
					RestoreWindowSettings();
				}
			}
		}

		virtual bool Update() override
		{
			if (Phase < EKnightAbilityNetworkPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EKnightAbilityNetworkPhase::WaitRoster && Phase < EKnightAbilityNetworkPhase::Cleanup
				&& !HasPreservedServerState())
			{
				Fail(TEXT("Network lifecycle lost a pinned world/Knight or changed its identity, attributes or grants."));
			}
			if (Phase >= EKnightAbilityNetworkPhase::WaitDamage
				&& Phase <= EKnightAbilityNetworkPhase::WaitDownedDamage && !Client.IsValid())
			{
				Fail(TEXT("The owning-client controller vanished before its actual ability/travel request."));
			}
			switch (Phase)
			{
			case EKnightAbilityNetworkPhase::Start:
				StartSession();
				break;
			case EKnightAbilityNetworkPhase::WaitRoster:
				WaitForRoster();
				break;
			case EKnightAbilityNetworkPhase::WaitDamage:
				WaitForDamage();
				break;
			case EKnightAbilityNetworkPhase::WaitRevive:
				WaitForRevive();
				break;
			case EKnightAbilityNetworkPhase::WaitDownedDamage:
				if (HaveConvergedClients(3))
				{
					FScopedConditionalWorldSwitcher WorldSwitcher(GetContextWorld(DisconnectedContext));
					Client->ClientTravel(DisconnectMap, TRAVEL_Absolute);
					EnterPhase(EKnightAbilityNetworkPhase::WaitTakeover);
				}
				else if (HasTimedOut())
				{
					Fail(TEXT("Downed ownership-reclaim fixture did not replicate actual zero health and life/cue state."));
				}
				break;
			case EKnightAbilityNetworkPhase::WaitTakeover:
				WaitForTakeover();
				break;
			case EKnightAbilityNetworkPhase::WaitJIP:
				WaitForJIP();
				break;
			case EKnightAbilityNetworkPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EKnightAbilityNetworkPhase::Complete;
		}

	private:
		void EnterPhase(EKnightAbilityNetworkPhase InPhase)
		{
			Phase = InPhase;
			PhaseStarted = FPlatformTime::Seconds();
		}

		bool HasTimedOut(double Timeout = PhaseTimeoutSeconds) const
		{
			return FPlatformTime::Seconds() - PhaseStarted > Timeout;
		}

		void Fail(const FString& Message)
		{
			Test.AddError(Message);
			BeginCleanup();
		}

		UWorld* GetContextWorld(FName Handle) const
		{
			const FWorldContext* Context = GEngine != nullptr ? GEngine->GetWorldContextFromHandle(Handle) : nullptr;
			return Context != nullptr ? Context->World() : nullptr;
		}

		void StartSession()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("GAS network regression requires an idle Editor and does not take ownership of user PIE."));
				return;
			}
			if (!FPackageName::DoesPackageExist(AbilityMap) || !FPackageName::DoesPackageExist(DisconnectMap))
			{
				Fail(TEXT("Run the integrated M2 bootstrap before the real GAS network regression."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			Defaults->GetPlayNetMode(OriginalNetMode);
			Defaults->GetRunUnderOneProcess(bOriginalOneProcess);
			Defaults->GetPlayNumberOfClients(OriginalClientCount);
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalWindowPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_ListenServer);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetPlayNumberOfClients(3);
			Settings->SetServerPort(ServerPort);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = false;
			Settings->SetClientWindowSize(FIntPoint(640, 360));
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.GlobalMapOverride = AbilityMap;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Test.AddInfo(TEXT("GAS gate requested one real listen-server world and two owning client worlds."));
			EnterPhase(EKnightAbilityNetworkPhase::WaitRoster);
		}

		bool HaveConvergedClients(int32 HumanCount, FName SkippedContext = NAME_None) const
		{
			const AKnightPartyState* Authority = GetParty(ServerWorld.Get());
			if (Authority == nullptr || Authority->GetHumanCount() != HumanCount
				|| !Authority->IsCanonicalRosterReady() || CountKnights(ServerWorld.Get()) != UKnightSettings::CanonicalPartySize)
			{
				return false;
			}
			for (FName Handle : ClientContexts)
			{
				if (Handle == SkippedContext)
				{
					continue;
				}
				UWorld* ClientWorld = GetContextWorld(Handle);
				const AKnightPartyState* Replica = GetParty(ClientWorld);
				if (Replica == nullptr || !Replica->IsCanonicalRosterReady()
					|| CountKnights(ClientWorld) != UKnightSettings::CanonicalPartySize
					|| Replica->GetPartyId() != Authority->GetPartyId()
					|| Replica->GetRosterRevision() != Authority->GetRosterRevision())
				{
					return false;
				}
				for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
				{
					const FKnightRosterEntry* ServerEntry = Authority->FindSlot(Slot);
					const FKnightRosterEntry* Entry = Replica->FindSlot(Slot);
					if (Entry == nullptr || ServerEntry == nullptr || !IsValid(Entry->Knight)
						|| !IsValid(ServerEntry->Knight)
						|| Entry->KnightId != ServerEntry->KnightId || Entry->HeroDefinitionId != ServerEntry->HeroDefinitionId
						|| Entry->OwnerKind != ServerEntry->OwnerKind || Entry->HumanId != ServerEntry->HumanId
						|| Entry->HumanPlayerId != ServerEntry->HumanPlayerId
						|| Entry->Knight->GetKnightEntityId() != ServerEntry->KnightId
						|| Entry->Knight->GetHeroDefinitionId() != ServerEntry->HeroDefinitionId
						|| !Entry->Knight->GetAbilityInitializationResult().IsSuccess())
					{
						return false;
					}
					if (Entry->OwnerKind == EKnightControllerOwner::Human
						&& (Entry->HumanPlayerState == nullptr
							|| Entry->HumanPlayerState->GetHumanId() != Entry->HumanId
							|| Entry->HumanPlayerState->GetAssignedSlot() != Slot))
					{
						return false;
					}
					const AKnightCharacter* ServerKnight = ServerEntry->Knight;
					UKnightAbilitySystemComponent* ServerASC = ServerKnight->GetKnightAbilitySystem();
					UKnightAbilitySystemComponent* ClientASC = Entry->Knight->GetKnightAbilitySystem();
					if (ClientASC == nullptr || ServerASC == nullptr
						|| ClientASC->GetOwnerActor() != Entry->Knight || ClientASC->GetAvatarActor() != Entry->Knight
						|| ClientASC->GetOwnedGrantCount() != ServerASC->GetOwnedGrantCount()
						|| !FKnightNetworkAttributeSnapshot(ServerKnight->GetKnightAttributeSet())
							.Matches(Entry->Knight->GetKnightAttributeSet())
						|| Entry->Knight->GetLifeComponent()->GetLifeState() != ServerKnight->GetLifeComponent()->GetLifeState()
						|| ClientASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Alive)
							!= ServerASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Alive)
						|| ClientASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Downed)
							!= ServerASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Downed)
						|| ClientASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Dead)
							!= ServerASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Dead)
						|| ClientASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
							!= ServerASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
						|| ClientASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed)
							!= ServerASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed)
						|| ClientASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive)
							!= ServerASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool FindReadyOwningClient(UWorld* World, AKnightPlayerController*& OutController, int32& OutSlot) const
		{
			OutController = nullptr;
			OutSlot = INDEX_NONE;
			AKnightPartyState* Party = GetParty(World);
			if (World == nullptr || World->GetNetMode() != NM_Client || Party == nullptr)
			{
				return false;
			}
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AKnightPlayerController* Candidate = Cast<AKnightPlayerController>(Iterator->Get());
				AKnightPlayerState* Human = Candidate != nullptr ? Candidate->GetPlayerState<AKnightPlayerState>() : nullptr;
				AKnightCharacter* Knight = Candidate != nullptr ? Candidate->GetControlledKnight() : nullptr;
				const FKnightRosterEntry* Entry = Human != nullptr ? Party->FindSlot(Human->GetAssignedSlot()) : nullptr;
				if (Candidate != nullptr && Candidate->IsLocalController() && !Candidate->HasAuthority()
					&& Knight != nullptr && Knight->GetLocalRole() == ROLE_AutonomousProxy && Knight->IsLocallyControlled()
					&& Knight->GetController() == Candidate && Entry != nullptr && Entry->Knight == Knight
					&& Entry->OwnerKind == EKnightControllerOwner::Human && Entry->HumanPlayerState == Human
					&& Entry->HumanId == Human->GetHumanId() && Entry->KnightId == Knight->GetKnightEntityId()
					&& !GetSpecHandles(Knight->GetKnightAbilitySystem()).IsEmpty())
				{
					OutController = Candidate;
					OutSlot = Human->GetAssignedSlot();
					return true;
				}
			}
			return false;
		}

		bool HasPreservedServerState() const
		{
			const AKnightPartyState* Party = GetParty(ServerWorld.Get());
			if (Party == nullptr || Knights.Num() != UKnightSettings::CanonicalPartySize
				|| CountKnights(ServerWorld.Get()) != UKnightSettings::CanonicalPartySize)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < Knights.Num(); ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !Knights[Slot].IsValid() || Entry->Knight != Knights[Slot].Get()
					|| Entry->KnightId != KnightIds[Slot] || Entry->HeroDefinitionId != HeroIds[Slot]
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnerActor() != Knights[Slot].Get()
					|| Knights[Slot]->GetKnightAbilitySystem()->GetAvatarActor() != Knights[Slot].Get()
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnedGrantCount() != GrantCounts[Slot]
					|| GetSpecHandles(Knights[Slot]->GetKnightAbilitySystem()) != SpecHandles[Slot]
					|| (Attributes.Num() == Knights.Num() && !Attributes[Slot].Matches(Knights[Slot]->GetKnightAttributeSet())))
				{
					return false;
				}
			}
			return true;
		}

		void WaitForRoster()
		{
			ClientContexts.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != EWorldType::PIE || World == nullptr)
				{
					continue;
				}
				if (World->GetNetMode() == NM_ListenServer)
				{
					ServerWorld = World;
				}
				else if (World->GetNetMode() == NM_Client)
				{
					ClientContexts.Add(Context.ContextHandle);
				}
			}
			if (!ServerWorld.IsValid() || ClientContexts.Num() != 2 || !HaveConvergedClients(3))
			{
				if (HasTimedOut(StartupTimeoutSeconds))
				{
					Fail(TEXT("Three real network worlds did not converge attributes, life tags/cues and canonical ownership."));
				}
				return;
			}
			AKnightPartyState* Party = GetParty(ServerWorld.Get());
			for (FName Handle : ClientContexts)
			{
				UWorld* World = GetContextWorld(Handle);
				AKnightPlayerController* Owner = nullptr;
				int32 Slot = INDEX_NONE;
				if (!FindReadyOwningClient(World, Owner, Slot))
				{
					if (HasTimedOut(StartupTimeoutSeconds))
					{
						APlayerController* First = World != nullptr ? World->GetFirstPlayerController() : nullptr;
						AKnightPlayerState* Human = First != nullptr ? First->GetPlayerState<AKnightPlayerState>() : nullptr;
						Fail(FString::Printf(TEXT("Client ownership did not become ready: context=%s local=%d authority=%d "
							"playerstate=%d slot=%d pawn=%s."), *Handle.ToString(),
							First != nullptr && First->IsLocalController(), First != nullptr && First->HasAuthority(),
							Human != nullptr, Human != nullptr ? Human->GetAssignedSlot() : INDEX_NONE,
							First != nullptr ? *GetNameSafe(First->GetPawn()) : TEXT("None")));
					}
					return;
				}
				if (Handle == ClientContexts[0])
				{
					DisconnectedContext = Handle;
					Client = Owner;
					SourceSlot = Slot;
				}
			}
			TargetSlot = Party->GetControlledSlot(ServerWorld->GetFirstPlayerController());
			if (SourceSlot < 0 || SourceSlot >= UKnightSettings::CanonicalPartySize || TargetSlot < 0
				|| TargetSlot >= UKnightSettings::CanonicalPartySize || SourceSlot == TargetSlot)
			{
				Fail(TEXT("The network revive setup requires different source-client and target-host canonical Knights."));
				return;
			}
			TSet<UKnightAbilitySystemComponent*> DistinctASCs;
			for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				UKnightAbilitySystemComponent* ASC = Entry->Knight->GetKnightAbilitySystem();
				if (DistinctASCs.Contains(ASC) || ASC->GetOwnerActor() != Entry->Knight
					|| ASC->GetAvatarActor() != Entry->Knight || ASC->GetOwnedGrantCount() != 1
					|| GetSpecHandles(ASC).IsEmpty())
				{
					Fail(TEXT("Each canonical Knight must own its own initialized ASC and one nonempty HeroDefaults grant."));
					return;
				}
				DistinctASCs.Add(ASC);
				Knights.Add(Entry->Knight.Get());
				KnightIds.Add(Entry->KnightId);
				HeroIds.Add(Entry->HeroDefinitionId);
				GrantCounts.Add(ASC->GetOwnedGrantCount());
				SpecHandles.Add(GetSpecHandles(ASC));
			}
			UNetDriver* Driver = ServerWorld->GetNetDriver();
			const TSharedPtr<const FInternetAddr> Address = Driver != nullptr ? Driver->GetLocalAddr() : nullptr;
			if (!Address.IsValid() || Address->GetPort() <= 0 || Address->GetPort() > 65535)
			{
				Fail(TEXT("The real listen driver did not publish a reconnect socket address."));
				return;
			}
			ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"), Address->GetPort(), SourceSlot);
			FScopedConditionalWorldSwitcher WorldSwitcher(ServerWorld.Get());
			UKnightLifeComponent* TargetLife = Knights[TargetSlot]->GetLifeComponent();
			FKnightResult Result = TargetLife->TrySetLifeTuning(FixtureBleedoutSeconds, FixtureReviveSeconds, 300.0f, 0.35f);
			if (Result.IsSuccess())
			{
				Result = UKnightAbilitySystemComponent::ApplyDamage(
					Knights[TargetSlot].Get(), Knights[SourceSlot].Get(), SourceDamage);
			}
			for (int32 Slot = 0; Result.IsSuccess() && Slot < Knights.Num(); ++Slot)
			{
				if (Slot != SourceSlot && Slot != TargetSlot)
				{
					Result = UKnightAbilitySystemComponent::ApplyDamage(Knights[TargetSlot].Get(), Knights[Slot].Get(), OtherDamage);
				}
			}
			if (Result.IsSuccess())
			{
				Result = UKnightAbilitySystemComponent::ApplyDamage(Knights[SourceSlot].Get(), Knights[TargetSlot].Get(),
					Knights[TargetSlot]->GetKnightAttributeSet()->GetMaxHealth() + 1.0f);
			}
			if (!Result.IsSuccess() || !TargetLife->IsDowned()
				|| FMath::IsNearlyEqual(Knights[SourceSlot]->GetKnightAttributeSet()->GetHealth(),
					Knights[SourceSlot]->GetKnightAttributeSet()->GetMaxHealth()))
			{
				Fail(FString::Printf(TEXT("Validated GAS damage fixture did not create distinct health and downed state: %s."),
					*Result.Message.ToString()));
				return;
			}
			BleedoutEnd = TargetLife->GetBleedoutEndServerTime();
			Test.AddInfo(FString::Printf(TEXT("Three distinct GAS snapshots prepared; host slot %d downed, "
				"source client slot %d wounded."),
				TargetSlot, SourceSlot));
			EnterPhase(EKnightAbilityNetworkPhase::WaitDamage);
		}

		void WaitForDamage()
		{
			if (HaveConvergedClients(3))
			{
				FScopedConditionalWorldSwitcher WorldSwitcher(GetContextWorld(DisconnectedContext));
				Client->RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, true);
				EnterPhase(EKnightAbilityNetworkPhase::WaitRevive);
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Real GAS health/downed tags and persistent cue did not replicate to both clients."));
			}
		}

		void WaitForRevive()
		{
			UKnightAbilitySystemComponent* SourceASC = Knights[SourceSlot]->GetKnightAbilitySystem();
			UKnightLifeComponent* TargetLife = Knights[TargetSlot]->GetLifeComponent();
			if (SourceASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
				&& SourceASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive)
				&& TargetLife->IsReviveInProgress() && TargetLife->GetActiveReviver() == Knights[SourceSlot].Get()
				&& HaveConvergedClients(3))
			{
				for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
				{
					Attributes.Emplace(Knight->GetKnightAttributeSet());
				}
				Test.AddInfo(TEXT("Owning-client RPC activated real held revive: "
					"reservation, activation tag and persistent cue replicate."));
				FScopedConditionalWorldSwitcher WorldSwitcher(GetContextWorld(DisconnectedContext));
				Client->ClientTravel(DisconnectMap, TRAVEL_Absolute);
				EnterPhase(EKnightAbilityNetworkPhase::WaitTakeover);
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Owning revive RPC did not start/converge an actual held ability, reservation and active cue."));
			}
		}

		bool HasCleanCanceledRevive() const
		{
			UKnightAbilitySystemComponent* ASC = Knights[SourceSlot]->GetKnightAbilitySystem();
			if (ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
				|| ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive)
				|| Knights[TargetSlot]->GetLifeComponent()->IsReviveInProgress()
				|| Knights[TargetSlot]->GetLifeComponent()->GetActiveReviver() != nullptr)
			{
				return false;
			}
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.InputPressed || Spec.IsActive())
				{
					return false;
				}
			}
			return true;
		}

		void WaitForTakeover()
		{
			const AKnightPartyState* Party = GetParty(ServerWorld.Get());
			const FKnightRosterEntry* Entry = Party != nullptr ? Party->FindSlot(SourceSlot) : nullptr;
			UWorld* Detached = GetContextWorld(DisconnectedContext);
			if (Entry != nullptr && Entry->OwnerKind == EKnightControllerOwner::Companion
				&& Detached != nullptr && Detached->GetNetMode() == NM_Standalone
				&& Detached->GetNetDriver() == nullptr && HaveConvergedClients(2, DisconnectedContext))
			{
				AKnightCompanionController* AI = Cast<AKnightCompanionController>(Knights[SourceSlot]->GetController());
				if (!HasPreservedServerState() || !HasCleanCanceledRevive() || AI == nullptr
					|| AI->IsFollowing() == bReclaimDowned
					|| Knights[TargetSlot]->GetLifeComponent()->GetBleedoutEndServerTime() != BleedoutEnd)
				{
					Fail(TEXT("Real disconnect leaked an active/input/cue/reservation, reset health/timer, or duplicated a grant."));
					return;
				}
				APlayerController* DetachedPlayer = Detached->GetFirstPlayerController();
				if (DetachedPlayer == nullptr)
				{
					Fail(TEXT("Detached network context has no controller for the real join-in-progress reconnect."));
					return;
				}
				Test.AddInfo(bReclaimDowned
					? TEXT("Real downed-owner disconnect preserves zero health/downed cue and assigns an inactive AI controller.")
					: TEXT("Actual disconnect canceled held revive and removed its cue/tag/reservation/input; "
						"all health, timer, grant/spec handles and canonical actors survive AI takeover."));
				FScopedConditionalWorldSwitcher WorldSwitcher(Detached);
				DetachedPlayer->ClientTravel(ReconnectURL, TRAVEL_Absolute);
				EnterPhase(EKnightAbilityNetworkPhase::WaitJIP);
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Real active-ability disconnect and AI takeover did not converge within the bounded deadline."));
			}
		}

		void WaitForJIP()
		{
			UWorld* World = GetContextWorld(DisconnectedContext);
			AKnightPlayerController* Rejoined = World != nullptr
				? Cast<AKnightPlayerController>(World->GetFirstPlayerController()) : nullptr;
			const AKnightPlayerState* Human = Rejoined != nullptr ? Rejoined->GetPlayerState<AKnightPlayerState>() : nullptr;
			if (World != nullptr && World->GetNetMode() == NM_Client && HaveConvergedClients(3)
				&& Human != nullptr && Human->GetAssignedSlot() == SourceSlot && Rejoined->GetControlledKnight() != nullptr)
			{
				UKnightAbilitySystemComponent* OwnerASC = Rejoined->GetControlledKnight()->GetKnightAbilitySystem();
				if (GetSpecHandles(OwnerASC) != SpecHandles[SourceSlot])
				{
					if (HasTimedOut())
					{
						Fail(TEXT("Rejoined owner did not receive the original server ability spec handles exactly once."));
					}
					return;
				}
				const AKnightCharacter* TargetReplica = GetParty(World)->FindSlot(TargetSlot)->Knight;
				if (!HasPreservedServerState() || !HasCleanCanceledRevive()
					|| !TargetReplica->GetLifeComponent()->IsDowned()
					|| !TargetReplica->GetKnightAbilitySystem()->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed)
					|| OwnerASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
					|| OwnerASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive)
					|| Rejoined->GetControlledKnight()->GetKnightEntityId() != KnightIds[SourceSlot]
					|| Knights[TargetSlot]->GetLifeComponent()->GetBleedoutEndServerTime() != BleedoutEnd)
				{
					Fail(TEXT("JIP/reclaim failed persistent health/life/tag/cue convergence or reset canonical attributes/grants."));
					return;
				}
				Test.AddInfo(TEXT("Real same-slot JIP/reclaim received current health snapshots and downed cue/tag; "
					"canceled revive stayed absent, original ability spec handles and three server actors remained."));
				if (bReclaimDowned)
				{
					if (!Rejoined->GetControlledKnight()->GetLifeComponent()->IsDowned()
						|| !OwnerASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed)
						|| Knights[SourceSlot]->GetLifeComponent()->GetBleedoutEndServerTime() != SourceBleedoutEnd)
					{
						Fail(TEXT("Login/reclaim reset a downed Knight instead of preserving its zero health, cue and bleedout timer."));
						return;
					}
					Test.AddInfo(TEXT("Human reclaimed the existing downed Knight through real login; "
						"grants, health and timer are preserved."));
					BeginCleanup();
				}
				else
				{
					PrepareDownedReclaim(Rejoined);
				}
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Real JIP/reclaim did not converge life/attributes/grants/cues within the bounded deadline."));
			}
		}

		void PrepareDownedReclaim(AKnightPlayerController* Rejoined)
		{
			FScopedConditionalWorldSwitcher WorldSwitcher(ServerWorld.Get());
			UKnightLifeComponent* Life = Knights[SourceSlot]->GetLifeComponent();
			FKnightResult Result = Life->TrySetLifeTuning(FixtureBleedoutSeconds, FixtureReviveSeconds, 300.0f, 0.35f);
			for (int32 Slot = 0; Result.IsSuccess() && Slot < Knights.Num(); ++Slot)
			{
				if (Slot != SourceSlot && Slot != TargetSlot)
				{
					Result = UKnightAbilitySystemComponent::ApplyDamage(Knights[Slot].Get(), Knights[SourceSlot].Get(),
						Knights[SourceSlot]->GetKnightAttributeSet()->GetMaxHealth() + 1.0f);
				}
			}
			if (!Result.IsSuccess() || !Life->IsDowned())
			{
				Fail(TEXT("The second real-network fixture could not down the owned Knight using validated GAS damage."));
				return;
			}
			Attributes.Reset();
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				Attributes.Emplace(Knight->GetKnightAttributeSet());
			}
			SourceBleedoutEnd = Life->GetBleedoutEndServerTime();
			Client = Rejoined;
			bReclaimDowned = true;
			Test.AddInfo(TEXT("Validated GAS downed the rejoined owner; repeating real disconnect/reclaim without life reset."));
			EnterPhase(EKnightAbilityNetworkPhase::WaitDownedDamage);
		}

		void BeginCleanup()
		{
			if (Phase >= EKnightAbilityNetworkPhase::Cleanup)
			{
				return;
			}
			EnterPhase(EKnightAbilityNetworkPhase::Cleanup);
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->RequestEndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
		}

		void RestoreWindowSettings()
		{
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalLastSize;
				Defaults->NewWindowPosition = OriginalWindowPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (!HasTimedOut(CleanupTimeoutSeconds))
				{
					return;
				}
				Test.AddError(TEXT("GAS network PIE cleanup timed out; forcing synchronous EndPlayMap."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
			RestoreWindowSettings();
			if (Settings.IsValid())
			{
				const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
				EPlayNetMode NetMode;
				bool bOneProcess = false;
				int32 ClientCount = 0;
				Defaults->GetPlayNetMode(NetMode);
				Defaults->GetRunUnderOneProcess(bOneProcess);
				Defaults->GetPlayNumberOfClients(ClientCount);
				Test.TestEqual(TEXT("User network mode unchanged after GAS gate"), NetMode, OriginalNetMode);
				Test.TestEqual(TEXT("User process preference unchanged after GAS gate"), bOneProcess, bOriginalOneProcess);
				Test.TestEqual(TEXT("User client count unchanged after GAS gate"), ClientCount, OriginalClientCount);
			}
			Settings.Reset();
			bOwnsSession = false;
			EnterPhase(EKnightAbilityNetworkPhase::Complete);
		}

		FAutomationTestBase& Test;
		EKnightAbilityNetworkPhase Phase = EKnightAbilityNetworkPhase::Start;
		double PhaseStarted = 0.0;
		double BleedoutEnd = 0.0;
		double SourceBleedoutEnd = 0.0;
		int32 SourceSlot = INDEX_NONE;
		int32 TargetSlot = INDEX_NONE;
		bool bOwnsSession = false;
		bool bReclaimDowned = false;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		EPlayNetMode OriginalNetMode = PIE_Standalone;
		bool bOriginalOneProcess = false;
		int32 OriginalClientCount = 0;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalWindowPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> ServerWorld;
		TArray<FName> ClientContexts;
		FName DisconnectedContext;
		TWeakObjectPtr<AKnightPlayerController> Client;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> KnightIds;
		TArray<FPrimaryAssetId> HeroIds;
		TArray<int32> GrantCounts;
		TArray<TArray<FGameplayAbilitySpecHandle>> SpecHandles;
		TArray<FKnightNetworkAttributeSnapshot> Attributes;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAbilityNetworkTest, "Knight.GAS.NetworkOwnershipJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAbilityNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<KnightAbilityNetwork::FKnightAbilityNetworkCommand>(*this));
	return true;
}

#endif
