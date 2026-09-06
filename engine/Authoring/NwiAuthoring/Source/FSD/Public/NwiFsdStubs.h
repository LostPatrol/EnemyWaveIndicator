// Minimal authoring declarations matching existing FSD reflected names; never ship this module.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameMode.h"
#include "NwiFsdStubs.generated.h"

UCLASS(BlueprintType)
class FSD_API UEnemyDescriptor : public UObject { GENERATED_BODY() };

UCLASS(BlueprintType, Blueprintable)
class FSD_API UEnemyWaveController : public UObject { GENERATED_BODY() };

// Parameter names and types match the game's existing multicast signature.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemySpawnedSignature, APawn*, enemy, UEnemyDescriptor*, descriptor);
UCLASS(BlueprintType)
class FSD_API UEnemySpawnManager : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="Spawning") FEnemySpawnedSignature OnEnemySpawned;
};

UCLASS(BlueprintType)
class FSD_API UEnemyWaveManager : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Spawning") UEnemySpawnManager* SpawnManager = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="Spawning") TArray<UEnemyWaveController*> ActiveScriptedWaves;
};

UCLASS(BlueprintType)
class FSD_API AFSDGameMode : public AGameMode
{
    GENERATED_BODY()
public:
    // The test fixture stores its manager here; this property is never referenced by cooked graphs.
    UPROPERTY() UEnemyWaveManager* FixtureWaveManager = nullptr;
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Spawning") UEnemyWaveManager* GetWaveManager() const { return FixtureWaveManager; }
};
