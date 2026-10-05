#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Drone/FPVDroneTypes.h"
#include "FPVDronePawn.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UFPVCameraComponent;
class UFPVDroneMovementComponent;
class UFPVMotorSoundComponent;
class UFPVImpactSoundComponent;
class UInputAction;
class UInputMappingContext;
class AFPVExplosionActor;
class AFPVDronePawn;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFPVDroneDestroyedSignature, AFPVDronePawn*, Drone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFPVDroneCrashedSignature, AFPVDronePawn*, Drone, const FText&, Reason);

/**
 * Player-controlled FPV quadcopter.
 *
 * Responsibilities are split into components:
 *  - CollisionSphere      : root, the physical body used for collision sweeps.
 *  - DroneMovement        : the flight model (UFPVDroneMovementComponent). All flight tuning lives there.
 *  - CameraMount + FPVCamera : the FPV camera rigidly attached to the frame.
 *  - VisualRoot + meshes  : placeholder visuals (hidden from the pilot's own camera).
 *
 * This class translates Enhanced Input actions into flight commands and handles the payload:
 * when the drone flies into a target it detonates, damages the target and disables itself.
 */
UCLASS(Blueprintable)
class FPVGAME_API AFPVDronePawn : public APawn
{
	GENERATED_BODY()

public:
	AFPVDronePawn();

	//~ APawn interface
	virtual void PostInitializeComponents() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;
	virtual void FellOutOfWorld(const class UDamageType& DamageType) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;

	/** Explodes the drone at the hit location, damaging whatever was hit. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Payload")
	void Detonate(const FHitResult& Hit);

	/** Breaks the drone (hard crash, out of bounds...). The mission fails. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "FPV Drone|Crash")
	void Crash(const FText& Reason);

	/** True after a detonation or a crash. */
	UFUNCTION(BlueprintPure, Category = "FPV Drone")
	bool IsDroneDestroyed() const { return bDroneDestroyed; }

	UFUNCTION(BlueprintPure, Category = "FPV Drone")
	bool IsCrashed() const { return bCrashed; }

	/** False while an absolute (stick/trigger) throttle is waiting to be pulled to zero before it takes over. */
	UFUNCTION(BlueprintPure, Category = "FPV Drone|Input")
	bool IsThrottleArmed() const { return !bWaitingForThrottleArm; }

	UFUNCTION(BlueprintPure, Category = "FPV Drone")
	UFPVDroneMovementComponent* GetDroneMovement() const { return DroneMovement; }

	UFUNCTION(BlueprintPure, Category = "FPV Drone")
	UFPVCameraComponent* GetFPVCamera() const { return FPVCamera; }

	/** Broadcast right after the drone detonated or crashed. */
	UPROPERTY(BlueprintAssignable, Category = "FPV Drone|Events")
	FFPVDroneDestroyedSignature OnDroneDestroyed;

	/** Broadcast when the drone broke without hitting a target (the game mode fails the mission). */
	UPROPERTY(BlueprintAssignable, Category = "FPV Drone|Events")
	FFPVDroneCrashedSignature OnDroneCrashed;

	// --- Input (assigned in BP_FPVDrone, assets live in Content/Input) ---------------------------

	/** Mapping context with all flight controls. Added while this drone is possessed by a player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputMappingContext> DroneMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	int32 DroneMappingPriority = 1;

	/** Axis1D (keyboard W/S): +1 raises the throttle level, -1 lowers it (the level stays where you leave it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	/** Axis1D -1..1 (gamepad left stick Y): absolute throttle, -1 = 0%, 0 = 50%, +1 = 100%. Used in Stick mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> ThrottleStickAction;

	/** Axis1D 0..1 (gamepad R2 / right trigger): absolute throttle. Used in Trigger mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> ThrottleTriggerAction;

	/** Which analog control is the throttle on a gamepad / PS5 controller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Input")
	EFPVGamepadThrottleMode GamepadThrottleMode = EFPVGamepadThrottleMode::Stick;

	/**
	 * Like Betaflight's arming check: an absolute throttle is ignored until it has been at (almost) zero once,
	 * so a centered gamepad stick (= 50%) does not launch the drone on spawn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Input")
	bool bRequireThrottleLowToArm = true;

	/** Throttle level below which the absolute throttle arms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Input", meta = (ClampMin = "0", ClampMax = "0.5"))
	float ThrottleArmThreshold = 0.06f;

	/** Axis1D: +1 = pitch forward (nose down). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> PitchAction;

	/** Axis1D: +1 = roll right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> RollAction;

	/** Axis1D: +1 = yaw right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> YawAction;

	/** Boolean: switches between Acro and Angle flight modes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Input")
	TObjectPtr<UInputAction> ToggleFlightModeAction;

	/** How fast the throttle level changes while the throttle input is fully held (fraction per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Input", meta = (ClampMin = "0.05", UIMax = "3"))
	float ThrottleChangeRate = 0.6f;

	// --- Payload ----------------------------------------------------------------------------------

	/** Explosion spawned on detonation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Payload")
	TSubclassOf<AFPVExplosionActor> ExplosionClass;

	/** Damage dealt to the actor the drone crashes into when it detonates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPV Drone|Payload", meta = (ClampMin = "0"))
	float PayloadDamage = 100.f;

	// --- Crash ------------------------------------------------------------------------------------

	/** If false the drone survives every impact (practice mode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Crash")
	bool bCanCrash = true;

	/** Impact speed into a surface (cm/s, along the surface normal) that breaks the drone. 800 cm/s = 29 km/h. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Drone|Crash", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrashImpactSpeed = 800.f;

protected:
	/** Decides whether hitting this actor should detonate the payload (V0.1: only intact targets). */
	virtual bool ShouldDetonateOnImpact(const FHitResult& Hit) const;

	/** Blueprint hook: called after the drone detonated. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FPV Drone|Payload", meta = (DisplayName = "On Detonated"))
	void ReceiveDetonated(const FHitResult& Hit);

	UFUNCTION()
	void HandleMovementImpact(const FHitResult& Hit, float ImpactSpeed);

	void HandleThrottleInput(const FInputActionValue& Value);
	void HandleThrottleStickInput(const FInputActionValue& Value);
	void HandleThrottleTriggerInput(const FInputActionValue& Value);
	void ApplyAbsoluteThrottle(float Throttle01);
	void HandlePitchInput(const FInputActionValue& Value);
	void HandleRollInput(const FInputActionValue& Value);
	void HandleYawInput(const FInputActionValue& Value);
	void HandleToggleFlightMode(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> PropMeshes;

	/** Where the FPV camera is mounted. Rotate this later to add camera uptilt. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> CameraMount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPVCameraComponent> FPVCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPVDroneMovementComponent> DroneMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPVMotorSoundComponent> MotorSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPVImpactSoundComponent> CrashSound;

	bool bDroneDestroyed = false;
	bool bCrashed = false;
	bool bWaitingForThrottleArm = false;
	bool bAbsoluteThrottleArmed = false;
};
