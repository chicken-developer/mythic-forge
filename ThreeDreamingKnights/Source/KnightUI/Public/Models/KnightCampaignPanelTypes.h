#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

struct KNIGHTUI_API FKnightCampaignJournalRow
{
	FName QuestId;
	FText Title;
	FText Progress;
	bool bCanActivate = false;
};

struct KNIGHTUI_API FKnightCampaignMarkerRow
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

struct KNIGHTUI_API FKnightCampaignRegionRow
{
	FName RegionId;
	FText Label;
	bool bDiscovered = false;
};

struct KNIGHTUI_API FKnightCampaignPanelSnapshot
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
	TArray<FKnightCampaignJournalRow> Quests;
	TArray<FKnightCampaignMarkerRow> Markers;
	TArray<FKnightCampaignRegionRow> Regions;
	FVector2D PlayerUV = FVector2D::ZeroVector;
	bool bPlayerVisible = false;
	float TextScale = 1.0f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightCampaignPanelAction, FName, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightCampaignKeyRemap, FName, FKey);
