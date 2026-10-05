#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "FPVGameState.generated.h"

class UFPVMissionManagerComponent;

/**
 * Game state: holds match-wide state that every system (and later every client) can read.
 * Owns the mission manager.
 */
UCLASS()
class FPVGAME_API AFPVGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AFPVGameState();

	UFUNCTION(BlueprintPure, Category = "Mission")
	UFPVMissionManagerComponent* GetMissionManager() const { return MissionManager; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	TObjectPtr<UFPVMissionManagerComponent> MissionManager;
};
