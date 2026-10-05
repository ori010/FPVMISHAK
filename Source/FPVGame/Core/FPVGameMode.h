#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPVGameMode.generated.h"

class UFPVMissionManagerComponent;
class AFPVDronePawn;

/**
 * Game rules for the prototype: spawns the player's drone at the PlayerStart, starts the mission
 * when play begins and restarts it on request (drone respawned, targets reset, mission Active again).
 *
 * BP_FPVGameMode (Content/Blueprints) is the configured child that selects BP_FPVDrone and
 * BP_FPVPlayerController.
 */
UCLASS()
class FPVGAME_API AFPVGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFPVGameMode();

	virtual void StartPlay() override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

	/** Resets targets and mission state and respawns every player's drone at its start point. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void RestartMission();

	UFUNCTION(BlueprintPure, Category = "Mission")
	UFPVMissionManagerComponent* GetMissionManager() const;

	/** If true the mission becomes Active as soon as the level starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bAutoStartMission = true;

protected:
	UFUNCTION()
	void HandleDroneCrashed(AFPVDronePawn* Drone, const FText& Reason);
};
