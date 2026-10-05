#include "Drone/FPVDroneMovementComponent.h"
#include "Drone/FPVFlightMath.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

namespace FPVFlight
{
	// Quad-X motor layout: 0 front-left, 1 front-right, 2 rear-left, 3 rear-right.
	// Mixer signs for a positive roll (right side down), pitch (nose up) and yaw (turn right) demand.
	constexpr float RollMix[4] = { 1.f, -1.f, 1.f, -1.f };
	constexpr float PitchMix[4] = { 1.f, 1.f, -1.f, -1.f };
	constexpr float YawMix[4] = { 1.f, -1.f, -1.f, 1.f };

	static float GravityMagnitude(const UFPVDroneMovementComponent& Movement)
	{
		// UMovementComponent::GetGravityZ needs a world (not available on class defaults).
		return (Movement.GetWorld() ? FMath::Abs(Movement.GetGravityZ()) : 980.f) * Movement.GravityScale;
	}
}

UFPVDroneMovementComponent::UFPVDroneMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bUpdateOnlyIfRendered = false;
	bConstrainToPlane = false;

	// Betaflight default rates (RC Rate 1.00, Super Rate 0.70, Expo 0 -> 667 deg/s at full stick).
	RollRates = FFPVAxisRates();
	PitchRates = FFPVAxisRates();
	YawRates = FFPVAxisRates();
}

void UFPVDroneMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	const float Idle = ComputeCollectiveOutput(0.f);
	for (float& Output : MotorOutputs)
	{
		Output = Idle;
	}
}

void UFPVDroneMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bFlightEnabled || !UpdatedComponent || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	// Clamp very long frames (hitches, breakpoints) and run the physics in fixed small steps.
	const float StepTime = 1.f / FMath::Max(SimulationRateHz, 1.f);
	float RemainingTime = FMath::Min(DeltaTime, 0.1f);
	int32 Iterations = 0;
	while (RemainingTime > UE_KINDA_SMALL_NUMBER && Iterations < MaxSubsteps && bFlightEnabled && UpdatedComponent)
	{
		const float Step = FMath::Min(RemainingTime, StepTime);
		SimulateStep(Step);
		RemainingTime -= Step;
		++Iterations;
	}

	UpdateComponentVelocity();

	// Below the level's KillZ -> AActor::FellOutOfWorld (the drone treats it as a crash).
	if (bFlightEnabled && GetOwner())
	{
		GetOwner()->CheckStillInWorld();
	}
}

FVector UFPVDroneMovementComponent::ComputeTargetRates(const FQuat& Rotation) const
{
	// Unreal rotator convention: X = roll (+ right side down), Y = pitch (+ nose up), Z = yaw (+ turn right).
	const float YawRate = UFPVFlightMath::ComputeRateDegPerSec(ControlInput.Yaw, YawRates, RatesType);

	if (FlightMode == EFPVFlightMode::Acro)
	{
		return FVector(
			UFPVFlightMath::ComputeRateDegPerSec(ControlInput.Roll, RollRates, RatesType),
			-UFPVFlightMath::ComputeRateDegPerSec(ControlInput.Pitch, PitchRates, RatesType),
			YawRate);
	}

	// Angle mode: sticks command a tilt; a P loop turns the attitude error into rotation speeds.
	const float TargetPitch = -FMath::Clamp(ControlInput.Pitch, -1.f, 1.f) * AngleLimitDeg;
	const float TargetRoll = FMath::Clamp(ControlInput.Roll, -1.f, 1.f) * AngleLimitDeg;
	const FQuat TargetRotation = FRotator(TargetPitch, ComputeHeadingYaw(Rotation), TargetRoll).Quaternion();
	const FRotator Error = (Rotation.Inverse() * TargetRotation).Rotator();

	const float RollLimit = UFPVFlightMath::ComputeMaxRateDegPerSec(RollRates, RatesType);
	const float PitchLimit = UFPVFlightMath::ComputeMaxRateDegPerSec(PitchRates, RatesType);
	return FVector(
		FMath::Clamp(Error.Roll * AngleModeStrength, -RollLimit, RollLimit),
		FMath::Clamp(Error.Pitch * AngleModeStrength, -PitchLimit, PitchLimit),
		YawRate);
}

void UFPVDroneMovementComponent::SimulateStep(float DeltaTime)
{
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();

	// 1) Flight controller: track the rate setpoints with limited angular acceleration.
	TargetAngularVelocityDeg = ComputeTargetRates(CurrentRotation);
	const FVector ResponseTimes(RateResponseTime, RateResponseTime, YawResponseTime);
	const FVector MaxAccel(MaxRollPitchAcceleration, MaxRollPitchAcceleration, MaxYawAcceleration);
	FVector AngularAcceleration;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		AngularAcceleration[Axis] = FMath::Clamp((TargetAngularVelocityDeg[Axis] - LocalAngularVelocityDeg[Axis]) / ResponseTimes[Axis], -MaxAccel[Axis], MaxAccel[Axis]);
		LocalAngularVelocityDeg[Axis] += AngularAcceleration[Axis] * DeltaTime;
	}

	const FRotator LocalDelta(LocalAngularVelocityDeg.Y * DeltaTime, LocalAngularVelocityDeg.Z * DeltaTime, LocalAngularVelocityDeg.X * DeltaTime);
	FQuat NewRotation = CurrentRotation * LocalDelta.Quaternion();
	NewRotation.Normalize();

	// 2) Motors: collective throttle + rotation demand, with spool-up/down lag.
	UpdateMotors(FVector(AngularAcceleration.X / MaxAccel.X, AngularAcceleration.Y / MaxAccel.Y, AngularAcceleration.Z / MaxAccel.Z), DeltaTime);

	// 3) Thrust, reduced by the air already flowing through the props.
	const FVector Up = NewRotation.GetUpVector();
	const FVector VelocityMS = Velocity * 0.01f;
	const float InflowSpeed = FVector::DotProduct(VelocityMS, Up);
	const float InflowFactor = FMath::Clamp(1.f - InflowSpeed / PropPitchSpeed, 0.f, 1.15f);
	float StaticThrust = 0.f;
	for (const float Output : MotorOutputs)
	{
		StaticThrust += 0.25f * MaxThrustNewtons * FMath::Pow(Output, ThrustExponent);
	}
	CurrentThrustNewtons = StaticThrust * InflowFactor;

	// 4) Aerodynamic drag in the drone's own frame (m/s^2).
	const float Mass = FMath::Max(DroneMassKg, UE_KINDA_SMALL_NUMBER);
	const FVector LocalVelocity = NewRotation.UnrotateVector(VelocityMS);
	const float Speed = LocalVelocity.Size();
	const FVector DragAreas(DragAreaFront, DragAreaSide, DragAreaTop);
	FVector LocalAcceleration = -0.5f * AirDensity * Speed * (DragAreas * LocalVelocity) / Mass;

	const float Weight = Mass * FPVFlight::GravityMagnitude(*this) * 0.01f;
	const float ThrustRatio = Weight > UE_KINDA_SMALL_NUMBER ? CurrentThrustNewtons / Weight : 0.f;
	LocalAcceleration -= RotorDragCoefficient * ThrustRatio * FVector(LocalVelocity.X, LocalVelocity.Y, 0.f);

	// 5) Sum forces (cm/s^2) and integrate.
	FVector Acceleration = NewRotation.RotateVector(LocalAcceleration) * 100.f;
	Acceleration += Up * (CurrentThrustNewtons / Mass * 100.f);
	Acceleration.Z -= FPVFlight::GravityMagnitude(*this);

	Velocity += Acceleration * DeltaTime;
	Velocity = Velocity.GetClampedToMaxSize(MaxSpeed);

	// 6) Move with a collision sweep.
	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, NewRotation, true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		HandleBlockingHit(Hit, Delta, DeltaTime);
	}
}

float UFPVDroneMovementComponent::ComputeCollectiveOutput(float Throttle) const
{
	const float Curved = UFPVFlightMath::ApplyThrottleCurve(Throttle, ThrottleMid, ThrottleExpo);
	return MotorIdle + (1.f - MotorIdle) * Curved;
}

void UFPVDroneMovementComponent::UpdateMotors(const FVector& NormalizedAngularDemand, float DeltaTime)
{
	const float Collective = ComputeCollectiveOutput(ControlInput.Throttle);

	float Commands[4];
	float MinCommand = TNumericLimits<float>::Max();
	float MaxCommand = TNumericLimits<float>::Lowest();
	for (int32 Motor = 0; Motor < 4; ++Motor)
	{
		const float Mix = FPVFlight::RollMix[Motor] * NormalizedAngularDemand.X
			+ FPVFlight::PitchMix[Motor] * NormalizedAngularDemand.Y
			+ FPVFlight::YawMix[Motor] * NormalizedAngularDemand.Z;
		Commands[Motor] = Collective + MixerAuthority * Mix;
		MinCommand = FMath::Min(MinCommand, Commands[Motor]);
		MaxCommand = FMath::Max(MaxCommand, Commands[Motor]);
	}

	// Air mode: shift all motors together so the rotation demand is kept even at 0% or 100% throttle.
	float Shift = 0.f;
	if (MaxCommand > 1.f)
	{
		Shift = 1.f - MaxCommand;
	}
	else if (MinCommand < MotorIdle)
	{
		Shift = MotorIdle - MinCommand;
	}

	for (int32 Motor = 0; Motor < 4; ++Motor)
	{
		const float Command = FMath::Clamp(Commands[Motor] + Shift, MotorIdle, 1.f);
		const float TimeConstant = Command > MotorOutputs[Motor] ? MotorSpinUpTime : MotorSpinDownTime;
		MotorOutputs[Motor] += (Command - MotorOutputs[Motor]) * (1.f - FMath::Exp(-DeltaTime / TimeConstant));
	}
}

void UFPVDroneMovementComponent::HandleBlockingHit(const FHitResult& Hit, const FVector& AttemptedDelta, float DeltaTime)
{
	const FVector Normal = Hit.Normal;
	const float NormalVelocity = FVector::DotProduct(Velocity, Normal);
	const float ImpactSpeed = FMath::Max(0.f, -NormalVelocity);

	// Let gameplay react first (detonation on a target, crash on a hard hit).
	OnImpact.Broadcast(Hit, ImpactSpeed);
	if (!bFlightEnabled || !UpdatedComponent)
	{
		return;
	}

	if (NormalVelocity < 0.f)
	{
		const float Restitution = ImpactSpeed > MinBounceSpeed ? BounceRestitution : 0.f;
		Velocity -= (1.f + Restitution) * NormalVelocity * Normal;

		const FVector NormalPart = FVector::DotProduct(Velocity, Normal) * Normal;
		const FVector TangentPart = Velocity - NormalPart;
		Velocity = NormalPart + TangentPart * FMath::Max(0.f, 1.f - SurfaceFriction * DeltaTime);
	}

	// Spend the rest of this step sliding along the surface instead of stopping dead.
	FHitResult SlideHit = Hit;
	SlideAlongSurface(AttemptedDelta, 1.f - Hit.Time, Normal, SlideHit, false);
}

float UFPVDroneMovementComponent::ComputeHeadingYaw(const FQuat& Rotation)
{
	const FVector Forward = Rotation.GetForwardVector();
	FVector Flat(Forward.X, Forward.Y, 0.f);
	if (Flat.SizeSquared() < 0.01f)
	{
		// Nose pointing straight up/down: the top of the drone tells us where "forward" was.
		const FVector Up = Rotation.GetUpVector() * (Forward.Z < 0.f ? 1.f : -1.f);
		Flat = FVector(Up.X, Up.Y, 0.f);
	}
	return Flat.IsNearlyZero() ? 0.f : FMath::RadiansToDegrees(FMath::Atan2(Flat.Y, Flat.X));
}

void UFPVDroneMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	LocalAngularVelocityDeg = FVector::ZeroVector;
	TargetAngularVelocityDeg = FVector::ZeroVector;
}

void UFPVDroneMovementComponent::SetThrottle(float NewThrottle)
{
	ControlInput.Throttle = FMath::Clamp(NewThrottle, 0.f, 1.f);
}

void UFPVDroneMovementComponent::AddThrottle(float DeltaThrottle)
{
	SetThrottle(ControlInput.Throttle + DeltaThrottle);
}

void UFPVDroneMovementComponent::SetPitchInput(float Value)
{
	ControlInput.Pitch = FMath::Clamp(Value, -1.f, 1.f);
}

void UFPVDroneMovementComponent::SetRollInput(float Value)
{
	ControlInput.Roll = FMath::Clamp(Value, -1.f, 1.f);
}

void UFPVDroneMovementComponent::SetYawInput(float Value)
{
	ControlInput.Yaw = FMath::Clamp(Value, -1.f, 1.f);
}

void UFPVDroneMovementComponent::SetControlInput(const FFPVDroneControlInput& NewInput)
{
	SetThrottle(NewInput.Throttle);
	SetPitchInput(NewInput.Pitch);
	SetRollInput(NewInput.Roll);
	SetYawInput(NewInput.Yaw);
}

void UFPVDroneMovementComponent::SetFlightMode(EFPVFlightMode NewMode)
{
	FlightMode = NewMode;
}

void UFPVDroneMovementComponent::ToggleFlightMode()
{
	SetFlightMode(FlightMode == EFPVFlightMode::Acro ? EFPVFlightMode::Angle : EFPVFlightMode::Acro);
}

void UFPVDroneMovementComponent::SetFlightEnabled(bool bEnabled)
{
	bFlightEnabled = bEnabled;
	if (!bEnabled)
	{
		ControlInput = FFPVDroneControlInput();
		CurrentThrustNewtons = 0.f;
		for (float& Output : MotorOutputs)
		{
			Output = 0.f;
		}
		StopMovementImmediately();
	}
}

float UFPVDroneMovementComponent::GetMotorOutput(int32 MotorIndex) const
{
	return (MotorIndex >= 0 && MotorIndex < 4) ? MotorOutputs[MotorIndex] : 0.f;
}

float UFPVDroneMovementComponent::GetAverageMotorOutput() const
{
	return 0.25f * (MotorOutputs[0] + MotorOutputs[1] + MotorOutputs[2] + MotorOutputs[3]);
}

float UFPVDroneMovementComponent::GetStaticThrustForThrottle(float Throttle) const
{
	return MaxThrustNewtons * FMath::Pow(ComputeCollectiveOutput(Throttle), ThrustExponent);
}

float UFPVDroneMovementComponent::GetThrustToWeightRatio() const
{
	const float Weight = FMath::Max(DroneMassKg * FPVFlight::GravityMagnitude(*this) * 0.01f, UE_KINDA_SMALL_NUMBER);
	return MaxThrustNewtons / Weight;
}

float UFPVDroneMovementComponent::GetHoverThrottle() const
{
	const float Weight = DroneMassKg * FPVFlight::GravityMagnitude(*this) * 0.01f;
	if (GetStaticThrustForThrottle(1.f) <= Weight)
	{
		return 1.f;
	}
	if (GetStaticThrustForThrottle(0.f) >= Weight)
	{
		return 0.f;
	}

	float Low = 0.f;
	float High = 1.f;
	for (int32 Iteration = 0; Iteration < 32; ++Iteration)
	{
		const float Mid = 0.5f * (Low + High);
		(GetStaticThrustForThrottle(Mid) < Weight ? Low : High) = Mid;
	}
	return 0.5f * (Low + High);
}
