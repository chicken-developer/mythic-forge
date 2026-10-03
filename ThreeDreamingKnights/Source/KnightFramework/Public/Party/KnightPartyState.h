#pragma once

#include "CoreMinimal.h"
#include "Events/KnightConfirmedEvent.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightPartyState.generated.h"

class AController;
class AKnightCharacter;
class AKnightPartyState;
class AKnightPlayerState;

UENUM(BlueprintType)
enum class EKnightControllerOwner : uint8
{
	Unassigned,
	Human,
	Companion
};

USTRUCT(BlueprintType)
struct KNIGHTFRAMEWORK_API FKnightRosterEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	int32 Slot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	FKnightEntityId KnightId;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	FPrimaryAssetId HeroDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	TObjectPtr<AKnightCharacter> Knight;

	/** Controller references can be unresolved on non-owning clients; ownership below is the public snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	TObjectPtr<AController> Controller;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	EKnightControllerOwner OwnerKind = EKnightControllerOwner::Unassigned;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	TObjectPtr<AKnightPlayerState> HumanPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	FGuid HumanId;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Framework|Party")
	int32 HumanPlayerId = INDEX_NONE;
};

USTRUCT()
struct KNIGHTFRAMEWORK_API FKnightRosterArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FKnightRosterEntry> Entries;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<AKnightPartyState> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <>
struct TStructOpsTypeTraits<FKnightRosterArray> : public TStructOpsTypeTraitsBase2<FKnightRosterArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

USTRUCT()
struct KNIGHTFRAMEWORK_API FKnightPartyRosterChangedEvent : public FKnightConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> ChangedSlots;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightRosterChanged);

/** Server-owned canonical aggregate, independent of the number of humans connected to the host campaign. */
UCLASS()
class KNIGHTFRAMEWORK_API AKnightPartyState : public AActor
{
	GENERATED_BODY()

public:
	AKnightPartyState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const TArray<FKnightRosterEntry>& GetRoster() const;
	const FKnightRosterEntry* FindSlot(int32 Slot) const;
	const FKnightRosterEntry* FindKnight(const AKnightCharacter* Knight) const;
	int32 GetControlledSlot(const AController* Controller) const;
	int32 GetHumanCount() const;
	int32 GetRosterRevision() const;
	FKnightEntityId GetPartyId() const;
	bool IsCanonicalRosterReady() const;
	FKnightResult InitializeRoster(const TArray<FKnightRosterEntry>& InEntries);
	FKnightResult RefreshControllerOwnership(TConstArrayView<int32> ChangedSlots);
	void NotifyRosterChanged();
	FOnKnightRosterChanged OnRosterChanged;

protected:
	virtual void PostInitializeComponents() override;

private:
	static FKnightResult ValidateRosterEntries(const TArray<FKnightRosterEntry>& Entries);
	static FKnightResult ReadControllerOwnership(FKnightRosterEntry& Entry);
	void PublishRosterConfirmation(const TArray<int32>& ChangedSlots);

	UPROPERTY(Replicated)
	FKnightRosterArray Roster;

	UPROPERTY(Replicated)
	FKnightEntityId PartyId;

	UPROPERTY(Replicated)
	int32 RosterRevision = 0;
};
