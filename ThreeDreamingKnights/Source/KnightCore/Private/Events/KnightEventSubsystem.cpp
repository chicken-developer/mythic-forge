#include "Events/KnightEventSubsystem.h"

#include "Logging/KnightLog.h"
#include "Engine/World.h"
#include "Subsystems/SubsystemCollection.h"

#define LOCTEXT_NAMESPACE "KnightEventSubsystem"

bool FKnightEventSubscription::IsValid() const
{
	UKnightEventSubsystem* EventBus = Bus.Get();
	return EventBus != nullptr && EventBus->HasSubscription(*this);
}

void UKnightEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Channels.Reset();
	Confirmations.Reset();
	bIsReady = true;
	UE_LOG(LogKnightEvents, Display, TEXT("Confirmed event bus initialized: %s"), *GetWorld()->GetName());
}

void UKnightEventSubsystem::Deinitialize()
{
	bIsReady = false;
	Channels.Reset();
	Confirmations.Reset();
	UE_LOG(LogKnightEvents, Display, TEXT("Confirmed event bus deinitialized"));
	Super::Deinitialize();
}

void UKnightEventSubsystem::Unsubscribe(const FKnightEventSubscription& Subscription)
{
	check(IsInGameThread());
	if (Subscription.Bus.Get() != this)
	{
		return;
	}
	if (TSharedPtr<FKnightEventChannelBase>* Channel = Channels.Find(Subscription.EventType))
	{
		(*Channel)->Remove(Subscription.Handle);
	}
}

void UKnightEventSubsystem::UnsubscribeOwner(const UObject* Owner)
{
	check(IsInGameThread());
	if (Owner == nullptr)
	{
		return;
	}
	for (const TPair<FName, TSharedPtr<FKnightEventChannelBase>>& Entry : Channels)
	{
		Entry.Value->RemoveOwner(Owner);
	}
}

int32 UKnightEventSubsystem::GetSubscriptionCount() const
{
	check(IsInGameThread());
	int32 Count = 0;
	for (const TPair<FName, TSharedPtr<FKnightEventChannelBase>>& Entry : Channels)
	{
		Count += Entry.Value->GetCount();
	}
	return Count;
}

bool UKnightEventSubsystem::IsReady() const
{
	return bIsReady;
}

bool UKnightEventSubsystem::HasSubscription(const FKnightEventSubscription& Subscription) const
{
	check(IsInGameThread());
	if (!bIsReady || !Subscription.Handle.IsValid() || Subscription.Bus.Get() != this)
	{
		return false;
	}
	const TSharedPtr<FKnightEventChannelBase>* Channel = Channels.Find(Subscription.EventType);
	return Channel != nullptr && (*Channel)->Contains(Subscription.Handle);
}

FKnightResult UKnightEventSubsystem::ValidateConfirmation(FName EventType, const FKnightConfirmedEvent& Event) const
{
	if (!bIsReady)
	{
		return FKnightResult::Failure(EKnightError::ShuttingDown, EventType,
			LOCTEXT("BusUnavailable", "The world's event bus is shutting down or not initialized."));
	}
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, EventType,
			LOCTEXT("ClientConfirmation", "Only an authoritative world may publish confirmed domain events."));
	}
	if (!Event.ConfirmationId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, EventType,
			LOCTEXT("InvalidConfirmationId", "A confirmed event requires a valid transaction confirmation ID."));
	}
	if (Event.Revision <= 0)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, EventType,
			LOCTEXT("InvalidConfirmationRevision", "A confirmed event requires a positive committed domain revision."));
	}
	const TSet<FGuid>* Delivered = Confirmations.Find(EventType);
	if (Delivered != nullptr && Delivered->Contains(Event.ConfirmationId))
	{
		return FKnightResult::Failure(EKnightError::DuplicateConfirmation, EventType,
			LOCTEXT("RepeatedConfirmation", "This transaction confirmation has already been delivered."));
	}
	return FKnightResult::Success();
}

FKnightScopedEventSubscription::FKnightScopedEventSubscription(const FKnightEventSubscription& InSubscription)
	: Subscription(InSubscription)
{
}

FKnightScopedEventSubscription::~FKnightScopedEventSubscription()
{
	Reset();
}

void FKnightScopedEventSubscription::Set(const FKnightEventSubscription& InSubscription)
{
	Reset();
	Subscription = InSubscription;
}

void FKnightScopedEventSubscription::Reset()
{
	if (UKnightEventSubsystem* Bus = Subscription.Bus.Get())
	{
		Bus->Unsubscribe(Subscription);
	}
	Subscription = FKnightEventSubscription();
}

#undef LOCTEXT_NAMESPACE
