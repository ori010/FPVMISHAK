#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "FPVCameraComponent.generated.h"

/**
 * The drone's FPV camera. It is rigidly attached to the drone's camera mount, so it follows every
 * pitch/roll/yaw of the drone like a real FPV camera.
 *
 * Use "Field Of View" (Camera Settings) to change the FOV.
 *
 * Extension point: future FPV features (camera uptilt, lens distortion, analog noise, signal
 * interference, motion effects) belong in this component (or post-process settings driven by it).
 */
UCLASS(ClassGroup = (FPV), meta = (BlueprintSpawnableComponent))
class FPVGAME_API UFPVCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UFPVCameraComponent();

	virtual void OnRegister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Camera uptilt in degrees (real freestyle quads: 20-40, racing: 30-50). Higher = fly faster while looking level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Camera", meta = (ClampMin = "-10", ClampMax = "70"))
	float CameraUptiltDeg = 25.f;

	UFUNCTION(BlueprintCallable, Category = "FPV Camera")
	void SetCameraUptilt(float NewUptiltDeg);

	/**
	 * Detaches the camera from the drone and places it at a safe observer position looking at
	 * FocusPoint (used after the drone explodes so the player can see the explosion).
	 */
	UFUNCTION(BlueprintCallable, Category = "FPV Camera")
	void ShowImpactView(const FVector& FocusPoint, const FVector& ApproachDirection, AActor* IgnoredActor = nullptr);

	UFUNCTION(BlueprintPure, Category = "FPV Camera")
	bool IsShowingImpactView() const { return bShowingImpactView; }

	/** Horizontal distance (cm) of the observer camera from the impact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Camera|Impact View", meta = (ClampMin = "0"))
	float ImpactViewDistance = 1400.f;

	/** Height (cm) of the observer camera above the impact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Camera|Impact View", meta = (ClampMin = "0"))
	float ImpactViewHeight = 500.f;

private:
	bool bShowingImpactView = false;
};
