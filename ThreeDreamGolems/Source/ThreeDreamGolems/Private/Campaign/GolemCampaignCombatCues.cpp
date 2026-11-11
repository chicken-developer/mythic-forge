#include "Campaign/GolemCampaignPlayerController.h"

#include "Character/GolemCharacter.h"
#include "Components/GolemCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Settings/GolemGameUserSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWaveProcedural.h"

void AGolemCampaignPlayerController::BindThreatSources()
{
	if (!ThreatSpawnHandle.IsValid())
	{
		ThreatSpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(
			this, &ThisClass::ObserveThreat));
	}
	for (TActorIterator<AGolemArenaEnemy> It(GetWorld()); It; ++It)
	{
		ObserveThreat(*It);
	}
	for (TActorIterator<AGolemStoryAnchor> It(GetWorld()); It; ++It)
	{
		ObserveThreat(*It);
	}
}

void AGolemCampaignPlayerController::ObserveThreat(AActor* Actor)
{
	if (AGolemStoryAnchor* Anchor = Cast<AGolemStoryAnchor>(Actor))
	{
		CampaignAnchors.RemoveAll([](const TWeakObjectPtr<AGolemStoryAnchor>& Existing)
		{
			return !Existing.IsValid() || Existing->IsActorBeingDestroyed();
		});
		CampaignAnchors.AddUnique(Anchor);
		UpdateInteractionHint();
		return;
	}
	AGolemArenaEnemy* Enemy = Cast<AGolemArenaEnemy>(Actor);
	if (Enemy == nullptr)
	{
		return;
	}
	for (int32 Index = ThreatBindings.Num() - 1; Index >= 0; --Index)
	{
		UGolemCombatComponent* Source = ThreatBindings[Index].Source.Get();
		if (Source == nullptr || Source->GetOwner()->IsActorBeingDestroyed())
		{
			if (Source != nullptr)
			{
				Source->OnCombatSnapshotChanged.Remove(ThreatBindings[Index].Handle);
			}
			ThreatBindings.RemoveAt(Index);
		}
	}
	UGolemCombatComponent* Combat = Enemy->GetCombatComponent();
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
		[this, WeakEnemy = TWeakObjectPtr<AGolemArenaEnemy>(Enemy), PreviousPhase = uint8(0)](const FGolemCombatSnapshot& Snapshot) mutable
	{
		const bool bWindupStarted = Snapshot.AttackPhase == 1 && PreviousPhase != 1;
		PreviousPhase = Snapshot.AttackPhase;
		if (!bWindupStarted || !WeakEnemy.IsValid() || GetControlledGolem() == nullptr || bFrontEnd)
		{
			return;
		}
		const FVector Direction = (WeakEnemy->GetActorLocation() - GetControlledGolem()->GetActorLocation()).GetSafeNormal2D();
		const FVector Forward = GetControlRotation().Vector().GetSafeNormal2D();
		const float Ahead = FVector::DotProduct(Forward, Direction);
		const float Side = FVector::CrossProduct(Forward, Direction).Z;
		const FText Heading = Ahead < -0.35f ? NSLOCTEXT("GolemThreat", "Behind", "BEHIND")
			: Ahead > 0.65f ? NSLOCTEXT("GolemThreat", "Ahead", "AHEAD")
			: Side > 0.0f ? NSLOCTEXT("GolemThreat", "Right", "RIGHT") : NSLOCTEXT("GolemThreat", "Left", "LEFT");
		const double Now = GetWorld()->GetGameState() != nullptr
			? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		Hud.Threats.RemoveAll([Now](const FGolemCampaignThreatView& View)
		{
			return View.EndTime <= Now;
		});
		if (Hud.Threats.Num() >= 3)
		{
			Hud.Threats.RemoveAt(0);
		}
		FGolemCampaignThreatView& View = Hud.Threats.AddDefaulted_GetRef();
		View.Text = FText::Format(NSLOCTEXT("GolemThreat", "Windup", "Threat {0}: hammer windup — prepare defense"), Heading);
		View.EndTime = Now + 1.1;
	});
	ThreatBindings.Add(Binding);
}

void AGolemCampaignPlayerController::RemoveThreatSources()
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

void AGolemCampaignPlayerController::UpdateInteractionHint()
{
	Hud.Interaction = Presentation.ActiveObjective;
	if (CampaignWorld == nullptr || GetControlledGolem() == nullptr)
	{
		return;
	}
	const FGolemStoryInteraction* Nearest = nullptr;
	float Distance = 1200.0f;
	for (const TWeakObjectPtr<AGolemStoryAnchor>& Anchor : CampaignAnchors)
	{
		if (!Anchor.IsValid() || Anchor->IsActorBeingDestroyed())
		{
			continue;
		}
		const FGolemStoryInteraction* Interaction = CampaignWorld->FindInteraction(Anchor->AnchorId);
		const float Candidate = FVector::Distance(GetControlledGolem()->GetActorLocation(), Anchor->GetActorLocation());
		if (Interaction != nullptr && Candidate < Distance)
		{
			Nearest = Interaction;
			Distance = Candidate;
		}
	}
	if (Nearest != nullptr)
	{
		UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
		const FText Key = Settings != nullptr ? Settings->GetActionKey(TEXT("Interact")).GetDisplayName() : FText();
		const FText Range = Distance <= Nearest->InteractionRange
			? FText::Format(NSLOCTEXT("GolemCampaignHUD", "InteractReady", "[{0} / Y] Interact"), Key)
			: NSLOCTEXT("GolemCampaignHUD", "Approach", "Move closer to interact");
		Hud.Interaction = FText::Format(NSLOCTEXT("GolemCampaignHUD", "NearbyInteraction", "{0}\n{1} | {2} cm | {3}"),
			Presentation.ActiveObjective, Nearest->Label, FText::AsNumber(FMath::RoundToInt(Distance)), Range);
	}
}

void AGolemCampaignPlayerController::PlayConfirmedImpactTone()
{
	// Prototype cue uses the real Effects bus; production foley remains a separate content gate.
	FeedbackTone = NewObject<USoundWaveProcedural>(this);
	FeedbackTone->SetSampleRate(24000);
	FeedbackTone->NumChannels = 1;
	FeedbackTone->Duration = 0.08f;
	FeedbackTone->SoundClassObject = LoadObject<USoundClass>(nullptr,
		TEXT("/Game/Golem/Audio/SC_Golem_Effects.SC_Golem_Effects"));
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
