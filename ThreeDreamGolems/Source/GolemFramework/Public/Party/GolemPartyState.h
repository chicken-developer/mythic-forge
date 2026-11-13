#pragma once

#include "CoreMinimal.h"
#include "Events/GolemConfirmedEvent.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemPartyState.generated.h"

class AController;
class AGolemCharacter;
class AGolemPartyState;
class AGolemPlayerState;

UENUM(BlueprintType)
enum class EGolemControllerOwner : uint8
{
	Unassigned,
	Human,
	Companion
};

USTRUCT(BlueprintType)
struct GOLEMFRAMEWORK_API FGolemRosterEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	int32 Slot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	FGolemEntityId GolemId;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	FPrimaryAssetId HeroDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	TObjectPtr<AGolemCharacter> Golem;

	/** Controller references can be unresolved on non-owning clients; ownership below is the public snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	TObjectPtr<AController> Controller;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	EGolemControllerOwner OwnerKind = EGolemControllerOwner::Unassigned;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	TObjectPtr<AGolemPlayerState> HumanPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	FGuid HumanId;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Framework|Party")
	int32 HumanPlayerId = INDEX_NONE;
};

USTRUCT()
struct GOLEMFRAMEWORK_API FGolemRosterArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGolemRosterEntry> Entries;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<AGolemPartyState> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <>
struct TStructOpsTypeTraits<FGolemRosterArray> : public TStructOpsTypeTraitsBase2<FGolemRosterArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

USTRUCT()
struct GOLEMFRAMEWORK_API FGolemPartyRosterChangedEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> ChangedSlots;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemRosterChanged);

/** Server-owned canonical aggregate, independent of the number of humans connected to the host campaign. */
UCLASS()
class GOLEMFRAMEWORK_API AGolemPartyState : public AActor
{
	GENERATED_BODY()

public:
	AGolemPartyState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const TArray<FGolemRosterEntry>& GetRoster() const;
	const FGolemRosterEntry* FindSlot(int32 Slot) const;
	const FGolemRosterEntry* FindGolem(const AGolemCharacter* Golem) const;
	int32 GetControlledSlot(const AController* Controller) const;
	int32 GetHumanCount() const;
	int32 GetRosterRevision() const;
	FGolemEntityId GetPartyId() const;
	bool IsCanonicalRosterReady() const;
	FGolemResult InitializeRoster(const TArray<FGolemRosterEntry>& InEntries);
	FGolemResult RefreshControllerOwnership(TConstArrayView<int32> ChangedSlots);
	void NotifyRosterChanged();
	FOnGolemRosterChanged OnRosterChanged;

protected:
	virtual void PostInitializeComponents() override;

private:
	static FGolemResult ValidateRosterEntries(const TArray<FGolemRosterEntry>& Entries);
	static FGolemResult ReadControllerOwnership(FGolemRosterEntry& Entry);
	void PublishRosterConfirmation(const TArray<int32>& ChangedSlots);

	UPROPERTY(Replicated)
	FGolemRosterArray Roster;

	UPROPERTY(Replicated)
	FGolemEntityId PartyId;

	UPROPERTY(Replicated)
	int32 RosterRevision = 0;
};
