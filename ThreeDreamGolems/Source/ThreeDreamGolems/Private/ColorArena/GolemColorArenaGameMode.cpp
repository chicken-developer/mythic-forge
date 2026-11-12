#include "ColorArena/GolemColorArenaGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "ColorArena/GolemColorArenaHUD.h"
#include "Engine/World.h"
#include "Xenon/GolemXenon.h"
#include "Xenon/GolemXenonDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemColorArena, Log, All);

AGolemColorArenaGameMode::AGolemColorArenaGameMode()
{
	HUDClass = AGolemColorArenaHUD::StaticClass();
	FGolemColorArenaSpawn& Grunt = XenonSpawns.AddDefaulted_GetRef();
	Grunt.XenonId = FPrimaryAssetId(TEXT("GolemXenon"), TEXT("Grunt"));
	Grunt.Location = FVector(-650.0, 500.0, 0.0);
	Grunt.Seed = 101;
	FGolemColorArenaSpawn& Shield = XenonSpawns.AddDefaulted_GetRef();
	Shield.XenonId = FPrimaryAssetId(TEXT("GolemXenon"), TEXT("Shield"));
	Shield.Location = FVector(650.0, 500.0, 0.0);
	Shield.Seed = 202;
}

void AGolemColorArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	const FGolemResult Result = RespawnXenons();
	if (!Result.IsSuccess())
	{
		UE_LOG(LogGolemColorArena, Error, TEXT("Colour arena could not place its Xenons [%s]."), *Result.Context.ToString());
	}
}

void AGolemColorArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Xenons.Reset();
	Super::EndPlay(EndPlayReason);
}

FGolemResult AGolemColorArenaGameMode::RespawnXenons()
{
	if (!HasAuthority() || GetWorld() == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("ColorArenaSpawn"), FText::GetEmpty());
	}
	for (AGolemXenon* Xenon : Xenons)
	{
		if (IsValid(Xenon))
		{
			Xenon->Destroy();
		}
	}
	Xenons.Reset();
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (Manager == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ColorArenaAssetManager"), FText::GetEmpty());
	}
	for (const FGolemColorArenaSpawn& Spawn : XenonSpawns)
	{
		UGolemDefinition* Resolved = nullptr;
		const FGolemResult Resolve = Manager->ResolveDefinition(Spawn.XenonId, UGolemXenonDefinition::StaticClass(), Resolved);
		const UGolemXenonDefinition* Definition = Cast<UGolemXenonDefinition>(Resolved);
		if (!Resolve.IsSuccess() || Definition == nullptr)
		{
			return Resolve.IsSuccess() ? FGolemResult::Failure(EGolemError::NotFound, TEXT("ColorArenaXenon"), FText::GetEmpty()) : Resolve;
		}
		// Stand the scaled capsule on the floor: the default half height is 88 cm.
		const FVector Location = Spawn.Location + FVector(0.0, 0.0, 90.0 * Definition->GetBodyScale());
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGolemXenon* Xenon = GetWorld()->SpawnActor<AGolemXenon>(AGolemXenon::StaticClass(),
			FTransform(FRotator(0.0, -90.0, 0.0), Location), Parameters);
		if (Xenon == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ColorArenaSpawnActor"), FText::GetEmpty());
		}
		const FGolemResult Initialize = Xenon->InitializeXenon(Definition, Spawn.Seed);
		if (!Initialize.IsSuccess())
		{
			Xenon->Destroy();
			return Initialize;
		}
		Xenons.Add(Xenon);
	}
	UE_LOG(LogGolemColorArena, Display, TEXT("Colour arena ready: %d Xenons placed."), Xenons.Num());
	return FGolemResult::Success();
}

FGolemResult AGolemColorArenaGameMode::TryInteract(APlayerController* Player)
{
	return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ColorArenaInteract"),
		NSLOCTEXT("GolemColorArena", "NoStart", "The Xenons are already here. Use heavy attacks on the glowing voids."));
}

FGolemResult AGolemColorArenaGameMode::TryRestartDrill(APlayerController* Player)
{
	const FGolemResult Result = Super::TryRestartDrill(Player);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return RespawnXenons();
}
