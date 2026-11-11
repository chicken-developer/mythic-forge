#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/GolemCharacter.h"
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
#include "SkillLab/GolemSkillLabGameMode.h"
#include "SkillLab/GolemSkillLabHUD.h"
#include "SkillLab/GolemSkillLabPlayerController.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/GolemRPGPanel.h"
#include "Widgets/GolemRPGActionButton.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace GolemSkillLabVisual
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M05_5_SkillLab");
	constexpr int32 CaptureCount = 6;
	constexpr double DrawWarmupSeconds = 0.5;

	enum class EPhase : uint8
	{
		Start,
		Ready,
		Draw,
		Cleanup,
		Complete
	};

	/** Readback of the real PIE window after the native CommonUI page has reached Slate. */
	class FGolemVisualCommand final : public IAutomationLatentCommand
	{
	public:
		FGolemVisualCommand(FAutomationTestBase& InTest, FIntPoint InRequestedSize, const FString& InOutputSubdirectory)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted),
			RequestedSize(InRequestedSize), OutputSubdirectory(InOutputSubdirectory)
		{
		}

		virtual ~FGolemVisualCommand() override
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
					Test.AddError(FString::Printf(TEXT("Rendered SkillLab timeout phase=%d capture=%d."),
						static_cast<int32>(Phase), CaptureIndex));
				}
				BeginCleanup();
			}
			switch (Phase)
			{
			case EPhase::Start: Start(); break;
			case EPhase::Ready: Ready(); break;
			case EPhase::Draw: Draw(); break;
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
				Fail(TEXT("Rendered SkillLab requires an idle Editor, Slate/RHI and the genuine SkillLab map."));
				return;
			}
			OutputDirectory = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Evidence/M55/rendered"), OutputSubdirectory));
			if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
			{
				Fail(TEXT("Could not create the rendered SkillLab evidence directory."));
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
				const AGolemSkillLabGameMode* Mode = World->GetAuthGameMode<AGolemSkillLabGameMode>();
				const AGolemGameState* State = World->GetGameState<AGolemGameState>();
				const AGolemPartyState* Party = State != nullptr ? State->GetPartyState() : nullptr;
				if (Mode == nullptr || Party == nullptr || !Party->IsCanonicalRosterReady()
					|| !Mode->GetRPGInitializationResult().IsSuccess())
				{
					continue;
				}
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					AGolemSkillLabPlayerController* Player = Cast<AGolemSkillLabPlayerController>(It->Get());
					AGolemCharacter* Golem = Player != nullptr ? Player->GetControlledGolem() : nullptr;
					if (Player == nullptr || !Player->IsLocalController() || Golem == nullptr
						|| !Golem->IsLocallyControlled() || !Golem->CanTransferControl()
						|| Party->GetControlledSlot(Player) == INDEX_NONE
						|| Player->GetHUD<AGolemSkillLabHUD>() == nullptr)
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
						Fail(TEXT("Rendered SkillLab has no genuine sanctuary or confirmed domains."));
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
						|| !Panel->GetSnapshot().bCanEditBuild)
					{
						Fail(TEXT("Real ToggleRPGPanel did not open a confirmed native SkillLab panel."));
						return;
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

		bool PressButton(UGolemRPGActionButton* Button)
		{
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			const TSharedPtr<FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (Button == nullptr || !Button->GetIsEnabled() || !User.IsValid())
			{
				Fail(TEXT("Actual native CommonButton is absent, disabled or has no local Slate user."));
				return false;
			}
			Button->SetUserFocus(Owner.Get());
			if (!Button->HasUserFocus(Owner.Get()) && !Button->HasUserFocusedDescendants(Owner.Get()))
			{
				Fail(TEXT("Real CommonButton did not receive local Slate focus."));
				return false;
			}
			FScopedConditionalWorldSwitcher Switch(Owner->GetWorld());
			const FKeyEvent Event(EKeys::Enter, FModifierKeysState(), User->GetUserIndex(), false, 0, 0);
			const bool bDown = FSlateApplication::Get().ProcessKeyDownEvent(Event);
			const bool bUp = FSlateApplication::Get().ProcessKeyUpEvent(Event);
			if (!bDown && !bUp)
			{
				Fail(TEXT("Focused native CommonButton did not handle actual Slate Enter."));
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

		bool CheckButtonBounds(UGolemRPGActionButton* Button, const FGeometry& ViewportGeometry)
		{
			if (Button == nullptr)
			{
				Fail(TEXT("Required native tab/action is absent during layout verification."));
				return false;
			}
			const FGeometry& Geometry = Button->GetCachedGeometry();
			const FVector2D Min = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D Max = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
			const FVector2D ViewportMin = ViewportGeometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D ViewportMax = ViewportGeometry.LocalToAbsolute(ViewportGeometry.GetLocalSize());
			constexpr double Tolerance = 2.0;
			bool bInside = Max.X - Min.X > 1.0 && Max.Y - Min.Y > 1.0
				&& Min.X >= ViewportMin.X - Tolerance && Min.Y >= ViewportMin.Y - Tolerance
				&& Max.X <= ViewportMax.X + Tolerance && Max.Y <= ViewportMax.Y + Tolerance;
			// Focused flyout/socket controls must also fit inside their actual scroll clipping region.
			for (UPanelWidget* Parent = Button->GetParent(); Parent != nullptr; Parent = Parent->GetParent())
			{
				if (const UScrollBox* Scroll = Cast<UScrollBox>(Parent))
				{
					const FGeometry& Clip = Scroll->GetCachedGeometry();
					const FVector2D ClipMin = Clip.LocalToAbsolute(FVector2D::ZeroVector);
					const FVector2D ClipMax = Clip.LocalToAbsolute(Clip.GetLocalSize());
					bInside &= Min.X >= ClipMin.X - Tolerance && Min.Y >= ClipMin.Y - Tolerance
						&& Max.X <= ClipMax.X + Tolerance && Max.Y <= ClipMax.Y + Tolerance;
					break;
				}
			}
			if (!bInside)
			{
				Fail(FString::Printf(TEXT("Native control '%s' is zero-sized or clipped at %dx%d: bounds(%s,%s), viewport(%s,%s)."),
					*Button->GetCaption().ToString(), RequestedSize.X, RequestedSize.Y,
					*Min.ToString(), *Max.ToString(), *ViewportMin.ToString(), *ViewportMax.ToString()));
			}
			return bInside;
		}

		bool CheckRequiredLayout(const TSharedRef<SViewport>& ViewportWidget)
		{
			const FGeometry& Geometry = ViewportWidget->GetCachedGeometry();
			const TCHAR* Captions[] =
			{
				TEXT("Character"), TEXT("Specialization"), TEXT("Talents"), TEXT("Spellbook"), TEXT("Runes"),
				TEXT("Revert / Reset draft"), TEXT("Apply changes [F]"), TEXT("Close [Esc / B]")
			};
			for (const TCHAR* Caption : Captions)
			{
				if (!CheckButtonBounds(FindButton(Caption), Geometry))
				{
					return false;
				}
			}
			if (CaptureIndex == 2)
			{
				return CheckButtonBounds(FindButton(TEXT("Endurance\n+10 Stamina")), Geometry)
					&& CheckButtonBounds(FindButton(TEXT("Reservoir\n+10 Resource")), Geometry);
			}
			if (CaptureIndex == 4)
			{
				const FGolemRPGRuneRow* Rune = Owner->GetRPGPanel()->GetSnapshot().Runes.FindByPredicate(
					[](const FGolemRPGRuneRow& Row)
					{
						return Row.RuneId == TEXT("Skill.Convergence");
					});
				return Rune != nullptr && CheckButtonBounds(FindButton(Rune->Name.ToString() + TEXT("\nCompatible")), Geometry);
			}
			return true;
		}

		void Draw()
		{
			if (!Owner.IsValid() || !ControlledGolem.IsValid()
				|| Owner->GetControlledGolem() != ControlledGolem.Get()
				|| ControlledGolem->GetGolemEntityId() != GolemId)
			{
				Fail(TEXT("Canonical local Golem identity changed during rendered capture."));
				return;
			}
			UGolemRPGPanel* Panel = Owner->GetRPGPanel();
			if (Panel == nullptr || Panel->IsActivated() != (CaptureIndex < 5))
			{
				Fail(TEXT("Actual CommonUI panel activation did not match the requested rendered page."));
				return;
			}
			if (++WarmFrames < 3 || Elapsed() < DrawWarmupSeconds)
			{
				return;
			}
			if (!ConfirmedStateUnchanged())
			{
				Fail(TEXT("Presentation staging changed confirmed RPG/inventory values or issued a domain command."));
				return;
			}
			if (!bPagePrepared && CaptureIndex == 2)
			{
				// Class is the first real choice diamond in the native two-tree widget hierarchy.
				if (!PressButton(FindButton(Panel->GetDraftBuild().ClassChoice.IsNone() ? TEXT("0/1") : TEXT("1/1"))))
				{
					return;
				}
				bPagePrepared = true;
				Enter(EPhase::Draw);
				return;
			}
			UGolemRPGActionButton* Endurance = CaptureIndex == 2
				? FindButton(TEXT("Endurance\n+10 Stamina")) : nullptr;
			if (CaptureIndex == 2 && (Endurance == nullptr
				|| FindButton(TEXT("Reservoir\n+10 Resource")) == nullptr
				|| (!Endurance->HasUserFocus(Owner.Get()) && !Endurance->HasUserFocusedDescendants(Owner.Get()))
				|| !(Panel->GetDraftBuild() == Panel->GetSnapshot().Build)))
			{
				Fail(TEXT("Actual Class choice flyout did not expose both exclusive branches without changing the draft."));
				return;
			}
			if (!bPagePrepared && CaptureIndex == 4)
			{
				const FGolemRPGRuneRow* Rune = Panel->GetSnapshot().Runes.FindByPredicate([](const FGolemRPGRuneRow& Row)
				{
					return Row.RuneId == TEXT("Skill.Convergence") && Row.bCompatible;
				});
				if (Rune == nullptr || !PressButton(FindButton(Rune->Name.ToString() + TEXT("\nCompatible"))))
				{
					if (Rune == nullptr)
					{
						Fail(TEXT("Actual shared inventory has no compatible Convergence rune instance."));
					}
					return;
				}
				bPagePrepared = true;
				Enter(EPhase::Draw);
				return;
			}
			if (CaptureIndex == 4)
			{
				bool bRunestoneDistinction = false;
				Panel->WidgetTree->ForEachWidget([&bRunestoneDistinction](UWidget* Widget)
				{
					const UTextBlock* Text = Cast<UTextBlock>(Widget);
					bRunestoneDistinction |= Text != nullptr
						&& Text->GetText().ToString().Contains(TEXT("Runestone is a separate world interaction"));
				});
				if (Panel->GetDraftBuild().SkillRune != TEXT("Skill.Convergence") || !bRunestoneDistinction)
				{
					Fail(TEXT("Real Convergence draft or Attach Rune versus Runestone explanation is missing."));
					return;
				}
			}
			if (CaptureIndex == 5 && (Owner->IsMoveInputIgnored() || Owner->IsLookInputIgnored()
				|| Owner->bShowMouseCursor || !(Panel->GetDraftBuild() == Panel->GetSnapshot().Build)))
			{
				Fail(TEXT("Actual Revert/close failed to restore the confirmed draft and gameplay input."));
				return;
			}
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
			const TSharedPtr<SViewport> Widget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			if (!Widget.IsValid() || !FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()).IsValid())
			{
				Fail(TEXT("The actual PIE viewport has no rendered Slate window for readback."));
				return;
			}
			if (CaptureIndex < 5 && !CheckRequiredLayout(Widget.ToSharedRef()))
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
				TEXT("Character"), TEXT("Specialization"), TEXT("Talents"),
				TEXT("Spellbook"), TEXT("Runes"), TEXT("HUD")
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
			if (CaptureIndex == 5)
			{
				if (!PressButton(FindButton(TEXT("Revert / Reset draft")))
					|| !(Panel->GetDraftBuild() == Panel->GetSnapshot().Build) || !ConfirmedStateUnchanged())
				{
					if (Phase != EPhase::Cleanup)
					{
						Fail(TEXT("Actual Revert button failed to discard the rune preview without mutation."));
					}
					return;
				}
				Owner->ToggleRPGPanel();
			}
			else
			{
				Panel->SetActiveTab(static_cast<EGolemRPGPanelTab>(CaptureIndex));
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
				Test.AddError(TEXT("Rendered SkillLab cleanup timed out; forcing synchronous EndPlay."));
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
		int32 CaptureIndex = 0;
		int32 WarmFrames = 0;
		FString OutputDirectory;
		TSet<uint32> ImageCRCs;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<AGolemSkillLabPlayerController> Owner;
		TWeakObjectPtr<AGolemCharacter> ControlledGolem;
		TWeakObjectPtr<AGolemRPGStateActor> DomainState;
		FGolemRPGView ConfirmedRPG;
		FGolemInventoryView ConfirmedInventory;
		FGolemEntityId GolemId;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabVisualTest, "Golem.M55.Visual.NativeTabs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGolemSkillLabVisualTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemSkillLabVisual::FGolemVisualCommand>(*this, FIntPoint(1600, 900), FString()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabVisual720pTest, "Golem.M55.Visual.NativeTabs720p",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGolemSkillLabVisual720pTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemSkillLabVisual::FGolemVisualCommand>(*this, FIntPoint(1280, 720), FString(TEXT("720p"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabVisual1080pTest, "Golem.M55.Visual.NativeTabs1080p",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGolemSkillLabVisual1080pTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemSkillLabVisual::FGolemVisualCommand>(*this, FIntPoint(1920, 1080), FString(TEXT("1080p"))));
	return true;
}

#endif
