#include "Commandlets/GolemCampaignPreferencesCommandlet.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Settings/GolemGameUserSettings.h"

UGolemCampaignPreferencesCommandlet::UGolemCampaignPreferencesCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UGolemCampaignPreferencesCommandlet::Main(const FString& Params)
{
	FString FixturePath;
	FString Operation;
	FParse::Value(*Params, TEXT("SettingsFixture="), FixturePath);
	FParse::Value(*Params, TEXT("Operation="), Operation);
	FixturePath = FPaths::ConvertRelativePathToFull(FixturePath);
	const FString AllowedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Saved/Evidence/M7/Settings/"));
	if (!FixturePath.StartsWith(AllowedRoot) || !FixturePath.EndsWith(TEXT(".ini")))
	{
		UE_LOG(LogTemp, Error, TEXT("Preferences probe requires a .ini inside Saved/Evidence/M7/Settings."));
		return 1;
	}
	UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
	FString ActualPath = UGolemGameUserSettings::GetPreferenceFilePath();
	FPaths::MakeStandardFilename(ActualPath);
	FPaths::MakeStandardFilename(FixturePath);
	if (ActualPath != FixturePath)
	{
		UE_LOG(LogTemp, Error, TEXT("Pass -GameUserSettingsINI with the exact isolated fixture path."));
		return 1;
	}
	Settings->EnsureBindings();
	bool bPassed = false;
	if (Operation == TEXT("Write"))
	{
		Settings->MasterVolume = 0.35f;
		Settings->MusicVolume = 0.25f;
		Settings->EffectsVolume = 0.45f;
		Settings->VoiceVolume = 0.55f;
		Settings->Sensitivity = 1.6f;
		Settings->TextScale = 1.5f;
		Settings->SubtitleScale = 2.0f;
		Settings->ShakeScale = 0.0f;
		Settings->bInvertY = Settings->bToggleBlock = Settings->bHoldLock = Settings->bReducedMotion = true;
		Settings->bSubtitleBackground = false;
		Settings->SetScreenResolution(FIntPoint(1600, 900));
		Settings->SetFullscreenMode(EWindowMode::Windowed);
		Settings->ConfirmTutorial(TEXT("Move"));
		bPassed = Settings->RemapKey(TEXT("Forward"), EKeys::Up).IsSuccess();
	}
	else if (Operation == TEXT("Verify"))
	{
		bPassed = Settings->ValidatePreferences().IsSuccess() && FMath::IsNearlyEqual(Settings->MasterVolume, 0.35f)
			&& FMath::IsNearlyEqual(Settings->MusicVolume, 0.25f) && FMath::IsNearlyEqual(Settings->EffectsVolume, 0.45f)
			&& FMath::IsNearlyEqual(Settings->VoiceVolume, 0.55f) && FMath::IsNearlyEqual(Settings->Sensitivity, 1.6f)
			&& Settings->TextScale == 1.5f && Settings->SubtitleScale == 2.0f && Settings->ShakeScale == 0.0f
			&& Settings->bInvertY && Settings->bToggleBlock && Settings->bHoldLock && Settings->bReducedMotion
			&& !Settings->bSubtitleBackground && Settings->GetActionKey(TEXT("Forward")) == EKeys::Up
			&& Settings->HasSeenTutorial(TEXT("Move")) && Settings->GetScreenResolution() == FIntPoint(1600, 900);
	}
	else if (Operation == TEXT("ReadOnly"))
	{
		const float OriginalMusic = Settings->MusicVolume;
		bPassed = !Settings->AdjustPreference(TEXT("Music"), true).IsSuccess() && Settings->MusicVolume == OriginalMusic
			&& !Settings->RemapKey(TEXT("Forward"), EKeys::Right).IsSuccess();
		Settings->ResetPreferences();
		bPassed &= !Settings->GetLastSaveResult().IsSuccess() && Settings->GetActionKey(TEXT("Forward")) == EKeys::Up
			&& Settings->Sensitivity == 1.6f;
	}
	else if (Operation == TEXT("Reset"))
	{
		Settings->ResetPreferences();
		bPassed = Settings->GetLastSaveResult().IsSuccess();
	}
	else if (Operation == TEXT("VerifyReset"))
	{
		bPassed = Settings->GetActionKey(TEXT("Forward")) == EKeys::W && Settings->Sensitivity == 1.0f
			&& Settings->TextScale == 1.0f && Settings->SubtitleScale == 1.0f && Settings->bSubtitleBackground
			&& !Settings->bInvertY && !Settings->bToggleBlock && !Settings->bHoldLock && !Settings->bReducedMotion;
	}
	bPassed &= UGolemGameUserSettings::GetPreferenceFilePath() == ActualPath;
	UE_LOG(LogTemp, Display, TEXT("CampaignPreferencesColdProof operation=%s result=%s path=%s bindings=%d"),
		*Operation, bPassed ? TEXT("PASS") : TEXT("FAIL"), *FixturePath, Settings->KeyBindings.Num());
	return bPassed ? 0 : 1;
}
