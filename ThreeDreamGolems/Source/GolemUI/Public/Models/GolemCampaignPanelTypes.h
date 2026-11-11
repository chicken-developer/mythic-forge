#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

struct GOLEMUI_API FGolemCampaignJournalRow
{
	FName QuestId;
	FText Title;
	FText Progress;
	bool bCanActivate = false;
};

struct GOLEMUI_API FGolemCampaignMarkerRow
{
	FName MarkerId;
	FName RegionId;
	FText Label;
	FText Symbol;
	FVector2D UV = FVector2D::ZeroVector;
	bool bDiscovered = false;
	bool bCanTravel = false;
	bool bCanPing = false;
};

struct GOLEMUI_API FGolemCampaignRegionRow
{
	FName RegionId;
	FText Label;
	bool bDiscovered = false;
};

struct GOLEMUI_API FGolemCampaignPanelSnapshot
{
	bool bEnabled = false;
	bool bHost = false;
	bool bFrontEnd = false;
	bool bPending = false;
	bool bHasCheckpoint = false;
	bool bHasDifficultyControls = false;
	FText Difficulty;
	FName Confirmation;
	FName RegionId;
	FText SaveStatus;
	FText SessionStatus;
	FText Tutorial;
	FText Subtitle;
	FText Feedback;
	FText RegionLabel;
	FText ActiveObjective;
	TArray<FGolemCampaignJournalRow> Quests;
	TArray<FGolemCampaignMarkerRow> Markers;
	TArray<FGolemCampaignRegionRow> Regions;
	FVector2D PlayerUV = FVector2D::ZeroVector;
	bool bPlayerVisible = false;
	float TextScale = 1.0f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemCampaignPanelAction, FName, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemCampaignKeyRemap, FName, FKey);
