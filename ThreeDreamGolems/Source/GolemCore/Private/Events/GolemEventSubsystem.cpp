#include "Events/GolemEventSubsystem.h"

#include "Logging/GolemLog.h"
#include "Engine/World.h"
#include "Subsystems/SubsystemCollection.h"

#define LOCTEXT_NAMESPACE "GolemEventSubsystem"

bool FGolemEventSubscription::IsValid() const
{
	UGolemEventSubsystem* EventBus = Bus.Get();
	return EventBus != nullptr && EventBus->HasSubscription(*this);
}

void UGolemEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Channels.Reset();
	Confirmations.Reset();
	bIsReady = true;
	UE_LOG(LogGolemEvents, Display, TEXT("Confirmed event bus initialized: %s"), *GetWorld()->GetName());
}

void UGolemEventSubsystem::Deinitialize()
{
	bIsReady = false;
	Channels.Reset();
	Confirmations.Reset();
	UE_LOG(LogGolemEvents, Display, TEXT("Confirmed event bus deinitialized"));
	Super::Deinitialize();
}

void UGolemEventSubsystem::Unsubscribe(const FGolemEventSubscription& Subscription)
{
	check(IsInGameThread());
	if (Subscription.Bus.Get() != this)
	{
		return;
	}
	if (TSharedPtr<FGolemEventChannelBase>* Channel = Channels.Find(Subscription.EventType))
	{
		(*Channel)->Remove(Subscription.Handle);
	}
}

void UGolemEventSubsystem::UnsubscribeOwner(const UObject* Owner)
{
	check(IsInGameThread());
	if (Owner == nullptr)
	{
		return;
	}
	for (const TPair<FName, TSharedPtr<FGolemEventChannelBase>>& Entry : Channels)
	{
		Entry.Value->RemoveOwner(Owner);
	}
}

int32 UGolemEventSubsystem::GetSubscriptionCount() const
{
	check(IsInGameThread());
	int32 Count = 0;
	for (const TPair<FName, TSharedPtr<FGolemEventChannelBase>>& Entry : Channels)
	{
		Count += Entry.Value->GetCount();
	}
	return Count;
}

bool UGolemEventSubsystem::IsReady() const
{
	return bIsReady;
}

bool UGolemEventSubsystem::HasSubscription(const FGolemEventSubscription& Subscription) const
{
	check(IsInGameThread());
	if (!bIsReady || !Subscription.Handle.IsValid() || Subscription.Bus.Get() != this)
	{
		return false;
	}
	const TSharedPtr<FGolemEventChannelBase>* Channel = Channels.Find(Subscription.EventType);
	return Channel != nullptr && (*Channel)->Contains(Subscription.Handle);
}

FGolemResult UGolemEventSubsystem::ValidateConfirmation(FName EventType, const FGolemConfirmedEvent& Event) const
{
	if (!bIsReady)
	{
		return FGolemResult::Failure(EGolemError::ShuttingDown, EventType,
			LOCTEXT("BusUnavailable", "The world's event bus is shutting down or not initialized."));
	}
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, EventType,
			LOCTEXT("ClientConfirmation", "Only an authoritative world may publish confirmed domain events."));
	}
	if (!Event.ConfirmationId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, EventType,
			LOCTEXT("InvalidConfirmationId", "A confirmed event requires a valid transaction confirmation ID."));
	}
	if (Event.Revision <= 0)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, EventType,
			LOCTEXT("InvalidConfirmationRevision", "A confirmed event requires a positive committed domain revision."));
	}
	const TSet<FGuid>* Delivered = Confirmations.Find(EventType);
	if (Delivered != nullptr && Delivered->Contains(Event.ConfirmationId))
	{
		return FGolemResult::Failure(EGolemError::DuplicateConfirmation, EventType,
			LOCTEXT("RepeatedConfirmation", "This transaction confirmation has already been delivered."));
	}
	return FGolemResult::Success();
}

FGolemScopedEventSubscription::FGolemScopedEventSubscription(const FGolemEventSubscription& InSubscription)
	: Subscription(InSubscription)
{
}

FGolemScopedEventSubscription::~FGolemScopedEventSubscription()
{
	Reset();
}

void FGolemScopedEventSubscription::Set(const FGolemEventSubscription& InSubscription)
{
	Reset();
	Subscription = InSubscription;
}

void FGolemScopedEventSubscription::Reset()
{
	if (UGolemEventSubsystem* Bus = Subscription.Bus.Get())
	{
		Bus->Unsubscribe(Subscription);
	}
	Subscription = FGolemEventSubscription();
}

#undef LOCTEXT_NAMESPACE
