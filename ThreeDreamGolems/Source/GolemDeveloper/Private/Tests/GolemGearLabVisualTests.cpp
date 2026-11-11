#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/GolemCharacter.h"
#include "Catalog/GolemGearCatalog.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Layout/WidgetPath.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Models/GolemRPGPanelTypes.h"
#include "Party/GolemPartyState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "GearLab/GolemGearLabGameMode.h"
#include "GearLab/GolemGearLabHUD.h"
#include "GearLab/GolemGearLabPlayerController.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/GolemRPGPanel.h"
#include "Widgets/GolemRPGActionButton.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace GolemGearLabVisual
{
	const TCHAR* MapPath = TEXT("/Game/Golem/Maps/Playables/L_M05_6_CatalogLab");
	constexpr int32 CaptureCount = 4;
	constexpr double DrawWarmupSeconds = 0.5;

	enum class EPhase : uint8
	{
		Start,
		Ready,
		Draw,
		ClosePanel,
		Cleanup,
		Complete
	};

	/** Readback of the real PIE window after the native CommonUI page has reached Slate. */
	class FGolemGearVisualCommand final : public IAutomationLatentCommand
	{
	public:
		FGolemGearVisualCommand(FAutomationTestBase& InTest, FIntPoint InRequestedSize, const FString& InOutputSubdirectory)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted),
			RequestedSize(InRequestedSize), OutputSubdirectory(InOutputSubdirectory)
		{
		}

		virtual ~FGolemGearVisualCommand() override
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
				RestoreSettings();
			}
		}

		virtual bool Update() override
		{
			if (Phase < EPhase::Cleanup && (Test.HasAnyErrors()
				|| FPlatformTime::Seconds() - RunStarted > 120.0 || Elapsed() > 60.0))
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("Rendered GearLab timeout phase=%d capture=%d."),
						static_cast<int32>(Phase), CaptureIndex));
				}
				BeginCleanup();
			}
			switch (Phase)
			{
			case EPhase::Start: Start(); break;
			case EPhase::Ready: Ready(); break;
			case EPhase::Draw: Draw(); break;
			case EPhase::ClosePanel: ClosePanel(); break;
			case EPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EPhase::Complete;
		}

	private:
		double Elapsed() const
		{
			return FPlatformTime::Seconds() - PhaseStarted;
		}

		void Enter(EPhase Next)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			WarmFrames = 0;
		}

		void Fail(const FString& Message)
		{
			Test.AddError(Message);
			BeginCleanup();
		}

		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || !FSlateApplication::IsInitialized()
				|| GEditor->IsPlaySessionInProgress() || !FPackageName::DoesPackageExist(MapPath))
			{
				Fail(TEXT("Rendered GearLab requires an idle Editor, Slate/RHI and the genuine CatalogLab map."));
				return;
			}
			OutputDirectory = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Evidence/M56/rendered"), OutputSubdirectory));
			if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
			{
				Fail(TEXT("Could not create the rendered GearLab evidence directory."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_Standalone);
			Settings->SetPlayNumberOfClients(1);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = false;
			// Primary PIE uses these fields; SetClientWindowSize only governs additional client windows.
			Settings->NewWindowWidth = RequestedSize.X;
			Settings->NewWindowHeight = RequestedSize.Y;
			Settings->SetClientWindowSize(RequestedSize);
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.GlobalMapOverride = MapPath;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Enter(EPhase::Ready);
		}

		void Ready()
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != EWorldType::PIE || World == nullptr || World->GetNetMode() != NM_Standalone)
				{
					continue;
				}
				const AGolemGearLabGameMode* Mode = World->GetAuthGameMode<AGolemGearLabGameMode>();
				const AGolemGameState* State = World->GetGameState<AGolemGameState>();
				const AGolemPartyState* Party = State != nullptr ? State->GetPartyState() : nullptr;
				if (Mode == nullptr || Party == nullptr || !Party->IsCanonicalRosterReady()
					|| !Mode->GetRPGInitializationResult().IsSuccess())
				{
					continue;
				}
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					AGolemGearLabPlayerController* Player = Cast<AGolemGearLabPlayerController>(It->Get());
					AGolemCharacter* Golem = Player != nullptr ? Player->GetControlledGolem() : nullptr;
					if (Player == nullptr || !Player->IsLocalController() || Golem == nullptr
						|| !Golem->IsLocallyControlled() || !Golem->CanTransferControl()
						|| Party->GetControlledSlot(Player) == INDEX_NONE
						|| Player->GetHUD<AGolemGearLabHUD>() == nullptr)
					{
						continue;
					}
					ULocalPlayer* Local = Player->GetLocalPlayer();
					UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
					if (Viewport == nullptr || !Viewport->GetGameViewportWidget().IsValid())
					{
						continue;
					}
					FSceneViewport* SceneViewport = Viewport->GetGameViewport();
					const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(
						Viewport->GetGameViewportWidget().ToSharedRef());
					if (SceneViewport == nullptr || !Window.IsValid())
					{
						continue;
					}
					if (SceneViewport->GetSizeXY() != RequestedSize)
					{
						const double Now = FPlatformTime::Seconds();
						const FIntPoint ActualSize = SceneViewport->GetSizeXY();
						if (ActualSize.X <= 0 || ActualSize.Y <= 0)
						{
							continue;
						}
						if (SizeRequestedAt == 0.0)
						{
							SizeRequestedAt = Now;
						}
						if (Now - SizeRequestedAt > 5.0)
						{
							Fail(FString::Printf(TEXT("PIE viewport did not reach requested %dx%d; actual %dx%d."),
								RequestedSize.X, RequestedSize.Y, ActualSize.X, ActualSize.Y));
							return;
						}
						if (Now - LastSizeAdjustedAt >= 0.25)
						{
							// Correct measured viewport delta in outer-window space, including real Slate borders.
							const FVector2D WindowSize = Window->GetSizeInScreen();
							const FVector2D DesiredWindowSize = WindowSize + FVector2D(RequestedSize - ActualSize);
							Window->ReshapeWindow(Window->GetPositionInScreen(), DesiredWindowSize);
							LastSizeAdjustedAt = Now;
							Test.AddInfo(FString::Printf(TEXT("Adaptive PIE resize: viewport %dx%d -> %dx%d; window %s -> %s."),
								ActualSize.X, ActualSize.Y, RequestedSize.X, RequestedSize.Y,
								*WindowSize.ToString(), *DesiredWindowSize.ToString()));
						}
						continue;
					}
					Test.AddInfo(FString::Printf(TEXT("Actual PIE viewport ready: requested %dx%d; measured %dx%d; DPI scale %.3f."),
						RequestedSize.X, RequestedSize.Y, SceneViewport->GetSizeXY().X,
						SceneViewport->GetSizeXY().Y, UWidgetLayoutLibrary::GetViewportScale(Viewport)));
					Owner = Player;
					ControlledGolem = Golem;
					GolemId = Golem->GetGolemEntityId();
					AGolemRPGStateActor* Domains = Mode->GetRPGState();
					if (Mode->GetUpgradeStation() == nullptr || Domains == nullptr
						|| Domains->GetRPG() == nullptr || Domains->GetInventory() == nullptr)
					{
						Fail(TEXT("Rendered GearLab has no genuine sanctuary or confirmed domains."));
						return;
					}
					{
						FScopedConditionalWorldSwitcher Switch(World);
						Golem->GetCharacterMovement()->StopMovementImmediately();
						Golem->SetActorLocationAndRotation(Mode->GetUpgradeStation()->GetActorLocation()
							+ FVector(0.0, -100.0, 100.0), FRotator::ZeroRotator, false, nullptr,
							ETeleportType::TeleportPhysics);
					}
					if (!Mode->IsInUpgradeSafeZone(Golem))
					{
						Fail(TEXT("Actual controlled Golem is outside the genuine upgrade sanctuary."));
						return;
					}
					DomainState = Domains;
					ConfirmedRPG = Domains->GetRPG()->GetView();
					ConfirmedInventory = Domains->GetInventory()->GetView();
					Player->ToggleRPGPanel();
					UGolemRPGPanel* Panel = Player->GetRPGPanel();
					if (Panel == nullptr || !Panel->IsActivated() || Panel->GetSnapshot().Build.SpecId.IsNone()
						|| !Panel->GetSnapshot().Gear.bEnabled || !Panel->GetSnapshot().Gear.bCanEdit
						|| Panel->GetSnapshot().Gear.Slots.Num() != 12 || Panel->GetSnapshot().Gear.Level != 8)
					{
						Fail(TEXT("Real ToggleRPGPanel did not open a confirmed native GearLab panel."));
						return;
					}
					for (const FGolemGearItemRow& Row : Panel->GetSnapshot().Gear.Items)
					{
						const FGolemItemInstance* Item = ConfirmedInventory.Items.FindByPredicate([&Row](const FGolemItemInstance& Candidate)
						{
							return Candidate.InstanceId == Row.InstanceId;
						});
						if (Item != nullptr && !Item->RewardReceiptId.IsValid() &&
							(!Row.Tooltip.ToString().Contains(TEXT("Acquisition receipt unavailable")) ||
								Row.Tooltip.ToString().Contains(TEXT("Fixture / vendor inventory"))))
						{
							Fail(TEXT("Receipt-free owned item tooltip invents an acquisition source."));
							return;
						}
					}
					Panel->SetActiveTab(EGolemRPGPanelTab::Character);
					Enter(EPhase::Draw);
					return;
				}
			}
		}

		UGolemRPGActionButton* FindButton(const FString& Caption) const
		{
			UGolemRPGActionButton* Found = nullptr;
			Owner->GetRPGPanel()->WidgetTree->ForEachWidget([&Found, &Caption](UWidget* Widget)
			{
				UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
				if (Found == nullptr && Button != nullptr && Button->GetCaption().ToString() == Caption)
				{
					Found = Button;
				}
			});
			return Found;
		}

		FString FocusedCaption() const
		{
			FString Caption = TEXT("none");
			Owner->GetRPGPanel()->WidgetTree->ForEachWidget([this, &Caption](UWidget* Widget)
			{
				const UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
				if (Button != nullptr &&
					(Button->HasUserFocus(Owner.Get()) || Button->HasUserFocusedDescendants(Owner.Get())))
				{
					Caption = Button->GetCaption().ToString();
				}
			});
			return Caption;
		}

		bool PressButton(UGolemRPGActionButton* Button, FKey ActivationKey = EKeys::Enter)
		{
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			const TSharedPtr<FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (Button == nullptr || !Button->GetIsEnabled() || !User.IsValid())
			{
				FString Candidates;
				Owner->GetRPGPanel()->WidgetTree->ForEachWidget([&Candidates](UWidget* Widget)
				{
					const UGolemRPGActionButton* Candidate = Cast<UGolemRPGActionButton>(Widget);
					if (Candidate == nullptr)
					{
						return;
					}
					const FString Caption = Candidate->GetCaption().ToString();
					if (Caption.Contains(TEXT("F03")) || Caption.StartsWith(TEXT("Compare Ring")) ||
						Caption == TEXT("Inventory") || Caption == TEXT("Runes"))
					{
						Candidates += FString::Printf(TEXT("\n[%s] %s"), *Candidate->GetName(), *Caption);
					}
				});
				Fail(FString::Printf(TEXT("Actual native control '%s' unavailable: capture%d preparation%d activeTab%d, selectedInstance=%s focused='%s', button=%s enabled%d SlateUser%d. Candidates:%s"),
					*ExpectedControl, CaptureIndex, PrepareStep, static_cast<int32>(Owner->GetRPGPanel()->GetActiveTab()),
					*Owner->GetRPGPanel()->GetSelectedGearInstanceId().ToString(), *FocusedCaption(),
					Button != nullptr ? *Button->GetCaption().ToString() : TEXT("null"),
					Button != nullptr && Button->GetIsEnabled(), User.IsValid(), *Candidates));
				return false;
			}
			if (PendingFocusButton.Get() != Button)
			{
				PendingFocusButton = Button;
				FocusRequestedAt = FPlatformTime::Seconds();
				UWidget* ScrollTarget = Button;
				for (UPanelWidget* Parent = Button->GetParent(); Parent != nullptr; Parent = Parent->GetParent())
				{
					if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
					{
						Scroll->ScrollWidgetIntoView(ScrollTarget, false, EDescendantScrollDestination::Center);
						ScrollTarget = Scroll;
					}
				}
				Button->SetUserFocus(Owner.Get());
				return false;
			}
			if (FPlatformTime::Seconds() - FocusRequestedAt < 0.08)
			{
				return false;
			}
			if (!Button->HasUserFocus(Owner.Get()) && !Button->HasUserFocusedDescendants(Owner.Get()))
			{
				if (FPlatformTime::Seconds() - FocusRequestedAt >= 2.0)
				{
					Fail(FString::Printf(TEXT("Real CommonButton '%s' did not receive local Slate focus within2s."),
						*ExpectedControl));
				}
				return false;
			}
			UGameViewportClient* Viewport = Local->ViewportClient.Get();
			const TSharedPtr<SViewport> ViewportWidget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			if (!ViewportWidget.IsValid())
			{
				Fail(TEXT("Focused native activation has no genuine PIE viewport geometry."));
				return false;
			}
			// A reconstructed scroll tree needs a painted layout, rather than its preceding Tick geometry.
			const bool bReportLayoutFailure = FPlatformTime::Seconds() - FocusRequestedAt >= 2.0;
			if (!CheckButtonBounds(Button, ViewportWidget->GetPaintSpaceGeometry(), bReportLayoutFailure) ||
				!CheckButtonHitPath(Button, bReportLayoutFailure))
			{
				return false;
			}
			PendingFocusButton.Reset();
			FScopedConditionalWorldSwitcher Switch(Owner->GetWorld());
			const FKeyEvent Event(ActivationKey, FModifierKeysState(), User->GetUserIndex(), false, 0, 0);
			const FString FocusBefore = FocusedCaption();
			const bool bDown = FSlateApplication::Get().ProcessKeyDownEvent(Event);
			const FString FocusAfterDown = FocusedCaption();
			const bool bUp = FSlateApplication::Get().ProcessKeyUpEvent(Event);
			Test.AddInfo(FString::Printf(TEXT("Real %s '%s': handledDown=%d handledUp=%d; focus before='%s', afterDown='%s', afterUp='%s'; selectedInstance=%s."),
				*ActivationKey.ToString(), *ExpectedControl, bDown, bUp, *FocusBefore, *FocusAfterDown, *FocusedCaption(),
				*Owner->GetRPGPanel()->GetSelectedGearInstanceId().ToString()));
			if (!bDown && !bUp)
			{
				Fail(FString::Printf(TEXT("Focused native CommonButton did not handle actual Slate %s."), *ActivationKey.ToString()));
				return false;
			}
			return true;
		}

		bool ConfirmedStateUnchanged() const
		{
			return DomainState.IsValid() && DomainState->GetRPG()->GetView() == ConfirmedRPG
				&& DomainState->GetInventory()->GetView() == ConfirmedInventory
				&& !Owner->GetRPGPanel()->HasPendingCommand();
		}

		bool CheckButtonBounds(UGolemRPGActionButton* Button, const FGeometry& ViewportGeometry,
			bool bReportFailure = true)
		{
			return CheckWidgetBounds(Button, Button != nullptr ? Button->GetCaption().ToString() : TEXT("absent action"),
				ViewportGeometry, bReportFailure);
		}

		bool CheckButtonHitPath(UGolemRPGActionButton* Button, bool bReportFailure = true)
		{
			const TSharedPtr<SWidget> SlateWidget = Button != nullptr ? Button->GetCachedWidget() : nullptr;
			const TSharedPtr<SWindow> Window = SlateWidget.IsValid() ?
				FSlateApplication::Get().FindWidgetWindow(SlateWidget.ToSharedRef()) : nullptr;
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			const TSharedPtr<FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (!Window.IsValid() || !User.IsValid())
			{
				if (bReportFailure)
				{
					Fail(TEXT("Painted native button has no actual Slate window/user for hit testing."));
				}
				return false;
			}
			const FGeometry& Geometry = Button->GetPaintSpaceGeometry();
			const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
			if (!Window->IsVisible() || !Window->AcceptsInput() || !Window->IsScreenspaceMouseWithin(Center))
			{
				if (bReportFailure)
				{
					Fail(TEXT("Painted native button center is outside its visible input-accepting PIE window."));
				}
				return false;
			}
			const TArray<TSharedRef<SWindow>> Windows = {Window.ToSharedRef()};
			const FWidgetPath HitPath = FSlateApplication::Get().LocateWindowUnderMouse(Center,
				Windows, false, User->GetUserIndex());
			const bool bHitsButton = HitPath.ContainsWidget(SlateWidget.Get());
			if (!bHitsButton && bReportFailure)
			{
				Fail(FString::Printf(TEXT("Native control '%s' is absent from its actual painted center hit path at %s: %s"),
					*Button->GetCaption().ToString(), *Center.ToString(), *HitPath.ToString()));
			}
			return bHitsButton;
		}

		bool CheckWidgetBounds(UWidget* Widget, const FString& Name, const FGeometry& ViewportGeometry,
			bool bReportFailure = true)
		{
			if (Widget == nullptr)
			{
				if (bReportFailure)
				{
					Fail(TEXT("Required native tab/action is absent during layout verification."));
				}
				return false;
			}
			const FGeometry& Geometry = Widget->GetPaintSpaceGeometry();
			const FVector2D Min = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D Max = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
			const FVector2D ViewportMin = ViewportGeometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D ViewportMax = ViewportGeometry.LocalToAbsolute(ViewportGeometry.GetLocalSize());
			constexpr double Tolerance = 2.0;
			bool bInside = Max.X - Min.X > 1.0 && Max.Y - Min.Y > 1.0
				&& Min.X >= ViewportMin.X - Tolerance && Min.Y >= ViewportMin.Y - Tolerance
				&& Max.X <= ViewportMax.X + Tolerance && Max.Y <= ViewportMax.Y + Tolerance;
			FString Clips;
			// Focused flyout/socket controls must also fit inside their actual scroll clipping region.
			for (UPanelWidget* Parent = Widget->GetParent(); Parent != nullptr; Parent = Parent->GetParent())
			{
				if (const UScrollBox* Scroll = Cast<UScrollBox>(Parent))
				{
					const FGeometry& Clip = Scroll->GetPaintSpaceGeometry();
					const FVector2D ClipMin = Clip.LocalToAbsolute(FVector2D::ZeroVector);
					const FVector2D ClipMax = Clip.LocalToAbsolute(Clip.GetLocalSize());
					Clips += FString::Printf(TEXT("\n%s clip(%s,%s), offset%g/end%g"), *Scroll->GetName(),
						*ClipMin.ToString(), *ClipMax.ToString(), Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd());
					bInside &= Min.X >= ClipMin.X - Tolerance && Min.Y >= ClipMin.Y - Tolerance
						&& Max.X <= ClipMax.X + Tolerance && Max.Y <= ClipMax.Y + Tolerance;
				}
			}
			if (!bInside && bReportFailure)
			{
				Fail(FString::Printf(TEXT("Native control '%s' is zero-sized or clipped at %dx%d: bounds(%s,%s), viewport(%s,%s).%s"),
					*Name, RequestedSize.X, RequestedSize.Y,
					*Min.ToString(), *Max.ToString(), *ViewportMin.ToString(), *ViewportMax.ToString(), *Clips));
			}
			return bInside;
		}

		UGolemRPGActionButton* FindButtonStarting(const FString& Prefix) const
		{
			UGolemRPGActionButton* Found = nullptr;
			Owner->GetRPGPanel()->WidgetTree->ForEachWidget([&Found, &Prefix](UWidget* Widget)
			{
				UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
				if (Found == nullptr && Button != nullptr && Button->GetCaption().ToString().StartsWith(Prefix))
				{
					Found = Button;
				}
			});
			return Found;
		}

		bool PressGamepad(FKey Key)
		{
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			const TSharedPtr<FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (!User.IsValid())
			{
				Fail(TEXT("Real gamepad UI input has no local Slate user."));
				return false;
			}
			FScopedConditionalWorldSwitcher Switch(Owner->GetWorld());
			const FKeyEvent Event(Key, FModifierKeysState(), User->GetUserIndex(), false, 0, 0);
			const bool bHandled = FSlateApplication::Get().ProcessKeyDownEvent(Event);
			FSlateApplication::Get().ProcessKeyUpEvent(Event);
			if (!bHandled)
			{
				Fail(TEXT("Native panel did not handle the real Slate gamepad key."));
			}
			return bHandled;
		}

		UGolemRPGActionButton* FindOwnedItemButton(const FGolemGearItemRow& Item) const
		{
			UGolemRPGActionButton* Found = nullptr;
			const FString Name = Item.Name.ToString();
			const FString Instance = TEXT("#") + Item.InstanceId.ToString().Left(8);
			Owner->GetRPGPanel()->WidgetTree->ForEachWidget([&Found, &Name, &Instance](UWidget* Widget)
			{
				UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
				if (Found == nullptr && Button != nullptr && Button->GetCaption().ToString().Contains(Name)
					&& Button->GetCaption().ToString().Contains(Instance)
					&& Button->GetCaption().ToString().Contains(TEXT("Shared inventory")))
				{
					Found = Button;
				}
			});
			return Found;
		}

		bool CheckRequiredLayout(const TSharedRef<SViewport>& ViewportWidget)
		{
			const FGeometry& Geometry = ViewportWidget->GetPaintSpaceGeometry();
			const TCHAR* Captions[] =
			{
				TEXT("Character"), TEXT("Specialization"), TEXT("Talents"), TEXT("Spellbook"), TEXT("Runes"),
				TEXT("Inventory"), TEXT("Revert / Reset draft"), TEXT("Apply changes [F]"), TEXT("Close [Esc / B]")
			};
			for (const TCHAR* Caption : Captions)
			{
				if (!CheckButtonBounds(FindButton(Caption), Geometry))
				{
					return false;
				}
			}
			if (CaptureIndex == 0)
			{
				for (const FGolemGearSlotRow& GearSlot : Owner->GetRPGPanel()->GetSnapshot().Gear.Slots)
				{
					if (!CheckButtonBounds(FindButtonStarting(GearSlot.Name.ToString() + TEXT("\n")), Geometry))
					{
						return false;
					}
				}
			}
			else
			{
				const TCHAR* CompareControls[] =
				{
					TEXT("Compare Ring1"), TEXT("Compare Ring2"), TEXT("Read complete comparison [LT / RT]")
				};
				for (const TCHAR* Caption : CompareControls)
				{
					UGolemRPGActionButton* Button = FindButton(Caption);
					if (!CheckButtonBounds(Button, Geometry) || !CheckButtonHitPath(Button))
					{
						return false;
					}
				}
				UTextBlock* FirstStat = nullptr;
				const FString Prefix = Owner->GetRPGPanel()->GetGearCompare().Stats[0].Name.ToString() + TEXT(":");
				Owner->GetRPGPanel()->WidgetTree->ForEachWidget([&FirstStat, &Prefix](UWidget* Widget)
				{
					UTextBlock* Text = Cast<UTextBlock>(Widget);
					if (Text != nullptr && Text->GetText().ToString().StartsWith(Prefix))
					{
						FirstStat = Text;
					}
				});
				return CheckWidgetBounds(FirstStat, TEXT("First actual comparison stat row"), Geometry);
			}
			return true;
		}

		bool VerifyGamepadComparisonDetails()
		{
			UGolemRPGPanel* Panel = Owner->GetRPGPanel();
			if (DetailStage == 0)
			{
				ExpectedControl = TEXT("Read complete comparison [LT / RT]");
				if (!PressButton(FindButton(ExpectedControl), EKeys::Gamepad_FaceButton_Bottom))
				{
					return false;
				}
				++DetailStage;
				Enter(EPhase::Draw);
				return false;
			}
			UScrollBox* Scroll = Cast<UScrollBox>(Panel->WidgetTree->FindWidget(TEXT("RPGDetailsScroll")));
			UTextBlock* Text = nullptr;
			Panel->WidgetTree->ForEachWidget([&Text](UWidget* Widget)
			{
				UTextBlock* Candidate = Cast<UTextBlock>(Widget);
				if (Candidate != nullptr && Candidate->GetText().ToString().StartsWith(TEXT("COMPLETE COMPARISON")))
				{
					Text = Candidate;
				}
			});
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
			const TSharedPtr<SViewport> ViewportWidget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			if (Scroll == nullptr || Text == nullptr || !ViewportWidget.IsValid())
			{
				Fail(TEXT("Gamepad comparison reader has no actual detail scroll, text or PIE geometry."));
				return false;
			}
			if (!CheckWidgetBounds(Scroll, TEXT("Complete comparison detail viewport"), ViewportWidget->GetPaintSpaceGeometry()))
			{
				return false;
			}
			const FString Content = Text->GetText().ToString();
			for (const FGolemGearStatRow& Row : Panel->GetGearCompare().Stats)
			{
				if (!Content.Contains(Row.Name.ToString()) || !Content.Contains(Row.Contribution.ToString()))
				{
					Fail(TEXT("Gamepad detail reader omitted a confirmed comparison stat or cap contribution."));
					return false;
				}
			}
			const FGeometry& TextGeometry = Text->GetPaintSpaceGeometry();
			const FGeometry& ClipGeometry = Scroll->GetPaintSpaceGeometry();
			const FVector2D TextMin = TextGeometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D TextMax = TextGeometry.LocalToAbsolute(TextGeometry.GetLocalSize());
			const FVector2D ClipMin = ClipGeometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D ClipMax = ClipGeometry.LocalToAbsolute(ClipGeometry.GetLocalSize());
			if (DetailStage == 1)
			{
				if (Scroll->GetScrollOffset() > 0.01f || Scroll->GetScrollOffsetOfEnd() <= 48.0f)
				{
					Fail(TEXT("Actual gamepad reader did not start at its scrollable comparison beginning."));
					return false;
				}
				DetailStartTop = TextMin.Y;
				if (!PressGamepad(EKeys::Gamepad_RightTrigger))
				{
					return false;
				}
				++DetailStage;
				Enter(EPhase::Draw);
				return false;
			}
			if (DetailStage == 2)
			{
				if (Scroll->GetScrollOffset() <= 0.01f || TextMin.Y >= DetailStartTop - 1.0)
				{
					Fail(TEXT("Actual gamepad RT did not advance both scroll offset and rendered comparison content."));
					return false;
				}
				for (int32 Pulse = 0; Pulse < 64 && Scroll->GetScrollOffset() < Scroll->GetScrollOffsetOfEnd() - 0.01f; ++Pulse)
				{
					const float Before = Scroll->GetScrollOffset();
					if (!PressGamepad(EKeys::Gamepad_RightTrigger))
					{
						return false;
					}
					if (Scroll->GetScrollOffset() <= Before)
					{
						Fail(TEXT("Bounded real RT traversal stopped before the final comparison rows."));
						return false;
					}
				}
				++DetailStage;
				Enter(EPhase::Draw);
				return false;
			}
			if (DetailStage == 3)
			{
				if (!FMath::IsNearlyEqual(Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd(), 0.02f) ||
					TextMax.Y > ClipMax.Y + 2.0 || TextMax.Y <= ClipMin.Y + 1.0)
				{
					Fail(FString::Printf(TEXT("Final comparison rows remain clipped after real RT: offset%g/end%g, text bottom%g, clip%g..%g."),
						Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd(), TextMax.Y, ClipMin.Y, ClipMax.Y));
					return false;
				}
				Test.AddInfo(FString::Printf(TEXT("Gamepad A comparison target Ring2 and all%d stat rows reached with actual RT: offset%g, rendered bottom%g inside detail clip%g..%g."),
					Panel->GetGearCompare().Stats.Num(), Scroll->GetScrollOffset(), TextMax.Y, ClipMin.Y, ClipMax.Y));
				for (int32 Pulse = 0; Pulse < 64 && Scroll->GetScrollOffset() > 0.01f; ++Pulse)
				{
					const float Before = Scroll->GetScrollOffset();
					if (!PressGamepad(EKeys::Gamepad_LeftTrigger))
					{
						return false;
					}
					if (Scroll->GetScrollOffset() >= Before)
					{
						Fail(TEXT("Bounded real LT traversal stopped before the comparison beginning."));
						return false;
					}
				}
				++DetailStage;
				Enter(EPhase::Draw);
				return false;
			}
			if (Scroll->GetScrollOffset() > 0.01f || TextMin.Y < ClipMin.Y - 2.0 || TextMin.Y >= ClipMax.Y)
			{
				Fail(TEXT("Real LT did not restore the comparison's visible first lines."));
				return false;
			}
			bDetailScrollVerified = true;
			Test.AddInfo(TEXT("Gamepad LT restored the actual comparison beginning; pure confirmed inventory and Golem state unchanged."));
			return true;
		}

		void Draw()
		{
			if (!Owner.IsValid() || !ControlledGolem.IsValid()
				|| Owner->GetControlledGolem() != ControlledGolem.Get()
				|| ControlledGolem->GetGolemEntityId() != GolemId)
			{
				Fail(TEXT("Canonical local Golem identity changed during actual GearLab capture."));
				return;
			}
			UGolemRPGPanel* Panel = Owner->GetRPGPanel();
			if (Panel == nullptr || Panel->IsActivated() != (CaptureIndex < 3))
			{
				Fail(TEXT("Real CommonUI activation differs from the rendered fixture phase."));
				return;
			}
			if (++WarmFrames < 3 || Elapsed() < DrawWarmupSeconds)
			{
				return;
			}
			if (!ConfirmedStateUnchanged())
			{
				Fail(TEXT("UI selection or comparison mutated confirmed gear/ASC state or submitted authority commands."));
				return;
			}
			if (CaptureIndex == 1 && PrepareStep == 0)
			{
				const FGolemGearItemRow* Ring = Panel->GetSnapshot().Gear.Items.FindByPredicate(
					[](const FGolemGearItemRow& Row)
					{
						return Row.DefinitionId == FGolemGearCatalog::DefinitionId(TEXT("F03"))
							&& Row.LocationId == TEXT("Inventory");
					});
				ExpectedControl = Ring != nullptr ? FString::Printf(TEXT("owned F03 '%s' #%s, category=%s subtype=%s location=%s"),
					*Ring->Name.ToString(), *Ring->InstanceId.ToString().Left(8), *Ring->Category.ToString(),
					*Ring->Subtype.ToString(), *Ring->LocationId.ToString()) : TEXT("owned F03 bag row");
				if (Ring == nullptr || !PressButton(FindOwnedItemButton(*Ring)))
				{
					if (Ring == nullptr)
					{
						Fail(TEXT("CatalogLab has no actual owned F03 ring for the two-target comparison."));
					}
					return;
				}
				if (Panel->GetSelectedGearInstanceId() != Ring->InstanceId)
				{
					Fail(FString::Printf(TEXT("Handled Enter did not select the exact F03 instance: expected=%s actual=%s focused='%s'."),
						*Ring->InstanceId.ToString(), *Panel->GetSelectedGearInstanceId().ToString(), *FocusedCaption()));
					return;
				}
				++PrepareStep;
				Enter(EPhase::Draw);
				return;
			}
			if (CaptureIndex > 0 && CaptureIndex < 3 && !bPagePrepared)
			{
				ExpectedControl = CaptureIndex == 1 ? TEXT("Compare Ring1") : TEXT("Compare Ring2");
				const FKey ActivationKey = CaptureIndex == 2 ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Enter;
				if (!PressButton(FindButton(ExpectedControl), ActivationKey))
				{
					return;
				}
				const FName ExpectedSlot = CaptureIndex == 1 ? TEXT("Equipment.Slot.Ring1") : TEXT("Equipment.Slot.Ring2");
				if (Panel->GetGearCompare().SlotId != ExpectedSlot)
				{
					Fail(FString::Printf(TEXT("Handled %s did not select the exact compare slot: expected=%s actual=%s."),
						*ActivationKey.ToString(), *ExpectedSlot.ToString(), *Panel->GetGearCompare().SlotId.ToString()));
					return;
				}
				bPagePrepared = true;
				UGolemRPGActionButton* Reader = FindButton(TEXT("Read complete comparison [LT / RT]"));
				if (Reader == nullptr)
				{
					Fail(TEXT("Confirmed ring comparison has no native reader to focus after reconstruction."));
					return;
				}
				// Moving to a valid new page child before the deferred reconstruction pass must win.
				Reader->SetUserFocus(Owner.Get());
				bAwaitFocusPreservation = true;
				Enter(EPhase::Draw);
				return;
			}
			if (bAwaitFocusPreservation)
			{
				UGolemRPGActionButton* Reader = FindButton(TEXT("Read complete comparison [LT / RT]"));
				if (Reader == nullptr || (!Reader->HasUserFocus(Owner.Get()) &&
					!Reader->HasUserFocusedDescendants(Owner.Get())))
				{
					Fail(TEXT("Deferred page reconstruction replaced the user's new valid reader focus."));
					return;
				}
				bAwaitFocusPreservation = false;
				Test.AddInfo(TEXT("Actual new reader focus survived the deferred reconstruction layout passes."));
			}
			if (CaptureIndex > 0 && CaptureIndex < 3)
			{
				const FName ExpectedSlot = CaptureIndex == 1 ? TEXT("Equipment.Slot.Ring1") : TEXT("Equipment.Slot.Ring2");
				const FGolemGearCompare& Compare = Panel->GetGearCompare();
				if (!Compare.bValid || Compare.SlotId != ExpectedSlot || Compare.GolemId != GolemId
					|| Compare.Stats.Num() < 11 || Compare.Summary.IsEmpty() || Panel->HasGearDraft())
				{
					Fail(TEXT("Actual Ring target did not produce a pure complete effective-stat comparison."));
					return;
				}
				bool bHasTooltip = false;
				Panel->WidgetTree->ForEachWidget([&bHasTooltip](UWidget* Widget)
				{
					const UTextBlock* Text = Cast<UTextBlock>(Widget);
					bHasTooltip |= Text != nullptr && Text->GetText().ToString().Contains(TEXT("Unique equipped group"))
						&& Text->GetText().ToString().Contains(TEXT("RequiredLevel"));
				});
				if (!bHasTooltip)
				{
					Fail(TEXT("Native item inspector lacks the actual requirement/unique tooltip contract."));
					return;
				}
			}
			if (CaptureIndex == 3 && (Owner->IsMoveInputIgnored() || Owner->IsLookInputIgnored()
				|| Owner->bShowMouseCursor || Panel->HasGearDraft()))
			{
				Fail(TEXT("Actual gamepad Back left menu focus/input or a stale gear draft active."));
				return;
			}
			if (CaptureIndex == 2 && !bDetailScrollVerified && !VerifyGamepadComparisonDetails())
			{
				return;
			}
			if (CaptureIndex == 1 && !bKeyboardDetailPrepared)
			{
				ExpectedControl = TEXT("Read complete comparison [LT / RT]");
				if (!PressButton(FindButton(ExpectedControl)))
				{
					return;
				}
				bKeyboardDetailPrepared = true;
				Enter(EPhase::Draw);
				return;
			}
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
			const TSharedPtr<SViewport> Widget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			if (!Widget.IsValid() || !FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()).IsValid())
			{
				Fail(TEXT("Actual PIE viewport has no rendered Slate window for genuine pixel readback."));
				return;
			}
			if (CaptureIndex < 3 && !CheckRequiredLayout(Widget.ToSharedRef()))
			{
				return;
			}
			TArray<FColor> Pixels;
			FIntVector Size(0, 0, 0);
			if (!FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size)
				|| Size.X < 640 || Size.Y < 360 || Pixels.Num() < Size.X * Size.Y)
			{
				Fail(TEXT("Slate/RHI failed to read the actual native PIE viewport and UI."));
				return;
			}
			if (Size.X != RequestedSize.X || Size.Y != RequestedSize.Y)
			{
				Fail(FString::Printf(TEXT("Actual Slate readback %dx%d does not match requested DPI fixture %dx%d."),
					Size.X, Size.Y, RequestedSize.X, RequestedSize.Y));
				return;
			}
			// Check the paint/hit-test geometry produced by this exact screenshot as well as the preceding frame.
			if (CaptureIndex < 3 && !CheckRequiredLayout(Widget.ToSharedRef()))
			{
				return;
			}
			Pixels.SetNum(Size.X * Size.Y);
			bool bVaried = false;
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
				bVaried |= Pixel != Pixels[0];
			}
			if (!bVaried)
			{
				Fail(TEXT("The rendered viewport readback was a uniform empty image."));
				return;
			}
			const int32 PixelBytes = Pixels.Num() * static_cast<int32>(sizeof(FColor));
			const uint32 PixelCRC = FCrc::MemCrc32(Pixels.GetData(), PixelBytes);
			if (ImageCRCs.Contains(PixelCRC))
			{
				Fail(TEXT("Two native tab captures have identical pixels; page rendering did not advance."));
				return;
			}
			ImageCRCs.Add(PixelCRC);
			TArray64<uint8> PNG;
			const TArrayView64<const FColor> PixelView(Pixels.GetData(), Pixels.Num());
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y, PixelView, PNG);
			const TCHAR* Names[CaptureCount] =
			{
				TEXT("Character12Slots"), TEXT("InventoryRing1Compare"),
				TEXT("InventoryRing2Compare"), TEXT("HUDQuickUse")
			};
			const FString FileName = FPaths::Combine(OutputDirectory,
				FString::Printf(TEXT("%02d_%s.png"), CaptureIndex + 1, Names[CaptureIndex]));
			if (PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG, *FileName))
			{
				Fail(TEXT("Could not write actual rendered PNG evidence."));
				return;
			}
			Test.AddInfo(FString::Printf(TEXT("Rendered native %s: requested %dx%d; actual %dx%d; DPI %.3f, %lld bytes, CRC=%08x, %s"),
				Names[CaptureIndex], RequestedSize.X, RequestedSize.Y, Size.X, Size.Y,
				UWidgetLayoutLibrary::GetViewportScale(Viewport), PNG.Num(), PixelCRC, *FileName));
			++CaptureIndex;
			if (CaptureIndex == CaptureCount)
			{
				BeginCleanup();
				return;
			}
			if (CaptureIndex == 1)
			{
				Panel->SetActiveTab(EGolemRPGPanelTab::Inventory);
				if (!PressGamepad(EKeys::Gamepad_LeftShoulder))
				{
					return;
				}
				if (Panel->GetActiveTab() != EGolemRPGPanelTab::Runes)
				{
					Fail(TEXT("Actual gamepad LB did not navigate Inventory to Runes."));
					return;
				}
				if (!PressGamepad(EKeys::Gamepad_RightShoulder))
				{
					return;
				}
				if (Panel->GetActiveTab() != EGolemRPGPanelTab::Inventory)
				{
					Fail(TEXT("Actual gamepad RB did not return Runes to Inventory."));
					return;
				}
			}
			else if (CaptureIndex == 3)
			{
				Enter(EPhase::ClosePanel);
				return;
			}
			bPagePrepared = false;
			Enter(EPhase::Draw);
		}

		void ClosePanel()
		{
			if (!Owner.IsValid() || Owner->GetRPGPanel() == nullptr || !Owner->GetRPGPanel()->IsActivated())
			{
				Fail(TEXT("Native panel closed before the real Revert and gamepad Back sequence."));
				return;
			}
			ExpectedControl = TEXT("Revert / Reset draft");
			if (!PressButton(FindButton(ExpectedControl)) || !PressGamepad(EKeys::Gamepad_FaceButton_Right))
			{
				return;
			}
			bPagePrepared = false;
			Enter(EPhase::Draw);
		}

		void BeginCleanup()
		{
			if (Phase >= EPhase::Cleanup)
			{
				return;
			}
			Enter(EPhase::Cleanup);
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

		void RestoreSettings()
		{
			if (!Settings.IsValid())
			{
				return;
			}
			ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
			Defaults->MultipleInstancePositions = OriginalPositions;
			Defaults->LastSize = OriginalLastSize;
			Defaults->NewWindowPosition = OriginalPosition;
			Defaults->NewWindowWidth = OriginalWidth;
			Defaults->NewWindowHeight = OriginalHeight;
			Defaults->SaveConfig();
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (Elapsed() < 15.0)
				{
					return;
				}
				Test.AddError(TEXT("Rendered GearLab cleanup timed out; forcing synchronous EndPlay."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
			RestoreSettings();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EPhase::Complete);
		}

		FAutomationTestBase& Test;
		EPhase Phase = EPhase::Start;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		FIntPoint RequestedSize;
		FString OutputSubdirectory;
		double SizeRequestedAt = 0.0;
		double LastSizeAdjustedAt = 0.0;
		bool bOwnsSession = false;
		bool bPagePrepared = false;
		bool bKeyboardDetailPrepared = false;
		bool bAwaitFocusPreservation = false;
		bool bDetailScrollVerified = false;
		int32 CaptureIndex = 0;
		int32 DetailStage = 0;
		double DetailStartTop = 0.0;
		int32 PrepareStep = 0;
		int32 WarmFrames = 0;
		FString OutputDirectory;
		FString ExpectedControl;
		double FocusRequestedAt = 0.0;
		TWeakObjectPtr<UGolemRPGActionButton> PendingFocusButton;
		TSet<uint32> ImageCRCs;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<AGolemGearLabPlayerController> Owner;
		TWeakObjectPtr<AGolemCharacter> ControlledGolem;
		TWeakObjectPtr<AGolemRPGStateActor> DomainState;
		FGolemRPGView ConfirmedRPG;
		FGolemInventoryView ConfirmedInventory;
		FGolemEntityId GolemId;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearLabVisual720Test, "Golem.M56.Visual.UI720",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGolemGearLabVisual720Test::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemGearLabVisual::FGolemGearVisualCommand>(*this, FIntPoint(1280, 720), FString(TEXT("720p"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearLabVisual1080Test, "Golem.M56.Visual.UI1080",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGolemGearLabVisual1080Test::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemGearLabVisual::FGolemGearVisualCommand>(*this, FIntPoint(1920, 1080), FString(TEXT("1080p"))));
	return true;
}

#endif
