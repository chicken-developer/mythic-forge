#pragma once

#include "CoreMinimal.h"
#include "Events/GolemConfirmedEvent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GolemResult.h"
#include "GolemEventSubsystem.generated.h"

class UGolemEventSubsystem;

struct GOLEMCORE_API FGolemEventSubscription
{
	TWeakObjectPtr<UGolemEventSubsystem> Bus;
	FName EventType;
	FDelegateHandle Handle;
	bool IsValid() const;
};

class FGolemEventChannelBase
{
public:
	virtual ~FGolemEventChannelBase() = default;
	virtual void Remove(FDelegateHandle Handle) = 0;
	virtual void RemoveOwner(const UObject* Owner) = 0;
	virtual int32 GetCount() const = 0;
	virtual bool Contains(FDelegateHandle Handle) const = 0;
};

template <typename TEvent>
class TGolemEventChannel final : public FGolemEventChannelBase
{
public:
	FDelegateHandle Add(UObject* Owner, TFunction<void(const TEvent&)>&& Handler)
	{
		PruneExpiredOwners();
		const FDelegateHandle Handle = OnConfirmed.AddWeakLambda(Owner,
			[Handler = MoveTemp(Handler)](const TEvent& Event)
			{
				Handler(Event);
			});
		Owners.Add(Handle, Owner);
		return Handle;
	}

	void Broadcast(const TEvent& Event)
	{
		PruneExpiredOwners();
		OnConfirmed.Broadcast(Event);
	}

	virtual void Remove(FDelegateHandle Handle) override
	{
		OnConfirmed.Remove(Handle);
		Owners.Remove(Handle);
	}

	virtual void RemoveOwner(const UObject* Owner) override
	{
		TArray<FDelegateHandle> Handles;
		for (const TPair<FDelegateHandle, TWeakObjectPtr<UObject>>& Entry : Owners)
		{
			if (Entry.Value.Get() == Owner)
			{
				Handles.Add(Entry.Key);
			}
		}
		for (FDelegateHandle Handle : Handles)
		{
			Remove(Handle);
		}
	}

	virtual int32 GetCount() const override
	{
		int32 Count = 0;
		for (const TPair<FDelegateHandle, TWeakObjectPtr<UObject>>& Entry : Owners)
		{
			if (Entry.Value.IsValid())
			{
				++Count;
			}
		}
		return Count;
	}

	virtual bool Contains(FDelegateHandle Handle) const override
	{
		const TWeakObjectPtr<UObject>* Owner = Owners.Find(Handle);
		return Owner != nullptr && Owner->IsValid();
	}

private:
	void PruneExpiredOwners()
	{
		TArray<FDelegateHandle> Expired;
		for (const TPair<FDelegateHandle, TWeakObjectPtr<UObject>>& Entry : Owners)
		{
			if (!Entry.Value.IsValid())
			{
				Expired.Add(Entry.Key);
			}
		}
		for (FDelegateHandle Handle : Expired)
		{
			Remove(Handle);
		}
	}

	TMulticastDelegate<void(const TEvent&)> OnConfirmed;
	TMap<FDelegateHandle, TWeakObjectPtr<UObject>> Owners;
};

/** Typed, world-local confirmed facts. Authority commands and replicated client snapshots remain domain-owned. */
UCLASS()
class GOLEMCORE_API UGolemEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	template <typename TEvent>
	FGolemEventSubscription Subscribe(UObject* Owner, TFunction<void(const TEvent&)>&& Handler)
	{
		static_assert(TIsDerivedFrom<TEvent, FGolemConfirmedEvent>::Value,
			"Domain events must derive from FGolemConfirmedEvent.");
		check(IsInGameThread());
		if (!bIsReady || !IsValid(Owner) || !Handler)
		{
			return FGolemEventSubscription();
		}
		const FName Type(*TEvent::StaticStruct()->GetPathName());
		TSharedPtr<FGolemEventChannelBase>& BaseChannel = Channels.FindOrAdd(Type);
		if (!BaseChannel.IsValid())
		{
			BaseChannel = MakeShared<TGolemEventChannel<TEvent>>();
		}
		TSharedPtr<TGolemEventChannel<TEvent>> Channel =
			StaticCastSharedPtr<TGolemEventChannel<TEvent>>(BaseChannel);
		FGolemEventSubscription Subscription;
		Subscription.Bus = this;
		Subscription.EventType = Type;
		Subscription.Handle = Channel->Add(Owner, MoveTemp(Handler));
		return Subscription;
	}

	template <typename TEvent>
	FGolemResult PublishConfirmed(const TEvent& Event)
	{
		static_assert(TIsDerivedFrom<TEvent, FGolemConfirmedEvent>::Value,
			"Domain events must derive from FGolemConfirmedEvent.");
		check(IsInGameThread());
		const FName Type(*TEvent::StaticStruct()->GetPathName());
		const FGolemResult Result = ValidateConfirmation(Type, Event);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		// Record before callbacks so reentrant publication cannot deliver the same confirmation twice.
		Confirmations.FindOrAdd(Type).Add(Event.ConfirmationId);
		if (const TSharedPtr<FGolemEventChannelBase>* BaseChannel = Channels.Find(Type))
		{
			const TSharedPtr<TGolemEventChannel<TEvent>> Channel =
				StaticCastSharedPtr<TGolemEventChannel<TEvent>>(*BaseChannel);
			Channel->Broadcast(Event);
		}
		return FGolemResult::Success();
	}

	void Unsubscribe(const FGolemEventSubscription& Subscription);
	void UnsubscribeOwner(const UObject* Owner);
	int32 GetSubscriptionCount() const;
	bool IsReady() const;
	bool HasSubscription(const FGolemEventSubscription& Subscription) const;

private:
	FGolemResult ValidateConfirmation(FName EventType, const FGolemConfirmedEvent& Event) const;

	bool bIsReady = false;
	TMap<FName, TSharedPtr<FGolemEventChannelBase>> Channels;
	TMap<FName, TSet<FGuid>> Confirmations;
};

/** RAII wrapper for a single registration; only its own delegate handle is removed. */
class GOLEMCORE_API FGolemScopedEventSubscription : public FNoncopyable
{
public:
	FGolemScopedEventSubscription() = default;
	explicit FGolemScopedEventSubscription(const FGolemEventSubscription& InSubscription);
	~FGolemScopedEventSubscription();
	void Set(const FGolemEventSubscription& InSubscription);
	void Reset();

private:
	FGolemEventSubscription Subscription;
};
