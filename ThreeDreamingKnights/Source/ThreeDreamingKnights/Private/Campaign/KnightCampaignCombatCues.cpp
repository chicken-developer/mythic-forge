#include "Campaign/KnightCampaignPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Components/KnightCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Definitions/KnightStoryWorldDefinition.h"
#include "Settings/KnightGameUserSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWaveProcedural.h"

void AKnightCampaignPlayerController::BindThreatSources()
{
	if (!ThreatSpawnHandle.IsValid())
	{
		ThreatSpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(
			this, &ThisClass::ObserveThreat));
	}
	for (TActorIterator<AKnightArenaEnemy> It(GetWorld()); It; ++It)
	{
		ObserveThreat(*It);
	}
	for (TActorIterator<AKnightStoryAnchor> It(GetWorld()); It; ++It)
	{
		ObserveThreat(*It);
	}
}

void AKnightCampaignPlayerController::ObserveThreat(AActor* Actor)
{
	if (AKnightStoryAnchor* Anchor = Cast<AKnightStoryAnchor>(Actor))
	{
		CampaignAnchors.RemoveAll([](const TWeakObjectPtr<AKnightStoryAnchor>& Existing)
		{
			return !Existing.IsValid() || Existing->IsActorBeingDestroyed();
		});
		CampaignAnchors.AddUnique(Anchor);
		UpdateInteractionHint();
		return;
	}
	AKnightArenaEnemy* Enemy = Cast<AKnightArenaEnemy>(Actor);
	if (Enemy == nullptr)
	{
		return;
	}
	for (int32 Index = ThreatBindings.Num() - 1; Index >= 0; --Index)
	{
		UKnightCombatComponent* Source = ThreatBindings[Index].Source.Get();
		if (Source == nullptr || Source->GetOwner()->IsActorBeingDestroyed())
		{
			if (Source != nullptr)
			{
				Source->OnCombatSnapshotChanged.Remove(ThreatBindings[Index].Handle);
			}
			ThreatBindings.RemoveAt(Index);
		}
	}
	UKnightCombatComponent* Combat = Enemy->GetCombatComponent();
	if (ThreatBindings.ContainsByPredicate([Combat](const FThreatBinding& Binding)
	{
		return Binding.Source.Get() == Combat;
	}))
	{
		return;
	}
	FThreatBinding Binding;
	Binding.Source = Combat;
	Binding.Handle = Combat->OnCombatSnapshotChanged.AddWeakLambda(this,
		[this, WeakEnemy = TWeakObjectPtr<AKnightArenaEnemy>(Enemy), PreviousPhase = uint8(0)](const FKnightCombatSnapshot& Snapshot) mutable
	{
		const bool bWindupStarted = Snapshot.AttackPhase == 1 && PreviousPhase != 1;
		PreviousPhase = Snapshot.AttackPhase;
		if (!bWindupStarted || !WeakEnemy.IsValid() || GetControlledKnight() == nullptr || bFrontEnd)
		{
			return;
		}
		const FVector Direction = (WeakEnemy->GetActorLocation() - GetControlledKnight()->GetActorLocation()).GetSafeNormal2D();
		const FVector Forward = GetControlRotation().Vector().GetSafeNormal2D();
		const float Ahead = FVector::DotProduct(Forward, Direction);
		const float Side = FVector::CrossProduct(Forward, Direction).Z;
		const FText Heading = Ahead < -0.35f ? NSLOCTEXT("KnightThreat", "Behind", "BEHIND")
			: Ahead > 0.65f ? NSLOCTEXT("KnightThreat", "Ahead", "AHEAD")
			: Side > 0.0f ? NSLOCTEXT("KnightThreat", "Right", "RIGHT") : NSLOCTEXT("KnightThreat", "Left", "LEFT");
		const double Now = GetWorld()->GetGameState() != nullptr
			? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		Hud.Threats.RemoveAll([Now](const FKnightCampaignThreatView& View)
		{
			return View.EndTime <= Now;
		});
		if (Hud.Threats.Num() >= 3)
		{
			Hud.Threats.RemoveAt(0);
		}
		FKnightCampaignThreatView& View = Hud.Threats.AddDefaulted_GetRef();
		View.Text = FText::Format(NSLOCTEXT("KnightThreat", "Windup", "Threat {0}: hammer windup — prepare defense"), Heading);
		View.EndTime = Now + 1.1;
	});
	ThreatBindings.Add(Binding);
}

void AKnightCampaignPlayerController::RemoveThreatSources()
{
	GetWorld()->RemoveOnActorSpawnedHandler(ThreatSpawnHandle);
	ThreatSpawnHandle.Reset();
	for (const FThreatBinding& Binding : ThreatBindings)
	{
		if (Binding.Source.IsValid())
		{
			Binding.Source->OnCombatSnapshotChanged.Remove(Binding.Handle);
		}
	}
	ThreatBindings.Reset();
	Hud.Threats.Reset();
	CampaignAnchors.Reset();
}

void AKnightCampaignPlayerController::UpdateInteractionHint()
{
	Hud.Interaction = Presentation.ActiveObjective;
	if (CampaignWorld == nullptr || GetControlledKnight() == nullptr)
	{
		return;
	}
	const FKnightStoryInteraction* Nearest = nullptr;
	float Distance = 1200.0f;
	for (const TWeakObjectPtr<AKnightStoryAnchor>& Anchor : CampaignAnchors)
	{
		if (!Anchor.IsValid() || Anchor->IsActorBeingDestroyed())
		{
			continue;
		}
		const FKnightStoryInteraction* Interaction = CampaignWorld->FindInteraction(Anchor->AnchorId);
		const float Candidate = FVector::Distance(GetControlledKnight()->GetActorLocation(), Anchor->GetActorLocation());
		if (Interaction != nullptr && Candidate < Distance)
		{
			Nearest = Interaction;
			Distance = Candidate;
		}
	}
	if (Nearest != nullptr)
	{
		UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
		const FText Key = Settings != nullptr ? Settings->GetActionKey(TEXT("Interact")).GetDisplayName() : FText();
		const FText Range = Distance <= Nearest->InteractionRange
			? FText::Format(NSLOCTEXT("KnightCampaignHUD", "InteractReady", "[{0} / Y] Interact"), Key)
			: NSLOCTEXT("KnightCampaignHUD", "Approach", "Move closer to interact");
		Hud.Interaction = FText::Format(NSLOCTEXT("KnightCampaignHUD", "NearbyInteraction", "{0}\n{1} | {2} cm | {3}"),
			Presentation.ActiveObjective, Nearest->Label, FText::AsNumber(FMath::RoundToInt(Distance)), Range);
	}
}

void AKnightCampaignPlayerController::PlayConfirmedImpactTone()
{
	// Prototype cue uses the real Effects bus; production foley remains a separate content gate.
	FeedbackTone = NewObject<USoundWaveProcedural>(this);
	FeedbackTone->SetSampleRate(24000);
	FeedbackTone->NumChannels = 1;
	FeedbackTone->Duration = 0.08f;
	FeedbackTone->SoundClassObject = LoadObject<USoundClass>(nullptr,
		TEXT("/Game/Knight/Audio/SC_Knight_Effects.SC_Knight_Effects"));
	TArray<int16> PCM;
	PCM.SetNumUninitialized(1920);
	for (int32 Index = 0; Index < PCM.Num(); ++Index)
	{
		const float Time = Index / 24000.0f;
		const float Envelope = FMath::Square(1.0f - Index / static_cast<float>(PCM.Num()));
		PCM[Index] = static_cast<int16>(2500.0f * Envelope * FMath::Sin(Time * 1900.0f * 2.0f * PI));
	}
	FeedbackTone->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));
	UGameplayStatics::PlaySound2D(this, FeedbackTone, 0.35f);
}
