#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Types/GolemElement.h"
#include "GolemColorProjectile.generated.h"

class UGolemColorVoidComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Everything one straight shot carries. Built by the host when the shot is fired. */
USTRUCT()
struct GOLEMCOMBAT_API FGolemProjectileShot
{
	GENERATED_BODY()

	/** Centimetres per second. */
	float Speed = 3000.0f;

	/** Centimetres flown before the shot fades out. */
	float Range = 2500.0f;

	float Radius = 18.0f;

	/** Damage on a body hit, before the target's own reduction (a Xenon's grey body takes 10%). */
	float Damage = 0.0f;

	/** Ability the outcome is reported under. */
	FGameplayTag ActionTag;

	/** True when the shot places Element on a void it touches. */
	bool bAppliesColor = false;

	EGolemElement Element = EGolemElement::Earth;

	float ColorHoldSeconds = 0.0f;

	/** AI shots may only colour the void they were sent to; every other void counts as grey body. */
	bool bRestrictColor = false;

	TWeakObjectPtr<UGolemColorVoidComponent> OnlyVoid;

	/** AI follow-up shots must not open a void: they only colour OnlyVoid while it is still Primed. */
	bool bOnlyIfPrimed = false;
};

/**
 * The heavy attack of a bow or a staff: a straight shot that places colour only when it touches
 * the void itself (DEV_SPEC section 4). The host moves it and decides every hit; clients see the
 * replicated movement.
 */
UCLASS()
class GOLEMCOMBAT_API AGolemColorProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGolemColorProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority only, once, right after spawning. Direction does not need to be normalised. */
	bool Launch(AActor* InSource, const FVector& Direction, const FGolemProjectileShot& InShot);

	/** Authority only. Flies for DeltaSeconds and resolves the first thing touched. */
	void AdvanceBy(float DeltaSeconds);

	/**
	 * The component a shot of this radius from Start to End would stop at, or nullptr when nothing
	 * is in the way. Teammates never stop a shot. Companions use it to check their line of fire.
	 */
	static UPrimitiveComponent* FindFirstStop(const UWorld* World, const AActor* InSource, const FVector& Start,
		const FVector& End, float Radius);

	float GetDistanceFlown() const { return DistanceFlown; }
	bool HasFinished() const { return bFinished; }

protected:
	virtual void BeginPlay() override;

private:
	/** True when the hit ends the flight. */
	bool ResolveHit(const FHitResult& Hit);
	static bool StopsShot(const AActor* InSource, const FHitResult& Hit);
	static void SweepPath(const UWorld* World, const AActor* InSource, const AActor* Ignored, const FVector& Start,
		const FVector& End, float Radius, TArray<FHitResult>& OutHits);
	void Finish();
	void RefreshVisual();

	UFUNCTION()
	void OnRep_ColorMask();

	UPROPERTY(VisibleAnywhere, Category = "Golem|Combat|Projectile")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VisualMaterial;

	/** GolemElements mask of the colour carried, 0 for a plain shot. Only drives the look. */
	UPROPERTY(ReplicatedUsing = OnRep_ColorMask)
	uint8 ColorMask = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ColorMask)
	float VisualRadius = 18.0f;

	TWeakObjectPtr<AActor> Source;
	FGolemProjectileShot Shot;
	FVector FlightDirection = FVector::ForwardVector;
	float DistanceFlown = 0.0f;
	bool bLaunched = false;
	bool bFinished = false;
};
