#pragma once

#include "CoreMinimal.h"
#include "FPVDroneTypes.generated.h"

/** Flight controller mode, matching the two most common modes on real FPV flight controllers. */
UENUM(BlueprintType)
enum class EFPVFlightMode : uint8
{
	/** Rate mode: sticks command rotation speed. The drone keeps its attitude when the sticks are centered. */
	Acro UMETA(DisplayName = "Acro (Rate)"),

	/** Self-level mode: pitch/roll sticks command a tilt angle. The drone levels out when the sticks are centered. */
	Angle UMETA(DisplayName = "Angle (Self-Level)")
};

/** Which Betaflight rate formula turns stick deflection into rotation speed. */
UENUM(BlueprintType)
enum class EFPVRatesType : uint8
{
	/** Classic Betaflight rates: RC Rate, Super Rate, RC Expo. */
	Betaflight UMETA(DisplayName = "Betaflight (RC Rate / Super Rate / Expo)"),

	/** Betaflight "Actual" rates (default in Betaflight 4.3+): Center Sensitivity, Max Rate, Expo. */
	Actual UMETA(DisplayName = "Actual (Center Sensitivity / Max Rate / Expo)")
};

/** Rate settings for one axis, using the same numbers you would type into Betaflight Configurator. */
USTRUCT(BlueprintType)
struct FFPVAxisRates
{
	GENERATED_BODY()

	/** [Betaflight rates] RC Rate (0.01 - 2.55). Overall sensitivity: rate = 200 deg/s x RC Rate at full stick before Super Rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Betaflight Rates", meta = (ClampMin = "0.01", ClampMax = "2.55"))
	float RcRate = 1.0f;

	/** [Betaflight rates] Super Rate (0 - 0.99). Boosts the rate towards full stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Betaflight Rates", meta = (ClampMin = "0", ClampMax = "0.99"))
	float SuperRate = 0.7f;

	/** [Betaflight rates] RC Expo (0 - 1). Softens the center of the stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Betaflight Rates", meta = (ClampMin = "0", ClampMax = "1"))
	float RcExpo = 0.0f;

	/** [Actual rates] Rotation speed per unit of stick near the center, deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actual Rates", meta = (ClampMin = "10", ClampMax = "2000"))
	float CenterSensitivity = 70.f;

	/** [Actual rates] Rotation speed at full stick, deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actual Rates", meta = (ClampMin = "10", ClampMax = "2000"))
	float MaxRate = 670.f;

	/** [Actual rates] Expo (0 - 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actual Rates", meta = (ClampMin = "0", ClampMax = "1"))
	float ActualExpo = 0.0f;
};

/** How an analog gamepad drives the throttle. */
UENUM(BlueprintType)
enum class EFPVGamepadThrottleMode : uint8
{
	/** Left stick up/down is an absolute throttle like a real RC radio: bottom = 0%, center = 50%, top = 100%. */
	Stick UMETA(DisplayName = "Left Stick (absolute, RC style)"),

	/** R2 / right trigger is an absolute throttle: released = 0%, fully pressed = 100%. */
	Trigger UMETA(DisplayName = "Right Trigger R2 (absolute)")
};

/**
 * Raw pilot commands fed into the flight model. This is the boundary between the input layer
 * (keyboard, gamepad, later RC transmitter / AI) and the flight model.
 */
USTRUCT(BlueprintType)
struct FFPVDroneControlInput
{
	GENERATED_BODY()

	/** Collective thrust command, 0 = motors idle, 1 = full thrust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0", ClampMax = "1"))
	float Throttle = 0.f;

	/** -1..1. Positive = pitch forward (nose down), like pushing an RC stick forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "-1", ClampMax = "1"))
	float Pitch = 0.f;

	/** -1..1. Positive = roll right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "-1", ClampMax = "1"))
	float Roll = 0.f;

	/** -1..1. Positive = yaw right (clockwise seen from above). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "-1", ClampMax = "1"))
	float Yaw = 0.f;
};
