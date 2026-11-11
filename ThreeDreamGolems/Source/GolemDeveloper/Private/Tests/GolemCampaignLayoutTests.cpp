#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Campaign/GolemCampaignPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/GolemGameUserSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/GolemRPGActionButton.h"
#include "Widgets/GolemRPGPanel.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace GolemCampaignLayout
{
	class FCapture final : public IAutomationLatentCommand
	{
	public:
		FCapture(FAutomationTestBase& InTest, const FIntPoint InSize)
			: Test(InTest), Requested(InSize), Started(FPlatformTime::Seconds())
		{
			Original.Reset(DuplicateObject<UGolemGameUserSettings>(UGolemGameUserSettings::Get(), GetTransientPackage()));
		}
		~FCapture()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
			if (Original.IsValid())
			{
				UEngine::CopyPropertiesForUnrelatedObjects(Original.Get(), UGolemGameUserSettings::Get());
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Started > 100.0)
			{
				Test.AddError(FString::Printf(TEXT("Campaign layout timeout phase%d page%d"), Phase, Page));
				return true;
			}
			if (Phase == 0)
			{
				Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				Settings->SetPlayNetMode(PIE_Standalone);
				Settings->SetPlayNumberOfClients(1);
				Settings->SetRunUnderOneProcess(true);
				Settings->NewWindowWidth = Requested.X;
				Settings->NewWindowHeight = Requested.Y;
				Settings->SetClientWindowSize(Requested);
				Settings->EnableGameSound = false;
				Settings->GameGetsMouseControl = false;
				UGolemGameUserSettings::Get()->TextScale = 1.5f;
				UGolemGameUserSettings::Get()->SubtitleScale = 2.0f;
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Golem/Maps/Playables/L_M07_Campaign");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			if (Phase == 6)
			{
				return !GEditor->IsPlaySessionInProgress();
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					Owner = Cast<AGolemCampaignPlayerController>(Context.World()->GetFirstPlayerController());
				}
			}
			if (!Owner.IsValid() || !Owner->GetCampaignPresentation().bEnabled)
			{
				return false;
			}
			if (Phase == 1)
			{
				if (!Owner->GetCampaignPresentation().bFrontEnd || Owner->GetRPGPanel() == nullptr)
				{
					return false;
				}
				Owner->RequestCampaignAction(TEXT("New"));
				Owner->RequestCampaignAction(TEXT("Confirm"), TEXT("New"));
				Phase = 2;
				return false;
			}
			if (Phase == 2)
			{
				if (Owner->GetCampaignPresentation().bFrontEnd || Owner->GetCampaignPresentation().bPending)
				{
					return false;
				}
				Phase = 3;
			}
			ULocalPlayer* Local = Owner->GetLocalPlayer();
			UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
			const TSharedPtr<SViewport> Widget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			if (!Widget.IsValid() || Viewport->GetGameViewport() == nullptr)
			{
				return false;
			}
			const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef());
			const FIntPoint Actual = Viewport->GetGameViewport()->GetSizeXY();
			if (Actual != Requested)
			{
				if (Window.IsValid() && Actual.X > 0 && Actual.Y > 0)
				{
					Window->ReshapeWindow(Window->GetPositionInScreen(), Window->GetSizeInScreen() + FVector2D(Requested - Actual));
				}
				return false;
			}
			const EGolemRPGPanelTab Tabs[]{EGolemRPGPanelTab::Campaign, EGolemRPGPanelTab::Journal,
				EGolemRPGPanelTab::Map, EGolemRPGPanelTab::Settings, EGolemRPGPanelTab::Inventory,
				EGolemRPGPanelTab::Runes, EGolemRPGPanelTab::Talents};
			if (Phase == 3)
			{
				Owner->OpenCampaignPage(Tabs[Page]);
				Frames = 0;
				Phase = 4;
				return false;
			}
			if (Phase == 4 && ++Frames > 15)
			{
				TArray<FColor> Pixels;
				FIntVector Size;
				if (!FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size)
					|| Size.X != Requested.X || Size.Y != Requested.Y)
				{
					Test.AddError(TEXT("Rendered campaign readback failed or had incorrect dimensions"));
					return true;
				}
				for (FColor& Pixel : Pixels)
				{
					Pixel.A = 255;
				}
				TArray64<uint8> PNG;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PNG);
				const FString Directory = FPaths::ProjectDir() / TEXT("Saved/Evidence/M7/rendered")
					/ FString::Printf(TEXT("%dx%d"), Requested.X, Requested.Y);
				IFileManager::Get().MakeDirectory(*Directory, true);
				Test.TestTrue(TEXT("Save actual rendered largest-text page"), FFileHelper::SaveArrayToFile(PNG,
					*(Directory / FString::Printf(TEXT("Page_%d.png"), Page))));
				Test.AddInfo(FString::Printf(TEXT("Actual rendered layout %dx%d page%d text1.5 subtitle2.0"), Size.X, Size.Y, Page));
				if (++Page < UE_ARRAY_COUNT(Tabs))
				{
					Phase = 3;
				}
				else
				{
					Owner->OpenCampaignPage(EGolemRPGPanelTab::Settings);
					UGolemRPGActionButton* Control = FindFocusControl();
					if (!Test.TestNotNull(TEXT("Unique sensitivity control exists among repeated +/- captions"), Control))
					{
						return true;
					}
					Frames = 0;
					Phase = 5;
				}
			}
			if (Phase == 5 && ++Frames == 5)
			{
				FindFocusControl()->SetUserFocus(Owner.Get());
			}
			if (Phase == 5 && Frames == 8)
			{
				UGolemRPGActionButton* Control = FindFocusControl();
				Test.TestTrue(TEXT("Layout-complete sensitivity button has actual user focus before reconstruction"),
					Control != nullptr && (Control->HasUserFocus(Owner.Get()) || Control->HasUserFocusedDescendants(Owner.Get())));
				Owner->RequestCampaignAction(TEXT("InspectLoad"));
			}
			if (Phase == 5 && Frames > 20)
			{
				UGolemRPGActionButton* Control = FindFocusControl();
				Test.TestTrue(TEXT("Event-driven rebuild restores the exact semantic control while simulation is paused"),
					Control != nullptr && (Control->HasUserFocus(Owner.Get()) || Control->HasUserFocusedDescendants(Owner.Get())));
				for (int32 Index = 0; Index < 20; ++Index)
				{
					Owner->RequestCampaignAction(TEXT("Resume"));
					Test.TestFalse(TEXT("Repeated Back releases actual solo pause"), Owner->GetWorld()->IsPaused());
					Owner->OpenCampaignPage(EGolemRPGPanelTab::Campaign);
				}
				Owner->RequestCampaignAction(TEXT("Resume"));
				Test.TestFalse(TEXT("Repeated reopen releases movement input"), Owner->IsMoveInputIgnored());
				Test.TestFalse(TEXT("Repeated reopen releases look input"), Owner->IsLookInputIgnored());
				GEditor->EndPlayMap();
				Phase = 6;
			}
			return false;
		}

	private:
		UGolemRPGActionButton* FindFocusControl() const
		{
			UGolemRPGActionButton* Found = nullptr;
			Owner->GetRPGPanel()->WidgetTree->ForEachWidget([&Found](UWidget* Widget)
			{
				if (UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
					Button != nullptr && Button->GetFocusIdentity() == TEXT("SettingUp:Sensitivity"))
				{
					Found = Button;
				}
			});
			return Found;
		}
		FAutomationTestBase& Test;
		FIntPoint Requested;
		double Started;
		int32 Phase = 0;
		int32 Page = 0;
		int32 Frames = 0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TStrongObjectPtr<UGolemGameUserSettings> Original;
		TWeakObjectPtr<AGolemCampaignPlayerController> Owner;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGolemCampaignLayoutTest, "Golem.M7.Visual.LargestText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

void FGolemCampaignLayoutTest::GetTests(TArray<FString>& OutNames, TArray<FString>& OutCommands) const
{
	for (const TCHAR* Resolution : {TEXT("1280x720"), TEXT("1600x900"), TEXT("1920x1080"), TEXT("2560x1080")})
	{
		OutNames.Add(Resolution);
		OutCommands.Add(Resolution);
	}
}

bool FGolemCampaignLayoutTest::RunTest(const FString& Parameters)
{
	FString Width;
	FString Height;
	Parameters.Split(TEXT("x"), &Width, &Height);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemCampaignLayout::FCapture>(*this,
		FIntPoint(FCString::Atoi(*Width), FCString::Atoi(*Height))));
	return true;
}

#endif
