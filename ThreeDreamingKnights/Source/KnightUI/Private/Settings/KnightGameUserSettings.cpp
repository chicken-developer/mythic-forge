#include "Settings/KnightGameUserSettings.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

FString UKnightGameUserSettings::GetPreferenceFilePath()
{
	// UE 5.8 uses a logical config name in GGameUserSettingsIni, rather than a disk path.
	return FPaths::ConvertRelativePathToFull(FConfigCacheIni::GetDestIniFilename(
		TEXT("GameUserSettings"), nullptr, *FPaths::GeneratedConfigDir()));
}

UKnightGameUserSettings* UKnightGameUserSettings::Get()
{
	UKnightGameUserSettings* Settings = GEngine != nullptr
		? Cast<UKnightGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
	if (Settings != nullptr)
	{
		if (!Settings->IsVersionValid())
		{
			Settings->ValidateSettings();
		}
		Settings->EnsureBindings();
	}
	return Settings;
}

void UKnightGameUserSettings::EnsureBindings()
{
	const TPair<FName, FKey> Defaults[]{
		{TEXT("Forward"), EKeys::W}, {TEXT("Backward"), EKeys::S},
		{TEXT("Left"), EKeys::A}, {TEXT("Right"), EKeys::D},
		{TEXT("Jump"), EKeys::SpaceBar}, {TEXT("Switch"), EKeys::Tab},
		{TEXT("Knight1"), EKeys::One}, {TEXT("Knight2"), EKeys::Two}, {TEXT("Knight3"), EKeys::Three},
		{TEXT("Retry"), EKeys::R}, {TEXT("Revive"), EKeys::E}, {TEXT("Interact"), EKeys::F},
		{TEXT("Light"), EKeys::LeftMouseButton}, {TEXT("Heavy"), EKeys::RightMouseButton},
		{TEXT("Block"), EKeys::Q}, {TEXT("Parry"), EKeys::C}, {TEXT("Dodge"), EKeys::LeftShift},
		{TEXT("Lock"), EKeys::MiddleMouseButton}, {TEXT("Rune"), EKeys::G},
		{TEXT("Finisher"), EKeys::H}, {TEXT("Traverse"), EKeys::V},
		{TEXT("Skill1"), EKeys::F1}, {TEXT("Skill2"), EKeys::F2},
		{TEXT("Skill3"), EKeys::F3}, {TEXT("Skill4"), EKeys::F4}};
	for (const TPair<FName, FKey>& Pair : Defaults)
	{
		if (KeyBindings.ContainsByPredicate([&Pair](const FKnightUserKeyBinding& Binding)
		{
			return Binding.ActionId == Pair.Key;
		}))
		{
			continue;
		}
		FKnightUserKeyBinding& Binding = KeyBindings.AddDefaulted_GetRef();
		Binding.ActionId = Pair.Key;
		Binding.DefaultKey = Pair.Value;
		Binding.Key = Pair.Value;
	}
}

FKnightResult UKnightGameUserSettings::ValidatePreferences() const
{
	for (const float Value : {MasterVolume, MusicVolume, EffectsVolume, VoiceVolume, ShakeScale})
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 1.0f)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("VolumeOrShake"), FText::GetEmpty());
		}
	}
	if (!FMath::IsFinite(Sensitivity) || Sensitivity < 0.1f || Sensitivity > 4.0f
		|| !FMath::IsFinite(TextScale) || TextScale < 0.75f || TextScale > 1.5f
		|| !FMath::IsFinite(SubtitleScale) || SubtitleScale < 0.75f || SubtitleScale > 2.0f)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("AccessibilityRange"), FText::GetEmpty());
	}
	TSet<FName> Actions;
	TSet<FKey> Keys;
	const TSet<FKey> Reserved{EKeys::Escape, EKeys::Enter, EKeys::I, EKeys::M, EKeys::J,
		EKeys::P, EKeys::T, EKeys::N, EKeys::Z, EKeys::B, EKeys::K, EKeys::MouseX, EKeys::MouseY};
	for (const FKnightUserKeyBinding& Binding : KeyBindings)
	{
		if (Binding.ActionId.IsNone() || Actions.Contains(Binding.ActionId) || !Binding.Key.IsValid()
			|| Binding.Key.IsGamepadKey() || Binding.Key.IsAnalog() || Reserved.Contains(Binding.Key)
			|| Keys.Contains(Binding.Key))
		{
			return FKnightResult::Failure(EKnightError::Conflict, Binding.ActionId,
				NSLOCTEXT("KnightSettings", "KeyConflict", "That key is reserved or assigned to another action."));
		}
		Actions.Add(Binding.ActionId);
		Keys.Add(Binding.Key);
	}
	return FKnightResult::Success();
}

FKnightResult UKnightGameUserSettings::RemapKey(const FName ActionId, const FKey& Key)
{
	const FKnightResult Writable = CheckWritable();
	if (!Writable.IsSuccess())
	{
		return Writable;
	}
	EnsureBindings();
	FKnightUserKeyBinding* Binding = KeyBindings.FindByPredicate([ActionId](const FKnightUserKeyBinding& Row)
	{
		return Row.ActionId == ActionId;
	});
	if (Binding == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotFound, ActionId, FText::GetEmpty());
	}
	const FKey Previous = Binding->Key;
	Binding->Key = Key;
	const FKnightResult Result = ValidatePreferences();
	if (!Result.IsSuccess())
	{
		Binding->Key = Previous;
		return Result;
	}
	return SavePreferences();
}

FKnightResult UKnightGameUserSettings::AdjustPreference(const FName Option, const bool bIncrease)
{
	const FKnightResult Writable = CheckWritable();
	if (!Writable.IsSuccess())
	{
		return Writable;
	}
	float* Scalar = Option == TEXT("Master") ? &MasterVolume : Option == TEXT("Music") ? &MusicVolume
		: Option == TEXT("Effects") ? &EffectsVolume : Option == TEXT("Voice") ? &VoiceVolume
		: Option == TEXT("Sensitivity") ? &Sensitivity : Option == TEXT("Text") ? &TextScale
		: Option == TEXT("Subtitles") ? &SubtitleScale : Option == TEXT("Shake") ? &ShakeScale : nullptr;
	const float Direction = bIncrease ? 1.0f : -1.0f;
	if (Scalar != nullptr)
	{
		const float Minimum = Option == TEXT("Sensitivity") ? 0.1f
			: Option == TEXT("Text") || Option == TEXT("Subtitles") ? 0.75f : 0.0f;
		const float Maximum = Option == TEXT("Sensitivity") ? 4.0f : Option == TEXT("Text") ? 1.5f
			: Option == TEXT("Subtitles") ? 2.0f : 1.0f;
		*Scalar = FMath::Clamp(*Scalar + Direction * 0.1f, Minimum, Maximum);
	}
	else if (Option == TEXT("Invert"))
	{
		bInvertY = !bInvertY;
	}
	else if (Option == TEXT("ToggleBlock"))
	{
		bToggleBlock = !bToggleBlock;
	}
	else if (Option == TEXT("HoldLock"))
	{
		bHoldLock = !bHoldLock;
	}
	else if (Option == TEXT("SubtitleBackground"))
	{
		bSubtitleBackground = !bSubtitleBackground;
	}
	else if (Option == TEXT("ReducedMotion"))
	{
		bReducedMotion = !bReducedMotion;
	}
	else if (Option == TEXT("VSync"))
	{
		SetVSyncEnabled(!IsVSyncEnabled());
	}
	else if (Option == TEXT("Quality"))
	{
		SetOverallScalabilityLevel(FMath::Clamp(GetOverallScalabilityLevel() + (bIncrease ? 1 : -1), 0, 3));
	}
	else if (Option == TEXT("Display"))
	{
		const FIntPoint Sizes[]{FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1080)};
		int32 Current = 1;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sizes); ++Index)
		{
			if (GetScreenResolution() == Sizes[Index])
			{
				Current = Index;
			}
		}
		SetScreenResolution(Sizes[(Current + (bIncrease ? 1 : 3)) % 4]);
		SetFullscreenMode(EWindowMode::Windowed);
	}
	else
	{
		return FKnightResult::Failure(EKnightError::NotFound, Option, FText::GetEmpty());
	}
	return SavePreferences();
}

FKnightResult UKnightGameUserSettings::CheckWritable() const
{
	if (GGameUserSettingsIni.IsEmpty()
		|| FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*GetPreferenceFilePath()))
	{
		return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("SettingsWrite"),
			NSLOCTEXT("KnightSettings", "WriteFailed", "Settings could not be saved. Check file permissions."));
	}
	return FKnightResult::Success();
}

const FKnightResult& UKnightGameUserSettings::GetLastSaveResult() const
{
	return LastSaveResult;
}

FKnightResult UKnightGameUserSettings::SavePreferences()
{
	LastSaveResult = ValidatePreferences();
	if (LastSaveResult.IsSuccess())
	{
		LastSaveResult = CheckWritable();
	}
	if (!LastSaveResult.IsSuccess())
	{
		return LastSaveResult;
	}
	ApplySettings(false);
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	FString Verified;
	if (!FFileHelper::LoadFileToString(Verified, *GetPreferenceFilePath()))
	{
		LastSaveResult = FKnightResult::Failure(EKnightError::LoadFailed, TEXT("SettingsReadBack"),
			NSLOCTEXT("KnightSettings", "ReadBackFailed", "Settings could not be verified on disk."));
		return LastSaveResult;
	}
	FConfigFile Disk;
	Disk.Read(GetPreferenceFilePath());
	const FString Section = GetClass()->GetPathName();
	bool bMatches = true;
	const TArray<TPair<FName, float>> Scalars{{TEXT("MasterVolume"), MasterVolume}, {TEXT("MusicVolume"), MusicVolume},
		{TEXT("EffectsVolume"), EffectsVolume}, {TEXT("VoiceVolume"), VoiceVolume}, {TEXT("Sensitivity"), Sensitivity},
		{TEXT("TextScale"), TextScale}, {TEXT("SubtitleScale"), SubtitleScale}, {TEXT("ShakeScale"), ShakeScale}};
	for (const TPair<FName, float>& Scalar : Scalars)
	{
		float Actual = 0.0f;
		bMatches &= Disk.GetFloat(*Section, *Scalar.Key.ToString(), Actual) && FMath::IsNearlyEqual(Actual, Scalar.Value);
	}
	const TArray<TPair<FName, bool>> Toggles{{TEXT("bInvertY"), bInvertY}, {TEXT("bToggleBlock"), bToggleBlock},
		{TEXT("bHoldLock"), bHoldLock}, {TEXT("bSubtitleBackground"), bSubtitleBackground}, {TEXT("bReducedMotion"), bReducedMotion}};
	for (const TPair<FName, bool>& Toggle : Toggles)
	{
		bool bActual = false;
		bMatches &= Disk.GetBool(*Section, *Toggle.Key.ToString(), bActual) && bActual == Toggle.Value;
	}
	TArray<FString> StoredBindings;
	Disk.GetArray(*Section, TEXT("KeyBindings"), StoredBindings);
	bMatches &= StoredBindings.Num() == KeyBindings.Num();
	if (!bMatches)
	{
		LastSaveResult = FKnightResult::Failure(EKnightError::LoadFailed, TEXT("SettingsReadBack"),
			NSLOCTEXT("KnightSettings", "ValuesNotSaved", "The settings file does not contain the requested values."));
		return LastSaveResult;
	}
	OnPreferencesChanged.Broadcast();
	return LastSaveResult;
}

void UKnightGameUserSettings::ResetPreferences()
{
	LastSaveResult = CheckWritable();
	if (!LastSaveResult.IsSuccess())
	{
		return;
	}
	MasterVolume = 1.0f;
	MusicVolume = 0.8f;
	EffectsVolume = VoiceVolume = Sensitivity = TextScale = SubtitleScale = ShakeScale = 1.0f;
	bInvertY = bToggleBlock = bHoldLock = bReducedMotion = false;
	bSubtitleBackground = true;
	KeyBindings.Reset();
	EnsureBindings();
	SavePreferences();
}

FKey UKnightGameUserSettings::GetActionKey(const FName ActionId) const
{
	const FKnightUserKeyBinding* Binding = KeyBindings.FindByPredicate([ActionId](const FKnightUserKeyBinding& Row)
	{
		return Row.ActionId == ActionId;
	});
	return Binding != nullptr ? Binding->Key : FKey();
}

FText UKnightGameUserSettings::GetPreferenceLabel(const FName Option) const
{
	if (Option == TEXT("Display"))
	{
		return FText::Format(NSLOCTEXT("KnightSettings", "Resolution", "Window {0} x {1}"),
			FText::AsNumber(GetScreenResolution().X), FText::AsNumber(GetScreenResolution().Y));
	}
	if (Option == TEXT("Quality"))
	{
		return FText::AsNumber(GetOverallScalabilityLevel());
	}
	if (Option == TEXT("VSync"))
	{
		return IsVSyncEnabled() ? NSLOCTEXT("KnightSettings", "On", "On") : NSLOCTEXT("KnightSettings", "Off", "Off");
	}
	const float* Value = Option == TEXT("Master") ? &MasterVolume : Option == TEXT("Music") ? &MusicVolume
		: Option == TEXT("Effects") ? &EffectsVolume : Option == TEXT("Voice") ? &VoiceVolume
		: Option == TEXT("Sensitivity") ? &Sensitivity : Option == TEXT("Text") ? &TextScale
		: Option == TEXT("Subtitles") ? &SubtitleScale : Option == TEXT("Shake") ? &ShakeScale : nullptr;
	if (Value != nullptr)
	{
		return FText::AsNumber(FMath::RoundToInt(*Value * 100.0f));
	}
	const bool bEnabled = Option == TEXT("Invert") ? bInvertY : Option == TEXT("ToggleBlock") ? bToggleBlock
		: Option == TEXT("HoldLock") ? bHoldLock : Option == TEXT("SubtitleBackground") ? bSubtitleBackground : bReducedMotion;
	return bEnabled ? NSLOCTEXT("KnightSettings", "On", "On") : NSLOCTEXT("KnightSettings", "Off", "Off");
}

bool UKnightGameUserSettings::HasSeenTutorial(const FName TutorialId) const
{
	return ConfirmedTutorials.Contains(TutorialId);
}

void UKnightGameUserSettings::ConfirmTutorial(const FName TutorialId)
{
	if (!TutorialId.IsNone() && !ConfirmedTutorials.Contains(TutorialId))
	{
		ConfirmedTutorials.Add(TutorialId);
		SaveConfig();
	}
}

void UKnightGameUserSettings::ApplyAudio(UObject* WorldContext) const
{
	USoundMix* Mix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Knight/Audio/SMix_Knight_User.SMix_Knight_User"));
	if (WorldContext == nullptr || Mix == nullptr)
	{
		return;
	}
	UGameplayStatics::SetBaseSoundMix(WorldContext, Mix);
	const TCHAR* Names[]{TEXT("Master"), TEXT("Music"), TEXT("Effects"), TEXT("Voice")};
	const float Values[]{MasterVolume, MusicVolume, EffectsVolume, VoiceVolume};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		const FString Name = FString(TEXT("SC_Knight_")) + Names[Index];
		USoundClass* Bus = LoadObject<USoundClass>(nullptr, *(TEXT("/Game/Knight/Audio/") + Name + TEXT(".") + Name));
		if (Bus != nullptr)
		{
			UGameplayStatics::SetSoundMixClassOverride(WorldContext, Mix, Bus, Values[Index], 1.0f, 0.05f, true);
		}
	}
}
