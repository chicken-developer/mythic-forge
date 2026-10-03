#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Types/KnightResult.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightPartyGameMode.generated.h"

class AAIController;
class AKnightCharacter;
class AKnightPartyState;

/** Composition chooses concrete Knight/AI classes; this layer owns authoritative roster and possession transactions. */
UCLASS(Config = Game)
class KNIGHTFRAMEWORK_API AKnightPartyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKnightPartyGameMode();
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	AKnightPartyState* GetPartyState() const;
	const FKnightResult& GetInitializationResult() const;
	FVector GetSpawnLocationForSlot(int32 Slot) const;
	FKnightResult TrySwitchKnight(APlayerController* Player, int32 DesiredSlot);
	virtual FKnightResult TryRestartDrill(APlayerController* Player);
	virtual FKnightResult TryInteract(APlayerController* Player);
	FKnightResult TryRecoverKnightFromFall(AKnightCharacter* Knight);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FKnightResult EnsureCanonicalParty();
	FKnightResult AssignHumanSlot(APlayerController* Player, int32 DesiredSlot);

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Framework|Composition")
	TSubclassOf<AKnightCharacter> KnightCharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Framework|Composition")
	TSubclassOf<AAIController> AIControllerClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Knight|Framework|Composition")
	FPrimaryAssetId PartyDefinitionId;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Framework|Spawn", meta = (Units = "cm"))
	FVector SpawnOrigin = FVector(-1100.0, 0.0, 150.0);

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Framework|Spawn", meta = (ClampMin = "100", Units = "cm"))
	float SlotSpacing = 260.0f;

private:
	AAIController* SpawnCompanionController(AKnightCharacter* Knight);
	void RejectHumanAssignment(APlayerController* Player, const FKnightResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<AKnightPartyState> PartyState;

	UPROPERTY(Transient)
	FKnightResult InitializationResult;

	TMap<TWeakObjectPtr<APlayerController>, int32> DesiredLoginSlots;
};
