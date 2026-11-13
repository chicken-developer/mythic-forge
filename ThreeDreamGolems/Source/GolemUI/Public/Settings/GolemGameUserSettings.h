#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Types/GolemResult.h"
#include "GolemGameUserSettings.generated.h"

USTRUCT()
struct GOLEMUI_API FGolemUserKeyBinding
{
	GENERATED_BODY()
	UPROPERTY()
	FName ActionId;
	UPROPERTY()
	FKey DefaultKey;
	UPROPERTY()
	FKey Key;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemLocalSettingsChanged);

/** Machine-local preferences use the engine settings owner, independently of host campaign checkpoints. */
UCLASS(Config = GameUserSettings)
class GOLEMUI_API UGolemGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UGolemGameUserSettings* Get();
	static FString GetPreferenceFilePath();
	void EnsureBindings();
	FGolemResult ValidatePreferences() const;
	FGolemResult AdjustPreference(FName Option, bool bIncrease);
	FGolemResult RemapKey(FName ActionId, const FKey& Key);
	FGolemResult SavePreferences();
	void ResetPreferences();
	const FGolemResult& GetLastSaveResult() const;
	FKey GetActionKey(FName ActionId) const;
	FText GetPreferenceLabel(FName Option) const;
	bool HasSeenTutorial(FName TutorialId) const;
	void ConfirmTutorial(FName TutorialId);
	void ApplyAudio(UObject* WorldContext) const;
	FOnGolemLocalSettingsChanged OnPreferencesChanged;

	UPROPERTY(Config)
	float MasterVolume = 1.0f;
	UPROPERTY(Config)
	float MusicVolume = 0.8f;
	UPROPERTY(Config)
	float EffectsVolume = 1.0f;
	UPROPERTY(Config)
	float VoiceVolume = 1.0f;
	UPROPERTY(Config)
	float Sensitivity = 1.0f;
	UPROPERTY(Config)
	float TextScale = 1.0f;
	UPROPERTY(Config)
	float SubtitleScale = 1.0f;
	UPROPERTY(Config)
	float ShakeScale = 1.0f;
	UPROPERTY(Config)
	bool bInvertY = false;
	UPROPERTY(Config)
	bool bToggleBlock = false;
	UPROPERTY(Config)
	bool bSubtitleBackground = true;
	UPROPERTY(Config)
	bool bReducedMotion = false;
	UPROPERTY(Config)
	TArray<FGolemUserKeyBinding> KeyBindings;
	UPROPERTY(Config)
	TArray<FName> ConfirmedTutorials;

private:
	FGolemResult CheckWritable() const;
	FGolemResult LastSaveResult;
};
