#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Drone/FPVDroneTypes.h"
#include "FPVDroneMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFPVDroneImpactSignature, const FHitResult&, Hit, float, ImpactSpeed);

/**
 * FPV quadcopter flight model (V0.2).
 *
 * Runs at 500 Hz (sub-stepped) and models what makes a real 5" freestyle quad feel the way it does:
 *  - Flight controller: Betaflight rates (classic or Actual) turn sticks into rotation-speed setpoints.
 *    A rate controller tracks them with a short response time and a maximum angular acceleration
 *    (motor authority). Angle mode adds a self-leveling outer loop. Air mode is always on.
 *  - Motors: throttle -> Betaflight throttle curve -> motor idle -> per-motor mixing of the rotation demand
 *    -> first-order spool-up/spool-down lag -> thrust ~ output^ThrustExponent (props: thrust ~ RPM^2).
 *  - Propellers lose thrust as air flows through them faster (climbing/fast forward flight), which limits top speed.
 *  - Aerodynamics: quadratic body drag with different areas front/side/top (a tilted quad pushes its top
 *    through the air), plus linear rotor drag in the prop plane, plus gravity.
 *  - Collision: the root sphere is swept every step; hits are reported via OnImpact, then the drone bounces/slides.
 *
 * Every value is tunable in BP_FPVDrone > DroneMovement > "FPV Drone".
 */
UCLASS(ClassGroup = (FPV), meta = (BlueprintSpawnableComponent))
class FPVGAME_API UFPVDroneMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UFPVDroneMovementComponent();

	//~ UActorComponent / UMovementComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override { return MaxSpeed; }
	virtual void StopMovementImmediately() override;

	// ---------------------------------------------------------------------------------------------
	// Control API (input layer, later AI / replays / RC transmitter)
	// ---------------------------------------------------------------------------------------------

	/** Sets the throttle stick position (0..1). */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Control")
	void SetThrottle(float NewThrottle);

	/** Adds to the throttle stick position (result is clamped to 0..1). */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Control")
	void AddThrottle(float DeltaThrottle);

	/** -1..1, positive = pitch forward (nose down). */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Control")
	void SetPitchInput(float Value);

	/** -1..1, positive = roll right. */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Control")
	void SetRollInput(float Value);

	/** -1..1, positive = yaw right. */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Control")
	void SetYawInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Control")
	void SetControlInput(const FFPVDroneControlInput& NewInput);

	UFUNCTION(BlueprintPure, Category = "FPV Drone|Control")
	FFPVDroneControlInput GetControlInput() const { return ControlInput; }

	UFUNCTION(BlueprintPure, Category = "FPV Drone|Control")
	float GetThrottle() const { return ControlInput.Throttle; }

	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Flight Controller")
	void SetFlightMode(EFPVFlightMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Flight Controller")
	void ToggleFlightMode();

	UFUNCTION(BlueprintPure, Category = "FPV Drone|Flight Controller")
	EFPVFlightMode GetFlightMode() const { return FlightMode; }

	/** Enables/disables the whole simulation (used when the drone is destroyed). */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|State")
	void SetFlightEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "FPV Drone|State")
	bool IsFlightEnabled() const { return bFlightEnabled; }

	// ---------------------------------------------------------------------------------------------
	// Telemetry
	// ---------------------------------------------------------------------------------------------

	/** Body rotation speed in deg/s (X = roll, Y = pitch (+ = nose up), Z = yaw). */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	FVector GetLocalAngularVelocity() const { return LocalAngularVelocityDeg; }

	/** Rotation speed the flight controller is currently asking for, deg/s (same axes as above). */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	FVector GetTargetAngularVelocity() const { return TargetAngularVelocityDeg; }

	/** Output (0..1) of motor 0..3 (front-left, front-right, rear-left, rear-right). */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	float GetMotorOutput(int32 MotorIndex) const;

	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	float GetAverageMotorOutput() const;

	/** Total propeller thrust right now, in Newtons. */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	float GetThrustNewtons() const { return CurrentThrustNewtons; }

	/** Max static thrust divided by weight. */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	float GetThrustToWeightRatio() const;

	/** Throttle stick position (0..1) that hovers the drone when level and still. */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	float GetHoverThrottle() const;

	/** Static thrust (N) produced at a steady throttle stick position, with no rotation demand. */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Telemetry")
	float GetStaticThrustForThrottle(float Throttle) const;

	/** Fired for every blocking hit, with the speed along the hit normal (cm/s). */
	UPROPERTY(BlueprintAssignable, Category = "FPV Drone|Events")
	FFPVDroneImpactSignature OnImpact;

	// ---------------------------------------------------------------------------------------------
	// Rates (same numbers as Betaflight Configurator)
	// ---------------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Rates")
	EFPVRatesType RatesType = EFPVRatesType::Betaflight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Rates")
	FFPVAxisRates RollRates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Rates")
	FFPVAxisRates PitchRates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Rates")
	FFPVAxisRates YawRates;

	// ---------------------------------------------------------------------------------------------
	// Flight controller
	// ---------------------------------------------------------------------------------------------

	/** Flight mode the drone starts in (F / Triangle toggles in game). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Flight Controller")
	EFPVFlightMode FlightMode = EFPVFlightMode::Acro;

	/** Roll/pitch rate controller response time (s). Real tuned 5" quads: ~0.02-0.04 s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Flight Controller", meta = (ClampMin = "0.005", ClampMax = "0.5"))
	float RateResponseTime = 0.025f;

	/** Yaw rate controller response time (s). Yaw is slower on real quads (it uses motor torque). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Flight Controller", meta = (ClampMin = "0.005", ClampMax = "0.5"))
	float YawResponseTime = 0.06f;

	/** Maximum roll/pitch angular acceleration the motors can produce (deg/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Flight Controller", meta = (ClampMin = "100", UIMax = "50000"))
	float MaxRollPitchAcceleration = 15000.f;

	/** Maximum yaw angular acceleration (deg/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Flight Controller", meta = (ClampMin = "100", UIMax = "50000"))
	float MaxYawAcceleration = 4000.f;

	/** Angle mode: tilt at full stick (Betaflight "angle limit"), degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Flight Controller", meta = (ClampMin = "5", ClampMax = "85"))
	float AngleLimitDeg = 55.f;

	/** Angle mode: how hard the self-leveling pulls (deg/s of rotation per degree of error). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Flight Controller", meta = (ClampMin = "0.5", UIMax = "30"))
	float AngleModeStrength = 8.f;

	// ---------------------------------------------------------------------------------------------
	// Motors & thrust
	// ---------------------------------------------------------------------------------------------

	/** All-up weight in kg (typical 5" freestyle quad with 6S battery: 0.6 - 0.75). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0.05", UIMax = "5"))
	float DroneMassKg = 0.65f;

	/** Total static thrust of the 4 motors at full throttle, in Newtons (45 N = 4.6 kg, thrust/weight ~7). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "1", UIMax = "200"))
	float MaxThrustNewtons = 45.f;

	/** Thrust ~ motor output ^ exponent. 2 = pure "thrust ~ RPM^2"; real ESC/motor curves sit around 1.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "1", ClampMax = "2.5"))
	float ThrustExponent = 1.5f;

	/** Motor output at zero throttle (Betaflight idle). Motors keep spinning so air mode works. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0", ClampMax = "0.3"))
	float MotorIdle = 0.05f;

	/** Time constant for motors spinning up (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float MotorSpinUpTime = 0.03f;

	/** Time constant for motors slowing down (s). Props brake more slowly than they accelerate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float MotorSpinDownTime = 0.05f;

	/** How much of the motor range the mixer may use for rotation at full angular acceleration (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0", ClampMax = "1"))
	float MixerAuthority = 0.25f;

	/** Betaflight throttle curve "Throttle Mid" (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float ThrottleMid = 0.5f;

	/** Betaflight throttle curve "Throttle Expo" (0..1). Flattens the curve around Throttle Mid for precise hovering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0", ClampMax = "1"))
	float ThrottleExpo = 0.0f;

	/** Airspeed through the props (m/s) at which they stop producing thrust (pitch speed). Limits top speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "5", UIMax = "150"))
	float PropPitchSpeed = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Motors", meta = (ClampMin = "0", UIMax = "3"))
	float GravityScale = 1.f;

	// ---------------------------------------------------------------------------------------------
	// Aerodynamics
	// ---------------------------------------------------------------------------------------------

	/** Air density, kg/m^3 (sea level 1.225). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Aerodynamics", meta = (ClampMin = "0.1", ClampMax = "2"))
	float AirDensity = 1.225f;

	/** Drag area (Cd x A, m^2) when air hits the front of the drone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Aerodynamics", meta = (ClampMin = "0", UIMax = "0.2"))
	float DragAreaFront = 0.010f;

	/** Drag area (Cd x A, m^2) when air hits the side of the drone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Aerodynamics", meta = (ClampMin = "0", UIMax = "0.2"))
	float DragAreaSide = 0.012f;

	/** Drag area (Cd x A, m^2) when air hits the top/bottom (props + frame). Dominant in fast, tilted flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Aerodynamics", meta = (ClampMin = "0", UIMax = "0.2"))
	float DragAreaTop = 0.020f;

	/** Linear rotor drag in the prop plane (1/s at hover thrust). Gives the "planted" carving feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Aerodynamics", meta = (ClampMin = "0", UIMax = "3"))
	float RotorDragCoefficient = 0.15f;

	/** Hard speed limit, cm/s (6000 = 216 km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Aerodynamics", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 6000.f;

	// ---------------------------------------------------------------------------------------------
	// Collision response
	// ---------------------------------------------------------------------------------------------

	/** Bounciness when hitting a surface (0 = no bounce, 1 = perfectly elastic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Collision", meta = (ClampMin = "0", ClampMax = "1"))
	float BounceRestitution = 0.25f;

	/** Impacts slower than this (cm/s, along the surface normal) do not bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Collision", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinBounceSpeed = 150.f;

	/** How quickly sliding speed is lost while touching a surface (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Collision", meta = (ClampMin = "0", UIMax = "20"))
	float SurfaceFriction = 4.f;

	// ---------------------------------------------------------------------------------------------
	// Simulation
	// ---------------------------------------------------------------------------------------------

	/** Physics update rate in Hz (each frame is split into steps of 1/rate seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "FPV Drone|Simulation", meta = (ClampMin = "60", ClampMax = "2000"))
	float SimulationRateHz = 500.f;

	/** Maximum physics steps per frame (protects the frame rate after hitches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "FPV Drone|Simulation", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxSubsteps = 64;

protected:
	void SimulateStep(float DeltaTime);
	FVector ComputeTargetRates(const FQuat& Rotation) const;
	void UpdateMotors(const FVector& AngularAcceleration, float DeltaTime);
	void HandleBlockingHit(const FHitResult& Hit, const FVector& AttemptedDelta, float DeltaTime);

	/** Motor command (0..1) for a throttle stick position, before mixing. */
	float ComputeCollectiveOutput(float Throttle) const;

	/** Heading (world yaw in degrees) of the drone, robust to steep/inverted attitudes. */
	static float ComputeHeadingYaw(const FQuat& Rotation);

	/** Current pilot commands. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPV Drone|State")
	FFPVDroneControlInput ControlInput;

	FVector LocalAngularVelocityDeg = FVector::ZeroVector;
	FVector TargetAngularVelocityDeg = FVector::ZeroVector;
	float MotorOutputs[4] = { 0.f, 0.f, 0.f, 0.f };
	float CurrentThrustNewtons = 0.f;
	bool bFlightEnabled = true;
};
