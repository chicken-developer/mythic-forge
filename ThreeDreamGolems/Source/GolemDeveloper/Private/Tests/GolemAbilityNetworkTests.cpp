#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Controllers/GolemCompanionController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/GolemGameState.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/PackageName.h"
#include "Party/GolemPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/GolemPlayerController.h"
#include "Player/GolemPlayerState.h"
#include "Settings/GolemSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/GolemGameplayTags.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace GolemAbilityNetwork
{
	const TCHAR* AbilityMap = TEXT("/Game/Golem/Maps/Playables/L_M02_AbilityYard");
	const TCHAR* DisconnectMap = TEXT("/Game/Golem/Maps/Playables/L_M00_Boot");
	constexpr uint16 ServerPort = 17807;
	constexpr double StartupTimeoutSeconds = 45.0;
	constexpr double PhaseTimeoutSeconds = 20.0;
	constexpr double CleanupTimeoutSeconds = 15.0;
	constexpr float SourceDamage = 17.0f;
	constexpr float OtherDamage = 23.0f;
	constexpr float FixtureBleedoutSeconds = 300.0f;
	constexpr float FixtureReviveSeconds = 30.0f;

	enum class EGolemAbilityNetworkPhase : uint8
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

	AGolemPartyState* GetParty(UWorld* World)
	{
		const AGolemGameState* State = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	int32 CountGolems(UWorld* World)
	{
		int32 Count = 0;
		if (World != nullptr)
		{
			for (TActorIterator<AGolemCharacter> Iterator(World); Iterator; ++Iterator)
			{
				++Count;
			}
		}
		return Count;
	}

	TArray<FGameplayAbilitySpecHandle> GetSpecHandles(const UGolemAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Handles;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Handles.Add(Spec.Handle);
		}
		return Handles;
	}

	struct FGolemNetworkAttributeSnapshot
	{
		TArray<float> Values;

		explicit FGolemNetworkAttributeSnapshot(const UGolemAttributeSet* Attributes)
			: Values { Attributes->GetHealth(), Attributes->GetMaxHealth(), Attributes->GetStamina(),
				Attributes->GetMaxStamina(), Attributes->GetRuneEnergy(), Attributes->GetMaxRuneEnergy(),
				Attributes->GetAttackPower(), Attributes->GetDefense() }
		{
		}

		bool Matches(const UGolemAttributeSet* Attributes) const
		{
			const FGolemNetworkAttributeSnapshot Current(Attributes);
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
	class FGolemAbilityNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemAbilityNetworkCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual ~FGolemAbilityNetworkCommand() override
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
			if (Phase < EGolemAbilityNetworkPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EGolemAbilityNetworkPhase::WaitRoster && Phase < EGolemAbilityNetworkPhase::Cleanup
				&& !HasPreservedServerState())
			{
				Fail(TEXT("Network lifecycle lost a pinned world/Golem or changed its identity, attributes or grants."));
			}
			if (Phase >= EGolemAbilityNetworkPhase::WaitDamage
				&& Phase <= EGolemAbilityNetworkPhase::WaitDownedDamage && !Client.IsValid())
			{
				Fail(TEXT("The owning-client controller vanished before its actual ability/travel request."));
			}
			switch (Phase)
			{
			case EGolemAbilityNetworkPhase::Start:
				StartSession();
				break;
			case EGolemAbilityNetworkPhase::WaitRoster:
				WaitForRoster();
				break;
			case EGolemAbilityNetworkPhase::WaitDamage:
				WaitForDamage();
				break;
			case EGolemAbilityNetworkPhase::WaitRevive:
				WaitForRevive();
				break;
			case EGolemAbilityNetworkPhase::WaitDownedDamage:
				if (HaveConvergedClients(3))
				{
					FScopedConditionalWorldSwitcher WorldSwitcher(GetContextWorld(DisconnectedContext));
					Client->ClientTravel(DisconnectMap, TRAVEL_Absolute);
					EnterPhase(EGolemAbilityNetworkPhase::WaitTakeover);
				}
				else if (HasTimedOut())
				{
					Fail(TEXT("Downed ownership-reclaim fixture did not replicate actual zero health and life/cue state."));
				}
				break;
			case EGolemAbilityNetworkPhase::WaitTakeover:
				WaitForTakeover();
				break;
			case EGolemAbilityNetworkPhase::WaitJIP:
				WaitForJIP();
				break;
			case EGolemAbilityNetworkPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EGolemAbilityNetworkPhase::Complete;
		}

	private:
		void EnterPhase(EGolemAbilityNetworkPhase InPhase)
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
			EnterPhase(EGolemAbilityNetworkPhase::WaitRoster);
		}

		bool HaveConvergedClients(int32 HumanCount, FName SkippedContext = NAME_None) const
		{
			const AGolemPartyState* Authority = GetParty(ServerWorld.Get());
			if (Authority == nullptr || Authority->GetHumanCount() != HumanCount
				|| !Authority->IsCanonicalRosterReady() || CountGolems(ServerWorld.Get()) != UGolemSettings::CanonicalPartySize)
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
				const AGolemPartyState* Replica = GetParty(ClientWorld);
				if (Replica == nullptr || !Replica->IsCanonicalRosterReady()
					|| CountGolems(ClientWorld) != UGolemSettings::CanonicalPartySize
					|| Replica->GetPartyId() != Authority->GetPartyId()
					|| Replica->GetRosterRevision() != Authority->GetRosterRevision())
				{
					return false;
				}
				for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
				{
					const FGolemRosterEntry* ServerEntry = Authority->FindSlot(Slot);
					const FGolemRosterEntry* Entry = Replica->FindSlot(Slot);
					if (Entry == nullptr || ServerEntry == nullptr || !IsValid(Entry->Golem)
						|| !IsValid(ServerEntry->Golem)
						|| Entry->GolemId != ServerEntry->GolemId || Entry->HeroDefinitionId != ServerEntry->HeroDefinitionId
						|| Entry->OwnerKind != ServerEntry->OwnerKind || Entry->HumanId != ServerEntry->HumanId
						|| Entry->HumanPlayerId != ServerEntry->HumanPlayerId
						|| Entry->Golem->GetGolemEntityId() != ServerEntry->GolemId
						|| Entry->Golem->GetHeroDefinitionId() != ServerEntry->HeroDefinitionId
						|| !Entry->Golem->GetAbilityInitializationResult().IsSuccess())
					{
						return false;
					}
					if (Entry->OwnerKind == EGolemControllerOwner::Human
						&& (Entry->HumanPlayerState == nullptr
							|| Entry->HumanPlayerState->GetHumanId() != Entry->HumanId
							|| Entry->HumanPlayerState->GetAssignedSlot() != Slot))
					{
						return false;
					}
					const AGolemCharacter* ServerGolem = ServerEntry->Golem;
					UGolemAbilitySystemComponent* ServerASC = ServerGolem->GetGolemAbilitySystem();
					UGolemAbilitySystemComponent* ClientASC = Entry->Golem->GetGolemAbilitySystem();
					if (ClientASC == nullptr || ServerASC == nullptr
						|| ClientASC->GetOwnerActor() != Entry->Golem || ClientASC->GetAvatarActor() != Entry->Golem
						|| ClientASC->GetOwnedGrantCount() != ServerASC->GetOwnedGrantCount()
						|| !FGolemNetworkAttributeSnapshot(ServerGolem->GetGolemAttributeSet())
							.Matches(Entry->Golem->GetGolemAttributeSet())
						|| Entry->Golem->GetLifeComponent()->GetLifeState() != ServerGolem->GetLifeComponent()->GetLifeState()
						|| ClientASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Alive)
							!= ServerASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Alive)
						|| ClientASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Downed)
							!= ServerASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Downed)
						|| ClientASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Dead)
							!= ServerASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Dead)
						|| ClientASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
							!= ServerASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
						|| ClientASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed)
							!= ServerASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed)
						|| ClientASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive)
							!= ServerASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool FindReadyOwningClient(UWorld* World, AGolemPlayerController*& OutController, int32& OutSlot) const
		{
			OutController = nullptr;
			OutSlot = INDEX_NONE;
			AGolemPartyState* Party = GetParty(World);
			if (World == nullptr || World->GetNetMode() != NM_Client || Party == nullptr)
			{
				return false;
			}
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AGolemPlayerController* Candidate = Cast<AGolemPlayerController>(Iterator->Get());
				AGolemPlayerState* Human = Candidate != nullptr ? Candidate->GetPlayerState<AGolemPlayerState>() : nullptr;
				AGolemCharacter* Golem = Candidate != nullptr ? Candidate->GetControlledGolem() : nullptr;
				const FGolemRosterEntry* Entry = Human != nullptr ? Party->FindSlot(Human->GetAssignedSlot()) : nullptr;
				if (Candidate != nullptr && Candidate->IsLocalController() && !Candidate->HasAuthority()
					&& Golem != nullptr && Golem->GetLocalRole() == ROLE_AutonomousProxy && Golem->IsLocallyControlled()
					&& Golem->GetController() == Candidate && Entry != nullptr && Entry->Golem == Golem
					&& Entry->OwnerKind == EGolemControllerOwner::Human && Entry->HumanPlayerState == Human
					&& Entry->HumanId == Human->GetHumanId() && Entry->GolemId == Golem->GetGolemEntityId()
					&& !GetSpecHandles(Golem->GetGolemAbilitySystem()).IsEmpty())
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
			const AGolemPartyState* Party = GetParty(ServerWorld.Get());
			if (Party == nullptr || Golems.Num() != UGolemSettings::CanonicalPartySize
				|| CountGolems(ServerWorld.Get()) != UGolemSettings::CanonicalPartySize)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < Golems.Num(); ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !Golems[Slot].IsValid() || Entry->Golem != Golems[Slot].Get()
					|| Entry->GolemId != GolemIds[Slot] || Entry->HeroDefinitionId != HeroIds[Slot]
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnerActor() != Golems[Slot].Get()
					|| Golems[Slot]->GetGolemAbilitySystem()->GetAvatarActor() != Golems[Slot].Get()
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnedGrantCount() != GrantCounts[Slot]
					|| GetSpecHandles(Golems[Slot]->GetGolemAbilitySystem()) != SpecHandles[Slot]
					|| (Attributes.Num() == Golems.Num() && !Attributes[Slot].Matches(Golems[Slot]->GetGolemAttributeSet())))
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
			AGolemPartyState* Party = GetParty(ServerWorld.Get());
			for (FName Handle : ClientContexts)
			{
				UWorld* World = GetContextWorld(Handle);
				AGolemPlayerController* Owner = nullptr;
				int32 Slot = INDEX_NONE;
				if (!FindReadyOwningClient(World, Owner, Slot))
				{
					if (HasTimedOut(StartupTimeoutSeconds))
					{
						APlayerController* First = World != nullptr ? World->GetFirstPlayerController() : nullptr;
						AGolemPlayerState* Human = First != nullptr ? First->GetPlayerState<AGolemPlayerState>() : nullptr;
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
			if (SourceSlot < 0 || SourceSlot >= UGolemSettings::CanonicalPartySize || TargetSlot < 0
				|| TargetSlot >= UGolemSettings::CanonicalPartySize || SourceSlot == TargetSlot)
			{
				Fail(TEXT("The network revive setup requires different source-client and target-host canonical Golems."));
				return;
			}
			TSet<UGolemAbilitySystemComponent*> DistinctASCs;
			for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				UGolemAbilitySystemComponent* ASC = Entry->Golem->GetGolemAbilitySystem();
				if (DistinctASCs.Contains(ASC) || ASC->GetOwnerActor() != Entry->Golem
					|| ASC->GetAvatarActor() != Entry->Golem || ASC->GetOwnedGrantCount() != 1
					|| GetSpecHandles(ASC).IsEmpty())
				{
					Fail(TEXT("Each canonical Golem must own its own initialized ASC and one nonempty HeroDefaults grant."));
					return;
				}
				DistinctASCs.Add(ASC);
				Golems.Add(Entry->Golem.Get());
				GolemIds.Add(Entry->GolemId);
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
			ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?GolemSlot=%d"), Address->GetPort(), SourceSlot);
			FScopedConditionalWorldSwitcher WorldSwitcher(ServerWorld.Get());
			UGolemLifeComponent* TargetLife = Golems[TargetSlot]->GetLifeComponent();
			FGolemResult Result = TargetLife->TrySetLifeTuning(FixtureBleedoutSeconds, FixtureReviveSeconds, 300.0f, 0.35f);
			if (Result.IsSuccess())
			{
				Result = UGolemAbilitySystemComponent::ApplyDamage(
					Golems[TargetSlot].Get(), Golems[SourceSlot].Get(), SourceDamage);
			}
			for (int32 Slot = 0; Result.IsSuccess() && Slot < Golems.Num(); ++Slot)
			{
				if (Slot != SourceSlot && Slot != TargetSlot)
				{
					Result = UGolemAbilitySystemComponent::ApplyDamage(Golems[TargetSlot].Get(), Golems[Slot].Get(), OtherDamage);
				}
			}
			if (Result.IsSuccess())
			{
				Result = UGolemAbilitySystemComponent::ApplyDamage(Golems[SourceSlot].Get(), Golems[TargetSlot].Get(),
					Golems[TargetSlot]->GetGolemAttributeSet()->GetMaxHealth() + 1.0f);
			}
			if (!Result.IsSuccess() || !TargetLife->IsDowned()
				|| FMath::IsNearlyEqual(Golems[SourceSlot]->GetGolemAttributeSet()->GetHealth(),
					Golems[SourceSlot]->GetGolemAttributeSet()->GetMaxHealth()))
			{
				Fail(FString::Printf(TEXT("Validated GAS damage fixture did not create distinct health and downed state: %s."),
					*Result.Message.ToString()));
				return;
			}
			BleedoutEnd = TargetLife->GetBleedoutEndServerTime();
			Test.AddInfo(FString::Printf(TEXT("Three distinct GAS snapshots prepared; host slot %d downed, "
				"source client slot %d wounded."),
				TargetSlot, SourceSlot));
			EnterPhase(EGolemAbilityNetworkPhase::WaitDamage);
		}

		void WaitForDamage()
		{
			if (HaveConvergedClients(3))
			{
				FScopedConditionalWorldSwitcher WorldSwitcher(GetContextWorld(DisconnectedContext));
				Client->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true);
				EnterPhase(EGolemAbilityNetworkPhase::WaitRevive);
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Real GAS health/downed tags and persistent cue did not replicate to both clients."));
			}
		}

		void WaitForRevive()
		{
			UGolemAbilitySystemComponent* SourceASC = Golems[SourceSlot]->GetGolemAbilitySystem();
			UGolemLifeComponent* TargetLife = Golems[TargetSlot]->GetLifeComponent();
			if (SourceASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
				&& SourceASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive)
				&& TargetLife->IsReviveInProgress() && TargetLife->GetActiveReviver() == Golems[SourceSlot].Get()
				&& HaveConvergedClients(3))
			{
				for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
				{
					Attributes.Emplace(Golem->GetGolemAttributeSet());
				}
				Test.AddInfo(TEXT("Owning-client RPC activated real held revive: "
					"reservation, activation tag and persistent cue replicate."));
				FScopedConditionalWorldSwitcher WorldSwitcher(GetContextWorld(DisconnectedContext));
				Client->ClientTravel(DisconnectMap, TRAVEL_Absolute);
				EnterPhase(EGolemAbilityNetworkPhase::WaitTakeover);
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Owning revive RPC did not start/converge an actual held ability, reservation and active cue."));
			}
		}

		bool HasCleanCanceledRevive() const
		{
			UGolemAbilitySystemComponent* ASC = Golems[SourceSlot]->GetGolemAbilitySystem();
			if (ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
				|| ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive)
				|| Golems[TargetSlot]->GetLifeComponent()->IsReviveInProgress()
				|| Golems[TargetSlot]->GetLifeComponent()->GetActiveReviver() != nullptr)
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
			const AGolemPartyState* Party = GetParty(ServerWorld.Get());
			const FGolemRosterEntry* Entry = Party != nullptr ? Party->FindSlot(SourceSlot) : nullptr;
			UWorld* Detached = GetContextWorld(DisconnectedContext);
			if (Entry != nullptr && Entry->OwnerKind == EGolemControllerOwner::Companion
				&& Detached != nullptr && Detached->GetNetMode() == NM_Standalone
				&& Detached->GetNetDriver() == nullptr && HaveConvergedClients(2, DisconnectedContext))
			{
				AGolemCompanionController* AI = Cast<AGolemCompanionController>(Golems[SourceSlot]->GetController());
				if (!HasPreservedServerState() || !HasCleanCanceledRevive() || AI == nullptr
					|| AI->IsFollowing() == bReclaimDowned
					|| Golems[TargetSlot]->GetLifeComponent()->GetBleedoutEndServerTime() != BleedoutEnd)
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
				EnterPhase(EGolemAbilityNetworkPhase::WaitJIP);
			}
			else if (HasTimedOut())
			{
				Fail(TEXT("Real active-ability disconnect and AI takeover did not converge within the bounded deadline."));
			}
		}

		void WaitForJIP()
		{
			UWorld* World = GetContextWorld(DisconnectedContext);
			AGolemPlayerController* Rejoined = World != nullptr
				? Cast<AGolemPlayerController>(World->GetFirstPlayerController()) : nullptr;
			const AGolemPlayerState* Human = Rejoined != nullptr ? Rejoined->GetPlayerState<AGolemPlayerState>() : nullptr;
			if (World != nullptr && World->GetNetMode() == NM_Client && HaveConvergedClients(3)
				&& Human != nullptr && Human->GetAssignedSlot() == SourceSlot && Rejoined->GetControlledGolem() != nullptr)
			{
				UGolemAbilitySystemComponent* OwnerASC = Rejoined->GetControlledGolem()->GetGolemAbilitySystem();
				if (GetSpecHandles(OwnerASC) != SpecHandles[SourceSlot])
				{
					if (HasTimedOut())
					{
						Fail(TEXT("Rejoined owner did not receive the original server ability spec handles exactly once."));
					}
					return;
				}
				const AGolemCharacter* TargetReplica = GetParty(World)->FindSlot(TargetSlot)->Golem;
				if (!HasPreservedServerState() || !HasCleanCanceledRevive()
					|| !TargetReplica->GetLifeComponent()->IsDowned()
					|| !TargetReplica->GetGolemAbilitySystem()->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed)
					|| OwnerASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
					|| OwnerASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive)
					|| Rejoined->GetControlledGolem()->GetGolemEntityId() != GolemIds[SourceSlot]
					|| Golems[TargetSlot]->GetLifeComponent()->GetBleedoutEndServerTime() != BleedoutEnd)
				{
					Fail(TEXT("JIP/reclaim failed persistent health/life/tag/cue convergence or reset canonical attributes/grants."));
					return;
				}
				Test.AddInfo(TEXT("Real same-slot JIP/reclaim received current health snapshots and downed cue/tag; "
					"canceled revive stayed absent, original ability spec handles and three server actors remained."));
				if (bReclaimDowned)
				{
					if (!Rejoined->GetControlledGolem()->GetLifeComponent()->IsDowned()
						|| !OwnerASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed)
						|| Golems[SourceSlot]->GetLifeComponent()->GetBleedoutEndServerTime() != SourceBleedoutEnd)
					{
						Fail(TEXT("Login/reclaim reset a downed Golem instead of preserving its zero health, cue and bleedout timer."));
						return;
					}
					Test.AddInfo(TEXT("Human reclaimed the existing downed Golem through real login; "
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

		void PrepareDownedReclaim(AGolemPlayerController* Rejoined)
		{
			FScopedConditionalWorldSwitcher WorldSwitcher(ServerWorld.Get());
			UGolemLifeComponent* Life = Golems[SourceSlot]->GetLifeComponent();
			FGolemResult Result = Life->TrySetLifeTuning(FixtureBleedoutSeconds, FixtureReviveSeconds, 300.0f, 0.35f);
			for (int32 Slot = 0; Result.IsSuccess() && Slot < Golems.Num(); ++Slot)
			{
				if (Slot != SourceSlot && Slot != TargetSlot)
				{
					Result = UGolemAbilitySystemComponent::ApplyDamage(Golems[Slot].Get(), Golems[SourceSlot].Get(),
						Golems[SourceSlot]->GetGolemAttributeSet()->GetMaxHealth() + 1.0f);
				}
			}
			if (!Result.IsSuccess() || !Life->IsDowned())
			{
				Fail(TEXT("The second real-network fixture could not down the owned Golem using validated GAS damage."));
				return;
			}
			Attributes.Reset();
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				Attributes.Emplace(Golem->GetGolemAttributeSet());
			}
			SourceBleedoutEnd = Life->GetBleedoutEndServerTime();
			Client = Rejoined;
			bReclaimDowned = true;
			Test.AddInfo(TEXT("Validated GAS downed the rejoined owner; repeating real disconnect/reclaim without life reset."));
			EnterPhase(EGolemAbilityNetworkPhase::WaitDownedDamage);
		}

		void BeginCleanup()
		{
			if (Phase >= EGolemAbilityNetworkPhase::Cleanup)
			{
				return;
			}
			EnterPhase(EGolemAbilityNetworkPhase::Cleanup);
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
			EnterPhase(EGolemAbilityNetworkPhase::Complete);
		}

		FAutomationTestBase& Test;
		EGolemAbilityNetworkPhase Phase = EGolemAbilityNetworkPhase::Start;
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
		TWeakObjectPtr<AGolemPlayerController> Client;
		TArray<TWeakObjectPtr<AGolemCharacter>> Golems;
		TArray<FGolemEntityId> GolemIds;
		TArray<FPrimaryAssetId> HeroIds;
		TArray<int32> GrantCounts;
		TArray<TArray<FGameplayAbilitySpecHandle>> SpecHandles;
		TArray<FGolemNetworkAttributeSnapshot> Attributes;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAbilityNetworkTest, "Golem.GAS.NetworkOwnershipJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAbilityNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemAbilityNetwork::FGolemAbilityNetworkCommand>(*this));
	return true;
}

#endif
