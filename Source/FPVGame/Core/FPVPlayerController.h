#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Mission/FPVMissionTypes.h"
#include "FPVPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UFPVFlightHUDWidget;
class UFPVMissionResultWidget;
struct FInputActionValue;

/**
 * Player controller: owns input that must keep working without a drone (restart) and the UI
 * (flight readout + mission result screen).
 */
UCLASS()
class FPVGAME_API AFPVPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFPVPlayerController();

	/** Asks the game mode to restart the mission. Also available as console command "RestartMission". */
	UFUNCTION(BlueprintCallable, Exec, Category = "Mission")
	void RestartMission();

	UFUNCTION(BlueprintPure, Category = "UI")
	UFPVMissionResultWidget* GetMissionResultWidget() const { return MissionResultWidget; }

	UFUNCTION(BlueprintPure, Category = "UI")
	UFPVFlightHUDWidget* GetFlightHUDWidget() const { return FlightHUDWidget; }

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsMissionResultScreenVisible() const;

	// --- Input (assigned in BP_FPVPlayerController) ----------------------------------------------

	/** Mapping context for game-level actions (restart). Always active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GameMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 GameMappingPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RestartMissionAction;

	// --- UI ---------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UFPVFlightHUDWidget> FlightHUDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UFPVMissionResultWidget> MissionResultWidgetClass;

	/** Seconds between the target being destroyed and the MISSION COMPLETE screen (lets the explosion play). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "0"))
	float MissionCompleteScreenDelay = 1.25f;

	/** Seconds between a crash and the MISSION FAILED screen (short, for a quick retry). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "0"))
	float MissionFailedScreenDelay = 0.6f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	void HandleRestartInput(const FInputActionValue& Value);

	UFUNCTION()
	void HandleMissionStateChanged(EFPVMissionState NewState, EFPVMissionState OldState);

	void BindToMissionManager();
	void ShowMissionResultScreen();
	void HideMissionResultScreen();

	UPROPERTY(Transient)
	TObjectPtr<UFPVFlightHUDWidget> FlightHUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFPVMissionResultWidget> MissionResultWidget;

	FTimerHandle MissionResultTimer;
};
