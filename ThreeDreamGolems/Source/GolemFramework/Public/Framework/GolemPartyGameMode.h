#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemPartyGameMode.generated.h"

class AAIController;
class AGolemCharacter;
class AGolemPartyState;

/** Composition chooses concrete Golem/AI classes; this layer owns authoritative roster and possession transactions. */
UCLASS(Config = Game)
class GOLEMFRAMEWORK_API AGolemPartyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGolemPartyGameMode();
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	AGolemPartyState* GetPartyState() const;
	const FGolemResult& GetInitializationResult() const;
	FVector GetSpawnLocationForSlot(int32 Slot) const;
	FGolemResult TrySwitchGolem(APlayerController* Player, int32 DesiredSlot);
	virtual FGolemResult TryRestartDrill(APlayerController* Player);
	virtual FGolemResult TryInteract(APlayerController* Player);
	FGolemResult TryRecoverGolemFromFall(AGolemCharacter* Golem);

protected:
	/** Composition may restore an existing canonical identity from a validated in-memory fixture snapshot. */
	virtual FGolemEntityId CreateGolemIdentity(int32 Slot, const FPrimaryAssetId& HeroId) const;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FGolemResult EnsureCanonicalParty();
	FGolemResult AssignHumanSlot(APlayerController* Player, int32 DesiredSlot);

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Framework|Composition")
	TSubclassOf<AGolemCharacter> GolemCharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Framework|Composition")
	TSubclassOf<AAIController> AIControllerClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Golem|Framework|Composition")
	FPrimaryAssetId PartyDefinitionId;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Framework|Spawn", meta = (Units = "cm"))
	FVector SpawnOrigin = FVector(-1100.0, 0.0, 150.0);

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Framework|Spawn", meta = (ClampMin = "100", Units = "cm"))
	float SlotSpacing = 260.0f;

private:
	AAIController* SpawnCompanionController(AGolemCharacter* Golem);
	void RejectHumanAssignment(APlayerController* Player, const FGolemResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<AGolemPartyState> PartyState;

	UPROPERTY(Transient)
	FGolemResult InitializationResult;

	TMap<TWeakObjectPtr<APlayerController>, int32> DesiredLoginSlots;
};
