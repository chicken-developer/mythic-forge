#pragma once

#include "CoreMinimal.h"
#include "Events/KnightConfirmedEvent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/KnightResult.h"
#include "KnightEventSubsystem.generated.h"

class UKnightEventSubsystem;

struct KNIGHTCORE_API FKnightEventSubscription
{
	TWeakObjectPtr<UKnightEventSubsystem> Bus;
	FName EventType;
	FDelegateHandle Handle;
	bool IsValid() const;
};

class FKnightEventChannelBase
{
public:
	virtual ~FKnightEventChannelBase() = default;
	virtual void Remove(FDelegateHandle Handle) = 0;
	virtual void RemoveOwner(const UObject* Owner) = 0;
	virtual int32 GetCount() const = 0;
	virtual bool Contains(FDelegateHandle Handle) const = 0;
};

template <typename TEvent>
class TKnightEventChannel final : public FKnightEventChannelBase
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
class KNIGHTCORE_API UKnightEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	template <typename TEvent>
	FKnightEventSubscription Subscribe(UObject* Owner, TFunction<void(const TEvent&)>&& Handler)
	{
		static_assert(TIsDerivedFrom<TEvent, FKnightConfirmedEvent>::Value,
			"Domain events must derive from FKnightConfirmedEvent.");
		check(IsInGameThread());
		if (!bIsReady || !IsValid(Owner) || !Handler)
		{
			return FKnightEventSubscription();
		}
		const FName Type(*TEvent::StaticStruct()->GetPathName());
		TSharedPtr<FKnightEventChannelBase>& BaseChannel = Channels.FindOrAdd(Type);
		if (!BaseChannel.IsValid())
		{
			BaseChannel = MakeShared<TKnightEventChannel<TEvent>>();
		}
		TSharedPtr<TKnightEventChannel<TEvent>> Channel =
			StaticCastSharedPtr<TKnightEventChannel<TEvent>>(BaseChannel);
		FKnightEventSubscription Subscription;
		Subscription.Bus = this;
		Subscription.EventType = Type;
		Subscription.Handle = Channel->Add(Owner, MoveTemp(Handler));
		return Subscription;
	}

	template <typename TEvent>
	FKnightResult PublishConfirmed(const TEvent& Event)
	{
		static_assert(TIsDerivedFrom<TEvent, FKnightConfirmedEvent>::Value,
			"Domain events must derive from FKnightConfirmedEvent.");
		check(IsInGameThread());
		const FName Type(*TEvent::StaticStruct()->GetPathName());
		const FKnightResult Result = ValidateConfirmation(Type, Event);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		// Record before callbacks so reentrant publication cannot deliver the same confirmation twice.
		Confirmations.FindOrAdd(Type).Add(Event.ConfirmationId);
		if (const TSharedPtr<FKnightEventChannelBase>* BaseChannel = Channels.Find(Type))
		{
			const TSharedPtr<TKnightEventChannel<TEvent>> Channel =
				StaticCastSharedPtr<TKnightEventChannel<TEvent>>(*BaseChannel);
			Channel->Broadcast(Event);
		}
		return FKnightResult::Success();
	}

	void Unsubscribe(const FKnightEventSubscription& Subscription);
	void UnsubscribeOwner(const UObject* Owner);
	int32 GetSubscriptionCount() const;
	bool IsReady() const;
	bool HasSubscription(const FKnightEventSubscription& Subscription) const;

private:
	FKnightResult ValidateConfirmation(FName EventType, const FKnightConfirmedEvent& Event) const;

	bool bIsReady = false;
	TMap<FName, TSharedPtr<FKnightEventChannelBase>> Channels;
	TMap<FName, TSet<FGuid>> Confirmations;
};

/** RAII wrapper for a single registration; only its own delegate handle is removed. */
class KNIGHTCORE_API FKnightScopedEventSubscription : public FNoncopyable
{
public:
	FKnightScopedEventSubscription() = default;
	explicit FKnightScopedEventSubscription(const FKnightEventSubscription& InSubscription);
	~FKnightScopedEventSubscription();
	void Set(const FKnightEventSubscription& InSubscription);
	void Reset();

private:
	FKnightEventSubscription Subscription;
};
