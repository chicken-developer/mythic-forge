#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Types/KnightResult.h"
#include "KnightGameUserSettings.generated.h"

USTRUCT()
struct KNIGHTUI_API FKnightUserKeyBinding
{
	GENERATED_BODY()
	UPROPERTY()
	FName ActionId;
	UPROPERTY()
	FKey DefaultKey;
	UPROPERTY()
	FKey Key;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightLocalSettingsChanged);

/** Machine-local preferences use the engine settings owner, independently of host campaign checkpoints. */
UCLASS(Config = GameUserSettings)
class KNIGHTUI_API UKnightGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UKnightGameUserSettings* Get();
	static FString GetPreferenceFilePath();
	void EnsureBindings();
	FKnightResult ValidatePreferences() const;
	FKnightResult AdjustPreference(FName Option, bool bIncrease);
	FKnightResult RemapKey(FName ActionId, const FKey& Key);
	FKnightResult SavePreferences();
	void ResetPreferences();
	const FKnightResult& GetLastSaveResult() const;
	FKey GetActionKey(FName ActionId) const;
	FText GetPreferenceLabel(FName Option) const;
	bool HasSeenTutorial(FName TutorialId) const;
	void ConfirmTutorial(FName TutorialId);
	void ApplyAudio(UObject* WorldContext) const;
	FOnKnightLocalSettingsChanged OnPreferencesChanged;

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
	bool bHoldLock = false;
	UPROPERTY(Config)
	bool bSubtitleBackground = true;
	UPROPERTY(Config)
	bool bReducedMotion = false;
	UPROPERTY(Config)
	TArray<FKnightUserKeyBinding> KeyBindings;
	UPROPERTY(Config)
	TArray<FName> ConfirmedTutorials;

private:
	FKnightResult CheckWritable() const;
	FKnightResult LastSaveResult;
};
